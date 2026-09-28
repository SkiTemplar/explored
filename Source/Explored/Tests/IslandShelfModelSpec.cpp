#include "Misc/AutomationTest.h"

#include "Core/ExploredNoise.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/IslandShapeModel.h"
#include "WorldGen/IslandShelfModel.h"
#include "WorldGen/PlayabilityMetricsModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace IslandShelfTest
{
	constexpr float Foot = -1.2f;
	constexpr float Floor = -45.0f;

	TArray<float> WidthsAround(const FIslandShelf& Shelf)
	{
		TArray<float> Widths;
		for (int32 I = 0; I < 360; ++I)
		{
			Widths.Add(Shelf.Width(FMath::DegreesToRadians(I - 180.0f)));
		}
		return Widths;
	}
}

BEGIN_DEFINE_SPEC(FIslandShelfModelSpec, "Explored.WorldGen.IslandShelf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FIslandShelfModelSpec)

void FIslandShelfModelSpec::Define()
{
	using namespace IslandShelfTest;

	Describe("Build", [this]()
	{
		It("es determinista y cambia de anchura a lo largo del perímetro sin salirse del alcance", [this]()
		{
			const FIslandShelf A = FIslandShelfModel::Build(1234u, {});
			const FIslandShelf B = FIslandShelfModel::Build(1234u, {});
			const TArray<float> Widths = WidthsAround(A);
			TestTrue(TEXT("determinista"), Widths == WidthsAround(B));
			const float Cv = FPlayabilityMetricsModel::CoefficientOfVariation(Widths);
			TestTrue(*FString::Printf(TEXT("variación %.2f"), Cv), Cv > 0.3f);
			for (int32 I = 0; I < 360; ++I)
			{
				const float Angle = FMath::DegreesToRadians(I - 180.0f);
				const float End = 1.0f + A.Width(Angle) + A.SlopeWidth(Angle);
				if (Widths[I] < 0.04f || End > FIslandShelfModel::MaxReach + 1.0e-3f || A.EdgeDepth(Angle) > -5.0f)
				{
					AddError(FString::Printf(TEXT("rumbo %d: anchura %.3f, fin %.3f, borde %.1f"), I, Widths[I], End, A.EdgeDepth(Angle)));
					return;
				}
			}
		});

		It("recoge los cayos en un lóbulo somero de la plataforma", [this]()
		{
			FShelfCay Cay;
			Cay.Angle = 1.0f;
			Cay.T = 1.4f;
			Cay.HalfWidth = 0.4f;
			const FIslandShelf Shelf = FIslandShelfModel::Build(99u, {Cay});
			const float AtCay = FIslandShelfModel::Profile(Shelf, Cay.Angle, Cay.T, Floor, Foot, 0.0f);
			const float Beside = FIslandShelfModel::Profile(Shelf, Cay.Angle + 0.2f, Cay.T, Floor, Foot, 0.0f);
			TestTrue(*FString::Printf(TEXT("bajo el cayo %.1f m"), AtCay), AtCay > -8.0f);
			TestTrue(*FString::Printf(TEXT("junto al cayo %.1f m"), Beside), Beside > -12.0f);
		});
	});

	Describe("Profile", [this]()
	{
		It("baja sin escalones desde el pie de la costa hasta el fondo real", [this]()
		{
			const FIslandShelf Shelf = FIslandShelfModel::Build(777u, {});
			for (int32 A = 0; A < 24; ++A)
			{
				const float Angle = UE_TWO_PI * A / 24.0f - UE_PI;
				float Previous = FIslandShelfModel::Profile(Shelf, Angle, 1.0f, Floor, Foot, 0.0f);
				TestEqual(TEXT("en la costa, el pie"), Previous, Foot);
				for (float T = 1.001f; T <= 1.95f; T += 0.001f)
				{
					const float H = FIslandShelfModel::Profile(Shelf, Angle, T, Floor, Foot, 0.0f);
					if (H > Previous + 1.0e-3f || Previous - H > 0.6f || !FMath::IsFinite(H))
					{
						AddError(FString::Printf(TEXT("rumbo %d, T %.3f: %.2f tras %.2f"), A, T, H, Previous));
						return;
					}
					Previous = H;
				}
				TestEqual(TEXT("acaba en el fondo"), Previous, Floor, 1.0e-3f);
			}
		});

		It("no devuelve alturas no finitas con entradas no finitas", [this]()
		{
			const FIslandShelf Shelf = FIslandShelfModel::Build(5u, {});
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			TestTrue(TEXT("rumbo NaN"), FMath::IsFinite(FIslandShelfModel::Profile(Shelf, NaN, 1.3f, Floor, Foot, 0.0f)));
			TestTrue(TEXT("T NaN"), FMath::IsFinite(FIslandShelfModel::Profile(Shelf, 0.5f, NaN, Floor, Foot, 0.0f)));
			TestTrue(TEXT("rizado NaN"), FMath::IsFinite(FIslandShelfModel::Profile(Shelf, 0.5f, 1.1f, Floor, Foot, NaN)));
		});
	});

	Describe("Cayos del archipiélago", [this]()
	{
		It("quedan sobre la plataforma de su isla, cerca de la costa real", [this]()
		{
			// Antes, a 1,45-1,75 radios del centro: T de 1,65 a 2,4, al borde del alcance de la isla.
			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed);
			int32 Cays = 0;
			for (const FIslandDesc& Island : Layout.Islands)
			{
				const FExploredNoise N(Island.Seed);
				for (const FCayDesc& Cay : Island.Cays)
				{
					const FVector2D Q = FIslandShapeModel::WarpedLocal(Island, N, static_cast<float>(Cay.Center.X), static_cast<float>(Cay.Center.Y));
					const float T = FIslandShapeModel::CoastT(Island, N, Q, false);
					TestTrue(*FString::Printf(TEXT("cayo de %s a T %.2f"), LexToString(Island.Archetype), T), T > 1.03f && T < 1.5f);
					++Cays;
				}
			}
			TestTrue(*FString::Printf(TEXT("cayos: %d"), Cays), Cays >= 8);
		});
	});
}

#endif
