#pragma once

#include "CoreMinimal.h"
#include "Save/SaveValue.h"

/** Estado de una celda de la rejilla de riesgos. */
enum class EMineCell : uint8
{
	/** Roca o tierra (y escombro de un derrumbe). */
	Solid,
	/** Hueco con aire: galería, sala o el aire sobre el suelo. */
	Open,
	/**
	 * Pieza `pared` calzada contra la roca (biblia 02 §2.4): sella el paso del agua y del
	 * aire, y apoya el techo como la roca.
	 */
	Wall,
	/** Agua de mar abierta (el fondo marino que se pica cuenta como conectado al mar, §2.7). */
	Sea,
};

/**
 * Rejilla de riesgos de una mina. Caja fija alrededor de las galerías; fuera de la caja todo
 * cuenta como roca. La capa superior (Z = Size.Z − 1) tiene que quedar por encima del suelo:
 * una celda abierta que ve el cielo en vertical es una salida (boca o chimenea).
 */
struct EXPLORED_API FMineGridSettings
{
	/** Esquina mínima de la celda (0, 0, 0), metros. */
	FVector Origin = FVector::ZeroVector;
	FIntVector Size = FIntVector(32, 32, 16);
	/** Lado de la celda. 0,5 m: el hueco de un golpe de pico ocupa una celda. */
	float CellSize = 0.5f;
};

/** Agua alrededor de la mina en el instante de la revisión. Metros, espacio de mundo. */
struct EXPLORED_API FMineWaterEnvironment
{
	/** Superficie del mar ahora mismo (marea incluida). */
	double SeaLevel = 0.0;
	/** Nivel freático: la cota 0 del mapa (biblia 02 §2.4). */
	double WaterTable = 0.0;
	/** Lluvia intensa o menos de 2 h desde que paró: el agua sube un 50 % más rápido (§5.4). */
	bool bHeavyRain = false;
};

/** Qué oye y ve el jugador (y el resto del grupo): nunca un texto de HUD. */
enum class EMineHazardEventKind : uint8
{
	/** Crujido y polvo: faltan 2 s para que ceda el techo. «Esto no aguanta.» */
	CollapseWarning,
	/** El techo cede y el hueco se llena de escombro. */
	Collapse,
	/** Un techo que iba a caer ha quedado apoyado a tiempo (logro `viga_a_tiempo`). */
	CollapseAverted,
	/** Una viga se ha quedado sin techo o sin suelo, o la ha tapado un derrumbe. */
	BeamLost,
};

struct EXPLORED_API FMineHazardEvent
{
	EMineHazardEventKind Kind = EMineHazardEventKind::CollapseWarning;
	/** Centro del techo afectado (media de las celdas), metros. */
	FVector Location = FVector::ZeroVector;
	/** Celdas de techo del hueco. */
	int32 Cells = 0;
};

struct EXPLORED_API FMineHazardResult
{
	/** En orden de celda (Z, Y, X) dentro de cada revisión. */
	TArray<FMineHazardEvent> Events;
	/** Celdas que un derrumbe ha rellenado: el mundo las devuelve al terreno como tierra. */
	TArray<FIntVector> CollapsedCells;
	/** Alguna galería gana agua: sonido de agua entrando. «Está entrando agua.» */
	bool bWaterEntering = false;
	int32 Ticks = 0;
};

/** Aire de un jugador bajo tierra (biblia 02 §2.4). Vive en el cuerpo de cada jugador. */
struct EXPLORED_API FMineAirState
{
	/** 1 = aire limpio, 0 = sin aire (nunca mata por sí solo). */
	float Air = 1.0f;
	/** Segundos seguidos en aire viciado (los 2 primeros minutos no bajan el aire). */
	float StaleSeconds = 0.0f;
};

/** Lo que el cuerpo enseña del aire, sin HUD (01 y 02 §2.6). */
struct EXPLORED_API FMineAirSignals
{
	/** Respiración agitada: ya se está perdiendo aire. «Me falta el aire aquí abajo.» */
	bool bLaboredBreathing = false;
	/** Mareo: cierre de la visión por los bordes, 0 con el 20 % de aire, 1 sin aire. */
	float Dizziness = 0.0f;
};

/**
 * Riesgos de una mina (biblia 02 §2.4 y §2.7) sobre una rejilla de celdas de 0,5 m:
 *
 * - **Derrumbe:** una celda de techo (abierta con roca justo encima) está sin apoyo si
 *   ningún apoyo de su misma capa (roca, `pared` o una `viga_apoyo`) queda a 1,5 m o
 *   menos: es decir, si el techo salva más de **3 m de luz**. Al quedar expuesto empieza
 *   una cuenta de **8 s**; a los 6 s cruje; a los 8 s el hueco se llena de escombro desde
 *   el techo hasta el suelo. Poner una viga antes lo evita.
 * - **Viga de apoyo:** un poste de suelo a techo de 3 m como mucho. Apoya el techo en un
 *   radio de 1,5 m (por eso una viga en el centro salva una sala de hasta 4 m).
 * - **Aire viciado:** distancia por dentro de la galería (26 vecinas) a la celda abierta
 *   más cercana que ve el cielo. A más de **15 m**, o sin camino, el aire está viciado:
 *   tras 2 min de gracia baja un 4 %/min; por debajo del 20 % marea.
 * - **Inundación:** una galería (componente 6-conexa de celdas abiertas) que toca el mar o
 *   baja del nivel freático se llena **1 m cada 40 s** (+50 % con lluvia intensa) hasta el
 *   nivel de la fuente, y se vacía igual si la fuente baja (la marea). Una `pared` entre
 *   la galería y la brecha la sella. El agua de una galería sellada se queda.
 * - **Crecida de monzón:** `MonsoonFloodFraction` (30 % durante la crecida, vacía en 2 días).
 *
 * El tiempo avanza en revisiones fijas de 250 ms: el resultado no depende de cómo se
 * trocee. Todo va en orden de celda: misma rejilla y mismas llamadas, mismo resultado.
 *
 * Red (biblia 08 §2.12): solo lo simula el servidor. El escombro de un derrumbe sale como
 * deltas de terreno normales; los avisos y el derrumbe son multicast cosméticos; el nivel
 * de agua de cada galería inundada se replica a 1 Hz; el aire de cada jugador va con su
 * cuerpo, solo a su dueño (§2.9).
 */
class EXPLORED_API FMineHazardModel
{
public:
	// --- Números de diseño (biblia 02 §2.4) ---

	/** Luz máxima sin apoyo: ningún punto del techo a más de la mitad de un apoyo. */
	static constexpr float MaxSpanMeters = 3.0f;
	static constexpr int32 CollapseDelayMs = 8000;
	/** El crujido y el polvo empiezan 2 s antes. */
	static constexpr int32 CollapseWarningMs = 6000;
	/** Altura máxima de una viga (dos troncos pequeños). */
	static constexpr float MaxBeamHeightMeters = 3.0f;
	/** Distancia a una salida a partir de la que el aire está viciado. */
	static constexpr float StaleAirDistanceMeters = 15.0f;
	static constexpr float StaleAirGraceSeconds = 120.0f;
	/** Pérdida de aire por minuto tras la gracia. */
	static constexpr float AirLossPerMinute = 0.04f;
	/** [Decisión] Al volver a aire limpio se recupera un 25 % por minuto. */
	static constexpr float AirRecoveryPerMinute = 0.25f;
	/** Por debajo de este aire empieza el mareo. */
	static constexpr float DizzyBelowAir = 0.2f;
	/** Subida del agua sin sellar: 1 m cada 40 s. */
	static constexpr double FloodRiseMetersPerSecond = 1.0 / 40.0;
	static constexpr double HeavyRainFloodFactor = 1.5;
	/** Crecida de monzón: una mina bajo el río se inunda al 30 % y se vacía en 2 días. */
	static constexpr double MonsoonFloodShare = 0.3;
	static constexpr double MonsoonDrainHours = 48.0;
	/** Paso de la simulación y tope de revisiones por llamada (60 s; el resto se descarta). */
	static constexpr int32 TickMs = 250;
	static constexpr int32 MaxTicksPerAdvance = 240;
	/** Tope de celdas de la caja (una isla entera no cabe; una mina, de sobra). */
	static constexpr int32 MaxCells = 1 << 22;

	/** Avanza el aire de un jugador; bStale = está en aire viciado (y no bajo el agua). */
	static void AdvanceAir(FMineAirState& State, bool bStale, float Seconds);
	static FMineAirSignals AirSignals(const FMineAirState& State);
	/**
	 * Fracción de la mina bajo el cauce que está inundada por la crecida: 0,3 durante la
	 * crecida y, al acabar, baja en línea recta hasta 0 en 48 h.
	 */
	static double MonsoonFloodFraction(bool bFlooding, double HoursSinceFloodEnded);

	explicit FMineHazardModel(const FMineGridSettings& InSettings = FMineGridSettings());

	// --- Rejilla ---

	const FMineGridSettings& GetSettings() const { return Settings; }
	bool IsInside(const FIntVector& Cell) const;
	/** Celda que contiene el punto (puede quedar fuera de la caja). */
	FIntVector CellOf(const FVector& P) const;
	FVector CellCenter(const FIntVector& Cell) const;
	/** Fuera de la caja: Solid. */
	EMineCell GetCell(const FIntVector& Cell) const;
	/** Cambia una celda; fuera de la caja no hace nada. */
	void SetCell(const FIntVector& Cell, EMineCell State);
	/**
	 * Rellena la caja desde un campo de densidad (> 0 aire, convenio de `FTerrainDensity`,
	 * medido en el centro de la celda). Las celdas abiertas que IsSea marque pasan a mar.
	 */
	void FillFromDensity(TFunctionRef<float(const FVector&)> Density, TFunctionRef<bool(const FVector&)> IsSea);

	// --- Vigas ---

	/** Se puede poner una viga aquí: celda abierta, con suelo y techo a ≤ 3 m, sin otra viga. */
	bool CanPlaceBeam(const FVector& P) const;
	bool PlaceBeam(const FVector& P);
	bool RemoveBeam(const FVector& P);
	int32 NumBeams() const { return Beams.Num(); }

	// --- Simulación ---

	FMineHazardResult Advance(int32 DeltaMs, const FMineWaterEnvironment& Env);

	// --- Consultas ---

	/** Celda de techo sin apoyo (la cuenta de 8 s corre o va a correr). */
	bool IsRoofUnsupported(const FIntVector& Cell) const;
	/** Celdas de techo sin apoyo, en orden (Z, Y, X). */
	TArray<FIntVector> UnsupportedRoofCells() const;
	/** Milisegundos que lleva expuesta una celda de techo sin apoyo (0 si no). */
	int32 ExposureMs(const FIntVector& Cell) const;
	/** Metros por dentro hasta la salida más cercana; negativo si no hay camino o no es aire. */
	float AirDistanceMeters(const FIntVector& Cell) const;
	/** Aire viciado en el punto (fuera de la caja o en roca: false). */
	bool IsStaleAir(const FVector& P) const;
	/** Superficie del agua de la galería de la celda; −∞ si está seca o no es una galería. */
	double WaterLevel(const FIntVector& Cell) const;
	/** Metros de agua sobre el punto (0 si seco). */
	double WaterDepthAt(const FVector& P) const;
	/** Galerías distintas (componentes de celdas abiertas). */
	int32 NumGalleries() const;

	// --- Guardado ---

	/**
	 * {"v":1,"water":[[X,Y,Z,Nivel],…]} en milímetros de mundo: una celda de cada galería
	 * con agua. La rejilla sale del terreno, las vigas de la construcción y la cuenta de
	 * derrumbe no se guarda (al cargar vuelve a empezar: 8 s de margen al jugador).
	 */
	FSaveValue ToValue() const;
	/** El agua se reparte a su galería en la revisión siguiente. false (y sin agua) si no es válido. */
	bool FromValue(const FSaveValue& Value);

private:
	struct FGallery
	{
		/** Primera celda en orden (Z, Y, X): la que representa la galería en el guardado. */
		int32 FirstCell = 0;
		/** Suelo (base de la celda más baja) y techo (cima de la más alta), metros. */
		double Floor = 0.0;
		double Top = 0.0;
		bool bTouchesSea = false;
		/** Superficie del agua; igual a Floor si está seca. */
		double Level = 0.0;
	};

	int32 Index(const FIntVector& Cell) const;
	FIntVector CellAt(int32 Index) const;
	bool IsOpen(const FIntVector& Cell) const { return GetCell(Cell) == EMineCell::Open; }
	/** Roca, pared o fuera de la caja (por los lados o por debajo): apoya un techo. */
	bool IsSupport(const FIntVector& Cell) const;
	/** Roca justo encima, dentro de la caja: la celda es techo. */
	bool IsRoof(const FIntVector& Cell) const;
	/** Tramo abierto de la viga (suelo a techo); false si no vale. */
	bool BeamSpan(const FIntVector& Cell, int32& OutBottomZ, int32& OutTopZ) const;
	/** Rehace los apoyos, el aire y las galerías si la rejilla ha cambiado. */
	void EnsureComputed() const;
	void ComputeGalleries() const;
	void ComputeSupport() const;
	void ComputeAir() const;
	void Step(const FMineWaterEnvironment& Env, FMineHazardResult& Result);
	void Collapse(const TArray<int32>& Region, FMineHazardResult& Result);
	void MarkDirty() { bDirty = true; bBeamsDirty = true; }

	FMineGridSettings Settings;
	int32 NumCells = 0;
	TArray<uint8> Cells;
	/** Celdas de las vigas. */
	TArray<FIntVector> Beams;
	/** Tiempo de exposición de cada celda de techo sin apoyo. */
	TMap<int32, int32> Exposure;
	int32 AccumulatedMs = 0;
	/** La rejilla ha cambiado desde la última revisión de vigas de `Step`. Va
	 *  aparte de `bDirty` porque las consultas `const` limpian `bDirty` y la
	 *  revisión no puede depender de si alguien ha preguntado antes. */
	bool bBeamsDirty = true;

	// Caché derivada de la rejilla (se rehace al cambiar una celda o una viga).
	mutable bool bDirty = true;
	/** Por celda: 1 si es techo sin apoyo. */
	mutable TArray<uint8> Unsupported;
	/** Por celda: distancia a la salida en décimas de celda (chaflán 10/14/17); −1 sin camino. */
	mutable TArray<int32> AirDistance;
	/** Por celda: galería (−1 si no es aire). */
	mutable TArray<int32> Gallery;
	mutable TArray<FGallery> Galleries;
	/** Niveles de agua cargados de un guardado (punto de mundo, nivel): se asignan al rehacer. */
	mutable TArray<TPair<FVector, double>> PendingWater;
};
