#include "Misc/AutomationTest.h"

#include "Fauna/ProceduralGait.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FGaitSpec, "Explored.Fauna.Gait",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FGaitSpec)

void FGaitSpec::Define()
{
	It("sincroniza las diagonales al trotar", [this]()
	{
		const float FL = FProceduralGait::LegPhaseOffset(ELocomotion::Quadruped, 0, 1.0f);
		const float FR = FProceduralGait::LegPhaseOffset(ELocomotion::Quadruped, 1, 1.0f);
		const float BL = FProceduralGait::LegPhaseOffset(ELocomotion::Quadruped, 2, 1.0f);
		const float BR = FProceduralGait::LegPhaseOffset(ELocomotion::Quadruped, 3, 1.0f);
		TestEqual(TEXT("FL con BR"), FL, BR);
		TestEqual(TEXT("FR con BL"), FR, BL);
		TestTrue(TEXT("Pares opuestos"), FMath::IsNearlyEqual(FMath::Abs(FL - FR), 0.5f));
	});

	It("mantiene al menos dos patas apoyadas en todo momento al andar", [this]()
	{
		for (float Phase = 0.0f; Phase < 1.0f; Phase += 0.01f)
		{
			int32 Grounded = 0;
			for (int32 Leg = 0; Leg < 4; ++Leg)
			{
				Grounded += FProceduralGait::Leg(ELocomotion::Quadruped, Leg, Phase, 0.3f).Lift < 0.05f ? 1 : 0;
			}
			if (Grounded < 2)
			{
				AddError(FString::Printf(TEXT("Solo %d patas apoyadas en la fase %.2f"), Grounded, Phase));
				return;
			}
		}
	});

	It("es continua a lo largo del ciclo (sin saltos de pose)", [this]()
	{
		FLegPose Prev = FProceduralGait::Leg(ELocomotion::Quadruped, 0, 0.0f, 0.8f);
		for (float Phase = 0.002f; Phase <= 1.0f; Phase += 0.002f)
		{
			const FLegPose Pose = FProceduralGait::Leg(ELocomotion::Quadruped, 0, Phase, 0.8f);
			if (FMath::Abs(Pose.UpperPitch - Prev.UpperPitch) > 2.0f || FMath::Abs(Pose.Lift - Prev.Lift) > 0.05f)
			{
				AddError(FString::Printf(TEXT("Salto en la fase %.3f"), Phase));
				return;
			}
			Prev = Pose;
		}
	});

	It("no mueve las patas parado y pliega las alas del ave posada", [this]()
	{
		const FLegPose Still = FProceduralGait::Leg(ELocomotion::Quadruped, 2, 0.4f, 0.0f);
		TestEqual(TEXT("Sin giro"), Still.UpperPitch, 0.0f);
		TestEqual(TEXT("Alas plegadas"), FProceduralGait::Body(ELocomotion::Bird, 0.3f, 0.0f, 1.0f).WingFold, 1.0f);
		TestTrue(TEXT("Aleteo en vuelo"), FMath::Abs(FProceduralGait::Body(ELocomotion::Bird, 0.25f, 1.0f, 1.0f).WingFlap) > 10.0f);
	});

	It("alterna los grupos de patas del cangrejo", [this]()
	{
		const float A = FProceduralGait::LegPhaseOffset(ELocomotion::Octopod, 0, 0.5f);
		const float B = FProceduralGait::LegPhaseOffset(ELocomotion::Octopod, 1, 0.5f);
		TestTrue(TEXT("Medio ciclo de diferencia"), FMath::IsNearlyEqual(FMath::Abs(A - B), 0.5f));
	});
}

#endif
