#include "Narrative/RunPacingModel.h"

namespace RunPacingDetail
{
	constexpr double HoursPerDay = 24.0;

	double NonNegative(double Value)
	{
		return FMath::IsFinite(Value) ? FMath::Max(Value, 0.0) : 0.0;
	}
}

FRunPacing FRunPacingModel::Competent(const FRunPacingInput& Input)
{
	using namespace RunPacingDetail;
	FRunPacing Out;
	const double WakeDay2 = HoursPerDay + Input.Body.WakeHour;
	const double WakeDay3 = 2.0 * HoursPerDay + Input.Body.WakeHour;
	Out.Day1Hours = NonNegative(Input.Body.SleepFromHour - Input.StartHour);
	Out.Day2Hours = NonNegative(Input.Director.CycloneEndHour - WakeDay2);
	Out.Day3Hours = NonNegative(2.0 * HoursPerDay + Input.Director.NightStartHour - WakeDay3);
	Out.AwakeHours = Out.Day1Hours + Out.Day2Hours + Out.Day3Hours;

	const double DayMinutes = NonNegative(Input.DayLengthMinutes);
	Out.PlayMinutes = Out.AwakeHours * DayMinutes / HoursPerDay;
	const FFinaleTiming Finale = FFinaleSequenceModel::Sanitize(Input.Finale);
	Out.FinaleMinutes = (Finale.IgniteSeconds + FFinaleSequenceModel::EpilogueAtSeconds(Finale)) / 60.0;
	Out.TotalMinutes = Out.PlayMinutes + Out.FinaleMinutes + Finale.EndCreditsSeconds / 60.0;
	return Out;
}

bool FRunPacingModel::IsInTarget(const FRunPacing& Pacing)
{
	return Pacing.TotalMinutes >= TargetMinMinutes && Pacing.TotalMinutes <= TargetMaxMinutes;
}
