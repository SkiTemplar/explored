#pragma once

#include "CoreMinimal.h"
#include "Weather/WeatherModel.h"

/**
 * Huerto y limonero (GDD §8.7, biblia §7.1–7.3, balance «Huerto y limonero»).
 *
 * Modelo puro: las definiciones reflejan Content/Data/plants.json (las parsea
 * UFarmSubsystem) y el estado es un struct plano que se guarda tal cual. El
 * tiempo se mide en días de juego enteros: UFarmSubsystem llama a EndDay() al
 * cerrar cada día con la lluvia que cayó en él.
 *
 * Reglas (una unidad de crecimiento = un día bueno):
 * - Un día cuenta como regado si el riego manual más la lluvia (en riegos
 *   equivalentes, ver RainHoursPerWatering) llega a waterPerDay. Con
 *   waterPerDay = 0 basta la lluvia de la estación: la planta nunca pasa sed.
 * - Día regado: la planta avanza 1 día, ×0.5 fuera de sus estaciones y ×1.5
 *   con compost activo (dura CompostDays días desde que se echa y no se acumula).
 * - Día seco: no crece. 1 día seco = sedienta, 2 = marchita, 4 = muerta. El
 *   limonero (neverRemoved) se queda marchito y en pausa, nunca muere.
 * - La etapa final (days = 0) da cosecha en cuanto se alcanza. Si
 *   harvest.everyDays > 0 vuelve a dar fruto tras ese número de días buenos
 *   (fuera de estación no da fruto); si es 0, cosechar arranca la planta.
 * - La cantidad es determinista por semilla + parcela + día. Las plantas con
 *   birdsEat pierden parte de la cosecha a las aves si no hay un espantapájaros
 *   a menos de ScarecrowRadius (biblia §7.2).
 */

/** Etapa estática de un cultivo (plants.json → stages[]). */
struct EXPLORED_API FPlantStageDef
{
	FName Id;
	FString NameEs;
	/** Días buenos que dura la etapa; 0 en la etapa final. */
	int32 Days = 0;
	/** Ruta de la malla; vacía si todavía es un marcador (mesh: null). */
	FString MeshPath;
};

/** Cosecha de la etapa final (plants.json → harvest). */
struct EXPLORED_API FPlantHarvestDef
{
	FName Item;
	int32 Min = 1;
	int32 Max = 1;
	/** Días buenos entre cosechas; 0 = se arranca al cosechar. */
	int32 EveryDays = 0;
};

/** Un cultivo de plants.json. */
struct EXPLORED_API FPlantDef
{
	FName Id;
	FString NameEs;
	/** Objeto que se gasta al plantar (semilla, esqueje, hijuelo). */
	FName PlantedFrom;
	/** Pieza de construcción que debe tener la parcela (bancal, espaldera, arriate). */
	FName RequiresPiece;
	/** Estaciones en que se puede plantar y en que crece a ritmo normal. */
	TArray<ESeason> Seasons;
	/** Riegos al día que necesita; 0 = basta la lluvia de la estación. */
	int32 WaterPerDay = 1;
	/** El limonero: no muere, no se arranca y la parcela no se puede retirar. */
	bool bNeverRemoved = false;
	/** Las aves picotean su fruta si no hay espantapájaros cerca. */
	bool bBirdsEat = false;
	TArray<FPlantStageDef> Stages;
	FPlantHarvestDef Harvest;

	bool GrowsIn(ESeason Season) const { return Seasons.Contains(Season); }

	/** Días buenos desde que se planta hasta la etapa final (primera cosecha). */
	int32 DaysToFirstHarvest() const;
};

/** Salud visible de una planta. */
enum class EPlantHealth : uint8
{
	Healthy,
	Thirsty,   // un día sin agua suficiente: no crece
	Wilting,   // dos o más días: hojas caídas (el limonero se queda aquí)
	Dead,
};

/** Resultado de las acciones del jugador sobre una parcela. */
enum class EFarmResult : uint8
{
	Ok,
	UnknownPlot,
	UnknownPlant,
	PlotOccupied,
	PlotEmpty,
	WrongItem,
	MissingPiece,
	OutOfSeason,
	PlantDead,
	NotReady,
	NeverRemoved,
};

EXPLORED_API const TCHAR* LexToString(EPlantHealth Health);
EXPLORED_API const TCHAR* LexToString(EFarmResult Result);

/** Cultivo plantado en una parcela. */
struct EXPLORED_API FFarmCrop
{
	/** NAME_None = parcela vacía. */
	FName PlantId;
	int32 PlantedDay = 0;
	/** Días buenos acumulados (fracciones por estación y compost). */
	float GrowthDays = 0.0f;
	/** Días buenos desde la última cosecha, en la etapa final. */
	float HarvestClock = 0.0f;
	/** Riegos manuales del día en curso. */
	float WaterToday = 0.0f;
	int32 DryDays = 0;
	bool bHarvestReady = false;
	bool bDead = false;
	int32 Harvests = 0;

	bool IsEmpty() const { return PlantId.IsNone(); }
};

/** Parcela: un bancal, una espaldera o el arriate del limonero en el mundo. */
struct EXPLORED_API FFarmPlot
{
	int32 Id = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
	/** Piezas que forman la parcela (una espaldera va sobre un bancal: {bancal, espaldera}). */
	TArray<FName> Pieces;
	/** Días que le quedan al compost echado (0 = sin compost). */
	int32 CompostDaysLeft = 0;
	FFarmCrop Crop;
};

/** Estado completo del huerto, en datos planos para el guardado (P-SAVE). */
struct EXPLORED_API FFarmState
{
	/** Último día cerrado con EndDay (INDEX_NONE = ninguno). */
	int32 LastEndedDay = INDEX_NONE;
	int32 NextPlotId = 1;
	TArray<FFarmPlot> Plots;
	TArray<FVector> Scarecrows;
};

/** Lo que devuelve una cosecha. */
struct EXPLORED_API FFarmHarvest
{
	FName Item;
	int32 Count = 0;
	/** Unidades que se llevaron las aves (ya descontadas de Count). */
	int32 LostToBirds = 0;
	/** La planta se arrancó y la parcela quedó libre. */
	bool bPlantRemoved = false;
};

/** Lo que necesita la capa visual de una parcela. */
struct EXPLORED_API FFarmStageView
{
	bool bHasPlant = false;
	FName PlantId;
	FName StageId;
	int32 StageIndex = 0;
	int32 StageCount = 0;
	/** Progreso dentro de la etapa (0–1; 1 en la etapa final). */
	float StageProgress = 0.0f;
	/** Progreso hasta la etapa final (0–1): para escalar la malla marcadora. */
	float OverallProgress = 0.0f;
	EPlantHealth Health = EPlantHealth::Healthy;
	bool bHarvestReady = false;
	/** Hoy todavía le falta agua. */
	bool bNeedsWater = false;
	FString MeshPath;
};

/** Ids de datos con significado propio en el código. */
namespace FarmIds
{
	/** El limonero (biblia §7.1). */
	inline FName LemonTree() { return FName(TEXT("limonero")); }
	/** El limón: se planta, se cosecha y previene y cura el escorbuto (lo aplica FSurvivalModel). */
	inline FName Lemon() { return FName(TEXT("limon")); }
}

class EXPLORED_API FFarmModel
{
public:
	/** Ritmo de crecimiento fuera de las estaciones de la planta. */
	static constexpr float OutOfSeasonGrowth = 0.5f;
	/** Multiplicador del compost y días que dura. */
	static constexpr float CompostGrowth = 1.5f;
	static constexpr int32 CompostDays = 8;
	/** Días secos seguidos para marchitarse y para morir. */
	static constexpr int32 DryDaysToWilt = 2;
	static constexpr int32 DryDaysToDie = 4;
	/** Horas de lluvia plena (Rain = 1) que equivalen a un riego. */
	static constexpr float RainHoursPerWatering = 2.0f;
	/** Radio de protección de un espantapájaros (cm). */
	static constexpr double ScarecrowRadius = 1500.0;
	/** Probabilidad de que las aves ataquen una cosecha sin protección y parte que se llevan. */
	static constexpr float BirdChance = 0.5f;
	static constexpr float BirdShare = 0.25f;

	FFarmModel(TArray<FPlantDef> InPlants, uint32 InSeed);

	const TArray<FPlantDef>& GetPlants() const { return Plants; }
	const FPlantDef* FindPlant(FName PlantId) const;

	/** Id de estación de plants.json («seca», «primeras_lluvias», «monzon», «ciclones»). */
	static bool SeasonFromId(const FString& Id, ESeason& OutSeason);

	/** Riegos equivalentes de una cantidad de lluvia (integral de Rain en horas). */
	static float WateringsFromRainHours(float RainHours);

	/** Lluvia de un día completo según el planificador del clima, en riegos equivalentes. */
	static float RainWateringsForDay(const FWeatherModel& Weather, int32 Day);

	/** Día en que un cultivo plantado ese día da su primera cosecha si se riega cada día. */
	static int32 FirstHarvestDay(const FPlantDef& Def, int32 PlantedDay);

	// --- Parcelas ------------------------------------------------------------------

	int32 AddPlot(const FVector& Location, const TArray<FName>& Pieces);
	/** Falla con NeverRemoved si la parcela tiene el limonero. */
	EFarmResult RemovePlot(int32 PlotId);
	const FFarmPlot* FindPlot(int32 PlotId) const;
	/** Parcela más cercana a menos de MaxDistance (cm); INDEX_NONE si no hay. */
	int32 FindPlotNear(const FVector& Location, double MaxDistance) const;

	void AddScarecrow(const FVector& Location);
	bool RemoveScarecrow(const FVector& Location, double Tolerance = 10.0);
	bool IsProtectedFromBirds(int32 PlotId) const;

	// --- Acciones del jugador -------------------------------------------------------

	/** Comprueba sin cambiar nada si se puede plantar ese cultivo con ese objeto hoy. */
	EFarmResult CanPlant(int32 PlotId, FName PlantId, FName OfferedItem, int32 Day) const;
	/** Planta si CanPlant lo permite (el llamador gasta el objeto solo si devuelve Ok). */
	EFarmResult Plant(int32 PlotId, FName PlantId, FName OfferedItem, int32 Day);
	/** Cultivo que se plantaría en esta parcela con este objeto hoy (NAME_None si ninguno). */
	FName PlantForItem(int32 PlotId, FName OfferedItem, int32 Day) const;
	/** Un riego manual (regadera, cuenco de agua). */
	EFarmResult Water(int32 PlotId, float Waterings = 1.0f);
	/** Echa compost: CompostDays días a ×CompostGrowth. Se puede echar en parcela vacía. */
	EFarmResult ApplyCompost(int32 PlotId);
	EFarmResult Harvest(int32 PlotId, int32 Day, FFarmHarvest& Out);
	/** Arranca la planta (o sus restos secos). El limonero no se arranca. */
	EFarmResult ClearPlot(int32 PlotId);

	// --- Tiempo -------------------------------------------------------------------

	/**
	 * Cierra un día de juego con la lluvia que cayó (riegos equivalentes). Cada día
	 * se cierra una sola vez y en orden: devuelve false si ya estaba cerrado.
	 */
	bool EndDay(int32 Day, float RainWaterings);

	// --- Consultas ------------------------------------------------------------------

	FFarmStageView GetStageView(int32 PlotId) const;
	EPlantHealth GetHealth(int32 PlotId) const;

	const FFarmState& GetState() const { return State; }
	/** Restaura un guardado; descarta los cultivos cuyo id ya no está en los datos. */
	void SetState(const FFarmState& InState);

private:
	FFarmPlot* FindPlotMutable(int32 PlotId);
	void GrowCrop(const FPlantDef& Def, FFarmCrop& Crop, float Units, bool bInSeason) const;

	TArray<FPlantDef> Plants;
	uint32 Seed = 0;
	FFarmState State;
};
