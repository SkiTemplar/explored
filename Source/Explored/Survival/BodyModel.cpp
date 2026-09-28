#include "Survival/BodyModel.h"

// Constantes del cuerpo. Reflejadas en Content/Data/survival_needs.json («body»):
// si cambias una aquí, cámbiala allí (Tools/DataCheck las compara).
namespace BodyModelDetail
{
	// Escorbuto: empieza con la vitamina C casi agotada y tarda días en empeorar.
	constexpr float ScurvyVitaminThreshold = 5.0f;
	constexpr float ScurvyRecoveryVitamins = 15.0f;
	constexpr float ScurvyOnsetHours = 240.0f;
	constexpr float ScurvyRecoveryHours = 48.0f;
	constexpr float ScurvyGumsAt = 0.15f;
	constexpr float ScurvyVisionAt = 0.45f;
	constexpr float ScurvyBleedingAt = 0.75f;
	constexpr float ScurvyBleedDamagePerHour = 0.5f;

	// Cortes.
	constexpr float WoundBleedDamagePerHour = 10.0f;
	constexpr float WoundClotPerHour = 0.25f;
	constexpr float WoundInfectionHours = 12.0f;
	constexpr float WoundHealHours = 36.0f;
	constexpr float BandageMaxDepth = 0.8f;

	// Caídas.
	constexpr float SafeFallHeight = 3.0f;
	constexpr float FallDamageScale = 4.0f;
	constexpr float FallDamageExponent = 1.6f;
	constexpr float SprainFallHeight = 4.5f;
	constexpr float SprainHours = 36.0f;
	constexpr float WaterSafeFallHeight = 12.0f;

	// Picaduras.
	constexpr float JellyfishStingHours = 6.0f;
	constexpr float JellyfishDamagePerHour = 1.0f;
	constexpr float RayStingHours = 12.0f;
	constexpr float RayDamagePerHour = 3.0f;
	constexpr float RayWoundDepth = 0.35f;

	// Sol.
	constexpr float SunBurnDoseHours = 2.0f;
	constexpr float HatSunFactor = 0.3f;
	constexpr float SunDoseDecayPerHour = 0.5f;

	// Dieta monótona.
	constexpr float MonotonyBalanceBelow = 0.5f;
	constexpr float MonotonyFullHours = 72.0f;

	// Toxicidad de cada fuente (probabilidad de intoxicarse, 0–1).
	constexpr float UnboiledWaterToxicity = 0.35f;
	constexpr float SwampWaterToxicity = 0.85f;
	constexpr float ToxicMushroomToxicity = 0.9f;
	constexpr float HallucinogenicMushroomToxicity = 0.15f;
	constexpr float HallucinogenHours = 4.0f;
	constexpr float RawCassavaToxicity = 0.8f;
	constexpr float RawCashewToxicity = 0.5f;
	constexpr float SeaWaterThirst = 15.0f;

	bool IsActive(const FSurvivalState& S, ECondition C)
	{
		return S.HasCondition(C);
	}
}

using namespace BodyModelDetail;

EScurvyStage FBodyModel::ScurvyStage(float Severity)
{
	if (Severity >= ScurvyBleedingAt)
	{
		return EScurvyStage::Bleeding;
	}
	if (Severity >= ScurvyVisionAt)
	{
		return EScurvyStage::Vision;
	}
	if (Severity >= ScurvyGumsAt)
	{
		return EScurvyStage::Gums;
	}
	return EScurvyStage::None;
}

void FBodyModel::AddCut(FSurvivalState& S, float Depth)
{
	FWound Wound;
	Wound.Depth = FMath::Clamp(Depth, 0.0f, 1.0f);
	Wound.Bleeding = Wound.Depth;
	if (Wound.Depth > 0.0f)
	{
		S.Wounds.Add(Wound);
	}
}

int32 FBodyModel::TreatWounds(FSurvivalState& S, EWoundTreatment Treatment)
{
	int32 Treated = 0;
	for (FWound& W : S.Wounds)
	{
		++Treated;
		W.HoursUntreated = 0.0f;
		switch (Treatment)
		{
		case EWoundTreatment::CleanWater:
			break;
		case EWoundTreatment::ClothBandage:
		case EWoundTreatment::LeafBandage:
			W.bBandaged = true;
			W.bMedicinal = W.bMedicinal || Treatment == EWoundTreatment::LeafBandage;
			// Una venda no basta para un corte muy profundo: sigue rezumando.
			W.Bleeding = W.Depth > BandageMaxDepth ? FMath::Min(W.Bleeding, W.Depth * 0.25f) : 0.0f;
			break;
		case EWoundTreatment::Suture:
			W.bBandaged = true;
			W.Bleeding = 0.0f;
			break;
		}
	}
	return Treated;
}

float FBodyModel::TotalBleeding(const FSurvivalState& S)
{
	float Total = IsActive(S, ECondition::Bleeding) ? 0.5f : 0.0f;
	for (const FWound& W : S.Wounds)
	{
		Total += W.Bleeding;
	}
	if (ScurvyStage(S.ScurvySeverity) == EScurvyStage::Bleeding)
	{
		Total += 0.1f;
	}
	return FMath::Clamp(Total, 0.0f, 1.0f);
}

float FBodyModel::BleedingDamagePerHour(const FSurvivalState& S)
{
	float Total = 0.0f;
	for (const FWound& W : S.Wounds)
	{
		Total += W.Bleeding * WoundBleedDamagePerHour;
	}
	return Total;
}

FFallResult FBodyModel::FallDamage(float HeightM, ELandingSurface Surface)
{
	FFallResult Result;
	// IsFinite explícito: con matemáticas rápidas `!(x > 0)` deja pasar un NaN y el daño saldría NaN.
	if (!FMath::IsFinite(HeightM) || HeightM <= 0.0f)
	{
		return Result;
	}
	if (Surface == ELandingSurface::Water)
	{
		// El agua amortigua: solo duelen los saltos muy altos, y como caer desde menos altura.
		const float Excess = HeightM - WaterSafeFallHeight;
		if (Excess > 0.0f)
		{
			Result.Damage = FallDamageScale * FMath::Pow(Excess, FallDamageExponent) * 0.5f;
		}
		return Result;
	}
	const float Excess = HeightM - SafeFallHeight;
	if (Excess <= 0.0f)
	{
		return Result;
	}
	float Hardness = 1.0f;
	float SprainHeight = SprainFallHeight;
	switch (Surface)
	{
	case ELandingSurface::Rock: Hardness = 1.15f; break;
	case ELandingSurface::Sand: Hardness = 0.75f; SprainHeight *= 1.3f; break;
	default: break;
	}
	Result.Damage = FallDamageScale * FMath::Pow(Excess, FallDamageExponent) * Hardness;
	if (HeightM >= SprainHeight)
	{
		Result.SprainHours = SprainHours * FMath::Clamp(Excess / 5.0f, 0.5f, 2.0f);
	}
	return Result;
}

FFallResult FBodyModel::ApplyFall(FSurvivalState& S, float HeightM, ELandingSurface Surface,
	const FSurvivalModeSettings& Mode, TArray<ESurvivalEvent>& OutEvents)
{
	FFallResult Result = FallDamage(HeightM, Surface);
	if (S.IsDead())
	{
		return Result;
	}
	float Damage = Result.Damage;
	if (!Mode.NeedsCanKill())
	{
		Damage = FMath::Min(Damage, FMath::Max(0.0f, S.Health - 10.0f));
	}
	S.Health = FMath::Clamp(S.Health - Damage, 0.0f, 100.0f);
	if (Result.SprainHours > 0.0f)
	{
		S.AddCondition(ECondition::Sprain, Result.SprainHours);
		OutEvents.AddUnique(ESurvivalEvent::Sprained);
	}
	if (Result.Damage > 5.0f)
	{
		ApplyMoraleEvent(S, EMoraleEvent::Injured);
	}
	if (S.IsDead())
	{
		OutEvents.AddUnique(ESurvivalEvent::Died);
	}
	return Result;
}

void FBodyModel::ApplySting(FSurvivalState& S, EStingKind Kind, TArray<ESurvivalEvent>& OutEvents)
{
	switch (Kind)
	{
	case EStingKind::Jellyfish:
		S.AddCondition(ECondition::JellyfishSting, JellyfishStingHours);
		break;
	case EStingKind::Ray:
		S.AddCondition(ECondition::RaySting, RayStingHours);
		// El aguijón de la raya deja además una herida punzante.
		AddCut(S, RayWoundDepth);
		break;
	}
	ApplyMoraleEvent(S, EMoraleEvent::Injured);
	OutEvents.AddUnique(ESurvivalEvent::Stung);
}

FConsumable FBodyModel::Antidote(EStingKind Kind)
{
	FConsumable Remedy;
	switch (Kind)
	{
	case EStingKind::Jellyfish:
		Remedy.Cures = SurvivalCureBit(ECondition::JellyfishSting);
		break;
	case EStingKind::Ray:
		Remedy.Cures = SurvivalCureBit(ECondition::RaySting);
		Remedy.Healing = 5.0f;
		break;
	}
	return Remedy;
}

FConsumable FBodyModel::WithHazard(const FConsumable& Base, EFoodHazard Hazard)
{
	FConsumable Item = Base;
	switch (Hazard)
	{
	case EFoodHazard::UnboiledWater: Item.Toxicity = FMath::Max(Item.Toxicity, UnboiledWaterToxicity); break;
	case EFoodHazard::SwampWater: Item.Toxicity = FMath::Max(Item.Toxicity, SwampWaterToxicity); break;
	case EFoodHazard::SeaWater: Item.Water = -SeaWaterThirst; break;
	case EFoodHazard::ToxicMushroom: Item.Toxicity = FMath::Max(Item.Toxicity, ToxicMushroomToxicity); break;
	case EFoodHazard::HallucinogenicMushroom:
		Item.Toxicity = FMath::Max(Item.Toxicity, HallucinogenicMushroomToxicity);
		Item.HallucinogenHours = FMath::Max(Item.HallucinogenHours, HallucinogenHours);
		break;
	case EFoodHazard::RawCassava: Item.Toxicity = FMath::Max(Item.Toxicity, RawCassavaToxicity); break;
	case EFoodHazard::RawCashew: Item.Toxicity = FMath::Max(Item.Toxicity, RawCashewToxicity); break;
	default: break;
	}
	return Item;
}

float FBodyModel::MoraleEventDelta(EMoraleEvent Event)
{
	switch (Event)
	{
	case EMoraleEvent::Discovery: return 8.0f;
	case EMoraleEvent::MapProgress: return 5.0f;
	case EMoraleEvent::IslandMapped: return 15.0f;
	case EMoraleEvent::MusicPlayed: return 4.0f;
	case EMoraleEvent::HotMeal: return 3.0f;
	case EMoraleEvent::SleptInBed: return 5.0f;
	case EMoraleEvent::Injured: return -6.0f;
	case EMoraleEvent::StormHit: return -4.0f;
	default: return 0.0f;
	}
}

void FBodyModel::ApplyMoraleEvent(FSurvivalState& S, EMoraleEvent Event)
{
	// El ánimo nunca toca la salud: solo se mueve en [0, 100].
	S.Morale = FMath::Clamp(S.Morale + MoraleEventDelta(Event), 0.0f, 100.0f);
}

float FBodyModel::Clumsiness(const FSurvivalState& S)
{
	float C = S.SleepDeprivation() * 0.7f;
	C += IsActive(S, ECondition::Sprain) ? 0.25f : 0.0f;
	C += FMath::Clamp((35.5f - S.BodyTemperature) / 2.0f, 0.0f, 1.0f) * 0.2f;
	C += IsActive(S, ECondition::Poisoned) ? 0.1f : 0.0f;
	return FMath::Clamp(C, 0.0f, 1.0f);
}

float FBodyModel::HallucinationIntensity(const FSurvivalState& S)
{
	const float FromMushroom = IsActive(S, ECondition::Hallucinating) ? 1.0f : 0.0f;
	// Solo con el sueño casi agotado (último tercio), y como mucho leve.
	const float FromSleep = FMath::Clamp((S.SleepDeprivation() - 0.66f) / 0.34f, 0.0f, 1.0f) * 0.35f;
	return FMath::Max(FromMushroom, FromSleep);
}

float FBodyModel::Pain(const FSurvivalState& S)
{
	float P = 0.0f;
	for (const FWound& W : S.Wounds)
	{
		P += W.Depth * (W.bBandaged ? 0.3f : 0.6f);
	}
	P += IsActive(S, ECondition::Sprain) ? 0.3f : 0.0f;
	P += IsActive(S, ECondition::JellyfishSting) ? 0.4f : 0.0f;
	P += IsActive(S, ECondition::RaySting) ? 0.8f : 0.0f;
	P += IsActive(S, ECondition::SunBurn) ? 0.2f : 0.0f;
	P += IsActive(S, ECondition::Infection) ? 0.2f : 0.0f;
	P += ScurvyStage(S.ScurvySeverity) >= EScurvyStage::Gums ? 0.1f : 0.0f;
	return FMath::Clamp(P, 0.0f, 1.0f);
}

float FBodyModel::MonotonyFactor(const FSurvivalState& S)
{
	return FMath::Clamp(S.MonotonyHours / MonotonyFullHours, 0.0f, 1.0f);
}

void FBodyModel::Tick(FSurvivalState& S, const FSurvivalInputs& In, float DeltaHours, const FSurvivalModeSettings& Mode,
	float& InOutDamage, float& InOutMoralePerHour, TArray<ESurvivalEvent>& OutEvents)
{
	const float Scale = Mode.NeedScale();

	// Escorbuto: avanza con la vitamina C agotada y retrocede al volver a comer fruta.
	const EScurvyStage StageBefore = ScurvyStage(S.ScurvySeverity);
	if (S.Vitamins <= ScurvyVitaminThreshold)
	{
		S.ScurvySeverity += DeltaHours / ScurvyOnsetHours * Scale;
	}
	else if (S.Vitamins >= ScurvyRecoveryVitamins)
	{
		S.ScurvySeverity -= DeltaHours / ScurvyRecoveryHours;
	}
	S.ScurvySeverity = FMath::Clamp(S.ScurvySeverity, 0.0f, 1.0f);
	const EScurvyStage Stage = ScurvyStage(S.ScurvySeverity);
	if (Stage > StageBefore)
	{
		OutEvents.AddUnique(ESurvivalEvent::ScurvyWorse);
	}
	if (Stage >= EScurvyStage::Gums)
	{
		InOutMoralePerHour -= 0.5f;
	}
	if (Stage == EScurvyStage::Bleeding)
	{
		InOutDamage += ScurvyBleedDamagePerHour * DeltaHours;
	}

	// Cortes: sangran, coagulan, se infectan si nadie los cura y cicatrizan vendados.
	const bool bInfectionActive = S.HasCondition(ECondition::Infection);
	const float ClotMul = Stage == EScurvyStage::Bleeding ? 0.5f : 1.0f;
	const float HealMul = Stage == EScurvyStage::Bleeding ? 0.5f : 1.0f;
	for (int32 Index = S.Wounds.Num() - 1; Index >= 0; --Index)
	{
		FWound& W = S.Wounds[Index];
		InOutDamage += W.Bleeding * WoundBleedDamagePerHour * DeltaHours;
		W.Bleeding = FMath::Max(0.0f, W.Bleeding - WoundClotPerHour * (1.0f - W.Depth) * ClotMul * DeltaHours);

		if (W.bInfected && !bInfectionActive)
		{
			// Curada (pasta de cúrcuma) o pasada: se puede volver a infectar si sigue sin tratar.
			W.bInfected = false;
			W.HoursUntreated = 0.0f;
		}
		if (!W.bBandaged && !W.bInfected)
		{
			W.HoursUntreated += DeltaHours * Scale;
			if (W.HoursUntreated >= WoundInfectionHours)
			{
				W.bInfected = true;
				S.AddCondition(ECondition::Infection, 48.0f);
				S.AddCondition(ECondition::Fever, 24.0f);
				OutEvents.AddUnique(ESurvivalEvent::WoundInfected);
			}
		}
		if (!W.bInfected)
		{
			const float Care = W.bBandaged ? (W.bMedicinal ? 1.5f : 1.0f) : 0.35f;
			W.Healed += DeltaHours / (WoundHealHours * (1.0f + W.Depth)) * Care * HealMul;
		}
		if (W.Healed >= 1.0f)
		{
			S.Wounds.RemoveAt(Index);
		}
	}

	// Picaduras.
	if (S.HasCondition(ECondition::JellyfishSting))
	{
		InOutDamage += JellyfishDamagePerHour * DeltaHours;
	}
	if (S.HasCondition(ECondition::RaySting))
	{
		InOutDamage += RayDamagePerHour * DeltaHours;
	}

	// Sol: la dosis se acumula sin sombrero ni sombra y se disipa a la sombra.
	const float SunIn = In.bSheltered ? 0.0f : In.SunExposure * (In.bHasHat ? HatSunFactor : 1.0f);
	if (SunIn > 0.2f)
	{
		S.SunDose += SunIn * DeltaHours;
	}
	else
	{
		S.SunDose -= SunDoseDecayPerHour * DeltaHours;
	}
	S.SunDose = FMath::Clamp(S.SunDose, 0.0f, SunBurnDoseHours * 2.0f);
	if (S.SunDose >= SunBurnDoseHours)
	{
		if (!S.HasCondition(ECondition::SunBurn))
		{
			OutEvents.AddUnique(ESurvivalEvent::SunBurned);
		}
		S.AddCondition(ECondition::SunBurn, 10.0f);
	}

	// Dieta monótona: solo cuenta si se come (con hambre, lo que falta es comida, no variedad).
	if (S.Hunger > 40.0f && S.DietBalance() < MonotonyBalanceBelow)
	{
		S.MonotonyHours += DeltaHours;
	}
	else
	{
		S.MonotonyHours -= DeltaHours * 2.0f;
	}
	S.MonotonyHours = FMath::Clamp(S.MonotonyHours, 0.0f, MonotonyFullHours);
	InOutMoralePerHour -= MonotonyFactor(S) * 0.8f;

	// Ánimo: tormentas y dolor lo bajan; la música lo sube. Nunca mata.
	InOutMoralePerHour -= In.StormIntensity * (In.bSheltered ? 1.0f : 3.0f);
	InOutMoralePerHour -= Pain(S) * 2.0f;
	InOutMoralePerHour += In.bPlayingMusic ? 3.0f : 0.0f;
}
