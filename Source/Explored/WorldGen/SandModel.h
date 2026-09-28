#pragma once

#include "CoreMinimal.h"
#include "Save/SaveValue.h"

/**
 * Rejilla de la arena viva. Mismo paso y tamaño de chunk que `FTerrainEditSettings`
 * (0,25 m × 32 = 8 m), para que un chunk de arena y uno de edición volumétrica
 * cubran exactamente el mismo suelo.
 */
struct EXPLORED_API FSandSettings
{
	float CellSize = 0.25f;
	int32 CellsPerChunk = 32;

	float ChunkSizeMeters() const { return CellSize * CellsPerChunk; }
};

/**
 * Estado del entorno en el instante de la revisión. Metros, espacio de mundo.
 * La marea del día viene de `FOceanTide` escalada por la amplitud de la isla; las
 * olas se promedian, no se simulan aquí.
 */
struct EXPLORED_API FSandEnvironment
{
	/**
	 * Línea de pleamar del día. Toda la arena a esa altura o por debajo (la franja
	 * intermareal y lo que queda bajo el agua) cuenta como húmeda (biblia 02 §5.1).
	 */
	double HighTide = 0.0;
	/** Lluvia activa o arena regada: toda la arena cuenta como húmeda. */
	bool bRaining = false;
	/** Jugador anfitrión. */
	FVector2D Focus = FVector2D::ZeroVector;
	/** Más focos (en cooperativo, el resto de jugadores). Un chunk cerca de varios se revisa una vez. */
	TArray<FVector2D> ExtraFoci;
	/** Solo se revisan los chunks a menos de esta distancia de algún jugador (biblia 08 §2.6). */
	double ActiveRadius = 80.0;

	/** Distancia al foco más cercano. */
	double DistanceToFoci(const FVector2D& P) const;
};

/** Marea de un medio ciclo (~6 h de juego), para el relleno por oleaje (biblia 02 §5.2). */
struct EXPLORED_API FSandTide
{
	/** Línea de pleamar del medio ciclo. Por encima no llega el oleaje. */
	double HighTide = 0.5;
	/** Línea de bajamar. Por debajo el oleaje rellena con la tasa máxima. */
	double LowTide = -0.5;
	/** Marea viva (luna llena o nueva): rellena más. */
	bool bSpring = false;
};

/** Pincel de pala: cono con el máximo en el centro. Metros. */
struct EXPLORED_API FSandBrush
{
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 0.6f;
	/** Profundidad (cavar) o altura (apilar) en el centro de una pasada. */
	float Depth = 0.15f;
	/**
	 * Masa máxima que mueve la pasada, en «mm·columna» (1 mm de altura en una columna
	 * de CellSize²). Cavar: lo que cabe en el cubo; apilar: lo que lleva el jugador.
	 * 0 = sin tope al cavar; al apilar, 0 no apila nada.
	 */
	int64 MassBudget = 0;
};

/**
 * Traslado de arena de una columna a otra (mm de altura en una columna). Lo usan los
 * modelos que empujan arena sin pala: el surco de una balsa arrastrada, por ejemplo.
 */
struct EXPLORED_API FSandMove
{
	FIntPoint From = FIntPoint::ZeroValue;
	FIntPoint To = FIntPoint::ZeroValue;
	int32 Mm = 0;
};

/** Qué ha hecho una edición, una revisión de pendiente o un medio ciclo de marea. */
struct EXPLORED_API FSandResult
{
	/** Chunks cuya malla hay que rehacer, ordenados por (Y, X) y sin repetir. */
	TArray<FIntPoint> DirtyChunks;
	/** Masa movida por la herramienta (mm·columna, siempre ≥ 0). */
	int64 Mass = 0;
	/**
	 * Masa que ha puesto el mar en la playa en un medio ciclo (mm·columna). Negativa si
	 * el oleaje se ha llevado más de lo que ha traído. Sale de `SeaBankMass`.
	 */
	int64 SeaMass = 0;
	/** Columnas cuyo delta ha cambiado: es lo que sale por la cola de terreno. */
	int32 ColumnsChanged = 0;
	/** Esas columnas, ordenadas por (Y, X) y sin repetir: se codifican con `EncodePackets`. */
	TArray<FIntPoint> ChangedColumns;
	/** Columnas sucias revisadas (de chunks a menos de `ActiveRadius`). */
	int32 ActiveColumns = 0;
	/** Columnas sucias que se han dejado congeladas por estar lejos de todos los jugadores. */
	int32 DormantColumns = 0;
	/** Columnas que querían moverse y esperan a la revisión siguiente por el tope por chunk. */
	int32 DeferredColumns = 0;
	/** Chunks revisados. */
	int32 ActiveChunks = 0;
	/** Revisiones de más para los chunks que vuelven a estar cerca de un jugador (≤ 4). */
	int32 CatchUpRevisions = 0;
	int32 Ticks = 0;

	bool Changed() const { return ColumnsChanged > 0; }
};

/**
 * Arena viva (biblia 02 §5 y 08 §2.6; GDD v2 §3.13): cavar y apilar arena sobre un campo
 * de alturas local de deltas, solo en la capa de superficie. Es mucho más barato que la
 * edición volumétrica de `FTerrainEditModel`: una columna es un entero, no una pila de
 * muestras, y el remallado solo desplaza vértices en vertical.
 *
 * - Las columnas están en la rejilla global `Column * CellSize` (metros, esquinas de
 *   la malla). La altura final es `Base(X, Y) + Delta`. El delta se guarda en
 *   milímetros enteros y la altura base se cachea en milímetros al crear el chunk.
 * - **Masa:** todo movimiento es una transferencia entera. La avalancha mueve arena
 *   entre dos columnas; el oleaje la cambia con el banco del mar (`SeaBankMass`), que
 *   es la arena en suspensión de la resaca. `TotalMass() + SeaBankMass()` solo cambia
 *   con lo que la pala saca o echa.
 * - **Avalancha (§5.1):** una revisión por segundo. Entre dos vecinas (4-conectado), si
 *   el desnivel pasa del ángulo de reposo (34° seca, 45° húmeda) la alta cede parte del
 *   exceso a la baja. El umbral nunca es menor que el desnivel de la base: una duna
 *   natural más empinada no se derrumba sola; solo lo que el jugador ha movido.
 * - **Oleaje (§5.2):** una vez por medio ciclo de marea, `ApplyHalfTide` devuelve cada
 *   columna editada bajo la pleamar hacia su altura original: 20 % en la pleamar, que
 *   crece hacia el agua hasta el 60 % en la bajamar (35 % y 75 % en marea viva).
 * - **Anclaje (§5.3):** tablones de contención, pilotes, muelles y sacos sujetan toda la
 *   arena a menos de 1 m de su huella: no desliza ni la rellena el oleaje. La pala no
 *   cava bajo la huella.
 * - **Presupuesto (08 §2.6):** solo se revisan chunks a menos de 80 m de algún jugador;
 *   los demás se congelan. Cada revisión cambia como mucho 64 columnas por chunk; el
 *   resto espera. Un salto de tiempo largo se resuelve con 4 revisiones como máximo y
 *   el resto se descarta.
 * - **Determinismo:** revisiones de 1 s, flujos calculados sobre una instantánea
 *   (Jacobi) y columnas en orden (Y, X): el resultado no depende del orden de los
 *   mapas ni de cómo se trocee el tiempo. Solo lo simula el servidor.
 */
class EXPLORED_API FSandModel
{
public:
	using FBaseHeight = TFunctionRef<double(double X, double Y)>;

	// --- Números de diseño (biblia 02 §5 y 08 §2.6) ---

	static constexpr float DryReposeDeg = 34.0f;
	static constexpr float WetReposeDeg = 45.0f;
	/** Relleno por medio ciclo en la línea de pleamar y en la de bajamar (milésimas, marea normal). */
	static constexpr int32 RefillAtHighTideMilli = 200;
	static constexpr int32 RefillAtLowTideMilli = 600;
	/** Lo que suma la marea viva al relleno en cualquier punto (20 % → 35 % en la pleamar). */
	static constexpr int32 SpringRefillBonusMilli = 150;
	/** El oleaje remata un delta tan pequeño como este (mm): la última onda lo alisa del todo. */
	static constexpr int32 RefillSnapMm = 20;
	/** Distancia a la huella de una estructura dentro de la que la arena queda sujeta. */
	static constexpr float AnchorHoldMeters = 1.0f;
	/** Una avalancha mueve 1/AvalancheDivisor del exceso por pasada y pareja (4 es el límite estable con 4 vecinas). */
	static constexpr int32 AvalancheDivisor = 4;
	/** Pasadas de avalancha dentro de una revisión de 1 s. */
	static constexpr int32 SweepsPerTick = 4;
	/** Capa de arena sobre la roca: no se puede cavar más hondo. */
	static constexpr int32 MaxDigDepthMm = 1500;
	/** Altura máxima de un montón sobre la base. */
	static constexpr int32 MaxPileHeightMm = 2000;
	/** Tope de columnas que cambian por chunk y revisión (08 §2.6). */
	static constexpr int32 MaxChangedColumnsPerChunk = 64;
	/** Una revisión de pendiente por segundo (02 §5.1). */
	static constexpr int32 TickMs = 1000;
	/** Máximo de revisiones acumuladas por llamada; el tiempo sobrante se descarta (08 §2.6). */
	static constexpr int32 MaxTicksPerAdvance = 4;
	/**
	 * Revisiones que un chunk congelado recupera de golpe al acercarse un jugador (08 §2.6):
	 * las que se ha saltado, como mucho estas.
	 */
	static constexpr int32 MaxCatchUpRevisions = 4;
	/** Cambio de la pleamar que obliga a revisar qué columnas editadas están ahora húmedas. */
	static constexpr int32 TideWakeStepMm = 50;
	/** Cota de columna (en celdas): lejos del borde de int32, así Column + 1 y los bucles X <= Hi no desbordan. */
	static constexpr int32 MaxAbsColumn = 1000000000;
	/** Tope de columnas sucias en una partida guardada (~16 000 m² de arena sin asentar en celdas de 0,25 m). */
	static constexpr int32 MaxSavedDirtyColumns = 1 << 18;

	/** Desnivel máximo (mm) entre dos columnas vecinas para un ángulo dado. */
	static int32 ReposeDropMm(float AngleDeg, float CellSize);
	/**
	 * Fracción (milésimas) del delta que el oleaje devuelve en un medio ciclo a una columna
	 * cuya altura original es BaseMm. 0 por encima de la pleamar.
	 */
	static int32 RefillMilli(int64 BaseMm, int64 HighMm, int64 LowMm, bool bSpring);
	/** Masa (mm·columna) → m³. */
	double MassToCubicMeters(int64 Mass) const;
	int64 CubicMetersToMass(double CubicMeters) const;

	explicit FSandModel(const FSandSettings& InSettings = FSandSettings());

	// --- Herramientas ---

	/** Cava con la pala; `Mass` es lo que va al cubo. Bajo la huella de una estructura no se cava. */
	FSandResult Dig(const FSandBrush& Brush, FBaseHeight Base);
	/** Echa arena; `Mass` es lo que sale del inventario (≤ MassBudget). */
	FSandResult Pile(const FSandBrush& Brush, FBaseHeight Base);

	/**
	 * Mueve arena entre columnas, en el orden dado; cada traslado ve el resultado de los
	 * anteriores. La masa se conserva exactamente: lo que sale de From entra en To.
	 * Un traslado se recorta a lo que From puede dar (MaxDigDepthMm) y To puede recibir
	 * (MaxPileHeightMm); si toca la huella de una estructura o sale de la rejilla, no se hace.
	 * `Mass` es la suma movida.
	 */
	FSandResult Transfer(const TArray<FSandMove>& Moves, FBaseHeight Base);

	// --- Estructuras ---

	/**
	 * Pone (o quita, con bAnchor = false) una estructura cuya huella es la caja. Las columnas
	 * de la caja son su huella; las que quedan a ≤ AnchorHoldMeters de la caja, arena sujeta.
	 * Se cuentan referencias: dos estructuras solapadas sujetan hasta que se quitan las dos.
	 */
	FSandResult SetAnchor(const FVector2D& Min, const FVector2D& Max, bool bAnchor, FBaseHeight Base);
	/**
	 * Piezas de `building_pieces.json` que sujetan la arena (biblia 02 §5.3): el tablón de
	 * contención, los pilotes y el muelle. El sistema de construcción llama a `SetAnchor` con
	 * la huella de cada una al ponerla o quitarla; el resto de piezas no toca la arena.
	 */
	static const TArray<FString>& SandAnchorPieces();
	static bool PieceAnchorsSand(const FString& PieceId);
	/** Bajo la huella de alguna estructura. */
	bool IsAnchored(const FIntPoint& Column) const;
	/** A menos de 1 m de alguna estructura: no desliza ni la rellena el oleaje. */
	bool IsHeld(const FIntPoint& Column) const;

	// --- Simulación ---

	/** Avanza DeltaMs milisegundos en revisiones de 1 s (máximo MaxTicksPerAdvance; el resto se descarta). */
	FSandResult Advance(int32 DeltaMs, const FSandEnvironment& Env, FBaseHeight Base);
	/**
	 * Una revisión de pendiente. Si un chunk vuelve a estar cerca de un jugador tras saltarse
	 * revisiones, las recupera de golpe aquí mismo (como mucho MaxCatchUpRevisions).
	 */
	FSandResult Tick(const FSandEnvironment& Env, FBaseHeight Base);
	/**
	 * Un medio ciclo de marea: el servidor lo llama al pasar cada pleamar o bajamar. Toca
	 * todas las columnas editadas bajo la pleamar, cerca o lejos de los jugadores.
	 */
	FSandResult ApplyHalfTide(const FSandTide& Tide, FBaseHeight Base);

	// --- Consultas ---

	const FSandSettings& GetSettings() const { return Settings; }
	FIntPoint ColumnOf(double X, double Y) const;
	FVector2D ColumnPosition(const FIntPoint& Column) const;
	FIntPoint ChunkOfColumn(const FIntPoint& Column) const;
	/** Distancia de P al chunk (0 si P está dentro). */
	double DistanceToChunk(const FVector2D& P, const FIntPoint& Chunk) const;
	/** Delta de la columna en mm (0 si no se ha tocado). */
	int32 DeltaMm(const FIntPoint& Column) const;
	/** Altura final de la columna en metros. */
	double Height(const FIntPoint& Column, FBaseHeight Base) const;
	/** Σ delta de todas las columnas (mm·columna). */
	int64 TotalMass() const;
	/** Arena que tiene el mar: lo que se ha llevado el oleaje menos lo que ha traído. */
	int64 SeaBankMass() const { return SeaBank; }
	int32 NumDirtyColumns() const { return Dirty.Num(); }
	bool IsDirty(const FIntPoint& Column) const { return Dirty.Contains(Column); }
	/** Chunks que leen la columna (1, 2 o 4): la malla de un chunk usa una columna de margen para las normales. */
	void ChunksReadingColumn(const FIntPoint& Column, TArray<FIntPoint>& Out) const;
	/** Chunks con algún delta distinto de cero, ordenados por (Y, X). */
	TArray<FIntPoint> EditedChunks() const;
	bool IsEmpty() const;
	void Reset();

	// --- Red (biblia 08 §2.2 y §2.6) ---

	/** Tope de bytes por paquete, el mismo que los deltas volumétricos: nunca fragmenta. */
	static constexpr int32 MaxPacketBytes = 512;
	/** `FExploredTerrainDeltaPacket` versión 2: la versión 1 es la volumétrica, sin capa. */
	static constexpr uint8 PacketVersion = 2;
	/** Capa del paquete versión 2: 0 = densidad (`FTerrainEditModel`), 1 = arena. */
	static constexpr uint8 PacketLayerSand = 1;
	/** Bit de `Flags`: vaciar el chunk antes de aplicar (primer paquete de un chunk completo). */
	static constexpr uint8 PacketFlagReset = 1;

	/**
	 * Paquetes con el valor actual (absoluto, también 0) de las columnas dadas que caen en el
	 * chunk. Las de otros chunks se ignoran. Aplicarlos dos veces da lo mismo que una.
	 */
	TArray<TArray<uint8>> EncodePackets(const FIntPoint& Chunk, const TArray<FIntPoint>& Columns) const;
	/** El chunk completo para un cliente que llega (el primer paquete lleva `PacketFlagReset`). */
	TArray<TArray<uint8>> EncodeFullChunk(const FIntPoint& Chunk) const;
	/**
	 * Lado cliente: valida el paquete entero y, si es correcto, lo aplica. No marca nada sucio
	 * (el cliente no simula arena). Devuelve false, sin tocar nada, si el paquete no es válido.
	 */
	bool ApplyPacket(const TArray<uint8>& Packet, FBaseHeight Base, FSandResult* OutResult = nullptr);
	/** FNV-1a de los 1 024 deltas del chunk en int16 little-endian: la comprobación de cada 30 s. */
	uint32 ChunkChecksum(const FIntPoint& Chunk) const;

	// --- Guardado ---

	/**
	 * {"v":1,"cell":0.25,"n":32,"chunks":[[CX,CY,[Inicio,Cuenta,d…,…]],…],"dirty":[X,Y,…],"sea":S}.
	 * Los anclajes no se guardan: los vuelve a poner el sistema de construcción al cargar.
	 */
	FSaveValue ToValue() const;
	/** Sustituye el contenido; devuelve false (y queda vacío) si no es válido o es de otra rejilla. */
	bool FromValue(const FSaveValue& Value);

	/** Mismos deltas, mismas columnas sucias y mismo banco del mar (los anclajes y la caché de base no cuentan). */
	bool operator==(const FSandModel& Other) const;
	bool operator!=(const FSandModel& Other) const { return !(*this == Other); }

private:
	struct FChunk
	{
		TArray<int32> Delta;
		TArray<int32> BaseMm;
		/** Referencias de huella (no se cava). */
		TArray<uint8> Anchor;
		/** Referencias de sujeción (≤ 1 m de una huella: no desliza ni se rellena). */
		TArray<uint8> Hold;
		int32 NonZero = 0;
		bool bBaseValid = false;
	};

	int32 LocalIndex(const FIntPoint& Column, const FIntPoint& Chunk) const;
	FChunk& TouchChunk(const FIntPoint& Chunk, FBaseHeight Base);
	FChunk& TouchColumn(const FIntPoint& Column, FBaseHeight Base, int32& OutLocal);
	const FChunk* FindChunk(const FIntPoint& Column, int32& OutLocal) const;
	void AddDelta(const FIntPoint& Column, int32 Amount, FBaseHeight Base);
	void MarkDirtyAround(const FIntPoint& Column);
	FSandResult Brush(const FSandBrush& Brush, FBaseHeight Base, bool bDig);
	void WakeForTide(const FSandEnvironment& Env);
	/** Una revisión de pendiente; con OnlyChunks, solo las columnas de esos chunks. */
	FSandResult Revise(const FSandEnvironment& Env, FBaseHeight Base, const TMap<FIntPoint, uint8>* OnlyChunks);
	bool ChunkIsActive(const FIntPoint& Chunk, const FSandEnvironment& Env) const;
	static void FinishDirtyChunks(TArray<FIntPoint>& Chunks);

	FSandSettings Settings;
	TMap<FIntPoint, FChunk> Chunks;
	/** Columnas pendientes de revisar (el valor no se usa). */
	TMap<FIntPoint, uint8> Dirty;
	/** Arena del mar (mm·columna): sube cuando el oleaje alisa un montón y baja cuando rellena un hoyo. */
	int64 SeaBank = 0;
	/**
	 * Chunks con deltas a los que no ha llegado el último cambio de pleamar o de lluvia por
	 * estar lejos de todos los jugadores: se revisan cuando alguien se acerca.
	 */
	TMap<FIntPoint, uint8> StaleWetChunks;
	/** Revisiones que se ha saltado cada chunk con arena pendiente lejos de los jugadores (≤ 4). */
	TMap<FIntPoint, int32> FrozenRevisions;
	int32 AccumulatedMs = 0;
	/** Pleamar (mm) y lluvia de la última revisión de columnas editadas. */
	int64 LastWakeHighMm = 0;
	bool bLastWakeRaining = false;
	bool bHasWoken = false;
};
