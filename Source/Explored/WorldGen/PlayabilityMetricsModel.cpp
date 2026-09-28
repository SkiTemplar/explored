#include "WorldGen/PlayabilityMetricsModel.h"

#include "WorldGen/DrainageModel.h"

namespace
{
	const FIntPoint Steps4[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	const FIntPoint Steps8[8] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

	float StepLength(int32 N) { return N < 4 ? 1.0f : 1.41421356f; }

	/** Tamaño de la componente 4-conexa de Start dentro de Mask (marca las celdas visitadas). */
	int32 FloodCount(const TArray<uint8>& Mask, int32 Width, int32 Height, int32 Start, TArray<uint8>& Seen)
	{
		int32 Count = 0;
		TArray<int32> Stack = {Start};
		Seen[Start] = 1;
		while (!Stack.IsEmpty())
		{
			const int32 Cell = Stack.Pop();
			++Count;
			for (const FIntPoint& S : Steps4)
			{
				const int32 X = Cell % Width + S.X;
				const int32 Y = Cell / Width + S.Y;
				const int32 N = Y * Width + X;
				if (X >= 0 && Y >= 0 && X < Width && Y < Height && Mask[N] && !Seen[N])
				{
					Seen[N] = 1;
					Stack.Add(N);
				}
			}
		}
		return Count;
	}
}

int32 FFlatPatchStats::CountAtLeast(float MinAreaM2) const
{
	int32 Count = 0;
	for (float Area : PatchAreas)
	{
		Count += Area >= MinAreaM2 ? 1 : 0;
	}
	return Count;
}

FFlatPatchStats FPlayabilityMetricsModel::FlatPatches(const TArray<float>& Heights, int32 Width, int32 Height, float Spacing,
	const TArray<uint8>& LandMask, float MaxSlopeDeg)
{
	FFlatPatchStats Stats;
	if (Width < 3 || Height < 3 || Heights.Num() != Width * Height || LandMask.Num() != Heights.Num()
		|| !FMath::IsFinite(Spacing) || Spacing <= 0.0f)
	{
		return Stats;
	}
	const float MaxGradient = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(MaxSlopeDeg, 0.0f, 89.0f)));
	TArray<uint8> Flat;
	Flat.Init(0, Heights.Num());
	for (int32 Y = 1; Y + 1 < Height; ++Y)
	{
		for (int32 X = 1; X + 1 < Width; ++X)
		{
			const int32 I = Y * Width + X;
			if (!LandMask[I])
			{
				continue;
			}
			++Stats.LandCells;
			const float Dx = (Heights[I + 1] - Heights[I - 1]) / (2.0f * Spacing);
			const float Dy = (Heights[I + Width] - Heights[I - Width]) / (2.0f * Spacing);
			const float Gradient = FMath::Sqrt(Dx * Dx + Dy * Dy);
			Flat[I] = FMath::IsFinite(Gradient) && Gradient < MaxGradient ? 1 : 0;
			Stats.FlatCells += Flat[I];
		}
	}
	Stats.FlatFraction = Stats.LandCells > 0 ? static_cast<float>(Stats.FlatCells) / Stats.LandCells : 0.0f;
	TArray<uint8> Seen;
	Seen.Init(0, Heights.Num());
	for (int32 I = 0; I < Flat.Num(); ++I)
	{
		if (Flat[I] && !Seen[I])
		{
			Stats.PatchAreas.Add(FloodCount(Flat, Width, Height, I, Seen) * Spacing * Spacing);
		}
	}
	Stats.PatchAreas.Sort([](float A, float B) { return A > B; });
	return Stats;
}

float FPlayabilityMetricsModel::ClarkEvans(const TArray<FVector2D>& Points, float AreaM2)
{
	if (Points.Num() < 2 || !FMath::IsFinite(AreaM2) || AreaM2 <= 0.0f)
	{
		return 0.0f;
	}
	double Sum = 0.0;
	for (int32 I = 0; I < Points.Num(); ++I)
	{
		double Best = TNumericLimits<double>::Max();
		for (int32 J = 0; J < Points.Num(); ++J)
		{
			Best = J != I ? FMath::Min(Best, FVector2D::Distance(Points[I], Points[J])) : Best;
		}
		Sum += Best;
	}
	const double Expected = 0.5 / FMath::Sqrt(Points.Num() / static_cast<double>(AreaM2));
	return static_cast<float>(Sum / Points.Num() / Expected);
}

float FPlayabilityMetricsModel::CoefficientOfVariation(const TArray<float>& Values)
{
	if (Values.Num() < 2)
	{
		return 0.0f;
	}
	double Sum = 0.0;
	double SumSq = 0.0;
	for (float V : Values)
	{
		Sum += V;
		SumSq += static_cast<double>(V) * V;
	}
	const double Mean = Sum / Values.Num();
	const double Variance = FMath::Max(0.0, SumSq / Values.Num() - Mean * Mean);
	return FMath::Abs(Mean) > 1.0e-6 ? static_cast<float>(FMath::Sqrt(Variance) / FMath::Abs(Mean)) : 0.0f;
}

float FPlayabilityMetricsModel::Jaggedness(const TArray<float>& Cyclic)
{
	if (Cyclic.Num() < 3)
	{
		return 0.0f;
	}
	double Steps = 0.0;
	double Sum = 0.0;
	for (int32 I = 0; I < Cyclic.Num(); ++I)
	{
		Steps += FMath::Abs(Cyclic[(I + 1) % Cyclic.Num()] - Cyclic[I]);
		Sum += Cyclic[I];
	}
	return Sum > 1.0e-6 ? static_cast<float>(Steps / Sum) : 0.0f;
}

float FPlayabilityMetricsModel::MeanResultantLength(const TArray<float>& Angles, const TArray<float>& Weights)
{
	double C = 0.0;
	double S = 0.0;
	double Total = 0.0;
	for (int32 I = 0; I < Angles.Num(); ++I)
	{
		const double W = Weights.IsValidIndex(I) ? FMath::Max(0.0f, Weights[I]) : 1.0;
		C += W * FMath::Cos(Angles[I]);
		S += W * FMath::Sin(Angles[I]);
		Total += W;
	}
	return Total > 0.0 ? static_cast<float>(FMath::Sqrt(C * C + S * S) / Total) : 0.0f;
}

namespace
{
	/** Grafo del agua sobre la superficie rellena: receptor por máxima pendiente y caudal. */
	struct FFlowGraph
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<float> Accumulation;
		TArray<int32> Receiver;
		TArray<uint8> Land;

		FVector2D Position(int32 Cell) const { return FVector2D(Cell % Width, Cell / Width); }
	};

	FFlowGraph BuildFlowGraph(const TArray<float>& Heights, int32 Width, int32 Height, float SeaLevel)
	{
		FErosionHeightGrid Grid;
		Grid.Init(Width, Height, 0.0f);
		Grid.Heights = Heights;
		FFlowGraph Graph;
		Graph.Width = Width;
		Graph.Height = Height;
		const TArray<float> Filled = FDrainageModel::FilledSurface(Grid, SeaLevel);
		Graph.Accumulation = FDrainageModel::FlowAccumulation(Grid, SeaLevel, 1.1f);
		Graph.Receiver.Init(INDEX_NONE, Heights.Num());
		Graph.Land.Init(0, Heights.Num());
		for (int32 I = 0; I < Heights.Num(); ++I)
		{
			Graph.Land[I] = Heights[I] > SeaLevel ? 1 : 0;
			float Best = 0.0f;
			for (int32 N = 0; N < 8 && Graph.Land[I]; ++N)
			{
				const int32 X = I % Width + Steps8[N].X;
				const int32 Y = I / Width + Steps8[N].Y;
				if (X < 0 || Y < 0 || X >= Width || Y >= Height)
				{
					continue;
				}
				const float Drop = (Filled[I] - Filled[Y * Width + X]) / StepLength(N);
				if (Drop > Best)
				{
					Best = Drop;
					Graph.Receiver[I] = Y * Width + X;
				}
			}
		}
		return Graph;
	}

	/** Remonta el cauce principal desde la desembocadura; devuelve longitud / distancia recta. */
	float MainStemSinuosity(const FFlowGraph& Graph, int32 Mouth, float MinRiverCells)
	{
		float Length = 0.0f;
		int32 Current = Mouth;
		for (int32 Guard = 0; Guard < Graph.Width * Graph.Height; ++Guard)
		{
			int32 Next = INDEX_NONE;
			int32 NextStep = 0;
			for (int32 N = 0; N < 8; ++N)
			{
				const int32 X = Current % Graph.Width + Steps8[N].X;
				const int32 Y = Current / Graph.Width + Steps8[N].Y;
				const int32 C = Y * Graph.Width + X;
				const bool bUpstream = X >= 0 && Y >= 0 && X < Graph.Width && Y < Graph.Height && Graph.Receiver[C] == Current
					&& Graph.Accumulation[C] >= MinRiverCells;
				if (bUpstream && (Next == INDEX_NONE || Graph.Accumulation[C] > Graph.Accumulation[Next]))
				{
					Next = C;
					NextStep = N;
				}
			}
			if (Next == INDEX_NONE)
			{
				break;
			}
			Length += StepLength(NextStep);
			Current = Next;
		}
		const float Straight = static_cast<float>(FVector2D::Distance(Graph.Position(Mouth), Graph.Position(Current)));
		return Straight >= 15.0f ? Length / Straight : 0.0f;
	}

	bool IsMouth(const FFlowGraph& Graph, int32 Cell, float MinRiverCells)
	{
		const int32 R = Graph.Receiver[Cell];
		return Graph.Land[Cell] && Graph.Accumulation[Cell] >= MinRiverCells && (R == INDEX_NONE || !Graph.Land[R]);
	}

	/** Una desembocadura ancha son varias celdas vecinas: las marca y devuelve la de más caudal. */
	int32 ClusterMouth(const FFlowGraph& Graph, int32 Start, float MinRiverCells, TArray<uint8>& MouthSeen)
	{
		int32 Best = Start;
		TArray<int32> Stack = {Start};
		MouthSeen[Start] = 1;
		while (!Stack.IsEmpty())
		{
			const int32 Cell = Stack.Pop();
			Best = Graph.Accumulation[Cell] > Graph.Accumulation[Best] ? Cell : Best;
			for (const FIntPoint& S : Steps8)
			{
				const int32 X = Cell % Graph.Width + S.X;
				const int32 Y = Cell / Graph.Width + S.Y;
				const int32 N = Y * Graph.Width + X;
				if (X >= 0 && Y >= 0 && X < Graph.Width && Y < Graph.Height && !MouthSeen[N] && IsMouth(Graph, N, MinRiverCells))
				{
					MouthSeen[N] = 1;
					Stack.Add(N);
				}
			}
		}
		return Best;
	}

	/** Coseno medio entre la dirección del agua y la radial hacia fuera en las celdas de río. */
	float MeanRadiality(const FFlowGraph& Graph, const FVector2D& Center, float MinRiverCells, int32& OutRiverCells)
	{
		double CosSum = 0.0;
		OutRiverCells = 0;
		for (int32 I = 0; I < Graph.Land.Num(); ++I)
		{
			if (!Graph.Land[I] || Graph.Accumulation[I] < MinRiverCells || Graph.Receiver[I] == INDEX_NONE)
			{
				continue;
			}
			++OutRiverCells;
			const FVector2D Radial = Graph.Position(I) - Center;
			const FVector2D Flow = Graph.Position(Graph.Receiver[I]) - Graph.Position(I);
			CosSum += Radial.Size() > 2.0 ? FVector2D::DotProduct(Radial.GetSafeNormal(), Flow.GetSafeNormal()) : 0.0;
		}
		return OutRiverCells > 0 ? static_cast<float>(CosSum / OutRiverCells) : 0.0f;
	}
}

FDrainagePattern FPlayabilityMetricsModel::DrainagePattern(const TArray<float>& Heights, int32 Width, int32 Height,
	const FVector2D& Center, float MinRiverCells, float SeaLevel)
{
	FDrainagePattern Result;
	if (Width < 3 || Height < 3 || Heights.Num() != Width * Height)
	{
		return Result;
	}
	const FFlowGraph Graph = BuildFlowGraph(Heights, Width, Height, SeaLevel);
	Result.Radiality = MeanRadiality(Graph, Center, MinRiverCells, Result.RiverCells);
	TArray<float> MouthAngles;
	TArray<float> MouthWeights;
	TArray<uint8> MouthSeen;
	MouthSeen.Init(0, Heights.Num());
	double SinuositySum = 0.0;
	double SinuosityWeight = 0.0;
	for (int32 I = 0; I < Heights.Num(); ++I)
	{
		if (MouthSeen[I] || !IsMouth(Graph, I, MinRiverCells))
		{
			continue;
		}
		const int32 Best = ClusterMouth(Graph, I, MinRiverCells, MouthSeen);
		++Result.Mouths;
		const FVector2D D = Graph.Position(Best) - Center;
		MouthAngles.Add(FMath::Atan2(static_cast<float>(D.Y), static_cast<float>(D.X)));
		MouthWeights.Add(Graph.Accumulation[Best]);
		const float Sinuosity = MainStemSinuosity(Graph, Best, MinRiverCells);
		if (Sinuosity > 0.0f)
		{
			SinuositySum += Sinuosity * Graph.Accumulation[Best];
			SinuosityWeight += Graph.Accumulation[Best];
		}
	}
	Result.MouthResultant = MeanResultantLength(MouthAngles, MouthWeights);
	Result.Sinuosity = SinuosityWeight > 0.0 ? static_cast<float>(SinuositySum / SinuosityWeight) : 1.0f;
	return Result;
}
