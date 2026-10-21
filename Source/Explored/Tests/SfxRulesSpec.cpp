#include "Misc/AutomationTest.h"

#include "Audio/SfxRules.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FSfxRulesSpec, "Explored.Audio.SfxRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSfxRulesSpec)

void FSfxRulesSpec::Define()
{
	Describe(TEXT("SurfaceFor"), [this]()
	{
		It(TEXT("elige arena, hierba, roca, madera y agua"), [this]()
		{
			FFootGround G;
			G.HeightM = 0.8f;
			TestTrue(TEXT("playa"), ExploredSfxRules::SurfaceFor(G) == EFootSurface::Sand);
			G.HeightM = 6.0f;
			TestTrue(TEXT("llano"), ExploredSfxRules::SurfaceFor(G) == EFootSurface::Grass);
			G.SlopeDeg = 40.0f;
			TestTrue(TEXT("pendiente"), ExploredSfxRules::SurfaceFor(G) == EFootSurface::Rock);
			G.bTerrain = false;
			TestTrue(TEXT("estructura"), ExploredSfxRules::SurfaceFor(G) == EFootSurface::Wood);
			G.WaterDepthM = 0.2f;
			TestTrue(TEXT("agua manda"), ExploredSfxRules::SurfaceFor(G) == EFootSurface::Water);
		});

		It(TEXT("nombra las cuatro variantes de cada superficie"), [this]()
		{
			TestEqual(TEXT("primera"), ExploredSfxRules::FootstepSound(EFootSurface::Grass, 0), FString(TEXT("sfx_footstep_grass_01")));
			TestEqual(TEXT("vuelta"), ExploredSfxRules::FootstepSound(EFootSurface::Water, 7), FString(TEXT("sfx_footstep_water_04")));
			TestEqual(TEXT("negativo"), ExploredSfxRules::FootstepSound(EFootSurface::Rock, -1), FString(TEXT("sfx_footstep_rock_02")));
		});
	});

	Describe(TEXT("FSfxCadence"), [this]()
	{
		It(TEXT("avisa una vez por periodo y no encadena tras un salto"), [this]()
		{
			FSfxCadence C;
			TestFalse(TEXT("a medias"), C.Advance(100.0f, 140.0f));
			TestTrue(TEXT("completa"), C.Advance(50.0f, 140.0f));
			TestTrue(TEXT("resto"), FMath::IsNearlyEqual(C.Accumulated, 10.0f));
			TestTrue(TEXT("salto"), C.Advance(5000.0f, 140.0f));
			TestTrue(TEXT("sin cola"), C.Accumulated < 140.0f);
			TestFalse(TEXT("periodo cero"), C.Advance(10.0f, 0.0f));
			TestFalse(TEXT("NaN"), C.Advance(NAN, 140.0f));
			TestFalse(TEXT("hacia atrás"), C.Advance(-10.0f, 140.0f));
		});
	});

	Describe(TEXT("Gaviotas y zancada"), [this]()
	{
		It(TEXT("solo de día, en la costa y sin temporal"), [this]()
		{
			TestTrue(TEXT("costa de día"), ExploredSfxRules::GullsAudible(0.8f, 0.0f, 0.0f));
			TestFalse(TEXT("interior"), ExploredSfxRules::GullsAudible(0.1f, 0.0f, 0.0f));
			TestFalse(TEXT("noche"), ExploredSfxRules::GullsAudible(0.8f, 0.9f, 0.0f));
			TestFalse(TEXT("ciclón"), ExploredSfxRules::GullsAudible(0.8f, 0.0f, 1.0f));
			TestEqual(TEXT("espera mínima"), ExploredSfxRules::GullGap(-1.0f), ExploredSfxRules::GullMinGapS);
			TestEqual(TEXT("espera máxima"), ExploredSfxRules::GullGap(2.0f), ExploredSfxRules::GullMaxGapS);
			TestTrue(TEXT("correr alarga la zancada"), ExploredSfxRules::StrideFor(600.0f) > ExploredSfxRules::StrideFor(200.0f));
		});
	});
}

#endif
