#include "Debug/PlaytestReportModel.h"

const TCHAR* LexToString(EPlaytestIssueType Type)
{
	switch (Type)
	{
	case EPlaytestIssueType::MaterialMissing: return TEXT("MaterialMissing");
	case EPlaytestIssueType::PhysicsDrift: return TEXT("PhysicsDrift");
	case EPlaytestIssueType::FloatingOrBuried: return TEXT("FloatingOrBuried");
	case EPlaytestIssueType::SpawnBlocked: return TEXT("SpawnBlocked");
	default: return TEXT("Desconocido");
	}
}

bool FPlaytestReportModel::IsFloatingOrBuried(float VerticalClearanceCm, bool& bOutFloating, bool& bOutBuried)
{
	bOutFloating = VerticalClearanceCm > FloatingThresholdCm;
	bOutBuried = VerticalClearanceCm < BuriedThresholdCm;
	return bOutFloating || bOutBuried;
}

bool FPlaytestReportModel::DidPhysicsDrift(const FVector& StartLocationCm, const FVector& LocationAtSampleCm, float ThresholdCm)
{
	return FVector::Dist(StartLocationCm, LocationAtSampleCm) > ThresholdCm;
}

void FPlaytestReportModel::AppendMaterialIssue(FPlaytestReport& Report, const FString& MeshName, const FString& SlotName,
	int32 InstanceCount, const FVector& LocationMeters)
{
	FPlaytestIssue Issue;
	Issue.Type = EPlaytestIssueType::MaterialMissing;
	Issue.SubjectName = MeshName;
	Issue.Detail = FString::Printf(TEXT("slot %s"), *SlotName);
	Issue.LocationMeters = LocationMeters;
	Issue.InstanceCount = InstanceCount;
	Issue.Summary = FString::Printf(TEXT("%s (slot %s): material nulo o por defecto en %d instancia(s), cerca de (%.0f, %.0f, %.0f) m"),
		*MeshName, *SlotName, InstanceCount, LocationMeters.X, LocationMeters.Y, LocationMeters.Z);
	Report.Issues.Add(MoveTemp(Issue));
}

void FPlaytestReportModel::AppendPhysicsDriftIssue(FPlaytestReport& Report, const FString& ActorName,
	const FVector& StartLocationMeters, const FVector& LocationAtSampleMeters, bool bEndedBelowTerrain, bool bEndedBelowWater)
{
	FPlaytestIssue Issue;
	Issue.Type = EPlaytestIssueType::PhysicsDrift;
	Issue.SubjectName = ActorName;
	Issue.LocationMeters = LocationAtSampleMeters;
	TArray<FString> Flags;
	if (bEndedBelowTerrain)
	{
		Flags.Add(TEXT("por debajo del terreno"));
	}
	if (bEndedBelowWater)
	{
		Flags.Add(TEXT("por debajo del agua"));
	}
	Issue.Detail = FString::Printf(TEXT("inicio (%.2f, %.2f, %.2f) m -> %.1f s (%.2f, %.2f, %.2f) m"),
		StartLocationMeters.X, StartLocationMeters.Y, StartLocationMeters.Z, PhysicsSampleSeconds,
		LocationAtSampleMeters.X, LocationAtSampleMeters.Y, LocationAtSampleMeters.Z);
	FString FlagsCsv;
	for (int32 Index = 0; Index < Flags.Num(); ++Index)
	{
		FlagsCsv += (Index > 0 ? TEXT(", ") : TEXT("")) + Flags[Index];
	}
	Issue.Summary = FString::Printf(TEXT("%s se movió solo %.1f m en %.0f s%s%s"), *ActorName,
		FVector::Dist(StartLocationMeters, LocationAtSampleMeters), PhysicsSampleSeconds,
		FlagsCsv.IsEmpty() ? TEXT("") : TEXT(": "), *FlagsCsv);
	Report.Issues.Add(MoveTemp(Issue));
}

void FPlaytestReportModel::AppendFloatingOrBuriedIssue(FPlaytestReport& Report, const FString& SubjectName,
	const FVector& LocationMeters, float ClearanceCm, bool bFloating)
{
	FPlaytestIssue Issue;
	Issue.Type = EPlaytestIssueType::FloatingOrBuried;
	Issue.SubjectName = SubjectName;
	Issue.LocationMeters = LocationMeters;
	Issue.Detail = FString::Printf(TEXT("clearance %.1f cm"), ClearanceCm);
	Issue.Summary = FString::Printf(TEXT("%s %s (%.1f cm) en (%.0f, %.0f, %.0f) m"), *SubjectName,
		bFloating ? TEXT("flotando sobre el terreno") : TEXT("enterrado bajo el terreno"),
		FMath::Abs(ClearanceCm), LocationMeters.X, LocationMeters.Y, LocationMeters.Z);
	Report.Issues.Add(MoveTemp(Issue));
}

void FPlaytestReportModel::AppendSpawnBlockedIssue(FPlaytestReport& Report, const FString& SpawnName,
	const FVector& LocationMeters, const FString& BlockingActorsCsv)
{
	FPlaytestIssue Issue;
	Issue.Type = EPlaytestIssueType::SpawnBlocked;
	Issue.SubjectName = SpawnName;
	Issue.LocationMeters = LocationMeters;
	Issue.Detail = BlockingActorsCsv;
	Issue.Summary = FString::Printf(TEXT("%s bloqueado por: %s"), *SpawnName, *BlockingActorsCsv);
	Report.Issues.Add(MoveTemp(Issue));
}

FString FPlaytestReportModel::EscapeJsonString(const FString& In)
{
	return In.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n"));
}

FString FPlaytestReportModel::ToJson(const FPlaytestReport& Report)
{
	FString Out = TEXT("{\n");

	Out += TEXT("  \"issues\": [\n");
	for (int32 Index = 0; Index < Report.Issues.Num(); ++Index)
	{
		const FPlaytestIssue& Issue = Report.Issues[Index];
		Out += TEXT("    {\n");
		Out += FString::Printf(TEXT("      \"type\": \"%s\",\n"), LexToString(Issue.Type));
		Out += FString::Printf(TEXT("      \"subject\": \"%s\",\n"), *EscapeJsonString(Issue.SubjectName));
		Out += FString::Printf(TEXT("      \"detail\": \"%s\",\n"), *EscapeJsonString(Issue.Detail));
		Out += FString::Printf(TEXT("      \"summary\": \"%s\",\n"), *EscapeJsonString(Issue.Summary));
		Out += FString::Printf(TEXT("      \"location_m\": [%.3f, %.3f, %.3f],\n"),
			Issue.LocationMeters.X, Issue.LocationMeters.Y, Issue.LocationMeters.Z);
		Out += FString::Printf(TEXT("      \"instance_count\": %d\n"), Issue.InstanceCount);
		Out += (Index + 1 < Report.Issues.Num()) ? TEXT("    },\n") : TEXT("    }\n");
	}
	Out += TEXT("  ],\n");

	Out += TEXT("  \"frame_samples\": [\n");
	for (int32 Index = 0; Index < Report.FrameSamples.Num(); ++Index)
	{
		const FPlaytestFrameSample& Sample = Report.FrameSamples[Index];
		Out += TEXT("    {\n");
		Out += FString::Printf(TEXT("      \"shot\": \"%s\",\n"), *EscapeJsonString(Sample.ShotName));
		Out += FString::Printf(TEXT("      \"avg_fps\": %.1f,\n"), Sample.AvgFPS);
		Out += FString::Printf(TEXT("      \"min_fps\": %.1f,\n"), Sample.MinFPS);
		Out += FString::Printf(TEXT("      \"vram_mb\": %.1f\n"), Sample.VRAMUsedMB);
		Out += (Index + 1 < Report.FrameSamples.Num()) ? TEXT("    },\n") : TEXT("    }\n");
	}
	Out += TEXT("  ]\n");

	Out += TEXT("}\n");
	return Out;
}

FString FPlaytestReportModel::ToReadableSummary(const FPlaytestReport& Report)
{
	FString Out;
	Out += FString::Printf(TEXT("Informe de playtest automático — %d defecto(s), %d punto(s) de captura medido(s)\n\n"),
		Report.Issues.Num(), Report.FrameSamples.Num());

	auto AppendSection = [&Out, &Report](EPlaytestIssueType Type, const TCHAR* Title)
	{
		TArray<const FPlaytestIssue*> Matching;
		for (const FPlaytestIssue& Issue : Report.Issues)
		{
			if (Issue.Type == Type)
			{
				Matching.Add(&Issue);
			}
		}
		Out += FString::Printf(TEXT("== %s (%d) ==\n"), Title, Matching.Num());
		for (const FPlaytestIssue* Issue : Matching)
		{
			Out += TEXT("- ") + Issue->Summary + TEXT("\n");
		}
		Out += TEXT("\n");
	};

	AppendSection(EPlaytestIssueType::MaterialMissing, TEXT("Materiales nulos o por defecto"));
	AppendSection(EPlaytestIssueType::PhysicsDrift, TEXT("Física a la deriva"));
	AppendSection(EPlaytestIssueType::FloatingOrBuried, TEXT("Flotando o enterrado"));
	AppendSection(EPlaytestIssueType::SpawnBlocked, TEXT("Puntos de aparición bloqueados"));

	Out += TEXT("== Rendimiento por punto de captura ==\n");
	for (const FPlaytestFrameSample& Sample : Report.FrameSamples)
	{
		Out += FString::Printf(TEXT("- %s: %.1f fps medios, %.1f fps mínimos, %.1f MB VRAM\n"),
			*Sample.ShotName, Sample.AvgFPS, Sample.MinFPS, Sample.VRAMUsedMB);
	}

	return Out;
}
