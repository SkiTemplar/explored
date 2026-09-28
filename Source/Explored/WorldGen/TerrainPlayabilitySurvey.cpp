#include "WorldGen/TerrainPlayabilitySurvey.h"

#include "WorldGen/TerrainDensity.h"

namespace
{
	/** Tierra construible: por encima de la franja que moja la marea. */
	constexpr float BuildableMinHeight = 0.5f;
	/** Cuenca mínima de un río en la rejilla de jugabilidad (celdas; ~1 ha a 4 m). */
	constexpr float MinRiverCells = 600.0f;
	/** Cotas que delimitan la plataforma y el talud. */
	constexpr float ShelfBreakDepth = -10.0f;
	constexpr float SlopeToeDepth = -20.0f;
	/** Un cayo cuyo camino a su isla baja de esta cota es una mota suelta en el mar. */
	constexpr float IsolatedCayDepth = -8.0f;

	/** Tierra de la isla (no un cayo ni otra isla): altura > 0 y dentro de su costa nominal. */
	bool IsIslandLand(const FTerrainColumn& C, int32 IslandIdx)
	{
		return C.Height > 0.0f && C.IslandIndex == IslandIdx && C.NormalizedDistance < 1.1f;
	}

	/** Distancia desde el centro hasta la costa exterior a lo largo de Dir; < 0 si no la hay. */
	float FindCoast(const FTerrainDensity& Density, const FIslandDesc& Island, int32 IslandIdx, const FVector2D& Dir)
	{
		constexpr float Coarse = 6.0f;
		for (float D = Island.Radius * 1.6f; D > 0.0f; D -= Coarse)
		{
			const FVector2D P = Island.Center + Dir * D;
			if (!IsIslandLand(Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y)), IslandIdx))
			{
				continue;
			}
			// Afina: último punto de agua antes de la tierra.
			for (float Fine = D + Coarse; Fine > D; Fine -= 0.5f)
			{
				const FVector2D Q = Island.Center + Dir * Fine;
				if (IsIslandLand(Density.SampleColumn(static_cast<float>(Q.X), static_cast<float>(Q.Y)), IslandIdx))
				{
					return Fine + 0.5f;
				}
			}
			return D;
		}
		return -1.0f;
	}

	float Gradient(const FTerrainDensity& Density, const FVector2D& P)
	{
		const float X = static_cast<float>(P.X);
		const float Y = static_cast<float>(P.Y);
		const float Dx = (Density.SampleColumn(X + 1.0f, Y).Height - Density.SampleColumn(X - 1.0f, Y).Height) * 0.5f;
		const float Dy = (Density.SampleColumn(X, Y + 1.0f).Height - Density.SampleColumn(X, Y - 1.0f).Height) * 0.5f;
		return FMath::Sqrt(Dx * Dx + Dy * Dy);
	}

	/** Pared de más de 60° en los primeros 20 m tierra adentro; OutHeight = cota más alta en 30 m. */
	bool IsCliff(const FTerrainDensity& Density, const FVector2D& Center, const FVector2D& Dir, float Coast, float& OutHeight)
	{
		const float MinGradient = FMath::Tan(FMath::DegreesToRadians(FTerrainPlayabilitySurvey::CliffSlopeDeg));
		bool bSteep = false;
		OutHeight = 0.0f;
		for (float S = 0.0f; S <= 30.0f; S += 1.0f)
		{
			const FVector2D P = Center + Dir * (Coast - S);
			OutHeight = FMath::Max(OutHeight, Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y)).Height);
			bSteep = bSteep || (S <= FTerrainPlayabilitySurvey::CliffBand && Gradient(Density, P) > MinGradient);
		}
		return bSteep;
	}

	/** Distancias desde la costa hasta las cotas de la plataforma y del talud (< 0 si no llega). */
	void MeasureShelf(const FTerrainDensity& Density, const FIslandDesc& Island, int32 IslandIdx, const FVector2D& Dir, float Coast,
		float& OutShelf, float& OutToe)
	{
		OutShelf = -1.0f;
		OutToe = -1.0f;
		for (float S = 0.0f; S < Island.Radius; S += 3.0f)
		{
			const FVector2D P = Island.Center + Dir * (Coast + S);
			const FTerrainColumn C = Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y));
			if (C.IslandIndex != IslandIdx && C.IslandIndex != INDEX_NONE)
			{
				return; // entra en el dominio de otra isla
			}
			if (OutShelf < 0.0f && C.Height <= ShelfBreakDepth)
			{
				OutShelf = S;
			}
			if (C.Height <= SlopeToeDepth)
			{
				OutToe = S;
				return;
			}
		}
	}

	float Median(TArray<float> Values)
	{
		if (Values.IsEmpty())
		{
			return 0.0f;
		}
		Values.Sort();
		return Values[Values.Num() / 2];
	}
}

FTerrainSampleGrid FTerrainPlayabilitySurvey::SampleIsland(const FTerrainDensity& Density, int32 IslandIdx, float Spacing)
{
	FTerrainSampleGrid Grid;
	const TArray<FIslandDesc>& Islands = Density.GetLayout().Islands;
	if (!Islands.IsValidIndex(IslandIdx) || !FMath::IsFinite(Spacing) || Spacing < 0.5f)
	{
		return Grid;
	}
	const FIslandDesc& Island = Islands[IslandIdx];
	const float Half = Island.Radius * 1.5f;
	const int32 Cells = FMath::Clamp(static_cast<int32>(2.0f * Half / Spacing) + 1, 3, 2048);
	Grid.Init(Cells, Cells, Island.Center - FVector2D(Half), Spacing);
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

FFlatPatchStats FTerrainPlayabilitySurvey::MeasureBuildable(const FTerrainSampleGrid& Grid, int32 IslandIdx)
{
	TArray<uint8> Land;
	Land.Init(0, Grid.Heights.Num());
	for (int32 I = 0; I < Land.Num(); ++I)
	{
		Land[I] = Grid.IslandIndex[I] == IslandIdx && Grid.Heights[I] > BuildableMinHeight ? 1 : 0;
	}
	return FPlayabilityMetricsModel::FlatPatches(Grid.Heights, Grid.Width, Grid.Height, Grid.Spacing, Land, BuildableSlopeDeg);
}

FDrainagePattern FTerrainPlayabilitySurvey::MeasureDrainage(const FTerrainSampleGrid& Grid, int32 IslandIdx, const FVector2D& Center)
{
	TArray<float> Heights = Grid.Heights;
	for (int32 I = 0; I < Heights.Num(); ++I)
	{
		Heights[I] = Grid.IslandIndex[I] == IslandIdx ? Heights[I] : FMath::Min(Heights[I], -1.0f);
	}
	const FVector2D CenterCell = (Center - Grid.Origin) / Grid.Spacing;
	return FPlayabilityMetricsModel::DrainagePattern(Heights, Grid.Width, Grid.Height, CenterCell, MinRiverCells, 0.0f);
}

FCoastStats FTerrainPlayabilitySurvey::MeasureCoast(const FTerrainDensity& Density, int32 IslandIdx, int32 RayCount)
{
	FCoastStats Stats;
	const TArray<FIslandDesc>& Islands = Density.GetLayout().Islands;
	if (!Islands.IsValidIndex(IslandIdx) || RayCount < 3)
	{
		return Stats;
	}
	const FIslandDesc& Island = Islands[IslandIdx];
	TArray<float> CliffHeights;
	for (int32 R = 0; R < RayCount; ++R)
	{
		const float Angle = UE_TWO_PI * R / RayCount;
		const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		const float Coast = FindCoast(Density, Island, IslandIdx, Dir);
		if (Coast < 0.0f)
		{
			continue;
		}
		++Stats.Rays;
		float Height = 0.0f;
		if (IsCliff(Density, Island.Center, Dir, Coast, Height))
		{
			++Stats.CliffRays;
			CliffHeights.Add(Height);
			Stats.CliffAngles.Add(Angle);
		}
		float Shelf = 0.0f;
		float Toe = 0.0f;
		MeasureShelf(Density, Island, IslandIdx, Dir, Coast, Shelf, Toe);
		if (Shelf >= 0.0f && Toe >= 0.0f)
		{
			Stats.ShelfWidths.Add(Shelf);
			Stats.SlopeWidths.Add(Toe - Shelf);
		}
	}
	Stats.CliffFraction = Stats.Rays > 0 ? static_cast<float>(Stats.CliffRays) / Stats.Rays : 0.0f;
	Stats.CliffMedianHeight = Median(CliffHeights);
	for (float H : CliffHeights)
	{
		Stats.CliffMaxHeight = FMath::Max(Stats.CliffMaxHeight, H);
	}
	Stats.ShelfWidthCV = FPlayabilityMetricsModel::CoefficientOfVariation(Stats.ShelfWidths);
	Stats.SlopeWidthCV = FPlayabilityMetricsModel::CoefficientOfVariation(Stats.SlopeWidths);
	Stats.ShelfJaggedness = FPlayabilityMetricsModel::Jaggedness(Stats.ShelfWidths);
	return Stats;
}

TArray<FVector2D> FTerrainPlayabilitySurvey::SeaMotes(const FTerrainDensity& Density)
{
	TArray<FVector2D> Motes;
	for (const FSeamountDesc& Mount : Density.GetSeafloor().GetSeamounts())
	{
		Motes.Add(Mount.Center);
	}
	const TArray<FIslandDesc>& Islands = Density.GetLayout().Islands;
	for (int32 I = 0; I < Islands.Num(); ++I)
	{
		for (const FCayDesc& Cay : Islands[I].Cays)
		{
			// Del cayo hacia su isla: si por el camino el fondo baja de -8 m, está suelto en el mar.
			const FVector2D Dir = (Islands[I].Center - Cay.Center).GetSafeNormal();
			const float Length = static_cast<float>(FVector2D::Distance(Islands[I].Center, Cay.Center));
			float Lowest = 0.0f;
			for (float S = 0.0f; S < Length; S += 4.0f)
			{
				const FVector2D P = Cay.Center + Dir * S;
				const FTerrainColumn C = Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y));
				if (IsIslandLand(C, I))
				{
					break;
				}
				Lowest = FMath::Min(Lowest, C.Height);
			}
			if (Lowest < IsolatedCayDepth)
			{
				Motes.Add(Cay.Center);
			}
		}
	}
	return Motes;
}

FPlayabilityReport FTerrainPlayabilitySurvey::Measure(const FTerrainDensity& Density, float SampleSpacing)
{
	FPlayabilityReport Report;
	const TArray<FIslandDesc>& Islands = Density.GetLayout().Islands;
	for (int32 I = 0; I < Islands.Num(); ++I)
	{
		const FTerrainSampleGrid Grid = SampleIsland(Density, I, SampleSpacing);
		FIslandPlayability Island;
		Island.Flat = MeasureBuildable(Grid, I);
		Island.Drainage = MeasureDrainage(Grid, I, Islands[I].Center);
		Island.Coast = MeasureCoast(Density, I, 360);
		Report.Islands.Add(MoveTemp(Island));
	}
	Report.SeaMotes = SeaMotes(Density);
	const float World = 2.0f * FArchipelagoLayout::WorldHalfExtent;
	Report.SeaMoteClarkEvans = FPlayabilityMetricsModel::ClarkEvans(Report.SeaMotes, World * World);
	return Report;
}
