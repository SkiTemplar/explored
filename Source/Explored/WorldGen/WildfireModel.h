#pragma once

#include "CoreMinimal.h"

#include "Save/SaveValue.h"
#include "Weather/WeatherModel.h"

/**
 * Incendio de vegetación (biblia 02 §6, GDD v2 §3.14): contagio de fuego entre
 * celdas de hierba y matorral sobre una rejilla de 2 m.
 *
 * - Solo se guardan las celdas que se apartan del mundo base (ardiendo,
 *   quemadas o mojadas). El combustible base lo da la capa de vegetación con
 *   FFuelQuery; el modelo no conoce actores ni HISM.
 * - Un paso = 1 s de simulación. Cada celda ardiendo tira, por cada vecina
 *   (8 vecinas), una probabilidad en milésimas que sale de la estación, del
 *   tiempo y del viento. La tirada es un hash de (semilla, celda destino,
 *   paso, vecina de origen), así que el resultado no depende del orden en que
 *   se visitan las celdas ni de cómo se trocea el avance.
 * - Solo se simulan los chunks con fuego a menos de ActiveRadiusCm de algún
 *   observador; el resto se congela (misma política que la arena viva, biblia
 *   08 §2.6). Nunca se recorre el mundo entero: el coste es proporcional a las
 *   celdas ardiendo en chunks activos.
 * - Dos relojes: los pasos van en segundos de simulación (el contagio es un
 *   fenómeno de segundos); el rebrote, la ceniza y la humedad, en minutos de
 *   juego (días de 1440 minutos).
 */

/** Combustible de una celda en el mundo base. */
enum class EFireFuel : uint8
{
	None,   // arena, roca, agua, suelo desnudo, construcción
	Grass,  // hierba
	Shrub,  // matorral y arbustos
	Count
};

enum class EFireCellState : uint8
{
	Unburnt,
	Burning,
	Burnt,
	Count
};

EXPLORED_API const TCHAR* LexToString(EFireFuel Fuel);
EXPLORED_API const TCHAR* LexToString(EFireCellState State);

/** Condiciones del tiempo en el paso (salen de FWeatherModel y del viento del cielo). */
struct EXPLORED_API FWildfireConditions
{
	ESeason Season = ESeason::Dry;
	EWeatherState Weather = EWeatherState::Clear;
	/** Hacia dónde sopla el viento en el plano (no hace falta normalizar). */
	FVector2D WindDirection = FVector2D(1.0, 0.0);
	/** Fuerza del viento 0–1 (FWeatherSample::Wind). Por debajo de CalmWind no hay efecto de viento. */
	float Wind = 0.2f;
};

/** Una celda que se aparta del mundo base. */
struct EXPLORED_API FWildfireCell
{
	EFireCellState State = EFireCellState::Unburnt;
	/** Combustible que tenía la celda al arder (decide el rebrote y la ceniza). */
	EFireFuel Fuel = EFireFuel::None;
	/** Pasos de simulación que le quedan ardiendo. */
	int32 BurnStepsLeft = 0;
	/** Minuto de juego en que se apagó (solo si Burnt). */
	int64 BurntMinute = 0;
	/** Mojada hasta este minuto de juego (excluido): no prende. */
	int64 WetUntilMinute = 0;
	bool bAshTaken = false;
};

/** Qué ha pasado en un avance: lo que la capa de UE necesita para efectos, red y coste. */
struct EXPLORED_API FWildfireStepResult
{
	/** Pasos simulados de verdad (los que exceden MaxCatchUpSteps se descartan). */
	int32 StepsSimulated = 0;
	int32 StepsDropped = 0;
	TArray<FIntPoint> Ignited;
	TArray<FIntPoint> BurnedOut;
	/** Celdas ardiendo que se han revisado (coste del avance). */
	int32 CellsVisited = 0;
	/** Contagios que el tope por chunk ha dejado para el paso siguiente. */
	int32 IgnitionsDeferred = 0;
	/** La lluvia (chubasco o más) ha apagado el incendio en este avance. */
	bool bRainQuenched = false;
	/** Chunks que han cambiado: los que la capa de red debe enviar. */
	TArray<FIntPoint> DirtyChunks;
};

class EXPLORED_API FWildfireModel
{
public:
	using FFuelQuery = TFunction<EFireFuel(FIntPoint Cell)>;

	static constexpr double CellSizeCm = 200.0;
	/** Celdas por lado de chunk: 16 × 2 m = 32 m. */
	static constexpr int32 ChunkCells = 16;
	/** Radio de simulación alrededor de cada jugador (biblia 08 §2.6). */
	static constexpr double ActiveRadiusCm = 8000.0;
	/** Como máximo 4 pasos acumulados al volver a simular (biblia 08 §2.6). */
	static constexpr int32 MaxCatchUpSteps = 4;
	/** Tope de celdas que prenden por chunk y paso (biblia 08 §2.6: 64 cambios/s/chunk). */
	static constexpr int32 MaxIgnitionsPerChunkStep = 64;

	/** Probabilidades de contagio por vecina y segundo, en milésimas (biblia 02 §6). */
	static constexpr int32 DryChancePermille = 450;
	static constexpr int32 WetSeasonChancePermille = 135;
	static constexpr int32 FogChancePermille = 90;
	static constexpr int32 WindBonusPermille = 250;
	static constexpr float CalmWind = 0.1f;

	/** Segundos que arde una celda [propuesto, pendiente de validar]. */
	static constexpr int32 GrassBurnSteps = 20;
	static constexpr int32 ShrubBurnSteps = 60;

	/** Rebrote y ceniza en minutos de juego (biblia 02 §6). */
	static constexpr int64 MinutesPerDay = 1440;
	static constexpr int64 GrassRegrowMinutes = 12 * MinutesPerDay;
	static constexpr int64 ShrubRegrowMinutes = 25 * MinutesPerDay;
	static constexpr int64 AshMinutes = 3 * MinutesPerDay;

	FWildfireModel(uint32 InSeed, FFuelQuery InFuel);

	static FIntPoint CellAt(const FVector2D& WorldCm);
	static FVector2D CellCenter(FIntPoint Cell);
	/** Chunk de una celda (división por defecto, también con coordenadas negativas). */
	static FIntPoint ChunkOf(FIntPoint Cell);

	/** Probabilidad base por vecina y segundo; 0 si llueve lo bastante para apagarlo. */
	static int32 BaseChancePermille(ESeason Season, EWeatherState Weather);
	/** Chubasco o más apaga el incendio (biblia 02 §6). */
	static bool RainQuenches(EWeatherState Weather);
	/** Probabilidad de que la celda Offset (vecina de una que arde) prenda en un paso, 0–1000. */
	static int32 SpreadChancePermille(const FWildfireConditions& Conditions, FIntPoint Offset);

	/** Prende una celda (chispa, antorcha, rayo, brea). False si no hay combustible, está mojada, arde o está quemada. */
	bool Ignite(FIntPoint Cell, int64 NowMinute);

	/**
	 * Agua (cubo, lluvia local) o arena sobre un disco de celdas: apaga lo que arde
	 * (queda quemado) y moja las celdas con combustible durante WetMinutes.
	 */
	void Douse(FIntPoint Center, int32 RadiusCells, int64 NowMinute, int64 WetMinutes);

	/**
	 * Avanza hasta el segundo de simulación NowSecond. Si hay más de
	 * MaxCatchUpSteps pasos pendientes, solo se simulan los últimos
	 * MaxCatchUpSteps. Un segundo anterior o igual al último no hace nada.
	 */
	FWildfireStepResult Advance(int64 NowSecond, int64 NowMinute, const FWildfireConditions& Conditions, const TArray<FVector2D>& ObserversCm);

	/** Estado visible de una celda, con el rebrote aplicado. */
	EFireCellState StateAt(FIntPoint Cell, int64 NowMinute) const;
	/** Combustible actual (None si está quemada y aún no ha rebrotado). */
	EFireFuel FuelAt(FIntPoint Cell, int64 NowMinute) const;
	bool IsWet(FIntPoint Cell, int64 NowMinute) const;

	/** Ceniza (ceniza_madera) que se puede recoger en la celda: 1 durante los 3 primeros días tras quemarse. */
	int32 AshAt(FIntPoint Cell, int64 NowMinute) const;
	/** Recoge la ceniza; devuelve las unidades recogidas (0 o 1). */
	int32 TakeAsh(FIntPoint Cell, int64 NowMinute);

	int32 NumBurning() const { return Burning.Num(); }
	bool IsActive() const { return Burning.Num() > 0; }
	int64 GetLastSecond() const { return LastSecond; }
	/** Celdas guardadas (lo que ocupa en memoria y en la partida). */
	int32 NumStoredCells() const { return Cells.Num(); }
	const FWildfireCell* FindCell(FIntPoint Cell) const { return Cells.Find(Cell); }

	/** Olvida las celdas que ya han vuelto al mundo base (rebrotadas y secas). */
	int32 Prune(int64 NowMinute);

	FSaveValue Save() const;
	/** Carga una partida; false (sin tocar el estado) si el formato no es válido. */
	bool Load(const FSaveValue& Value);

private:
	bool IsChunkActive(FIntPoint Chunk, const TArray<FVector2D>& ObserversCm) const;
	void Step(int64 StepIndex, int64 NowMinute, const FWildfireConditions& Conditions, const TArray<FVector2D>& ObserversCm, FWildfireStepResult& Out);
	void Extinguish(FIntPoint Cell, int64 NowMinute);
	void RebuildBurning();
	static void AddUnique(TArray<FIntPoint>& Array, FIntPoint Value);

	uint32 Seed = 0;
	FFuelQuery Fuel;
	TMap<FIntPoint, FWildfireCell> Cells;
	/** Celdas ardiendo, ordenadas por (Y, X): el orden de proceso no depende de la historia del mapa. */
	TArray<FIntPoint> Burning;
	int64 LastSecond = 0;
};
