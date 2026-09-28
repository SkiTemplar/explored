#include "Misc/AutomationTest.h"

#include <limits>

#include "Core/ExploredRandom.h"
#include "WorldGen/DrainageModel.h"
#include "WorldGen/TerrainMetricsModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DrainageTest
{
	/** Isla cónica que baja bajo el mar en los bordes, con rugosidad determinista. */
	FErosionHeightGrid RoughIsland(int32 Size, float Peak, uint32 Seed)
	{
		FErosionHeightGrid Grid;
		Grid.Init(Size, Size, 0.0f);
		const float C = (Size - 1) * 0.5f;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const float R = FMath::Sqrt(FMath::Square(X - C) + FMath::Square(Y - C)) / C;
				const float Rough = (ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Seed, X, Y)) - 0.5f) * 0.6f;
				Grid.At(X, Y) = Peak * (1.0f - R) - 3.0f + Rough;
			}
		}
		return Grid;
	}

	/** La misma isla ya erosionada por gotas: tiene valles de verdad en todas direcciones. */
	FErosionHeightGrid ErodedIsland(int32 Size, float Peak, uint32 Seed)
	{
		FErosionHeightGrid Grid = RoughIsland(Size, Peak, Seed);
		FErosionParams Params;
		Params.Seed = Seed;
		Params.DropletCount = Size * Size;
		Params.ThermalIterations = 10;
		FTerrainErosionModel::Erode(Grid, Params);
		return Grid;
	}

	/** Plano inclinado hacia X = 0 (el mar) con un pozo somero y una cubeta honda. */
	FErosionHeightGrid PlaneWithHoles()
	{
		FErosionHeightGrid Grid;
		Grid.Init(40, 40, 0.0f);
		for (int32 Y = 0; Y < 40; ++Y)
		{
			for (int32 X = 0; X < 40; ++X)
			{
				Grid.At(X, Y) = -1.0f + 0.5f * X;
			}
		}
		for (int32 Y = 9; Y <= 11; ++Y)
		{
			for (int32 X = 19; X <= 21; ++X)
			{
				Grid.At(X, Y) -= 1.2f; // pozo somero: cerrado, 0,7 m bajo su borde
			}
		}
		for (int32 Y = 25; Y <= 32; ++Y)
		{
			for (int32 X = 20; X <= 27; ++X)
			{
				Grid.At(X, Y) -= 6.0f; // lago
			}
		}
		return Grid;
	}
}

BEGIN_DEFINE_SPEC(FDrainageModelSpec, "Explored.WorldGen.Drainage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDrainageModelSpec)

void FDrainageModelSpec::Define()
{
	using namespace DrainageTest;

	Describe("FillShallowDepressions", [this]()
	{
		It("rellena los pozos someros y respeta los lagos hondos", [this]()
		{
			FErosionHeightGrid Grid = PlaneWithHoles();
			const FErosionHeightGrid Before = Grid;
			FDrainageModel::FillShallowDepressions(Grid, 0.0f, 1.5f);
			TestTrue(TEXT("el pozo sube hasta su borde"), Grid.At(19, 10) >= Before.At(19, 10) + 0.5f);
			TestTrue(TEXT("el lago sigue hondo"), FMath::Abs(Grid.At(23, 28) - Before.At(23, 28)) < 1.0e-4f);
			for (int32 I = 0; I < Grid.Heights.Num(); ++I)
			{
				if (Grid.Heights[I] < Before.Heights[I])
				{
					AddError(TEXT("rellenar nunca rebaja una celda"));
					return;
				}
			}
		});
	});

	Describe("FlowAccumulation", [this]()
	{
		It("conserva el agua: todo lo que llueve en tierra acaba en el mar o en el borde", [this]()
		{
			const FErosionHeightGrid Grid = ErodedIsland(61, 40.0f, 3);
			const TArray<float> Flow = FDrainageModel::FlowAccumulation(Grid, 0.0f, 1.1f);
			TestEqual(TEXT("una celda por celda"), Flow.Num(), Grid.Heights.Num());
			float Peak = 0.0f;
			for (float F : Flow)
			{
				Peak = FMath::Max(Peak, F);
				if (!(F >= 1.0f - 1.0e-3f) || !FMath::IsFinite(F))
				{
					AddError(FString::Printf(TEXT("caudal fuera de rango: %f"), F));
					return;
				}
			}
			TestTrue(TEXT("el agua se concentra en cauces"), Peak > 100.0f);
		});
	});

	Describe("CarveRivers", [this]()
	{
		It("talla ríos isótropos que bajan al mar sin dejar pozos", [this]()
		{
			FErosionHeightGrid Grid = ErodedIsland(121, 60.0f, 9);
			const FErosionHeightGrid Before = Grid;
			FDrainageParams Params;
			Params.MinRiverArea = 150.0f;
			const TArray<float> Depth = FDrainageModel::CarveRivers(Grid, Params);

			TArray<uint8> River;
			River.Init(0, Grid.Heights.Num());
			int32 RiverCells = 0;
			for (int32 Y = 0; Y < Grid.Height; ++Y)
			{
				for (int32 X = 0; X < Grid.Width; ++X)
				{
					const int32 I = Y * Grid.Width + X;
					const bool bDisc = FMath::Square(X - 60) + FMath::Square(Y - 60) < 55 * 55;
					River[I] = bDisc && Depth[I] > 0.5f ? 1 : 0;
					RiverCells += River[I];
					if (Grid.Heights[I] < Params.MinBedHeight - 1.0e-3f && Before.Heights[I] >= Params.MinBedHeight)
					{
						AddError(TEXT("un lecho baja de la cota mínima"));
						return;
					}
				}
			}
			TestTrue(*FString::Printf(TEXT("hay ríos (%d celdas)"), RiverCells), RiverCells > 300);
			const FOrientationStats Stats = FTerrainMetricsModel::GradientOrientation(Grid.Heights, Grid.Width, Grid.Height, River, 0.05f);
			TestTrue(*FString::Printf(TEXT("ejes %.2f"), Stats.AxisExcess), Stats.AxisExcess > 0.7f && Stats.AxisExcess < 1.35f);
			TestTrue(*FString::Printf(TEXT("diagonales %.2f"), Stats.DiagonalExcess), Stats.DiagonalExcess > 0.7f && Stats.DiagonalExcess < 1.35f);
			const int32 Pits = FTerrainMetricsModel::CountPits(Grid.Heights, Grid.Width, Grid.Height, 0.5f, 0.3f);
			TestTrue(*FString::Printf(TEXT("pozos %d"), Pits), Pits <= 2);
		});

		It("es determinista y no propaga NaN", [this]()
		{
			FErosionHeightGrid A = RoughIsland(41, 30.0f, 5);
			A.At(7, 7) = std::numeric_limits<float>::quiet_NaN();
			FErosionHeightGrid B = A;
			FDrainageParams Params;
			Params.DepthPerSqrtArea = std::numeric_limits<float>::infinity();
			FDrainageModel::CarveRivers(A, Params);
			FDrainageModel::CarveRivers(B, Params);
			for (int32 I = 0; I < A.Heights.Num(); ++I)
			{
				if (!FMath::IsFinite(A.Heights[I]) || A.Heights[I] != B.Heights[I])
				{
					AddError(FString::Printf(TEXT("celda %d: %f vs %f"), I, A.Heights[I], B.Heights[I]));
					return;
				}
			}
		});
	});
}

#endif
