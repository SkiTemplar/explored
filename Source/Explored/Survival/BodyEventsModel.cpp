#include "Survival/BodyEventsModel.h"

namespace BodyEventsDetail
{
	constexpr double HoursPerDay = 24.0;

	float ClampBar(float Value)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 100.0f) : 0.0f;
	}

	bool IsEmpty(float Value)
	{
		return !FMath::IsFinite(Value) || Value <= 0.0f;
	}
}

bool FBodyEventsModel::AnyBarEmpty(const FBodyBars& Bars)
{
	using namespace BodyEventsDetail;
	return IsEmpty(Bars.Thirst) || IsEmpty(Bars.Hunger) || IsEmpty(Bars.Energy) || IsEmpty(Bars.Warmth);
}

double FBodyEventsModel::HourOfDay(double AbsoluteHour)
{
	if (!FMath::IsFinite(AbsoluteHour))
	{
		return 0.0;
	}
	const double Hour = FMath::Fmod(AbsoluteHour, BodyEventsDetail::HoursPerDay);
	return Hour < 0.0 ? Hour + BodyEventsDetail::HoursPerDay : Hour;
}

FFaintTick FBodyEventsModel::TickFaint(const FFaintTracker& Tracker, const FBodyBars& Bars, float DeltaSeconds,
	const FBodyEventsTuning& Tuning)
{
	FFaintTick Tick;
	Tick.Tracker = Tracker;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f)
	{
		return Tick;
	}
	if (!AnyBarEmpty(Bars))
	{
		Tick.Tracker.SecondsAtZero = 0.0f;
		return Tick;
	}
	const float Previous = FMath::IsFinite(Tracker.SecondsAtZero) ? FMath::Max(Tracker.SecondsAtZero, 0.0f) : 0.0f;
	const float Elapsed = Previous + DeltaSeconds;
	if (Elapsed >= Tuning.FaintSeconds)
	{
		Tick.Tracker.SecondsAtZero = 0.0f;
		Tick.bFainted = true;
		return Tick;
	}
	Tick.Tracker.SecondsAtZero = Elapsed;
	return Tick;
}

FFaintOutcome FBodyEventsModel::Faint(double NowHour, const FBodyEventsTuning& Tuning)
{
	FFaintOutcome Outcome;
	Outcome.Bars = Tuning.WakeAfterFaint;
	Outcome.WakeHour = FMath::IsFinite(NowHour) ? NowHour + Tuning.FaintHours : Tuning.FaintHours;
	return Outcome;
}

ESleepCheck FBodyEventsModel::CanSleep(double NowHour, bool bSheltered, const FBodyEventsTuning& Tuning, bool bCyclone)
{
	if (!bSheltered)
	{
		return ESleepCheck::NotSheltered;
	}
	if (bCyclone)
	{
		return ESleepCheck::Storm;
	}
	const double Hour = HourOfDay(NowHour);
	const bool bInWindow = FMath::IsFinite(NowHour) && (Hour >= Tuning.SleepFromHour || Hour < Tuning.SleepUntilHour);
	return bInWindow ? ESleepCheck::Allowed : ESleepCheck::WrongHour;
}

FSleepOutcome FBodyEventsModel::Sleep(const FBodyBars& Bars, double NowHour, bool bSheltered, const FBodyEventsTuning& Tuning,
	bool bCyclone)
{
	using namespace BodyEventsDetail;
	FSleepOutcome Outcome;
	Outcome.Bars = Bars;
	Outcome.WakeHour = NowHour;
	Outcome.Check = CanSleep(NowHour, bSheltered, Tuning, bCyclone);
	if (Outcome.Check != ESleepCheck::Allowed)
	{
		return Outcome;
	}

	const double Hour = HourOfDay(NowHour);
	const double DayStart = NowHour - Hour;
	Outcome.WakeHour = DayStart + Tuning.WakeHour + (Hour >= Tuning.SleepFromHour ? HoursPerDay : 0.0);
	Outcome.bThirstWarning = Bars.Thirst < Tuning.SleepThirstWarning;
	Outcome.Bars.Thirst = ClampBar(Bars.Thirst + Tuning.SleepThirstDelta);
	Outcome.Bars.Hunger = ClampBar(Bars.Hunger + Tuning.SleepHungerDelta);
	Outcome.Bars.Energy = ClampBar(Tuning.SleepEnergy);
	Outcome.bSlept = true;
	return Outcome;
}
