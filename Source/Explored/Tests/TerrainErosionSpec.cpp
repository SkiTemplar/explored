#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/TerrainErosion.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TerrainErosionTest
{
	/** Rejilla cuadrada de un cono perfectamente simétrico centrado, para probar la ruptura de simetría. */
	FErosionHeightGrid ConeGrid(int32 Size, float PeakHeight)
	{
		FErosionHeightGrid Grid;
		Grid.Init(Size, Size, 0.0f);
		const float Center = (Size - 1) * 0.5f;
		const float MaxDist = Center * 1.41421356f;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const float Dist = FMath::Sqrt(FMath::Square(X - Center) + FMath::Square(Y - Center));
				Grid.At(X, Y) = PeakHeight * (1.0f - Dist / MaxDist);
			}
		}
		return Grid;
	}

	/** Muestrea la altura en Count puntos de un anillo de radio Radius alrededor del centro de la rejilla. */
	TArray<float> SampleRing(const FErosionHeightGrid& Grid, float Radius, int32 Count)
	{
		TArray<float> Samples;
		const float Center = (FMath::Min(Grid.Width, Grid.Height) - 1) * 0.5f;
		for (int32 I = 0; I < Count; ++I)
		{
			const float Angle = I * UE_TWO_PI / Count;
			Samples.Add(Grid.Sample(Center + FMath::Cos(Angle) * Radius, Center + FMath::Sin(Angle) * Radius));
		}
		return Samples;
	}

	float Variance(const TArray<float>& Values)
	{
		double Mean = 0.0;
		for (float V : Values)
		{
			Mean += V;
		}
		Mean /= FMath::Max(Values.Num(), 1);
		double SqSum = 0.0;
		for (float V : Values)
		{
			SqSum += FMath::Square(V - Mean);
		}
		return static_cast<float>(SqSum / FMath::Max(Values.Num(), 1));
	}

	/** Cono con una ligera irregularidad determinista para dar a la erosión hidráulica algo con lo que romper la simetría. */
	FErosionHeightGrid RoughConeGrid(int32 Size, float PeakHeight, uint32 Seed)
	{
		FErosionHeightGrid Grid = ConeGrid(Size, PeakHeight);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const uint32 H = ExploredHash::Hash2D(Seed, X, Y);
				Grid.At(X, Y) += (ExploredHash::ToUnitFloat(H) - 0.5f) * 0.15f;
			}
		}
		return Grid;
	}
}

BEGIN_DEFINE_SPEC(FTerrainErosionSpec, "Explored.WorldGen.Erosion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FTerrainErosionSpec)

void FTerrainErosionSpec::Define()
{
	using namespace TerrainErosionTest;

	Describe("FTerrainErosionModel", [this]()
	{
		It("es determinista: misma semilla y misma rejilla dan la misma salida", [this]()
		{
			FErosionParams Params;
			Params.Seed = 4242;
			Params.DropletCount = 3000;
			Params.CellSizeMeters = 4.0f;

			FErosionHeightGrid A = RoughConeGrid(33, 40.0f, 7);
			FErosionHeightGrid B = RoughConeGrid(33, 40.0f, 7);
			FTerrainErosionModel::Erode(A, Params);
			FTerrainErosionModel::Erode(B, Params);

			TestEqual(TEXT("Tamaño"), A.Heights.Num(), B.Heights.Num());
			for (int32 I = 0; I < A.Heights.Num(); ++I)
			{
				if (A.Heights[I] != B.Heights[I])
				{
					AddError(FString::Printf(TEXT("Divergencia en la celda %d: %f vs %f"), I, A.Heights[I], B.Heights[I]));
					return;
				}
			}
		});

		It("conserva la masa de forma aproximada en la pasada hidráulica", [this]()
		{
			FErosionParams Params;
			Params.Seed = 99;
			Params.DropletCount = 4000;
			Params.CellSizeMeters = 4.0f;

			FErosionHeightGrid Grid = RoughConeGrid(33, 40.0f, 3);
			const double Before = Grid.TotalHeight();
			FTerrainErosionModel::ErodeHydraulic(Grid, Params);
			const double After = Grid.TotalHeight();

			// Las gotas que se evaporan cerca del borde con sedimento a cuestas pueden perder algo de
			// masa (sale de la rejilla sin depositar), pero nunca puede aparecer material de la nada
			// y la pérdida debe ser pequeña frente al volumen total del cono.
			TestTrue(TEXT("No gana masa"), After <= Before + 1.0);
			TestTrue(TEXT("No pierde más de un 10% de la masa"), After >= Before - FMath::Abs(Before) * 0.10);
		});

		It("reduce la pendiente máxima de forma coherente con el talud", [this]()
		{
			FErosionParams Params;
			Params.CellSizeMeters = 4.0f;
			Params.TalusAngleTangent = 0.7f;
			Params.ThermalIterations = 150;
			Params.ThermalTransferRate = 0.6f;
			Params.DropletCount = 0; // solo térmica

			// Escarpe vertical: la mitad izquierda alta, la mitad derecha baja.
			FErosionHeightGrid Grid;
			Grid.Init(24, 24, 0.0f);
			for (int32 Y = 0; Y < 24; ++Y)
			{
				for (int32 X = 0; X < 24; ++X)
				{
					Grid.At(X, Y) = X < 12 ? 60.0f : 0.0f;
				}
			}
			const float SlopeBefore = Grid.MaxNeighborSlope(Params.CellSizeMeters);
			FTerrainErosionModel::ErodeThermal(Grid, Params);
			const float SlopeAfter = Grid.MaxNeighborSlope(Params.CellSizeMeters);

			TestTrue(TEXT("La pendiente máxima baja"), SlopeAfter < SlopeBefore);
			TestTrue(TEXT("Se acerca al talud de reposo"), SlopeAfter < Params.TalusAngleTangent * 1.5f);
		});

		It("no supera nunca la pendiente de reposo tras suficientes pasadas térmicas, venga de donde venga", [this]()
		{
			FErosionParams Params;
			Params.CellSizeMeters = 3.0f;
			Params.TalusAngleTangent = 0.9f;
			Params.ThermalIterations = 120;
			Params.ThermalTransferRate = 0.5f;

			FErosionHeightGrid Grid = ConeGrid(20, 80.0f);
			// Pico puntiagudo adicional en el centro: la pendiente de partida es extrema.
			Grid.At(10, 10) += 200.0f;
			FTerrainErosionModel::ErodeThermal(Grid, Params);

			TestTrue(TEXT("Pendiente máxima acotada por el talud"), Grid.MaxNeighborSlope(Params.CellSizeMeters) <= Params.TalusAngleTangent + 0.05f);
		});

		It("erosiona un cono simétrico en canales: la varianza angular de la altura aumenta", [this]()
		{
			FErosionParams Params;
			Params.Seed = 1;
			Params.CellSizeMeters = 4.0f;
			Params.DropletCount = 6000;
			Params.MaxDropletLifetime = 40;
			Params.ThermalIterations = 10;

			FErosionHeightGrid Grid = RoughConeGrid(41, 60.0f, 11);
			const float VarianceBefore = Variance(SampleRing(Grid, 14.0f, 32));
			FTerrainErosionModel::Erode(Grid, Params);
			const float VarianceAfter = Variance(SampleRing(Grid, 14.0f, 32));

			TestTrue(TEXT("La erosión rompe la simetría del cono en canales"), VarianceAfter > VarianceBefore * 3.0f);
		});
	});
}

#endif
