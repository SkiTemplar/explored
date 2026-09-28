#include "Misc/AutomationTest.h"

#include "WorldGen/TerrainMetricsModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TerrainMetricsTest
{
	/** Fondo que baja en rampa continua de -10 a -60 m a lo largo de X. */
	FTerrainSampleGrid RampFloor(int32 Size)
	{
		FTerrainSampleGrid Grid;
		Grid.Init(Size, Size, FVector2D::ZeroVector, 10.0f);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				Grid.Heights[Grid.Index(X, Y)] = -10.0f - 50.0f * X / (Size - 1) - 0.37f * ((X * 7 + Y * 13) % 5);
			}
		}
		return Grid;
	}

	void AddDisc(FTerrainSampleGrid& Grid, int32 Cx, int32 Cy, int32 Radius, float Top)
	{
		for (int32 Y = Cy - Radius; Y <= Cy + Radius; ++Y)
		{
			for (int32 X = Cx - Radius; X <= Cx + Radius; ++X)
			{
				if (Grid.IsValid(X, Y) && FMath::Square(X - Cx) + FMath::Square(Y - Cy) <= Radius * Radius)
				{
					Grid.Heights[Grid.Index(X, Y)] = Top;
				}
			}
		}
	}

	/** Cono radial: la pendiente apunta en todas direcciones por igual. */
	TArray<float> Cone(int32 Size)
	{
		TArray<float> H;
		H.SetNum(Size * Size);
		const float C = (Size - 1) * 0.5f;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				H[Y * Size + X] = 100.0f - FMath::Sqrt(FMath::Square(X - C) + FMath::Square(Y - C));
			}
		}
		return H;
	}
}

BEGIN_DEFINE_SPEC(FTerrainMetricsModelSpec, "Explored.WorldGen.TerrainMetrics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FTerrainMetricsModelSpec)

void FTerrainMetricsModelSpec::Define()
{
	using namespace TerrainMetricsTest;

	Describe("FindUnexplainedBumps", [this]()
	{
		It("cuenta los bultos someros sueltos y respeta los anclajes declarados", [this]()
		{
			FTerrainSampleGrid Grid = RampFloor(60);
			AddDisc(Grid, 10, 10, 3, 2.0f);   // isla declarada
			AddDisc(Grid, 40, 40, 2, -1.0f);  // bulto suelto
			AddDisc(Grid, 50, 12, 0, 1.0f);   // una sola celda: ruido de muestreo
			const TArray<FVector2D> Anchors = {Grid.WorldPosition(10, 10)};
			const TArray<FTerrainBump> Bumps = FTerrainMetricsModel::FindUnexplainedBumps(Grid, -3.0f, 4, Anchors);
			TestEqual(TEXT("un bulto sin explicar"), Bumps.Num(), 1);
			if (Bumps.Num() == 1)
			{
				TestTrue(TEXT("en su sitio"), FVector2D::Distance(Bumps[0].Centroid, Grid.WorldPosition(40, 40)) < 1.0);
				TestEqual(TEXT("altura"), Bumps[0].MaxHeight, -1.0f);
			}
		});
	});

	Describe("SeafloorHistogram", [this]()
	{
		It("no ve meseta ni escalón en una rampa continua", [this]()
		{
			const FSeafloorHistogram H = FTerrainMetricsModel::SeafloorHistogram(RampFloor(80), -3.0f);
			TestTrue(TEXT("hay muestras"), H.SampleCount > 1000);
			TestTrue(*FString::Printf(TEXT("sin cota dominante (%.3f)"), H.ModeFraction), H.ModeFraction < 0.05f);
			TestTrue(*FString::Printf(TEXT("sin pico (%.2f)"), H.MaxSpike), H.MaxSpike < 2.0f);
		});

		It("detecta un fondo recortado a una cota fija", [this]()
		{
			FTerrainSampleGrid Grid = RampFloor(80);
			for (float& V : Grid.Heights)
			{
				V = FMath::Max(V, -30.0f);
			}
			const FSeafloorHistogram H = FTerrainMetricsModel::SeafloorHistogram(Grid, -3.0f);
			TestTrue(*FString::Printf(TEXT("cota dominante (%.3f)"), H.ModeFraction), H.ModeFraction > 0.3f);
			TestTrue(TEXT("a -30 m"), FMath::Abs(H.ModeHeight + 30.0f) < 0.2f);
			TestTrue(*FString::Printf(TEXT("pico (%.2f)"), H.MaxSpike), H.MaxSpike > 3.0f);
		});
	});

	Describe("MaxStepBelow", [this]()
	{
		It("encuentra un escalón en el fondo", [this]()
		{
			FTerrainSampleGrid Grid = RampFloor(40);
			const float Smooth = FTerrainMetricsModel::MaxStepBelow(Grid, -3.0f);
			for (int32 Y = 0; Y < 40; ++Y)
			{
				Grid.Heights[Grid.Index(20, Y)] -= 9.0f;
			}
			TestTrue(TEXT("rampa suave"), Smooth < 3.5f);
			TestTrue(TEXT("escalón"), FTerrainMetricsModel::MaxStepBelow(Grid, -3.0f) > 8.0f);
		});
	});

	Describe("LocalVariance", [this]()
	{
		It("es cero en un llano y positiva con relieve", [this]()
		{
			FTerrainSampleGrid Grid;
			Grid.Init(20, 20, FVector2D::ZeroVector, 4.0f);
			for (int32 I = 0; I < Grid.Heights.Num(); ++I)
			{
				Grid.Heights[I] = 5.0f;
				Grid.IslandIndex[I] = 0;
			}
			TestEqual(TEXT("llano"), FTerrainMetricsModel::LocalVariance(Grid, 0, 2.0f, 1), 0.0f);
			for (int32 I = 0; I < Grid.Heights.Num(); ++I)
			{
				Grid.Heights[I] += (I % 3) * 1.0f;
			}
			TestTrue(TEXT("relieve"), FTerrainMetricsModel::LocalVariance(Grid, 0, 2.0f, 1) > 0.3f);
			TestEqual(TEXT("isla sin tierra"), FTerrainMetricsModel::LocalVariance(Grid, 3, 2.0f, 1), -1.0f);
		});
	});

	Describe("GradientOrientation", [this]()
	{
		It("da un cono isótropo y detecta surcos alineados con la rejilla", [this]()
		{
			// Solo un disco: en el cuadrado entero las esquinas meten más celdas en diagonal.
			const TArray<float> Round = Cone(81);
			TArray<uint8> Disc;
			Disc.SetNum(Round.Num());
			for (int32 I = 0; I < Round.Num(); ++I)
			{
				Disc[I] = Round[I] > 62.0f ? 1 : 0;
			}
			const FOrientationStats Iso = FTerrainMetricsModel::GradientOrientation(Round, 81, 81, Disc, 0.1f);
			TestTrue(TEXT("muestras"), Iso.SampleCount > 1000);
			TestTrue(*FString::Printf(TEXT("ejes %.2f"), Iso.AxisExcess), FMath::Abs(Iso.AxisExcess - 1.0f) < 0.25f);
			TestTrue(*FString::Printf(TEXT("diagonales %.2f"), Iso.DiagonalExcess), FMath::Abs(Iso.DiagonalExcess - 1.0f) < 0.25f);

			TArray<float> Furrows;
			Furrows.SetNum(81 * 81);
			for (int32 I = 0; I < Furrows.Num(); ++I)
			{
				Furrows[I] = static_cast<float>((I % 81) % 2) * 3.0f + (I % 81) * 0.3f;
			}
			const FOrientationStats Aligned = FTerrainMetricsModel::GradientOrientation(Furrows, 81, 81, {}, 0.1f);
			TestTrue(*FString::Printf(TEXT("surcos en el eje %.2f"), Aligned.AxisExcess), Aligned.AxisExcess > 3.0f);
		});
	});

	Describe("CountPits", [this]()
	{
		It("cuenta pozos sobre el mar y no en una ladera", [this]()
		{
			TArray<float> H = Cone(21);
			TestEqual(TEXT("cono sin pozos"), FTerrainMetricsModel::CountPits(H, 21, 21, 0.0f, 0.2f), 0);
			H[5 * 21 + 5] -= 5.0f;
			H[15 * 21 + 12] -= 5.0f;
			TestEqual(TEXT("dos pozos"), FTerrainMetricsModel::CountPits(H, 21, 21, 0.0f, 0.2f), 2);
		});
	});
}

#endif
