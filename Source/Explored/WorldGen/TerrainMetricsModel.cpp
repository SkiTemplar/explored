#include "WorldGen/TerrainMetricsModel.h"

void FTerrainSampleGrid::Init(int32 InWidth, int32 InHeight, const FVector2D& InOrigin, float InSpacing)
{
	Width = FMath::Max(InWidth, 0);
	Height = FMath::Max(InHeight, 0);
	Origin = InOrigin;
	Spacing = FMath::IsFinite(InSpacing) && InSpacing > 0.0f ? InSpacing : 1.0f;
	Heights.Init(0.0f, Width * Height);
	IslandIndex.Init(INDEX_NONE, Width * Height);
}

int32 FTerrainSampleGrid::CellAt(const FVector2D& World) const
{
	const FVector2D Local = (World - Origin) / Spacing;
	if (!FMath::IsFinite(Local.X) || !FMath::IsFinite(Local.Y))
	{
		return INDEX_NONE;
	}
	const int32 X = FMath::RoundToInt32(Local.X);
	const int32 Y = FMath::RoundToInt32(Local.Y);
	return IsValid(X, Y) ? Index(X, Y) : INDEX_NONE;
}

namespace
{
	/** Etiqueta (4-vecindad) la componente de Start por encima de Threshold; acumula su resumen. */
	FTerrainBump FloodComponent(const FTerrainSampleGrid& Grid, float Threshold, int32 Start, int32 Id, TArray<int32>& Label)
	{
		static const FIntPoint Steps[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
		FTerrainBump Bump;
		Bump.MaxHeight = Grid.Heights[Start];
		FVector2D Sum = FVector2D::ZeroVector;
		TArray<int32> Stack = {Start};
		Label[Start] = Id;
		while (!Stack.IsEmpty())
		{
			const int32 Cell = Stack.Pop();
			const int32 X = Cell % Grid.Width;
			const int32 Y = Cell / Grid.Width;
			++Bump.CellCount;
			Bump.MaxHeight = FMath::Max(Bump.MaxHeight, Grid.Heights[Cell]);
			Sum += Grid.WorldPosition(X, Y);
			for (const FIntPoint& S : Steps)
			{
				const int32 Nx = X + S.X;
				const int32 Ny = Y + S.Y;
				if (!Grid.IsValid(Nx, Ny))
				{
					continue;
				}
				const int32 N = Grid.Index(Nx, Ny);
				if (Label[N] == INDEX_NONE && Grid.Heights[N] > Threshold)
				{
					Label[N] = Id;
					Stack.Add(N);
				}
			}
		}
		Bump.Centroid = Sum / FMath::Max(Bump.CellCount, 1);
		return Bump;
	}
}

TArray<FTerrainBump> FTerrainMetricsModel::FindUnexplainedBumps(const FTerrainSampleGrid& Grid, float Threshold, int32 MinCells,
	const TArray<FVector2D>& Anchors)
{
	TArray<int32> Label;
	Label.Init(INDEX_NONE, Grid.Heights.Num());
	TArray<FTerrainBump> Components;
	for (int32 Cell = 0; Cell < Grid.Heights.Num(); ++Cell)
	{
		if (Label[Cell] == INDEX_NONE && Grid.Heights[Cell] > Threshold)
		{
			Components.Add(FloodComponent(Grid, Threshold, Cell, Components.Num(), Label));
		}
	}

	TArray<uint8> Explained;
	Explained.Init(0, Components.Num());
	for (const FVector2D& Anchor : Anchors)
	{
		const int32 Cell = Grid.CellAt(Anchor);
		if (Cell != INDEX_NONE && Label[Cell] != INDEX_NONE)
		{
			Explained[Label[Cell]] = 1;
		}
	}

	TArray<FTerrainBump> Result;
	for (int32 I = 0; I < Components.Num(); ++I)
	{
		if (!Explained[I] && Components[I].CellCount >= MinCells)
		{
			Result.Add(Components[I]);
		}
	}
	Result.Sort([](const FTerrainBump& A, const FTerrainBump& B) { return A.CellCount > B.CellCount; });
	return Result;
}

FSeafloorHistogram FTerrainMetricsModel::SeafloorHistogram(const FTerrainSampleGrid& Grid, float BelowHeight)
{
	FSeafloorHistogram Result;
	TMap<int32, int32> Fine;   // intervalos de 0,1 m
	TMap<int32, int32> Coarse; // intervalos de 1 m
	for (float H : Grid.Heights)
	{
		if (!FMath::IsFinite(H) || H >= BelowHeight)
		{
			continue;
		}
		++Result.SampleCount;
		Fine.FindOrAdd(FMath::FloorToInt32(H * 10.0f))++;
		Coarse.FindOrAdd(FMath::FloorToInt32(H))++;
	}
	if (Result.SampleCount == 0)
	{
		return Result;
	}
	int32 ModeCount = 0;
	for (const auto& Pair : Fine)
	{
		if (Pair.Value > ModeCount)
		{
			ModeCount = Pair.Value;
			Result.ModeHeight = (Pair.Key + 0.5f) * 0.1f;
		}
	}
	Result.ModeFraction = static_cast<float>(ModeCount) / Result.SampleCount;

	const int32 MinBinCount = FMath::Max(1, Result.SampleCount / 500);
	for (const auto& Pair : Coarse)
	{
		if (Pair.Value < MinBinCount)
		{
			continue;
		}
		const int32* Below = Coarse.Find(Pair.Key - 1);
		const int32* Above = Coarse.Find(Pair.Key + 1);
		const int32 Neighbor = FMath::Max3(1, Below ? *Below : 0, Above ? *Above : 0);
		const float Spike = static_cast<float>(Pair.Value) / Neighbor;
		if (Spike > Result.MaxSpike)
		{
			Result.MaxSpike = Spike;
			Result.SpikeHeight = Pair.Key + 0.5f;
		}
	}
	return Result;
}

namespace
{
	float WindowVariance(const FTerrainSampleGrid& Grid, int32 X, int32 Y, int32 Radius)
	{
		double Sum = 0.0;
		double SumSq = 0.0;
		int32 N = 0;
		for (int32 Dy = -Radius; Dy <= Radius; ++Dy)
		{
			for (int32 Dx = -Radius; Dx <= Radius; ++Dx)
			{
				const double H = Grid.Heights[Grid.Index(X + Dx, Y + Dy)];
				Sum += H;
				SumSq += H * H;
				++N;
			}
		}
		const double Mean = Sum / N;
		return static_cast<float>(FMath::Max(SumSq / N - Mean * Mean, 0.0));
	}
}

float FTerrainMetricsModel::LocalVariance(const FTerrainSampleGrid& Grid, int32 IslandIdx, float MinHeight, int32 Radius)
{
	const int32 R = FMath::Max(Radius, 1);
	double Sum = 0.0;
	int64 Count = 0;
	for (int32 Y = R; Y < Grid.Height - R; ++Y)
	{
		for (int32 X = R; X < Grid.Width - R; ++X)
		{
			const int32 I = Grid.Index(X, Y);
			if (Grid.IslandIndex[I] == IslandIdx && Grid.Heights[I] >= MinHeight)
			{
				Sum += WindowVariance(Grid, X, Y, R);
				++Count;
			}
		}
	}
	return Count > 0 ? static_cast<float>(Sum / Count) : -1.0f;
}

float FTerrainMetricsModel::SmoothFraction(const FTerrainSampleGrid& Grid, int32 IslandIdx, float MinHeight, float MaxVariance)
{
	int64 Land = 0;
	int64 Smooth = 0;
	for (int32 Y = 1; Y < Grid.Height - 1; ++Y)
	{
		for (int32 X = 1; X < Grid.Width - 1; ++X)
		{
			const int32 I = Grid.Index(X, Y);
			if (Grid.IslandIndex[I] != IslandIdx || Grid.Heights[I] < MinHeight)
			{
				continue;
			}
			++Land;
			Smooth += WindowVariance(Grid, X, Y, 1) < MaxVariance ? 1 : 0;
		}
	}
	return Land > 0 ? static_cast<float>(Smooth) / Land : 0.0f;
}

float FTerrainMetricsModel::MaxStepBelow(const FTerrainSampleGrid& Grid, float BelowHeight, FVector2D* OutWhere)
{
	float Best = 0.0f;
	for (int32 Y = 0; Y < Grid.Height; ++Y)
	{
		for (int32 X = 0; X < Grid.Width; ++X)
		{
			const float H = Grid.Heights[Grid.Index(X, Y)];
			if (H >= BelowHeight)
			{
				continue;
			}
			const float Right = X + 1 < Grid.Width ? Grid.Heights[Grid.Index(X + 1, Y)] : H;
			const float Up = Y + 1 < Grid.Height ? Grid.Heights[Grid.Index(X, Y + 1)] : H;
			const float Step = FMath::Max(Right < BelowHeight ? FMath::Abs(H - Right) : 0.0f, Up < BelowHeight ? FMath::Abs(H - Up) : 0.0f);
			if (Step > Best)
			{
				Best = Step;
				if (OutWhere)
				{
					*OutWhere = Grid.WorldPosition(X, Y);
				}
			}
		}
	}
	return Best;
}

FOrientationStats FTerrainMetricsModel::GradientOrientation(const TArray<float>& Heights, int32 Width, int32 Height,
	const TArray<uint8>& Mask, float MinGradient)
{
	FOrientationStats Stats;
	if (Width < 3 || Height < 3 || Heights.Num() != Width * Height)
	{
		return Stats;
	}
	const bool bMasked = Mask.Num() == Heights.Num();
	int32 NearAxis = 0;
	int32 NearDiagonal = 0;
	for (int32 Y = 1; Y < Height - 1; ++Y)
	{
		for (int32 X = 1; X < Width - 1; ++X)
		{
			const int32 I = Y * Width + X;
			if (bMasked && Mask[I] == 0)
			{
				continue;
			}
			const float Gx = 0.5f * (Heights[I + 1] - Heights[I - 1]);
			const float Gy = 0.5f * (Heights[I + Width] - Heights[I - Width]);
			if (!FMath::IsFinite(Gx) || !FMath::IsFinite(Gy) || Gx * Gx + Gy * Gy < MinGradient * MinGradient)
			{
				continue;
			}
			// Ángulo plegado a [0°, 45°]: 0 = eje de la rejilla, 45 = diagonal.
			const float Deg = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Gy), FMath::Abs(Gx)));
			const float Folded = Deg > 45.0f ? 90.0f - Deg : Deg;
			++Stats.SampleCount;
			NearAxis += Folded < 7.5f ? 1 : 0;
			NearDiagonal += Folded > 37.5f ? 1 : 0;
		}
	}
	if (Stats.SampleCount > 0)
	{
		// Isótropo: 15° de cada 90° caen junto a un eje y otros 15° junto a una diagonal.
		constexpr float Expected = 15.0f / 90.0f;
		Stats.AxisExcess = static_cast<float>(NearAxis) / Stats.SampleCount / Expected;
		Stats.DiagonalExcess = static_cast<float>(NearDiagonal) / Stats.SampleCount / Expected;
	}
	return Stats;
}

int32 FTerrainMetricsModel::CountPits(const TArray<float>& Heights, int32 Width, int32 Height, float MinHeight, float MinDepth)
{
	if (Width < 3 || Height < 3 || Heights.Num() != Width * Height)
	{
		return 0;
	}
	int32 Pits = 0;
	for (int32 Y = 1; Y < Height - 1; ++Y)
	{
		for (int32 X = 1; X < Width - 1; ++X)
		{
			const float H = Heights[Y * Width + X];
			if (H < MinHeight)
			{
				continue;
			}
			float Lowest = TNumericLimits<float>::Max();
			for (int32 Dy = -1; Dy <= 1; ++Dy)
			{
				for (int32 Dx = -1; Dx <= 1; ++Dx)
				{
					Lowest = (Dx == 0 && Dy == 0) ? Lowest : FMath::Min(Lowest, Heights[(Y + Dy) * Width + X + Dx]);
				}
			}
			Pits += Lowest - H >= MinDepth ? 1 : 0;
		}
	}
	return Pits;
}
