#include "WorldGen/TerrainSurvey.h"

#include "WorldGen/TerrainDensity.h"

namespace
{
	/** Tierra firme a partir de la cual se mide el relieve interior (por encima de la playa). */
	constexpr float LandHeight = 2.0f;
	/** Una celda es cauce si queda al menos esto por debajo del máximo de su entorno 5x5. */
	constexpr float ChannelDepth = 1.0f;

	bool IsChannelCell(const FTerrainSampleGrid& Grid, int32 X, int32 Y)
	{
		const float H = Grid.Heights[Grid.Index(X, Y)];
		float Highest = H;
		for (int32 Dy = -2; Dy <= 2; ++Dy)
		{
			for (int32 Dx = -2; Dx <= 2; ++Dx)
			{
				if (Grid.IsValid(X + Dx, Y + Dy))
				{
					Highest = FMath::Max(Highest, Grid.Heights[Grid.Index(X + Dx, Y + Dy)]);
				}
			}
		}
		return Highest - H >= ChannelDepth;
	}

	FIslandRealism MeasureIsland(const FTerrainSampleGrid& Grid, int32 IslandIdx)
	{
		FIslandRealism Result;
		TArray<uint8> ChannelMask;
		ChannelMask.Init(0, Grid.Heights.Num());
		TArray<float> IslandOnly;
		IslandOnly.Init(-1000.0f, Grid.Heights.Num());
		for (int32 Y = 0; Y < Grid.Height; ++Y)
		{
			for (int32 X = 0; X < Grid.Width; ++X)
			{
				const int32 I = Grid.Index(X, Y);
				if (Grid.IslandIndex[I] != IslandIdx || Grid.Heights[I] < LandHeight)
				{
					continue;
				}
				++Result.LandCells;
				IslandOnly[I] = Grid.Heights[I];
				ChannelMask[I] = IsChannelCell(Grid, X, Y) ? 1 : 0;
			}
		}
		Result.FineVariance = FTerrainMetricsModel::LocalVariance(Grid, IslandIdx, LandHeight, 1);
		Result.Channels = FTerrainMetricsModel::GradientOrientation(Grid.Heights, Grid.Width, Grid.Height, ChannelMask, 0.05f * Grid.Spacing);
		const int32 Pits = FTerrainMetricsModel::CountPits(IslandOnly, Grid.Width, Grid.Height, LandHeight, 0.3f);
		const float LandKm2 = Result.LandCells * Grid.Spacing * Grid.Spacing / 1.0e6f;
		Result.PitsPerKm2 = LandKm2 > 0.0f ? Pits / LandKm2 : 0.0f;
		return Result;
	}
}

FTerrainSampleGrid FTerrainSurvey::SampleWorld(const FTerrainDensity& Density, float HalfExtent, float Spacing)
{
	FTerrainSampleGrid Grid;
	const float Step = FMath::IsFinite(Spacing) && Spacing > 0.5f ? Spacing : 0.5f;
	const int32 Cells = FMath::Clamp(static_cast<int32>(2.0f * HalfExtent / Step) + 1, 1, 4096);
	Grid.Init(Cells, Cells, FVector2D(-HalfExtent), Step);
	for (int32 Y = 0; Y < Cells; ++Y)
	{
		for (int32 X = 0; X < Cells; ++X)
		{
			const FVector2D P = Grid.WorldPosition(X, Y);
			const FTerrainColumn Column = Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y));
			Grid.Heights[Grid.Index(X, Y)] = Column.Height;
			Grid.IslandIndex[Grid.Index(X, Y)] = Column.IslandIndex;
		}
	}
	return Grid;
}

TArray<FVector2D> FTerrainSurvey::DeclaredAnchors(const FTerrainDensity& Density, const FTerrainSampleGrid& Grid)
{
	const FArchipelagoLayout& Layout = Density.GetLayout();
	TArray<FVector2D> Anchors;
	TArray<float> Highest;
	TArray<int32> HighestCell;
	Highest.Init(-1000.0f, Layout.Islands.Num());
	HighestCell.Init(INDEX_NONE, Layout.Islands.Num());
	for (int32 Cell = 0; Cell < Grid.Heights.Num(); ++Cell)
	{
		const int32 Idx = Grid.IslandIndex[Cell];
		if (Layout.Islands.IsValidIndex(Idx) && Grid.Heights[Cell] > Highest[Idx])
		{
			Highest[Idx] = Grid.Heights[Cell];
			HighestCell[Idx] = Cell;
		}
	}
	for (int32 Cell : HighestCell)
	{
		if (Cell != INDEX_NONE)
		{
			Anchors.Add(Grid.WorldPosition(Cell % Grid.Width, Cell / Grid.Width));
		}
	}
	for (const FIslandDesc& Island : Layout.Islands)
	{
		for (const FVector2D& Islet : Island.Islets)
		{
			Anchors.Add(Island.Center + Islet);
		}
		for (const FCayDesc& Cay : Island.Cays)
		{
			Anchors.Add(Cay.Center);
		}
	}
	for (const FSeamountDesc& Mount : Density.GetSeafloor().GetSeamounts())
	{
		if (Mount.IsIslet())
		{
			Anchors.Add(Mount.Center);
		}
	}
	return Anchors;
}

FTerrainRealismReport FTerrainSurvey::Measure(const FTerrainDensity& Density, const FTerrainSampleGrid& Grid)
{
	FTerrainRealismReport Report;
	constexpr int32 MinBumpCells = 4;
	const TArray<FTerrainBump> Bumps = FTerrainMetricsModel::FindUnexplainedBumps(Grid, BumpThreshold, MinBumpCells,
		DeclaredAnchors(Density, Grid));
	Report.UnexplainedBumps = Bumps.Num();
	for (const FTerrainBump& Bump : Bumps)
	{
		Report.HighestUnexplainedBump = FMath::Max(Report.HighestUnexplainedBump, Bump.MaxHeight);
	}
	Report.Seafloor = FTerrainMetricsModel::SeafloorHistogram(Grid, BumpThreshold);
	Report.MaxSeafloorStep = FTerrainMetricsModel::MaxStepBelow(Grid, BumpThreshold, &Report.MaxSeafloorStepAt);
	for (int32 I = 0; I < FMath::Min(Bumps.Num(), 5); ++I)
	{
		Report.LargestBumps.Add(Bumps[I]);
	}
	for (int32 I = 0; I < Density.GetLayout().Islands.Num(); ++I)
	{
		FIslandRealism Island = MeasureIsland(Grid, I);
		Island.SmoothFraction = FTerrainMetricsModel::SmoothFraction(Grid, I, 0.5f, 0.005f);
		if (const FIslandReliefGrid* Relief = Density.GetRelief(I))
		{
			Island.ErosionPitsBefore = Relief->GetPitsBefore();
			Island.ErosionPitsAfter = Relief->GetPitsAfter();
		}
		Report.Islands.Add(Island);
	}
	return Report;
}
