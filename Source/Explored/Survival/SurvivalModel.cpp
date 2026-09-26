#include "Survival/SurvivalModel.h"

namespace
{
	// Horas de juego que tarda cada necesidad en vaciarse en reposo activo (modo Superviviente).
	constexpr float HungerHours = 36.0f;
	constexpr float ThirstHours = 20.0f;
	constexpr float RestHours = 30.0f;
	constexpr float SleepRecoveryHours = 6.0f;
	constexpr float NutrientHours = 72.0f;

	float ModeScale(ESurvivalMode Mode)
	{
		switch (Mode)
		{
		case ESurvivalMode::Explorer: return 0.6f;
		case ESurvivalMode::Castaway: return 1.35f;
		default: return 1.0f;
		}
	}

	float ActivityMetabolism(EActivity Activity)
	{
		switch (Activity)
		{
		case EActivity::Resting: return 0.8f;
		case EActivity::Walking: return 1.0f;
		case EActivity::Sprinting: return 1.8f;
		case EActivity::Swimming: return 1.7f;
		case EActivity::Working: return 1.5f;
		case EActivity::Sleeping: return 0.5f;
		default: return 1.0f;
		}
	}

	float Drain(float Value, float PerHour, float Hours)
	{
		return FMath::Clamp(Value - PerHour * Hours, 0.0f, 100.0f);
	}

	// Apnea de referencia (segundos) con pulmón base (ratio 1), sin esfuerzo ni peso.
	constexpr float BaseBreathHoldSeconds = 40.0f;
}

void FSurvivalState::AddCondition(ECondition C, float Hours)
{
	float& T = ConditionTime[static_cast<int32>(C)];
	T = FMath::Max(T, Hours);
}

float FSurvivalState::MaxEnergy() const
{
	float Max = 100.0f;
	Max -= FMath::Max(0.0f, 40.0f - Hunger) * 0.8f;
	Max -= FMath::Max(0.0f, 30.0f - Rest) * 0.8f;
	Max -= FMath::Max(0.0f, 60.0f - Health) * 0.4f;
	const float Diet = (Protein + Carbs + Vitamins) / 3.0f;
	Max -= FMath::Max(0.0f, 25.0f - Diet) * 0.6f;
	if (HasCondition(ECondition::Fever) || HasCondition(ECondition::Poisoned))
	{
		Max -= 20.0f;
	}
	return FMath::Clamp(Max, 20.0f, 100.0f);
}

float FSurvivalState::WorkEfficiency() const
{
	float E = 1.0f;
	E += (Morale - 50.0f) / 50.0f * 0.15f;
	E -= FMath::Max(0.0f, 30.0f - Rest) / 30.0f * 0.25f;
	E -= FMath::Clamp(FMath::Abs(BodyTemperature - 37.0f) - 1.0f, 0.0f, 2.0f) * 0.1f;
	if (HasCondition(ECondition::Sprain))
	{
		E -= 0.15f;
	}
	return FMath::Clamp(E, 0.5f, 1.15f);
}

float FSurvivalModel::EffectiveTemperature(const FSurvivalState& State, const FSurvivalInputs& In)
{
	float T = In.AirTemperature;
	T -= In.Wind * 6.0f * (In.bSheltered ? 0.2f : 1.0f);
	T -= State.Wetness * 7.0f;
	T -= In.bInWater ? 6.0f : 0.0f;
	T += In.FireHeat * 14.0f;
	T += In.SunExposure * 6.0f;
	T += In.ClothingInsulation * 8.0f;
	T += In.bSheltered ? 3.0f : 0.0f;
	return T;
}

float FSurvivalModel::EnergyDrainPerSecond(EActivity Activity, float CarriedWeightRatio)
{
	const float Weight = 1.0f + FMath::Max(0.0f, CarriedWeightRatio - 0.5f) * 1.2f;
	switch (Activity)
	{
	case EActivity::Sprinting: return 11.0f * Weight;
	case EActivity::Swimming: return 5.0f * Weight;
	case EActivity::Working: return 3.0f;
	case EActivity::Resting: return -14.0f;
	case EActivity::Sleeping: return -20.0f;
	default: return -8.0f; // Andar recupera, algo más despacio que descansar.
	}
}

float FSurvivalModel::OxygenDrainPerSecond(float CarriedWeightRatio, float LungCapacityRatio, bool bExerting)
{
	const float Lung = FMath::Max(LungCapacityRatio, 0.1f);
	const float Base = 100.0f / (BaseBreathHoldSeconds * Lung);
	const float ExertionMul = bExerting ? 1.6f : 1.0f;
	const float WeightMul = 1.0f + FMath::Max(0.0f, CarriedWeightRatio - 0.3f) * 0.8f;
	return Base * ExertionMul * WeightMul;
}

float FSurvivalModel::OxygenRecoveryPerSecond(float LungCapacityRatio)
{
	return 45.0f * FMath::Max(LungCapacityRatio, 0.1f);
}

void FSurvivalModel::Tick(FSurvivalState& S, const FSurvivalInputs& In, float DeltaHours, ESurvivalMode Mode,
	float RandomRoll, TArray<ESurvivalEvent>& OutEvents)
{
	if (S.IsDead() || DeltaHours <= 0.0f)
	{
		return;
	}
	const float Scale = ModeScale(Mode);
	const float Metabolism = ActivityMetabolism(In.Activity);
	const bool bSleeping = In.Activity == EActivity::Sleeping;
	const float Heat = FMath::Max(0.0f, In.AirTemperature - 30.0f) / 6.0f + In.SunExposure * 0.5f;

	// Necesidades.
	S.Hunger = Drain(S.Hunger, 100.0f / HungerHours * Scale * Metabolism, DeltaHours);
	S.Thirst = Drain(S.Thirst, 100.0f / ThirstHours * Scale * Metabolism * (1.0f + Heat * 0.6f), DeltaHours);
	if (bSleeping)
	{
		S.Rest = FMath::Clamp(S.Rest + 100.0f / SleepRecoveryHours * DeltaHours * (In.bSheltered ? 1.0f : 0.7f), 0.0f, 100.0f);
	}
	else
	{
		S.Rest = Drain(S.Rest, 100.0f / RestHours * Scale, DeltaHours);
	}
	for (float* Nutrient : {&S.Protein, &S.Carbs, &S.Vitamins})
	{
		*Nutrient = Drain(*Nutrient, 100.0f / NutrientHours * Scale, DeltaHours);
	}

	// Humedad: se moja con lluvia o agua; se seca al sol, con fuego o bajo techo.
	if (In.bInWater)
	{
		S.Wetness = 1.0f;
	}
	else
	{
		const float Wetting = In.bSheltered ? 0.0f : In.Rain * 2.0f;
		const float Drying = 0.35f + In.SunExposure * 1.2f + In.FireHeat * 2.5f + In.Wind * 0.4f;
		S.Wetness = FMath::Clamp(S.Wetness + (Wetting - Drying * (1.0f - In.Rain)) * DeltaHours, 0.0f, 1.0f);
	}

	// Temperatura corporal: tiende a un equilibrio según la temperatura efectiva.
	const float Effective = EffectiveTemperature(S, In);
	const float Comfort = 24.0f;
	const float Target = 37.0f + FMath::Clamp((Effective - Comfort) * 0.2f, -4.5f, 3.5f)
		+ (S.HasCondition(ECondition::Fever) ? 1.6f : 0.0f);
	S.BodyTemperature = FMath::FInterpTo(S.BodyTemperature, Target, DeltaHours, 1.2f);

	// Insolación y quemaduras solares.
	if (In.SunExposure > 0.7f && !In.bHasHat && In.AirTemperature > 29.0f)
	{
		S.AddCondition(ECondition::SunBurn, 10.0f);
	}

	// Estados: se consumen con el tiempo y dañan mientras duran.
	float Damage = 0.0f;
	for (int32 C = 0; C < static_cast<int32>(ECondition::Count); ++C)
	{
		float& T = S.ConditionTime[C];
		if (T <= 0.0f)
		{
			continue;
		}
		T = FMath::Max(0.0f, T - DeltaHours);
		switch (static_cast<ECondition>(C))
		{
		case ECondition::Bleeding: Damage += 6.0f * DeltaHours; break;
		case ECondition::Poisoned: Damage += 3.0f * DeltaHours; S.Thirst = Drain(S.Thirst, 4.0f, DeltaHours); break;
		case ECondition::Infection: Damage += 1.5f * DeltaHours; break;
		case ECondition::SunBurn: S.Morale = Drain(S.Morale, 1.0f, DeltaHours); break;
		default: break;
		}
	}
	// Una herida que sangra sin tratar puede infectarse (probabilidad por hora).
	if (S.HasCondition(ECondition::Bleeding) && RandomRoll < 0.04f * DeltaHours)
	{
		S.AddCondition(ECondition::Infection, 36.0f);
		S.AddCondition(ECondition::Fever, 24.0f);
	}

	// Necesidades en cero, hipotermia y golpe de calor.
	if (S.Hunger <= 0.0f)
	{
		Damage += 2.0f * DeltaHours;
		OutEvents.AddUnique(ESurvivalEvent::Starving);
	}
	if (S.Thirst <= 0.0f)
	{
		Damage += 5.0f * DeltaHours;
		OutEvents.AddUnique(ESurvivalEvent::Dehydrated);
	}
	if (S.Rest <= 0.0f)
	{
		OutEvents.AddUnique(ESurvivalEvent::Exhausted);
	}
	if (S.BodyTemperature < 35.0f)
	{
		Damage += (35.0f - S.BodyTemperature) * 6.0f * DeltaHours;
		OutEvents.AddUnique(ESurvivalEvent::Hypothermia);
	}
	if (S.BodyTemperature > 39.5f)
	{
		Damage += (S.BodyTemperature - 39.5f) * 6.0f * DeltaHours;
		OutEvents.AddUnique(ESurvivalEvent::Heatstroke);
	}

	// Recuperación: bien alimentado, hidratado, descansado y sin estados activos.
	const bool bHealthy = S.Hunger > 45.0f && S.Thirst > 45.0f && Damage <= 0.0f;
	if (bHealthy)
	{
		S.Health = FMath::Min(100.0f, S.Health + (bSleeping ? 6.0f : 1.5f) * DeltaHours);
	}

	// Ánimo: fuego, compañía, refugio y buena dieta suben; hambre, frío y lluvia bajan.
	float MoraleDelta = 0.0f;
	MoraleDelta += In.FireHeat * 4.0f + (In.bCompanionNearby ? 2.0f : 0.0f) + (In.bSheltered ? 1.0f : 0.0f);
	MoraleDelta -= (S.Hunger < 25.0f ? 3.0f : 0.0f) + (S.Thirst < 25.0f ? 3.0f : 0.0f);
	MoraleDelta -= (S.BodyTemperature < 36.0f ? 3.0f : 0.0f) + In.Rain * (In.bSheltered ? 0.0f : 2.0f);
	MoraleDelta -= 0.5f; // la soledad pesa
	S.Morale = FMath::Clamp(S.Morale + MoraleDelta * DeltaHours, 0.0f, 100.0f);

	// En modo Explorador las necesidades nunca matan.
	if (Mode == ESurvivalMode::Explorer)
	{
		Damage = FMath::Min(Damage, FMath::Max(0.0f, S.Health - 10.0f));
	}
	S.Health = FMath::Clamp(S.Health - Damage, 0.0f, 100.0f);
	S.Energy = FMath::Min(S.Energy, S.MaxEnergy());
	if (S.IsDead())
	{
		OutEvents.AddUnique(ESurvivalEvent::Died);
	}
}

void FSurvivalModel::Consume(FSurvivalState& S, const FConsumable& Item, float RandomRoll, TArray<ESurvivalEvent>& OutEvents)
{
	S.Hunger = FMath::Clamp(S.Hunger + Item.Food, 0.0f, 100.0f);
	S.Thirst = FMath::Clamp(S.Thirst + Item.Water, 0.0f, 100.0f);
	S.Protein = FMath::Clamp(S.Protein + Item.Protein, 0.0f, 100.0f);
	S.Carbs = FMath::Clamp(S.Carbs + Item.Carbs, 0.0f, 100.0f);
	S.Vitamins = FMath::Clamp(S.Vitamins + Item.Vitamins, 0.0f, 100.0f);
	S.BodyTemperature = FMath::Min(38.0f, S.BodyTemperature + Item.Warmth * 0.1f);
	S.Morale = FMath::Clamp(S.Morale + Item.Morale, 0.0f, 100.0f);
	S.Health = FMath::Clamp(S.Health + Item.Healing, 0.0f, 100.0f);
	for (int32 C = 0; C < static_cast<int32>(ECondition::Count); ++C)
	{
		if (Item.Cures & (1u << C))
		{
			S.ClearCondition(static_cast<ECondition>(C));
		}
	}
	if (Item.Toxicity > 0.0f && RandomRoll < Item.Toxicity)
	{
		S.AddCondition(ECondition::Poisoned, 8.0f + 16.0f * Item.Toxicity);
		OutEvents.AddUnique(ESurvivalEvent::GotPoisoned);
	}
}
