#pragma once

#include "CoreMinimal.h"

/** Fases con nombre del ciclo lunar (octavos centrados en cada fase principal). */
enum class EMoonPhase : uint8
{
	New,             // Luna nueva
	WaxingCrescent,  // Creciente
	FirstQuarter,    // Cuarto creciente
	WaxingGibbous,   // Gibosa creciente
	Full,            // Luna llena
	WaningGibbous,   // Gibosa menguante
	LastQuarter,     // Cuarto menguante
	WaningCrescent,  // Menguante
	Count
};

EXPLORED_API const TCHAR* LexToString(EMoonPhase Phase);

/**
 * La Luna del archipiélago (GDD §9.1): ciclo de 12 días de juego. Es la única
 * fuente de verdad de la fase lunar; la usan el cielo (ExploredSky::MoonPhase),
 * las mareas (FOceanTide::SpringNeapFactorAt) y los eventos del mundo
 * (FWorldEventsModel). Funciones puras sobre días totales de juego.
 *
 * Convenio: fase 0 es luna nueva y 0.5 luna llena. El día 0 a medianoche es
 * luna nueva, así que la luna llena cae a medianoche de los días 6, 18, 30...
 * y la noche de luna llena es la que va del día 12k + 5 al 12k + 6.
 */
struct EXPLORED_API FMoonModel
{
	/** Días de un ciclo lunar completo (GDD §9.1). */
	static constexpr int32 DaysPerCycle = 12;

	/** Semiancho (días) de las ventanas de luna llena y luna nueva. */
	static constexpr float PhaseWindowDays = 1.0f;

	/** Fase en [0, 1): 0 luna nueva, 0.5 luna llena. */
	static float Phase(float TotalDays);

	/** Fracción iluminada del disco (0 nueva, 1 llena) para una fase en [0, 1). */
	static float Illumination(float Phase01);

	/** Fracción iluminada en un instante. */
	static float IlluminationAt(float TotalDays);

	/** Fase con nombre para una fase en [0, 1). */
	static EMoonPhase NamedPhase(float Phase01);

	/** Índice del ciclo lunar (0, 1, 2...) que contiene el instante. */
	static int32 CycleIndex(float TotalDays);

	/** Instante exacto de la luna llena del ciclo indicado (días totales). */
	static float FullMoonOfCycle(int32 Cycle);

	/** Instante exacto de la luna nueva con la que empieza el ciclo indicado. */
	static float NewMoonOfCycle(int32 Cycle);

	/** Dentro de la ventana de luna llena (±PhaseWindowDays del instante exacto). */
	static bool IsFullMoonWindow(float TotalDays);

	/** Dentro de la ventana de luna nueva (±PhaseWindowDays del instante exacto). */
	static bool IsNewMoonWindow(float TotalDays);

	/** Primera luna llena exacta estrictamente posterior a AfterDays. */
	static float NextFullMoon(float AfterDays);

	/** Primera luna nueva exacta estrictamente posterior a AfterDays. */
	static float NextNewMoon(float AfterDays);

	/**
	 * Intensidad de la bioluminiscencia marina por la Luna en [0, 1]: máxima
	 * en luna nueva (el mar brilla sin luz que lo tape) y nula en luna llena.
	 * No incluye el día/noche; quien la pinta multiplica por la oscuridad.
	 */
	static float Bioluminescence(float Phase01);
};
