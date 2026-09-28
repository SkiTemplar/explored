#include "Misc/AutomationTest.h"

#include "Core/ExploredNoise.h"
#include "WorldGen/IslandReliefModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace IslandReliefTest
{
	/** Isla cónica con rugosidad: baja al mar a partir de |Q| = 1. */
	float RoughCone(float Qx, float Qy)
	{
		static const FExploredNoise Noise(12);
		const float R = FMath::Sqrt(Qx * Qx + Qy * Qy);
		return 80.0f * (1.0f - R) + 4.0f * Noise.Fbm2D(Qx * 8.0f, Qy * 8.0f, 3);
	}

	FIslandReliefSettings SmallSettings(bool bRivers)
	{
		FIslandReliefSettings Settings;
		Settings.Resolution = 121;
		Settings.Erosion.Seed = 3;
		Settings.Erosion.CellSizeMeters = 8.0f;
		Settings.Erosion.DropletCount = 12000;
		Settings.Erosion.ThermalIterations = 10;
		Settings.bRivers = bRivers;
		Settings.Drainage.MinRiverArea = 120.0f;
		return Settings;
	}
}

BEGIN_DEFINE_SPEC(FIslandReliefModelSpec, "Explored.WorldGen.IslandRelief",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FIslandReliefModelSpec)

void FIslandReliefModelSpec::Define()
{
	using namespace IslandReliefTest;

	Describe("FIslandReliefGrid", [this]()
	{
		It("talla el relieve y se apaga sin costura en el borde de la rejilla", [this]()
		{
			const TSharedPtr<const FIslandReliefGrid> Grid = FIslandReliefGrid::Build(SmallSettings(true), &RoughCone,
				[](float, float) { return 1.0f; });
			float Deepest = 0.0f;
			for (float Q = -0.9f; Q <= 0.9f; Q += 0.01f)
			{
				for (float P = -0.9f; P <= 0.9f; P += 0.05f)
				{
					Deepest = FMath::Min(Deepest, Grid->SampleDelta(Q, P));
				}
			}
			TestTrue(*FString::Printf(TEXT("hay cauces (%.2f m)"), Deepest), Deepest < -1.0f);
			TestEqual(TEXT("fuera de la rejilla"), Grid->SampleDelta(1.35f, 0.2f), 0.0f);
			float Jump = 0.0f;
			for (float Q = 1.15f; Q < 1.4f; Q += 0.001f)
			{
				for (float P = -1.2f; P <= 1.2f; P += 0.2f)
				{
					Jump = FMath::Max(Jump, FMath::Abs(Grid->SampleDelta(Q + 0.001f, P) - Grid->SampleDelta(Q, P)));
					Jump = FMath::Max(Jump, FMath::Abs(Grid->SampleDelta(P, Q + 0.001f) - Grid->SampleDelta(P, Q)));
				}
			}
			TestTrue(*FString::Printf(TEXT("sin salto en el borde (%.3f m)"), Jump), Jump < 0.3f);
			TestTrue(*FString::Printf(TEXT("sin pozos de gota (%d → %d)"), Grid->GetPitsBefore(), Grid->GetPitsAfter()),
				Grid->GetPitsAfter() <= Grid->GetPitsBefore() + 2);
		});

		It("no toca la roca dura ni con la erosión ni con los ríos", [this]()
		{
			const TSharedPtr<const FIslandReliefGrid> Grid = FIslandReliefGrid::Build(SmallSettings(true), &RoughCone,
				[](float Qx, float Qy) { return Qx > 0.0f ? 0.0f : 1.0f; });
			float Lowest = 0.0f;
			for (float Q = 0.05f; Q <= 0.9f; Q += 0.01f)
			{
				for (float P = -0.9f; P <= 0.9f; P += 0.03f)
				{
					Lowest = FMath::Min(Lowest, Grid->SampleDelta(Q, P));
				}
			}
			TestTrue(*FString::Printf(TEXT("la mitad dura no baja (%.3f m)"), Lowest), Lowest > -0.05f);
		});

		It("es determinista", [this]()
		{
			const auto Soft = [](float, float) { return 1.0f; };
			const TSharedPtr<const FIslandReliefGrid> A = FIslandReliefGrid::Build(SmallSettings(true), &RoughCone, Soft);
			const TSharedPtr<const FIslandReliefGrid> B = FIslandReliefGrid::Build(SmallSettings(true), &RoughCone, Soft);
			for (float Q = -1.0f; Q <= 1.0f; Q += 0.037f)
			{
				if (A->SampleDelta(Q, Q * 0.5f) != B->SampleDelta(Q, Q * 0.5f))
				{
					AddError(TEXT("dos construcciones iguales difieren"));
					return;
				}
			}
		});
	});

	Describe("FIslandReliefModel", [this]()
	{
		It("erosiona las islas con relieve de tierra y deja el atolón y los islotes", [this]()
		{
			FIslandReliefSettings S;
			for (EIslandArchetype A : {EIslandArchetype::Landing, EIslandArchetype::Emerald, EIslandArchetype::Smoke,
				EIslandArchetype::Mangrove, EIslandArchetype::Mesa})
			{
				TestTrue(*FString::Printf(TEXT("%s se erosiona"), LexToString(A)), FIslandReliefModel::SettingsFor(A, 600.0f, 1, S));
				const int32 Cells = S.Resolution * S.Resolution;
				TestTrue(TEXT("resolución acotada"), S.Resolution >= 64 && S.Resolution <= FIslandReliefModel::MaxResolution);
				TestTrue(TEXT("gotas suficientes"), S.Erosion.DropletCount * 3 >= Cells);
				TestTrue(TEXT("pincel ancho"), S.Erosion.ErosionRadius >= 2);
			}
			TestFalse(TEXT("atolón"), FIslandReliefModel::SettingsFor(EIslandArchetype::WhiteSands, 500.0f, 1, S));
			TestFalse(TEXT("islotes"), FIslandReliefModel::SettingsFor(EIslandArchetype::Teeth, 400.0f, 1, S));
		});

		It("comparte la rejilla de la misma isla y no la de otra altura", [this]()
		{
			FIslandDesc Island;
			Island.Archetype = EIslandArchetype::Landing;
			Island.Seed = 4321;
			Island.Radius = 300.0f;
			Island.MaxHeight = 40.0f;
			FIslandReliefSettings S = SmallSettings(false);
			const auto Soft = [](float, float) { return 1.0f; };
			const TSharedPtr<const FIslandReliefGrid> A = FIslandReliefModel::GetOrBuild(Island, S, &RoughCone, Soft);
			const TSharedPtr<const FIslandReliefGrid> B = FIslandReliefModel::GetOrBuild(Island, S, &RoughCone, Soft);
			Island.MaxHeight = 80.0f;
			const TSharedPtr<const FIslandReliefGrid> C = FIslandReliefModel::GetOrBuild(Island, S, &RoughCone, Soft);
			TestTrue(TEXT("misma isla, misma rejilla"), A == B);
			TestTrue(TEXT("otra altura, otra rejilla"), A != C);
		});
	});
}

#endif
