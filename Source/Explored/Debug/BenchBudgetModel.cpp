#include "Debug/BenchBudgetModel.h"

FBenchViewStats FBenchBudgetModel::ReduceView(const FString& Name, const TArray<float>& FrameSeconds, const FBenchBudget& Budget)
{
	FBenchViewStats Stats;
	Stats.Name = Name;

	TArray<float> Fps;
	Fps.Reserve(FrameSeconds.Num());
	double TotalSeconds = 0.0;
	for (const float Seconds : FrameSeconds)
	{
		if (!FMath::IsFinite(Seconds) || Seconds <= 0.0f)
		{
			continue;
		}
		TotalSeconds += Seconds;
		Fps.Add(1.0f / Seconds);
	}
	if (Fps.IsEmpty())
	{
		return Stats;
	}

	Fps.Sort();
	Stats.Frames = Fps.Num();
	Stats.Seconds = static_cast<float>(TotalSeconds);
	Stats.AverageFps = static_cast<float>(Fps.Num() / TotalSeconds);
	Stats.MinimumFps = Fps[0];
	Stats.OnePercentLowFps = Fps[FMath::FloorToInt32(0.01f * static_cast<float>(Fps.Num() - 1))];
	Stats.bPassed = Stats.Frames >= Budget.MinFrames
		&& Stats.AverageFps >= Budget.MinAverageFps
		&& Stats.MinimumFps >= Budget.MinFrameFps;
	return Stats;
}

FBenchSummary FBenchBudgetModel::Summarize(const TArray<FBenchViewStats>& Views, const FBenchBudget& Budget)
{
	FBenchSummary Summary;
	Summary.Views = Views.Num();
	double TotalSeconds = 0.0;
	bool bHasMinimum = false;
	for (const FBenchViewStats& View : Views)
	{
		if (!View.bPassed)
		{
			Summary.FailedViews.Add(View.Name);
		}
		if (View.Frames <= 0)
		{
			continue;
		}
		Summary.Frames += View.Frames;
		TotalSeconds += View.Seconds;
		Summary.MinimumFps = bHasMinimum ? FMath::Min(Summary.MinimumFps, View.MinimumFps) : View.MinimumFps;
		bHasMinimum = true;
	}
	Summary.AverageFps = TotalSeconds > 0.0 ? static_cast<float>(Summary.Frames / TotalSeconds) : 0.0f;
	// Sin vistas no hay medida: un recorrido vacío (mapa sin PlayerStart, cuelgue) no aprueba.
	Summary.bPassed = Summary.Views > 0
		&& Summary.FailedViews.IsEmpty()
		&& Summary.AverageFps >= Budget.MinAverageFps
		&& Summary.MinimumFps >= Budget.MinFrameFps;
	return Summary;
}

float FBenchBudgetModel::SweepYawOffset(float Alpha, float HalfSweepDegrees)
{
	const float Clamped = FMath::Clamp(FMath::IsFinite(Alpha) ? Alpha : 0.0f, 0.0f, 1.0f);
	return FMath::Lerp(-HalfSweepDegrees, HalfSweepDegrees, Clamped);
}

FString FBenchBudgetModel::FormatView(const FBenchViewStats& View)
{
	return FString::Printf(TEXT("[Bench] fps %s: media %.1f, mínimo %.1f, 1 %% %.1f, %d fotogramas en %.1f s, %s"),
		*View.Name, View.AverageFps, View.MinimumFps, View.OnePercentLowFps, View.Frames, View.Seconds,
		View.bPassed ? TEXT("OK") : TEXT("FALLA"));
}

FString FBenchBudgetModel::FormatSummary(const FBenchSummary& Summary, const FBenchBudget& Budget)
{
	FString Line = FString::Printf(TEXT("[Bench] Resumen: %d vistas, %d fotogramas, media %.1f fps (objetivo ≥ %.0f), mínimo %.1f fps (objetivo ≥ %.0f): %s"),
		Summary.Views, Summary.Frames, Summary.AverageFps, Budget.MinAverageFps, Summary.MinimumFps, Budget.MinFrameFps,
		Summary.bPassed ? TEXT("OK") : TEXT("FALLA"));
	if (!Summary.FailedViews.IsEmpty())
	{
		Line += TEXT(" (fuera de presupuesto: ");
		Line += FString::Join(Summary.FailedViews, TEXT(", "));
		Line += TEXT(")");
	}
	return Line;
}
