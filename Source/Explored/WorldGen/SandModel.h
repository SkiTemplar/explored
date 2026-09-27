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
 * Estado del entorno en el instante del paso. Metros, espacio de mundo.
 * `SeaLevel` es el nivel del agua con la marea (`FOceanTide::Level` escalado por la
 * amplitud de la isla); las olas se promedian, no se simulan aquí.
 */
struct EXPLORED_API FSandEnvironment
{
	double SeaLevel = 0.0;
	/** Lluvia o rocío fuerte: toda la arena cuenta como húmeda. */
	bool bRaining = false;
	/** Centro de la zona activa: el jugador. */
	FVector2D Focus = FVector2D::ZeroVector;
	/** Más focos (en cooperativo, el resto de jugadores). Una columna cerca de varios se simula una vez. */
	TArray<FVector2D> ExtraFoci;
	/** Solo se simulan las columnas sucias a menos de esta distancia de algún foco. */
	double ActiveRadius = 24.0;

	/** Distancia al foco más cercano. */
	double DistanceToFoci(const FVector2D& P) const;
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

/** Qué ha hecho una edición o un paso de simulación. */
struct EXPLORED_API FSandResult
{
	/** Chunks cuya malla hay que rehacer, ordenados por (Y, X) y sin repetir. */
	TArray<FIntPoint> DirtyChunks;
	/** Masa movida por la herramienta (mm·columna, siempre ≥ 0). */
	int64 Mass = 0;
	int32 ColumnsChanged = 0;
	/** Columnas sucias simuladas en este paso (dentro del radio activo). */
	int32 ActiveColumns = 0;
	/** Columnas sucias que se han dejado dormidas por estar lejos del foco. */
	int32 DormantColumns = 0;
	int32 Ticks = 0;

	bool Changed() const { return ColumnsChanged > 0; }
};

/**
 * Arena viva (GDD v2 §3.13): cavar y apilar arena sobre un campo de alturas local de
 * deltas, solo en la capa de superficie. Es mucho más barato que la edición
 * volumétrica de `FTerrainEditModel`: una columna es un entero, no una pila de
 * muestras, y el remallado solo desplaza vértices en vertical.
 *
 * - Las columnas están en la rejilla global `Column * CellSize` (metros, esquinas de
 *   la malla). La altura final es `Base(X, Y) + Delta`. El delta se guarda en
 *   milímetros enteros y la altura base se cachea en milímetros al crear el chunk.
 * - **Masa:** todo movimiento (herramienta, avalancha, olas) es una transferencia
 *   entera entre dos columnas, así que Σ delta solo cambia con lo que la pala saca
 *   o echa. `TotalMass()` lo comprueba.
 * - **Avalancha:** entre dos vecinas (4-conectado), si el desnivel pasa del ángulo de
 *   reposo (34° seca, 45° húmeda) se transfiere parte del exceso. El umbral nunca es
 *   menor que el desnivel de la base: una duna natural más empinada no se derrumba
 *   sola; solo lo que el jugador ha movido.
 * - **Olas:** en la franja intermareal los deltas se difunden entre vecinas con una
 *   tasa que crece hacia el agua: los agujeros se rellenan y los montones se alisan.
 * - **Anclajes:** las estructuras (tablones, pilotes, muelles, sacos) congelan sus
 *   columnas y endurecen las de alrededor (reposo 60°, olas ×0,25).
 * - **Coste:** solo se simulan columnas sucias (tocadas, o vecinas de algo que se
 *   movió) a menos de `ActiveRadius` del foco. Las demás quedan dormidas, conservan
 *   su estado y siguen sucias hasta que el jugador vuelve.
 * - **Determinismo:** paso fijo de 100 ms, flujos calculados sobre una instantánea
 *   (Jacobi) y columnas en orden (Y, X): el resultado no depende del orden de los
 *   mapas ni de cómo se trocee el tiempo.
 */
class EXPLORED_API FSandModel
{
public:
	using FBaseHeight = TFunctionRef<double(double X, double Y)>;

	// --- Números de diseño (GDD v2 §3.13) ---

	static constexpr float DryReposeDeg = 34.0f;
	static constexpr float WetReposeDeg = 45.0f;
	/** Reposo de la arena pegada a una estructura. */
	static constexpr float AnchoredReposeDeg = 60.0f;
	/** La arena está húmeda hasta esta altura sobre el nivel del agua (salpicadura y capilaridad). */
	static constexpr float WetBandMeters = 0.6f;
	/** Las olas llegan hasta esta altura sobre el nivel del agua; su efecto crece hacia abajo. */
	static constexpr float SwashHeightMeters = 1.0f;
	/** Por debajo de esta profundidad el oleaje ya no remueve el fondo. */
	static constexpr float SwashDepthMeters = 1.5f;
	/** Fracción de la diferencia de delta que igualan las olas por paso en la orilla (milésimas). */
	static constexpr int32 WaveRateMilliAtShore = 80;
	/** Factor de las olas junto a una estructura (milésimas). */
	static constexpr int32 AnchoredWaveMilli = 250;
	/** Una avalancha mueve 1/AvalancheDivisor del exceso por paso y pareja. */
	static constexpr int32 AvalancheDivisor = 5;
	/** Capa de arena sobre la roca: no se puede cavar más hondo. */
	static constexpr int32 MaxDigDepthMm = 1500;
	/** Altura máxima de un montón sobre la base. */
	static constexpr int32 MaxPileHeightMm = 2000;
	/** Tope de columnas simuladas por paso; las más lejanas esperan al siguiente. */
	static constexpr int32 MaxActiveColumnsPerTick = 4096;
	static constexpr int32 TickMs = 100;
	/** Tope de pasos por llamada a Advance (el resto se acumula para la siguiente). */
	static constexpr int32 MaxTicksPerAdvance = 20;
	/** Cambio de marea que obliga a revisar qué columnas editadas mojan ahora las olas. */
	static constexpr int32 TideWakeStepMm = 50;

	/** Desnivel máximo (mm) entre dos columnas vecinas para un ángulo dado. */
	static int32 ReposeDropMm(float AngleDeg, float CellSize);
	/** Peso de las olas (milésimas, 0–1000) para una columna a HeightMm con el agua a SeaMm. */
	static int32 WaveWeightMilli(int64 HeightMm, int64 SeaMm);
	/** Masa (mm·columna) → m³. */
	double MassToCubicMeters(int64 Mass) const;
	int64 CubicMetersToMass(double CubicMeters) const;

	explicit FSandModel(const FSandSettings& InSettings = FSandSettings());

	// --- Herramientas ---

	/** Cava con la pala; `Mass` es lo que va al cubo. Las columnas ancladas no se tocan. */
	FSandResult Dig(const FSandBrush& Brush, FBaseHeight Base);
	/** Echa arena; `Mass` es lo que sale del inventario (≤ MassBudget). */
	FSandResult Pile(const FSandBrush& Brush, FBaseHeight Base);

	// --- Estructuras ---

	/** Ancla (o suelta, con bAnchor = false) las columnas cuyo punto cae en la caja. Se cuentan referencias. */
	FSandResult SetAnchor(const FVector2D& Min, const FVector2D& Max, bool bAnchor, FBaseHeight Base);
	bool IsAnchored(const FIntPoint& Column) const;

	// --- Simulación ---

	/** Avanza DeltaMs milisegundos en pasos fijos (máximo MaxTicksPerAdvance por llamada). */
	FSandResult Advance(int32 DeltaMs, const FSandEnvironment& Env, FBaseHeight Base);
	/** Un paso de simulación. */
	FSandResult Tick(const FSandEnvironment& Env, FBaseHeight Base);

	// --- Consultas ---

	const FSandSettings& GetSettings() const { return Settings; }
	FIntPoint ColumnOf(double X, double Y) const;
	FVector2D ColumnPosition(const FIntPoint& Column) const;
	FIntPoint ChunkOfColumn(const FIntPoint& Column) const;
	/** Delta de la columna en mm (0 si no se ha tocado). */
	int32 DeltaMm(const FIntPoint& Column) const;
	/** Altura final de la columna en metros. */
	double Height(const FIntPoint& Column, FBaseHeight Base) const;
	/** Σ delta de todas las columnas (mm·columna). Solo cambia con Dig y Pile. */
	int64 TotalMass() const;
	int32 NumDirtyColumns() const { return Dirty.Num(); }
	bool IsDirty(const FIntPoint& Column) const { return Dirty.Contains(Column); }
	/** Chunks que leen la columna (1, 2 o 4): la malla de un chunk usa una columna de margen para las normales. */
	void ChunksReadingColumn(const FIntPoint& Column, TArray<FIntPoint>& Out) const;
	/** Chunks con algún delta distinto de cero, ordenados por (Y, X). */
	TArray<FIntPoint> EditedChunks() const;
	bool IsEmpty() const;
	void Reset();

	// --- Guardado ---

	/**
	 * {"v":1,"cell":0.25,"n":32,"chunks":[[CX,CY,[Inicio,Cuenta,d…,…]],…],"dirty":[X,Y,…]}.
	 * Los anclajes no se guardan: los vuelve a poner el sistema de construcción al cargar.
	 */
	FSaveValue ToValue() const;
	/** Sustituye el contenido; devuelve false (y queda vacío) si no es válido o es de otra rejilla. */
	bool FromValue(const FSaveValue& Value);

	/** Mismos deltas y mismas columnas sucias (los anclajes y la caché de base no cuentan). */
	bool operator==(const FSandModel& Other) const;
	bool operator!=(const FSandModel& Other) const { return !(*this == Other); }

private:
	struct FChunk
	{
		TArray<int32> Delta;
		TArray<int32> BaseMm;
		TArray<uint8> Anchor;
		int32 NonZero = 0;
		bool bBaseValid = false;
	};

	int32 LocalIndex(const FIntPoint& Column, const FIntPoint& Chunk) const;
	FChunk& TouchChunk(const FIntPoint& Chunk, FBaseHeight Base);
	FChunk& TouchColumn(const FIntPoint& Column, FBaseHeight Base, int32& OutLocal);
	const FChunk* FindChunk(const FIntPoint& Column, int32& OutLocal) const;
	int64 HeightMm(const FIntPoint& Column, FBaseHeight Base);
	bool NearAnchor(const FIntPoint& Column) const;
	void AddDelta(const FIntPoint& Column, int32 Amount, FBaseHeight Base);
	void MarkDirtyAround(const FIntPoint& Column);
	FSandResult Brush(const FSandBrush& Brush, FBaseHeight Base, bool bDig);
	void WakeForTide(const FSandEnvironment& Env, FBaseHeight Base);
	static void FinishDirtyChunks(TArray<FIntPoint>& Chunks);

	FSandSettings Settings;
	TMap<FIntPoint, FChunk> Chunks;
	/** Columnas pendientes de simular (el valor no se usa). */
	TMap<FIntPoint, uint8> Dirty;
	/** Columnas con algún anclaje: si no hay ninguna, no se buscan vecinas ancladas. */
	int32 AnchoredColumns = 0;
	int32 AccumulatedMs = 0;
	/** Nivel del agua (mm) y lluvia de la última revisión de columnas editadas. */
	int64 LastWakeSeaMm = 0;
	bool bLastWakeRaining = false;
	bool bHasWoken = false;
};
