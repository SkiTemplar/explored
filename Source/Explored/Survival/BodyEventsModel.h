#pragma once

#include "CoreMinimal.h"

/**
 * Desmayo y sueño de Baliza Siete (GDD v3 §5.1) sobre las cuatro barras del
 * cuerpo (sed, hambre, energía y calor, 0-100; 100 = bien).
 *
 * Va aparte de FBodyModel/FSurvivalModel para no chocar con la poda de F1:
 * aquellos modelan el cuerpo de v1 (temperatura en °C, sueño separado de la
 * energía) y el GDD v3 tiene cuatro barras con deltas fijos. La integración
 * decide cómo se proyectan sobre FSurvivalState.
 *
 * Tiempo de juego en horas absolutas desde el día 1 a las 00:00 (como
 * FStormClockModel); el desmayo se cuenta en segundos reales. Funciones
 * puras: devuelven barras y contadores nuevos.
 */

struct EXPLORED_API FBodyBars
{
	float Thirst = 55.0f;
	float Hunger = 70.0f;
	float Energy = 70.0f;
	float Warmth = 100.0f;

	bool operator==(const FBodyBars& Other) const
	{
		return Thirst == Other.Thirst && Hunger == Other.Hunger && Energy == Other.Energy && Warmth == Other.Warmth;
	}
};

/** Valores de survival_needs.json; los de por defecto son los del GDD §5.1. */
struct EXPLORED_API FBodyEventsTuning
{
	/** Segundos reales con una barra a 0 hasta el desmayo. */
	float FaintSeconds = 20.0f;
	/** Horas de juego que pasan durante el desmayo. */
	double FaintHours = 3.0;
	FBodyBars WakeAfterFaint = MakeWakeBars();

	/** Se puede dormir desde SleepFromHour hasta antes de SleepUntilHour (cruza la medianoche). */
	double SleepFromHour = 20.0;
	double SleepUntilHour = 5.0;
	/** Hora a la que se despierta tras dormir. */
	double WakeHour = 6.0;
	float SleepThirstDelta = -25.0f;
	float SleepHungerDelta = -15.0f;
	float SleepEnergy = 100.0f;
	/** Con menos sed que esto se avisa al dormir, sin impedirlo. */
	float SleepThirstWarning = 30.0f;

	static FBodyBars MakeWakeBars()
	{
		FBodyBars Bars;
		Bars.Thirst = 40.0f;
		Bars.Hunger = 40.0f;
		Bars.Energy = 40.0f;
		Bars.Warmth = 60.0f;
		return Bars;
	}
};

/** Cuenta del desmayo: segundos seguidos con alguna barra a 0. */
struct EXPLORED_API FFaintTracker
{
	float SecondsAtZero = 0.0f;
};

struct EXPLORED_API FFaintTick
{
	FFaintTracker Tracker;
	/** Se ha cumplido el plazo en este paso: el juego hace el fundido y llama a Faint. */
	bool bFainted = false;
};

struct EXPLORED_API FFaintOutcome
{
	FBodyBars Bars;
	double WakeHour = 0.0;
};

enum class ESleepCheck : uint8
{
	Allowed,
	NotSheltered,  // fuera de un ShelterVolume
	WrongHour,     // entre las 05:00 y las 20:00
};

struct EXPLORED_API FSleepOutcome
{
	bool bSlept = false;
	ESleepCheck Check = ESleepCheck::Allowed;
	FBodyBars Bars;
	double WakeHour = 0.0;
	/** «Tienes demasiada sed para dormir»: se avisa y se duerme igual. */
	bool bThirstWarning = false;
};

struct EXPLORED_API FBodyEventsModel
{
	static bool AnyBarEmpty(const FBodyBars& Bars);

	/** Hora del día (0-24) de una hora absoluta; 0 si no es finita. */
	static double HourOfDay(double AbsoluteHour);

	/**
	 * Avanza la cuenta del desmayo DeltaSeconds reales. Con todas las barras por
	 * encima de 0 la cuenta vuelve a 0; al llegar a FaintSeconds se dispara el
	 * desmayo y la cuenta se reinicia. Un paso no finito o negativo no cuenta.
	 */
	static FFaintTick TickFaint(const FFaintTracker& Tracker, const FBodyBars& Bars, float DeltaSeconds,
		const FBodyEventsTuning& Tuning = FBodyEventsTuning());

	/** Al despertar del desmayo: barras fijas (40/40/40/60) y +3 h. */
	static FFaintOutcome Faint(double NowHour, const FBodyEventsTuning& Tuning = FBodyEventsTuning());

	static ESleepCheck CanSleep(double NowHour, bool bSheltered, const FBodyEventsTuning& Tuning = FBodyEventsTuning());

	/** Duerme hasta las 06:00 con los deltas fijos; si no se puede, las barras no cambian. */
	static FSleepOutcome Sleep(const FBodyBars& Bars, double NowHour, bool bSheltered,
		const FBodyEventsTuning& Tuning = FBodyEventsTuning());
};
