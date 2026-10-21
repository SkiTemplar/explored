#include "Debug/PlaytestRunModel.h"

namespace PlaytestRunDetail
{
	bool IsUsable(double Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0;
	}

	FString Escape(const FString& In)
	{
		return In.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n"));
	}

	const TCHAR* Bool(bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString StringArray(const TArray<FString>& Values)
	{
		FString Out = TEXT("[");
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			Out += FString::Printf(TEXT("%s\"%s\""), Index > 0 ? TEXT(", ") : TEXT(""), *Escape(Values[Index]));
		}
		return Out + TEXT("]");
	}

	const TCHAR* Verdict(bool bOk)
	{
		return bOk ? TEXT("VERDE") : TEXT("ROJO");
	}
}

FPlaytestRunClock FPlaytestRunModel::Accumulate(const FPlaytestRunClock& Clock, double RealDeltaSeconds, double ClockScale)
{
	using namespace PlaytestRunDetail;
	FPlaytestRunClock Next = Clock;
	if (!IsUsable(RealDeltaSeconds) || !IsUsable(ClockScale))
	{
		return Next;
	}
	Next.RealSeconds += RealDeltaSeconds;
	Next.Est1xSeconds += RealDeltaSeconds * ClockScale;
	return Next;
}

double FPlaytestRunModel::TravelSeconds1x(double DistanceMeters, const FPlaytestRunTuning& Tuning)
{
	if (!PlaytestRunDetail::IsUsable(DistanceMeters) || !(Tuning.WalkSpeedMps > 0.0))
	{
		return 0.0;
	}
	return DistanceMeters * FMath::Max(Tuning.PathFactor, 1.0) / Tuning.WalkSpeedMps;
}

double FPlaytestRunModel::RealWaitSeconds(double Seconds1x, double ClockScale)
{
	if (!PlaytestRunDetail::IsUsable(Seconds1x))
	{
		return 0.0;
	}
	const double Scale = FMath::IsFinite(ClockScale) ? FMath::Max(ClockScale, 1.0) : 1.0;
	return Seconds1x / Scale;
}

FPlaytestRunSummary FPlaytestRunModel::MarkStage(const FPlaytestRunSummary& Summary, int32 StageIndex, const FString& StageName)
{
	FPlaytestRunSummary Next = Summary;
	if (StageIndex < 0 || StageIndex >= StageCount)
	{
		return Next;
	}
	const uint32 Bit = 1u << static_cast<uint32>(StageIndex);
	if ((Next.StagesMask & Bit) == 0)
	{
		Next.StagesMask |= Bit;
		Next.StagesReached.Add(StageName);
	}
	return Next;
}

FPlaytestRunSummary FPlaytestRunModel::AddLogError(const FPlaytestRunSummary& Summary, const FString& Line)
{
	FPlaytestRunSummary Next = Summary;
	++Next.LogErrors;
	if (Next.ErrorSamples.Num() < MaxErrorSamples)
	{
		Next.ErrorSamples.Add(Line.Left(300));
	}
	return Next;
}

double FPlaytestRunModel::EstimatedMinutes(const FPlaytestRunSummary& Summary)
{
	return (Summary.Clock.Est1xSeconds + FMath::Max(Summary.CreditsSeconds, 0.0)) / 60.0;
}

bool FPlaytestRunModel::IsDurationInTarget(const FPlaytestRunSummary& Summary, const FPlaytestRunTuning& Tuning)
{
	const double Minutes = EstimatedMinutes(Summary);
	return Minutes >= Tuning.MinMinutes && Minutes <= Tuning.MaxMinutes;
}

bool FPlaytestRunModel::AllStagesReached(const FPlaytestRunSummary& Summary)
{
	return Summary.StagesMask == (1u << StageCount) - 1u;
}

bool FPlaytestRunModel::Passed(const FPlaytestRunSummary& Summary, const FPlaytestRunTuning& Tuning)
{
	return Summary.bRan && Summary.bReachedEpilogue && Summary.FailedStep.IsEmpty() && AllStagesReached(Summary)
		&& IsDurationInTarget(Summary, Tuning) && Summary.SaveLoadFailures == 0 && Summary.LogErrors == 0;
}

bool FPlaytestRunModel::AllCompletable(const TArray<FPlaytestSoftlockCheck>& Checks)
{
	return Checks.Num() > 0 && Checks.FindByPredicate([](const FPlaytestSoftlockCheck& C) { return !C.bCompletable; }) == nullptr;
}

FString FPlaytestRunModel::FormatGameClock(double AbsHours)
{
	if (!PlaytestRunDetail::IsUsable(AbsHours))
	{
		return TEXT("?");
	}
	const int64 TotalMinutes = static_cast<int64>(FMath::FloorToDouble(AbsHours * 60.0 + 0.5));
	const int64 Day = TotalMinutes / (24 * 60) + 1;
	const int64 Minute = TotalMinutes % (24 * 60);
	return FString::Printf(TEXT("D%lld %02lld:%02lld"), Day, Minute / 60, Minute % 60);
}

FString FPlaytestRunModel::ToJson(const FPlaytestRunSummary& Summary, const TArray<FPlaytestSoftlockCheck>& Softlocks,
	const FPlaytestRunTuning& Tuning)
{
	using namespace PlaytestRunDetail;
	FString Out = TEXT("{\n");
	Out += FString::Printf(TEXT("    \"route\": \"%s\",\n"), *Escape(Summary.Route));
	Out += FString::Printf(TEXT("    \"ran\": %s,\n"), Bool(Summary.bRan));
	Out += FString::Printf(TEXT("    \"reached_epilogue\": %s,\n"), Bool(Summary.bReachedEpilogue));
	Out += FString::Printf(TEXT("    \"failed_step\": \"%s\",\n"), *Escape(Summary.FailedStep));
	Out += FString::Printf(TEXT("    \"real_seconds\": %.1f,\n"), Summary.Clock.RealSeconds);
	Out += FString::Printf(TEXT("    \"estimated_1x_seconds\": %.1f,\n"), Summary.Clock.Est1xSeconds);
	Out += FString::Printf(TEXT("    \"credits_seconds\": %.1f,\n"), Summary.CreditsSeconds);
	Out += FString::Printf(TEXT("    \"estimated_1x_minutes\": %.2f,\n"), EstimatedMinutes(Summary));
	Out += FString::Printf(TEXT("    \"target_minutes\": [%.0f, %.0f],\n"), Tuning.MinMinutes, Tuning.MaxMinutes);
	Out += FString::Printf(TEXT("    \"duration_in_target\": %s,\n"), Bool(IsDurationInTarget(Summary, Tuning)));
	Out += FString::Printf(TEXT("    \"faints\": %d,\n    \"sleeps\": %d,\n    \"pages_read\": %d,\n"), Summary.Faints, Summary.Sleeps,
		Summary.PagesRead);
	Out += FString::Printf(TEXT("    \"stages_reached\": %s,\n"), *StringArray(Summary.StagesReached));
	Out += FString::Printf(TEXT("    \"all_stages\": %s,\n"), Bool(AllStagesReached(Summary)));
	Out += FString::Printf(TEXT("    \"log_errors\": %d,\n    \"log_warnings\": %d,\n"), Summary.LogErrors, Summary.LogWarnings);
	Out += FString::Printf(TEXT("    \"error_samples\": %s,\n"), *StringArray(Summary.ErrorSamples));
	Out += FString::Printf(TEXT("    \"save_load_checks\": %d,\n    \"save_load_failures\": %d,\n"), Summary.SaveLoadChecks,
		Summary.SaveLoadFailures);
	Out += FString::Printf(TEXT("    \"save_load_notes\": %s,\n"), *StringArray(Summary.SaveLoadNotes));
	Out += TEXT("    \"steps\": [\n");
	for (int32 Index = 0; Index < Summary.Steps.Num(); ++Index)
	{
		const FPlaytestRunStep& Step = Summary.Steps[Index];
		Out += FString::Printf(TEXT("      {\"name\": \"%s\", \"ok\": %s, \"t_1x_s\": %.1f, \"clock\": \"%s\", \"stage\": \"%s\"}%s\n"),
			*Escape(Step.Name), Bool(Step.bOk), Step.Est1xSeconds, *Escape(Step.GameClock), *Escape(Step.Stage),
			Index + 1 < Summary.Steps.Num() ? TEXT(",") : TEXT(""));
	}
	Out += TEXT("    ],\n");
	Out += TEXT("    \"softlocks\": [\n");
	for (int32 Index = 0; Index < Softlocks.Num(); ++Index)
	{
		const FPlaytestSoftlockCheck& Check = Softlocks[Index];
		Out += FString::Printf(TEXT("      {\"scenario\": \"%s\", \"completable\": %s, \"detail\": \"%s\"}%s\n"), *Escape(Check.Name),
			Bool(Check.bCompletable), *Escape(Check.Detail), Index + 1 < Softlocks.Num() ? TEXT(",") : TEXT(""));
	}
	Out += TEXT("    ],\n");
	Out += FString::Printf(TEXT("    \"softlocks_found\": %d,\n"),
		Softlocks.FilterByPredicate([](const FPlaytestSoftlockCheck& C) { return !C.bCompletable; }).Num());
	Out += FString::Printf(TEXT("    \"passed\": %s\n"), Bool(Passed(Summary, Tuning) && AllCompletable(Softlocks)));
	Out += TEXT("  }");
	return Out;
}

FString FPlaytestRunModel::ToReadable(const FPlaytestRunSummary& Summary, const TArray<FPlaytestSoftlockCheck>& Softlocks,
	const FPlaytestRunTuning& Tuning)
{
	using namespace PlaytestRunDetail;
	FString Out = FString::Printf(TEXT("== Partida completa del bot (ruta «%s») ==\n"), *Summary.Route);
	Out += FString::Printf(TEXT("- Epílogo alcanzado: %s%s\n"), Summary.bReachedEpilogue ? TEXT("sí") : TEXT("no"),
		Summary.FailedStep.IsEmpty() ? TEXT("") : *(TEXT(" (falló: ") + Summary.FailedStep + TEXT(")")));
	Out += FString::Printf(TEXT("- Duración estimada a 1×: %.1f min (%.1f de partida + %.0f s de créditos), objetivo %.0f-%.0f: %s\n"),
		EstimatedMinutes(Summary), Summary.Clock.Est1xSeconds / 60.0, Summary.CreditsSeconds, Tuning.MinMinutes, Tuning.MaxMinutes,
		Verdict(IsDurationInTarget(Summary, Tuning)));
	Out += FString::Printf(TEXT("- Tiempo real de la prueba: %.1f min\n"), Summary.Clock.RealSeconds / 60.0);
	Out += FString::Printf(TEXT("- Etapas: %d/%d (%s)\n"), Summary.StagesReached.Num(), StageCount,
		*FString::Join(Summary.StagesReached, TEXT(" → ")));
	Out += FString::Printf(TEXT("- Desmayos: %d · sueños: %d · páginas leídas: %d\n"), Summary.Faints, Summary.Sleeps, Summary.PagesRead);
	Out += FString::Printf(TEXT("- Guardar/cargar: %d comprobaciones, %d fallos\n"), Summary.SaveLoadChecks, Summary.SaveLoadFailures);
	for (const FString& Note : Summary.SaveLoadNotes)
	{
		Out += TEXT("    · ") + Note + TEXT("\n");
	}
	Out += FString::Printf(TEXT("- Log: %d errores, %d avisos\n"), Summary.LogErrors, Summary.LogWarnings);
	for (const FString& Line : Summary.ErrorSamples)
	{
		Out += TEXT("    · ") + Line + TEXT("\n");
	}
	Out += TEXT("- Pasos:\n");
	for (const FPlaytestRunStep& Step : Summary.Steps)
	{
		Out += FString::Printf(TEXT("    %s %6.1f min  %s  [%s] %s\n"), Step.bOk ? TEXT("ok   ") : TEXT("FALLO"), Step.Est1xSeconds / 60.0,
			*Step.GameClock, *Step.Stage, *Step.Name);
	}
	Out += FString::Printf(TEXT("\n== Softlocks (%d escenarios) ==\n"), Softlocks.Num());
	for (const FPlaytestSoftlockCheck& Check : Softlocks)
	{
		Out += FString::Printf(TEXT("- %s: %s%s\n"), *Check.Name, Check.bCompletable ? TEXT("se puede terminar") : TEXT("SOFTLOCK"),
			Check.Detail.IsEmpty() ? TEXT("") : *(TEXT(" — ") + Check.Detail));
	}
	Out += FString::Printf(TEXT("\nRESULTADO: %s\n"), Verdict(Passed(Summary, Tuning) && AllCompletable(Softlocks)));
	return Out;
}
