#include "WorldGen/ShovelPathModel.h"

namespace ShovelPathDetail
{
	bool IsFinite(const FVector& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
	}
}

FShovelPathPlan FShovelPathModel::Plan(const FShovelPathRequest& Request, FGroundHeight Ground)
{
	FShovelPathPlan Out;
	if (!ShovelPathDetail::IsFinite(Request.Start) || !ShovelPathDetail::IsFinite(Request.End))
	{
		return Out;
	}
	const FVector2D From(Request.Start.X, Request.Start.Y);
	FVector2D Flat = FVector2D(Request.End.X, Request.End.Y) - From;
	const double Wanted = Flat.Size();
	if (!(Wanted >= MinLength))
	{
		return Out;
	}
	const FVector2D Dir = Flat / Wanted;
	const double Length = FMath::Min(Wanted, static_cast<double>(MaxLength));
	const FVector2D To = From + Dir * Length;

	const double ZA = Ground(From.X, From.Y);
	const double ZB = Ground(To.X, To.Y);
	if (!FMath::IsFinite(ZA) || !FMath::IsFinite(ZB))
	{
		return Out;
	}
	// El plano gira sobre el punto medio: corta arriba lo que rellena abajo.
	const double MaxRise = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(MaxSlopeDeg))) * Length;
	const double Rise = FMath::Clamp(ZB - ZA, -MaxRise, MaxRise);
	const double Mid = 0.5 * (ZA + ZB);

	Out.bValid = true;
	Out.Start = FVector(From.X, From.Y, Mid - 0.5 * Rise);
	Out.End = FVector(To.X, To.Y, Mid + 0.5 * Rise);
	Out.Length = Length;
	Out.GroundSlopeDeg = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(ZB - ZA), Length));
	Out.PathSlopeDeg = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Rise), Length));
	Out.HitsRequired = FMath::Max(1, FMath::CeilToInt32(HitsPerFullStrip * Length / MaxLength - 1.0e-9));
	Out.Material = Request.Material;
	Out.ToolTier = Request.ToolTier;
	return Out;
}

TArray<FShovelStroke> FShovelPathModel::Strokes(const FShovelPathPlan& Plan)
{
	TArray<FShovelStroke> Out;
	if (!Plan.bValid || !(Plan.Length > 0.0))
	{
		return Out;
	}
	const FVector Axis = Plan.End - Plan.Start;
	const FVector2D Dir = FVector2D(Axis.X, Axis.Y) / Plan.Length;
	const double Grade = Axis.Z / Plan.Length;
	// Normal del plano z = z0 + Grade·(P − Start)·Dir: plano a lo ancho, inclinado a lo largo.
	const FVector Normal = FVector(-Grade * Dir.X, -Grade * Dir.Y, 1.0).GetSafeNormal();
	const int32 Count = FMath::Max(1, FMath::CeilToInt32(Plan.Length / StrokeSpacing - 1.0e-9)) + 1;
	for (int32 I = 0; I < Count; ++I)
	{
		const double T = Count > 1 ? static_cast<double>(I) / (Count - 1) : 0.5;
		FShovelStroke Stroke;
		Stroke.Center = Plan.Start + Axis * T;
		Stroke.PlaneNormal = Normal;
		Stroke.Radius = StripWidth * 0.5f;
		Stroke.EdgeWidth = StrokeEdge;
		Stroke.VerticalReach = VerticalReach;
		Stroke.Material = Plan.Material;
		Stroke.ToolTier = Plan.ToolTier;
		Stroke.bMarkPath = false;
		Out.Add(Stroke);
	}
	return Out;
}

FShovelPathHitResult FShovelPathModel::ApplyHit(FShovelPathJob& Job, FTerrainEditModel& Terrain, FTerrainEditModel::FBaseDensity Base)
{
	FShovelPathHitResult Result;
	if (!Job.Plan.bValid || Job.bFinished || Job.HitsDone >= Job.Plan.HitsRequired)
	{
		return Result;
	}
	for (FShovelStroke Stroke : Strokes(Job.Plan))
	{
		Stroke.SoilBudget = FMath::Max(0.0, Job.Spare);
		const FTerrainEditResult R = Terrain.Shovel(Stroke, Base);
		if (R.bRejected)
		{
			// La pala no puede con este suelo: no cuenta el golpe.
			Result.Edit.bRejected = true;
			return Result;
		}
		Job.Spare = FMath::Max(0.0, Job.Spare + R.VolumeRemoved - R.VolumeAdded);
		Result.Edit.VolumeRemoved += R.VolumeRemoved;
		Result.Edit.VolumeAdded += R.VolumeAdded;
		Result.Edit.SamplesChanged += R.SamplesChanged;
		for (const FIntVector& Chunk : R.DirtyChunks)
		{
			Result.Edit.DirtyChunks.AddUnique(Chunk);
		}
	}
	++Job.HitsDone;
	if (Job.HitsDone >= Job.Plan.HitsRequired)
	{
		const FTerrainEditResult Compact = Terrain.CompactStrip(Job.Plan.Start, Job.Plan.End, StripWidth * 0.5f);
		for (const FIntVector& Chunk : Compact.DirtyChunks)
		{
			Result.Edit.DirtyChunks.AddUnique(Chunk);
		}
		Job.bFinished = true;
		Result.bFinished = true;
		Result.LootItem = Job.Plan.Material == ETerrainMaterial::Arena ? TEXT("arena") : TEXT("tierra_suelta");
		// Hacia abajo: una fracción de unidad se queda en el suelo.
		Result.LootUnits = static_cast<int32>(FMath::Min(1.0e6, FMath::FloorToDouble(Job.Spare * UnitsPerCubicMeter + 1.0e-9)));
		Job.Spare = 0.0;
	}
	Result.Edit.DirtyChunks.Sort([](const FIntVector& A, const FIntVector& B)
	{
		if (A.Z != B.Z) { return A.Z < B.Z; }
		if (A.Y != B.Y) { return A.Y < B.Y; }
		return A.X < B.X;
	});
	return Result;
}

float FShovelPathModel::StaminaFactor(const FTerrainEditModel& Terrain, double X, double Y)
{
	return Terrain.IsPath(X, Y) ? PathStaminaFactor : 1.0f;
}
