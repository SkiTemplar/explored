#include "Fauna/ProceduralGait.h"

float FProceduralGait::LegPhaseOffset(ELocomotion Type, int32 LegIndex, float SpeedRatio)
{
	switch (Type)
	{
	case ELocomotion::Quadruped:
	case ELocomotion::BipedClimber:
	case ELocomotion::Reptile:
	{
		// Paso (lento): cada pata a un cuarto de ciclo. Trote (rápido): diagonales sincronizadas.
		static constexpr float Walk[4] = {0.0f, 0.5f, 0.75f, 0.25f};
		static constexpr float Trot[4] = {0.0f, 0.5f, 0.5f, 0.0f};
		const int32 I = FMath::Clamp(LegIndex, 0, 3);
		const float Blend = FMath::SmoothStep(0.45f, 0.75f, SpeedRatio);
		return FMath::Lerp(Walk[I], Trot[I], Blend);
	}
	case ELocomotion::Octopod:
		// Grupos alternos: patas pares con fase 0, impares con 0,5, con un pequeño desfase a lo largo del cuerpo.
		return FMath::Fmod((LegIndex % 2) * 0.5f + (LegIndex / 2) * 0.08f, 1.0f);
	default:
		return 0.0f;
	}
}

FLegPose FProceduralGait::Leg(ELocomotion Type, int32 LegIndex, float Phase, float SpeedRatio)
{
	FLegPose Pose;
	if (SpeedRatio <= 0.01f || Type == ELocomotion::Bird || Type == ELocomotion::Swimmer)
	{
		return Pose;
	}
	const float P = FMath::Fmod(Phase + LegPhaseOffset(Type, LegIndex, SpeedRatio) + 1.0f, 1.0f);
	// Apoyo el 60 % del ciclo (la pata va hacia atrás) y vuelo el 40 % (va hacia delante levantada).
	constexpr float Stance = 0.6f;
	const float Amplitude = FMath::Lerp(14.0f, 32.0f, SpeedRatio) * (Type == ELocomotion::Octopod ? 0.7f : 1.0f);
	if (P < Stance)
	{
		const float T = P / Stance;
		Pose.UpperPitch = FMath::Lerp(Amplitude, -Amplitude, T);
		Pose.LowerPitch = 4.0f;
		Pose.Lift = 0.0f;
	}
	else
	{
		const float T = (P - Stance) / (1.0f - Stance);
		const float Smooth = 0.5f - 0.5f * FMath::Cos(T * UE_PI);
		Pose.UpperPitch = FMath::Lerp(-Amplitude, Amplitude, Smooth);
		Pose.Lift = FMath::Sin(T * UE_PI);
		Pose.LowerPitch = 4.0f + Pose.Lift * FMath::Lerp(25.0f, 55.0f, SpeedRatio);
	}
	return Pose;
}

FBodyPose FProceduralGait::Body(ELocomotion Type, float Phase, float SpeedRatio, float Time)
{
	FBodyPose Pose;
	const float Breath = FMath::Sin(Time * 1.6f);
	switch (Type)
	{
	case ELocomotion::Bird:
	{
		// En vuelo: aleteo rápido que se reduce al planear; posado (SpeedRatio≈0): alas plegadas.
		const bool bFlying = SpeedRatio > 0.05f;
		Pose.WingFold = bFlying ? 0.0f : 1.0f;
		Pose.WingFlap = bFlying ? FMath::Sin(Phase * UE_TWO_PI) * FMath::Lerp(15.0f, 45.0f, SpeedRatio) : 0.0f;
		Pose.BodyBob = bFlying ? -FMath::Sin(Phase * UE_TWO_PI) * 2.0f : Breath * 0.3f;
		Pose.TailYaw = FMath::Sin(Time * 2.0f) * 4.0f;
		return Pose;
	}
	case ELocomotion::Swimmer:
		return Pose;
	default:
		break;
	}

	// Dos rebotes por ciclo (uno por cada par de apoyos), más fuertes al trotar.
	const float Bounce = FMath::Abs(FMath::Sin(Phase * UE_TWO_PI));
	Pose.BodyBob = SpeedRatio > 0.01f ? (Bounce - 0.5f) * FMath::Lerp(1.0f, 4.0f, SpeedRatio) : Breath * 0.4f;
	Pose.BodyRoll = FMath::Sin(Phase * UE_TWO_PI) * FMath::Lerp(1.0f, 3.0f, SpeedRatio);
	Pose.HeadPitch = -Pose.BodyBob * 0.8f + (SpeedRatio < 0.01f ? FMath::Sin(Time * 0.4f) * 3.0f : 0.0f);
	Pose.TailYaw = FMath::Sin(Time * FMath::Lerp(1.5f, 5.0f, SpeedRatio)) * FMath::Lerp(8.0f, 18.0f, SpeedRatio);
	if (Type == ELocomotion::Reptile)
	{
		Pose.SpineYaw = FMath::Sin(Phase * UE_TWO_PI) * FMath::Lerp(4.0f, 14.0f, SpeedRatio);
	}
	return Pose;
}
