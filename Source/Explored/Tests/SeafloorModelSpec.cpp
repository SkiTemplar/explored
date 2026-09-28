#include "Misc/AutomationTest.h"

#include <limits>

#include "Core/ExploredRandom.h"
#include "WorldGen/SeafloorModel.h"
#include "WorldGen/TerrainMetricsModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SeafloorTest
{
	constexpr uint32 OfficialSeed = 20260926;

	float DistanceToSpine(const FArchipelagoLayout& Layout, const FVector2D& P)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 I = 0; I + 1 < Layout.Spine.Num(); ++I)
		{
			const FVector2D A = Layout.Spine[I];
			const FVector2D AB = Layout.Spine[I + 1] - A;
			const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0), 0.0, 1.0);
			Best = FMath::Min(Best, static_cast<float>(FVector2D::Distance(P, A + AB * T)));
		}
		return Best;
	}

	FTerrainSampleGrid SampleFloor(const FSeafloorModel& Model, float Spacing)
	{
		const float Half = FArchipelagoLayout::WorldHalfExtent;
		const int32 Cells = static_cast<int32>(2.0f * Half / Spacing) + 1;
		FTerrainSampleGrid Grid;
		Grid.Init(Cells, Cells, FVector2D(-Half), Spacing);
		for (int32 Y = 0; Y < Cells; ++Y)
		{
			for (int32 X = 0; X < Cells; ++X)
			{
				const FVector2D P = Grid.WorldPosition(X, Y);
				Grid.Heights[Grid.Index(X, Y)] = Model.FloorHeight(static_cast<float>(P.X), static_cast<float>(P.Y));
			}
		}
		return Grid;
	}
}

BEGIN_DEFINE_SPEC(FSeafloorModelSpec, "Explored.WorldGen.Seafloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSeafloorModelSpec)

void FSeafloorModelSpec::Define()
{
	using namespace SeafloorTest;

	Describe("FloorHeight", [this]()
	{
		It("no tiene cotas recortadas ni escalones", [this]()
		{
			const FSeafloorModel Model(FArchipelagoLayout::Generate(OfficialSeed));
			const FSeafloorHistogram H = FTerrainMetricsModel::SeafloorHistogram(SampleFloor(Model, 20.0f), 0.0f);
			TestTrue(*FString::Printf(TEXT("cota dominante %.2f %% a %.1f m"), H.ModeFraction * 100.0f, H.ModeHeight), H.ModeFraction < 0.01f);
			TestTrue(*FString::Printf(TEXT("pico del histograma %.2f a %.1f m"), H.MaxSpike, H.SpikeHeight), H.MaxSpike < 1.6f);
		});

		It("es continuo: dos puntos a medio metro nunca difieren más que una pendiente fuerte", [this]()
		{
			const FSeafloorModel Model(FArchipelagoLayout::Generate(OfficialSeed));
			FExploredRandom Rng(77);
			float Worst = 0.0f;
			for (int32 I = 0; I < 20000; ++I)
			{
				const float X = Rng.RangeFloat(-3000.0f, 3000.0f);
				const float Y = Rng.RangeFloat(-3000.0f, 3000.0f);
				Worst = FMath::Max(Worst, FMath::Abs(Model.FloorHeight(X, Y) - Model.FloorHeight(X + 0.5f, Y + 0.2f)));
			}
			TestTrue(*FString::Printf(TEXT("salto máximo %.3f m"), Worst), Worst < 0.9f);
		});

		It("sigue bajando lejos de la cadena y la dorsal no tiene la cresta plana", [this]()
		{
			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(OfficialSeed);
			const FSeafloorModel Model(Layout);
			double Near = 0.0, Far = 0.0;
			int32 NearN = 0, FarN = 0;
			TArray<float> Crest;
			for (int32 Y = -2950; Y <= 2950; Y += 25)
			{
				for (int32 X = -2950; X <= 2950; X += 25)
				{
					const float D = DistanceToSpine(Layout, FVector2D(X, Y));
					const float H = Model.BaseFloorHeight(static_cast<float>(X), static_cast<float>(Y));
					Near += (D > 1000.0f && D < 1300.0f) ? H : 0.0;
					NearN += (D > 1000.0f && D < 1300.0f) ? 1 : 0;
					Far += D > 1900.0f ? H : 0.0;
					FarN += D > 1900.0f ? 1 : 0;
					if (D < 40.0f)
					{
						Crest.Add(H);
					}
				}
			}
			TestTrue(TEXT("hay muestras"), NearN > 100 && FarN > 100 && Crest.Num() > 50);
			TestTrue(*FString::Printf(TEXT("más hondo lejos (%.1f vs %.1f)"), Far / FarN, Near / NearN), Far / FarN < Near / NearN - 2.0);
			Crest.Sort();
			TestTrue(*FString::Printf(TEXT("cresta irregular (%.1f a %.1f)"), Crest[0], Crest.Last()), Crest.Last() - Crest[0] > 4.0f);
		});

		It("devuelve un fondo finito aunque le lleguen coordenadas no finitas", [this]()
		{
			const FSeafloorModel Model(FArchipelagoLayout::Generate(OfficialSeed));
			TestTrue(TEXT("NaN"), FMath::IsFinite(Model.FloorHeight(std::numeric_limits<float>::quiet_NaN(), 0.0f)));
			TestTrue(TEXT("infinito"), FMath::IsFinite(Model.FloorHeight(0.0f, std::numeric_limits<float>::infinity())));
		});
	});

	Describe("GenerateSeamounts", [this]()
	{
		It("reparte pocos montículos sumergidos y algún islote, lejos de las islas", [this]()
		{
			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(OfficialSeed);
			const FSeafloorModel Model(Layout);
			const TArray<FSeamountDesc>& Mounts = Model.GetSeamounts();
			int32 Islets = 0;
			for (const FSeamountDesc& M : Mounts)
			{
				Islets += M.IsIslet() ? 1 : 0;
				const float Top = Model.FloorHeight(static_cast<float>(M.Center.X), static_cast<float>(M.Center.Y));
				if (M.IsIslet() ? Top < 2.0f : (Top > FSeafloorModel::MaxSubmergedPeak + 0.5f || Top < M.Peak - 2.0f))
				{
					AddError(FString::Printf(TEXT("cima a %.1f m (prevista %.1f)"), Top, M.Peak));
				}
				for (const FIslandDesc& Island : Layout.Islands)
				{
					if (FVector2D::Distance(Island.Center, M.Center) < Island.Radius * 2.0f + M.Radius)
					{
						AddError(TEXT("montículo dentro del alcance de una isla"));
					}
				}
			}
			TestTrue(*FString::Printf(TEXT("sumergidos (%d)"), Mounts.Num() - Islets), Mounts.Num() - Islets >= 3);
			TestTrue(*FString::Printf(TEXT("islotes (%d)"), Islets), Islets >= 1 && Islets <= 3);
		});

		It("es determinista", [this]()
		{
			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(424242);
			const TArray<FSeamountDesc> A = FSeafloorModel::GenerateSeamounts(Layout);
			const TArray<FSeamountDesc> B = FSeafloorModel::GenerateSeamounts(Layout);
			TestEqual(TEXT("cantidad"), A.Num(), B.Num());
			for (int32 I = 0; I < FMath::Min(A.Num(), B.Num()); ++I)
			{
				TestTrue(TEXT("misma posición"), A[I].Center == B[I].Center && A[I].Peak == B[I].Peak);
			}
		});
	});

	Describe("BlendIslandToFloor y CayHeight", [this]()
	{
		It("llevan el talud de las islas y la falda de los cayos al fondo real sin escalón", [this]()
		{
			const float Floor = -48.0f;
			TestEqual(TEXT("fin del alcance"), FSeafloorModel::BlendIslandToFloor(-12.0f, Floor, 1.0f), Floor);
			TestEqual(TEXT("isla intacta"), FSeafloorModel::BlendIslandToFloor(35.0f, Floor, 0.5f), 35.0f);
			float Previous = FSeafloorModel::BlendIslandToFloor(-12.0f, Floor, 0.7f);
			for (float R = 0.7f; R <= 1.0f; R += 0.01f)
			{
				const float H = FSeafloorModel::BlendIslandToFloor(-12.0f, Floor, R);
				TestTrue(TEXT("baja sin saltos"), H <= Previous + 1.0e-4f && Previous - H < 3.0f);
				Previous = H;
			}

			FCayDesc Cay;
			Cay.Center = FVector2D(100.0f, 0.0f);
			Cay.Radius = 30.0f;
			Cay.Height = 2.0f;
			TestTrue(TEXT("el cayo emerge"), FSeafloorModel::CayHeight(Cay, 100.0f, 0.0f, 0.0f, Floor) > 0.5f);
			float Worst = 0.0f;
			for (float X = 100.0f; X < 250.0f; X += 0.5f)
			{
				Worst = FMath::Max(Worst, FMath::Abs(FSeafloorModel::CayHeight(Cay, X + 0.5f, 0.0f, 0.0f, Floor) - FSeafloorModel::CayHeight(Cay, X, 0.0f, 0.0f, Floor)));
			}
			TestTrue(*FString::Printf(TEXT("falda continua (salto %.2f m)"), Worst), Worst < 1.0f);
			TestEqual(TEXT("fuera del cayo, el fondo"), FSeafloorModel::CayHeight(Cay, 400.0f, 0.0f, 0.0f, Floor), Floor);
		});
	});
}

#endif
