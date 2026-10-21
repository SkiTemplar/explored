#pragma once

#include "CoreMinimal.h"

#include "Narrative/BeaconDirectorModel.h"
#include "Narrative/FinaleSequenceModel.h"
#include "Survival/BodyEventsModel.h"
#include "Weather/StormClockModel.h"

/**
 * Duración prevista de la partida competente (GDD v3 §3, equilibrio de F6). En Baliza Siete la
 * duración la fija el reloj, no las tareas: el jugador competente duerme en cuanto se puede
 * (día 1 a las SleepFromHour), aguanta el ciclón despierto hasta CycloneEndHour, vuelve a dormir y
 * enciende la baliza el día 3 al hacerse de noche (NightStartHour). Las horas despierto se pasan a
 * minutos reales con la duración del día y se suman el final y los créditos.
 *
 * Es una cota del reloj: los fundidos del sueño, el paseo hasta la linterna y la lectura del
 * epílogo la alargan un poco (la medición del bot con 26 min por día dio 52,4 min).
 */
struct EXPLORED_API FRunPacingInput
{
	/** Hora absoluta de inicio de la partida (D1 07:00). */
	double StartHour = 7.0;
	double DayLengthMinutes = FStormClockConfig::DayLengthMinutes;
	FBeaconDirectorTuning Director;
	FBodyEventsTuning Body;
	FFinaleTiming Finale;
};

struct EXPLORED_API FRunPacing
{
	/** Horas de juego despierto en cada día (D1, D2 con el ciclón, D3 hasta encender). */
	double Day1Hours = 0.0;
	double Day2Hours = 0.0;
	double Day3Hours = 0.0;
	double AwakeHours = 0.0;
	/** Minutos reales de juego hasta encender, del final hasta el epílogo y totales con créditos. */
	double PlayMinutes = 0.0;
	double FinaleMinutes = 0.0;
	double TotalMinutes = 0.0;
};

struct EXPLORED_API FRunPacingModel
{
	/** Rango objetivo de la partida competente a 1× (plan F6, criterio 1 afinado). */
	static constexpr double TargetMinMinutes = 50.0;
	static constexpr double TargetMaxMinutes = 60.0;

	static FRunPacing Competent(const FRunPacingInput& Input);
	static bool IsInTarget(const FRunPacing& Pacing);
};
