#include "Player/SwimModel.h"

#include "Survival/SurvivalModel.h"

namespace SwimModelDetail
{
	// Ganancia del control de flotación: cm/s de velocidad vertical por cm de error.
	constexpr float FloatGain = 6.0f;
	constexpr float FloatMaxSpeedCm = 300.0f;
	constexpr float FloatInterpSpeed = 3.0f;
	constexpr float DiveInterpSpeed = 2.0f;
	// Fracción de DiveSpeed con la que se baja manteniendo la tecla de bucear.
	constexpr float DiveDescentFactor = 0.6f;
	// Cargar de más cansa y hunde al nadar (GDD §4.1): pérdida de velocidad por unidad de carga.
	constexpr float FatiguePerWeightRatio = 0.35f;
	constexpr float MinFatigue = 0.35f;
	// Nadar por encima de esta fracción de SwimSpeed cuenta como esfuerzo en apnea.
	constexpr float ExertionSpeedRatio = 0.5f;
	constexpr float MaxStrokeSpeedRatio = 1.2f;

	bool IsSwimmingState(ESwimState State)
	{
		return State == ESwimState::Swimming || State == ESwimState::Diving;
	}
}

float FSwimModel::CoverageCm(const FSwimInputs& In)
{
	return In.WaterZ - (In.CenterZ - In.HalfHeight);
}

float FSwimModel::HeadDepthCm(const FSwimTuning& Tuning, const FSwimInputs& In)
{
	return In.WaterZ - (In.CenterZ + Tuning.HeadHeightCm);
}

ESwimState FSwimModel::NextState(const FSwimTuning& Tuning, ESwimState Previous, float Coverage, bool bDiveHeld,
	bool bHeadUnderwater)
{
	// Histéresis (H5): para empezar a nadar hace falta SwimDepthCm de agua, pero una
	// vez a flote solo se hace pie cuando baja claramente de ese umbral. El equilibrio
	// de la flotación queda justo en SwimDepthCm y los valles de ola no lo sacan.
	const bool bWasSwimming = SwimModelDetail::IsSwimmingState(Previous);
	const float ExitDepth = FMath::Max(0.0f, Tuning.SwimDepthCm - FMath::Max(0.0f, Tuning.SwimExitHysteresisCm));
	const bool bSwim = Coverage > 0.0f && Coverage >= (bWasSwimming ? ExitDepth : Tuning.SwimDepthCm);
	if (!bSwim)
	{
		return Coverage <= 0.0f ? ESwimState::OnLand : ESwimState::Wading;
	}
	// Bucear no es solo mantener la tecla (H4): con la cabeza bajo el agua se bucea,
	// se quiera o no, hasta que el pulmón devuelva al nadador a la superficie.
	return (bDiveHeld || bHeadUnderwater) ? ESwimState::Diving : ESwimState::Swimming;
}

FSwimStep FSwimModel::Tick(const FSwimTuning& Tuning, const FSwimInputs& In, float DeltaTime)
{
	DeltaTime = FMath::Max(0.0f, DeltaTime);

	FSwimStep Out;
	Out.Velocity = In.Velocity;

	// La cabeza se sumerge en cuanto el agua la cubre y sale cuando asoma un margen,
	// para que el oleaje en la superficie no alterne apnea y respiración.
	const bool bWasHeadUnderwater = bHeadUnderwater;
	if (In.bHasWater)
	{
		const float HeadDepth = HeadDepthCm(Tuning, In);
		bHeadUnderwater = bHeadUnderwater
			? HeadDepth > -FMath::Max(0.0f, Tuning.HeadSurfaceHysteresisCm)
			: HeadDepth > 0.0f;
	}
	else
	{
		bHeadUnderwater = false;
	}

	const bool bWasSwimming = IsSwimming();
	State = In.bHasWater
		? NextState(Tuning, State, CoverageCm(In), In.bDiveHeld, bHeadUnderwater)
		: ESwimState::OnLand;
	Out.bSwimming = IsSwimming();
	Out.bEnteredWater = !bWasSwimming && Out.bSwimming;
	Out.bLeftWater = bWasSwimming && !Out.bSwimming;

	TickMovement(Tuning, In, DeltaTime, Out);
	TickBreath(Tuning, In, DeltaTime, bWasHeadUnderwater, Out);
	return Out;
}

void FSwimModel::TickMovement(const FSwimTuning& Tuning, const FSwimInputs& In, float DeltaTime, FSwimStep& Out)
{
	using namespace SwimModelDetail;

	if (!IsSwimming())
	{
		StrokePhase = 0.0f;
		bExerting = false;
		return;
	}

	const float WeightRatio = Tuning.ComfortableWeightKg > 0.0f ? In.CarriedWeightKg / Tuning.ComfortableWeightKg : 0.0f;
	const float Fatigue = FMath::Clamp(1.0f - WeightRatio * FatiguePerWeightRatio, MinFatigue, 1.0f);
	Out.MaxSpeed = (State == ESwimState::Diving ? Tuning.DiveSpeed : Tuning.SwimSpeed) * Fatigue;
	Out.BrakingDeceleration = Tuning.SwimBrakingDeceleration;

	const float VelZ = static_cast<float>(In.Velocity.Z);
	if (State == ESwimState::Swimming)
	{
		// Flota a la altura de la ola bajo el jugador, con la cabeza fuera.
		const float Error = In.WaterZ - Tuning.FloatCenterDepthCm - In.CenterZ;
		const float DesiredVelZ = FMath::Clamp(Error * FloatGain, -FloatMaxSpeedCm, FloatMaxSpeedCm);
		Out.Velocity.Z = FMath::FInterpTo(VelZ, DesiredVelZ, DeltaTime, FloatInterpSpeed);
	}
	else
	{
		// Bucear es mantener la tecla para bajar; soltarla deja que el pulmón empuje
		// hacia arriba despacio hasta que la cabeza asome (entonces pasa a Swimming).
		const float DesiredVelZ = In.bDiveHeld ? -Tuning.DiveSpeed * DiveDescentFactor : Tuning.BuoyancyRiseSpeedCm;
		Out.Velocity.Z = FMath::FInterpTo(VelZ, DesiredVelZ, DeltaTime, DiveInterpSpeed);
	}

	// La corriente arrastra como velocidad (cm/s), no como aceleración (M4).
	Out.CurrentOffset = In.CurrentCmPerSecond * static_cast<double>(DeltaTime);

	// El esfuerzo y las brazadas salen de la velocidad propia, sin la corriente.
	const float OwnSpeed2D = static_cast<float>(In.Velocity.Size2D());
	bExerting = OwnSpeed2D > Tuning.SwimSpeed * ExertionSpeedRatio;

	// Brazadas: fase continua con la velocidad; el personaje la usa para animar las manos.
	const float SpeedRatio = FMath::Clamp(OwnSpeed2D / FMath::Max(Tuning.SwimSpeed, 1.0f), 0.0f, MaxStrokeSpeedRatio);
	if (SpeedRatio > KINDA_SMALL_NUMBER)
	{
		const float Prev = StrokePhase;
		StrokePhase = FMath::Fmod(StrokePhase + DeltaTime * SpeedRatio * Tuning.StrokeFrequency, 1.0f);
		Out.bStrokeCompleted = StrokePhase < Prev;
	}
}

void FSwimModel::TickBreath(const FSwimTuning& Tuning, const FSwimInputs& In, float DeltaTime, bool bWasHeadUnderwater,
	FSwimStep& Out)
{
	const double Before = Oxygen;
	if (bHeadUnderwater)
	{
		// Apnea mientras la cabeza esté bajo el agua, con o sin la tecla de bucear (H4).
		const float WeightRatio = Tuning.ComfortableWeightKg > 0.0f ? In.CarriedWeightKg / Tuning.ComfortableWeightKg : 0.0f;
		const float Drain = FSurvivalModel::OxygenDrainPerSecond(WeightRatio, In.LungCapacityRatio, bExerting);
		// Se acumula en double y sin umbral (M1): a 1000 fps o más el paso por
		// fotograma es de milésimas y en float cerca de 100 perdería precisión.
		Oxygen = FMath::Max(0.0, Oxygen - static_cast<double>(Drain) * DeltaTime);
		if (Oxygen <= 0.0)
		{
			Out.DrowningDamagePerSecond = Tuning.DrowningDamagePerSecond;
		}
	}
	else
	{
		// Solo se respira con la cabeza fuera.
		if (bWasHeadUnderwater && Oxygen < Tuning.GaspThreshold)
		{
			Out.bGasp = true;
		}
		const float Recovery = FSurvivalModel::OxygenRecoveryPerSecond(In.LungCapacityRatio);
		Oxygen = FMath::Min(100.0, Oxygen + static_cast<double>(Recovery) * DeltaTime);
	}
	Out.bOxygenChanged = Oxygen != Before;
}
