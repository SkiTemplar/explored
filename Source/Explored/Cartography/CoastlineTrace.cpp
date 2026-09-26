#include "Cartography/CoastlineTrace.h"

#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"

namespace CoastlineTraceDetail
{
	/** Franja de tierra a lo largo de un rayo: radio exterior e interior (m). */
	struct FLandRun
	{
		float Outer = 0.0f;
		float Inner = 0.0f;
	};

	/** Tierra emergida de esa isla (la de una vecina cercana no cuenta como su costa). */
	bool IsLand(const FTerrainDensity& Density, const FVector2D& P, int32 IslandIndex)
	{
		const FTerrainColumn Column = Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y));
		return Column.Height >= FArchipelagoLayout::SeaLevel && Column.IslandIndex == IslandIndex;
	}
}

TArray<FVector2D> FCoastlineTrace::TraceIsland(const FTerrainDensity& Density, int32 IslandIndex, int32 NumRays)
{
	using namespace CoastlineTraceDetail;

	TArray<FVector2D> Coast;
	const TArray<FIslandDesc>& Islands = Density.GetLayout().Islands;
	if (!Islands.IsValidIndex(IslandIndex) || NumRays < 3)
	{
		return Coast;
	}
	const FIslandDesc& Island = Islands[IslandIndex];
	// La costa nominal con lóbulos y ruido no pasa de ~1,45 radios.
	const float Start = Island.Radius * 1.5f;
	const float Step = FMath::Max(4.0f, Island.Radius / 60.0f);

	Coast.Reserve(NumRays);
	TArray<FLandRun> Runs;
	for (int32 Ray = 0; Ray < NumRays; ++Ray)
	{
		const float Angle = UE_TWO_PI * static_cast<float>(Ray) / static_cast<float>(NumRays);
		const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		auto At = [&Island, &Dir](float R) { return Island.Center + Dir * R; };

		Runs.Reset();
		bool bInLand = false;
		for (float R = Start; R >= 0.0f; R -= Step)
		{
			const bool bLand = IsLand(Density, At(R), IslandIndex);
			if (bLand && !bInLand)
			{
				FLandRun& Run = Runs.AddDefaulted_GetRef();
				Run.Outer = R;
				Run.Inner = R;
			}
			if (bLand)
			{
				Runs.Last().Inner = R;
			}
			bInLand = bLand;
		}
		if (Runs.Num() == 0)
		{
			continue;
		}

		int32 Chosen = INDEX_NONE;
		int32 Widest = 0;
		for (int32 I = 0; I < Runs.Num(); ++I)
		{
			const float Width = Runs[I].Outer - Runs[I].Inner;
			if (Chosen == INDEX_NONE && Width >= MinLandWidth)
			{
				Chosen = I;
			}
			if (Width > Runs[Widest].Outer - Runs[Widest].Inner)
			{
				Widest = I;
			}
		}
		if (Chosen == INDEX_NONE)
		{
			Chosen = Widest;
		}

		// Bisección entre el último mar y la primera tierra de esa franja.
		float Sea = FMath::Min(Runs[Chosen].Outer + Step, Start + Step);
		float Land = Runs[Chosen].Outer;
		for (int32 Iteration = 0; Iteration < 10; ++Iteration)
		{
			const float Mid = 0.5f * (Sea + Land);
			if (IsLand(Density, At(Mid), IslandIndex))
			{
				Land = Mid;
			}
			else
			{
				Sea = Mid;
			}
		}
		Coast.Add(At(0.5f * (Sea + Land)));
	}
	return Coast;
}
