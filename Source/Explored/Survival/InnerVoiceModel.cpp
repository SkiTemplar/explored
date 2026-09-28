#include "Survival/InnerVoiceModel.h"

#include "Survival/BodyModel.h"

namespace InnerVoiceModelDetail
{
	void FillDefaultLines(TArray<FInnerVoiceText>& D)
	{
#include "Survival/InnerVoiceData.inl"
	}

	const TArray<FInnerVoiceText>& VoiceLines()
	{
		static const TArray<FInnerVoiceText> Data = []()
		{
			TArray<FInnerVoiceText> D;
			D.SetNum(static_cast<int32>(EInnerVoiceLine::Count));
			FillDefaultLines(D);
			return D;
		}();
		return Data;
	}

	bool IsValidVoiceLine(EInnerVoiceLine Line)
	{
		return static_cast<int32>(Line) < static_cast<int32>(EInnerVoiceLine::Count);
	}

	bool AnyCutBleedingAbove(const FSurvivalState& S, float Above)
	{
		for (const FWound& W : S.Wounds)
		{
			if (!W.bBurn && W.Bleeding > Above)
			{
				return true;
			}
		}
		return false;
	}

	/** Aviso que dispara cada suceso del cuerpo (Count si ninguno). */
	EInnerVoiceLine VoiceLineForEvent(ESurvivalEvent Event)
	{
		switch (Event)
		{
		case ESurvivalEvent::Burned: return EInnerVoiceLine::ContactBurn;
		default: return EInnerVoiceLine::Count;
		}
	}
}

using namespace InnerVoiceModelDetail;

FName FInnerVoiceModel::Id(EInnerVoiceLine Line)
{
	return IsValidVoiceLine(Line) ? VoiceLines()[static_cast<int32>(Line)].Id : NAME_None;
}

bool FInnerVoiceModel::FromId(FName InId, EInnerVoiceLine& OutLine)
{
	if (InId.IsNone())
	{
		return false;
	}
	const TArray<FInnerVoiceText>& All = VoiceLines();
	for (int32 I = 0; I < All.Num(); ++I)
	{
		if (All[I].Id == InId)
		{
			OutLine = static_cast<EInnerVoiceLine>(I);
			return true;
		}
	}
	return false;
}

const FInnerVoiceText& FInnerVoiceModel::Text(EInnerVoiceLine Line)
{
	static const FInnerVoiceText Empty;
	return IsValidVoiceLine(Line) ? VoiceLines()[static_cast<int32>(Line)] : Empty;
}

bool FInnerVoiceModel::IsEventLine(EInnerVoiceLine Line)
{
	switch (Line)
	{
	case EInnerVoiceLine::Drowning:
	case EInnerVoiceLine::OutOfBreath:
	case EInnerVoiceLine::ContactBurn:
		return true;
	default:
		return false;
	}
}

bool FInnerVoiceModel::IsActive(EInnerVoiceLine Line, const FSurvivalState& S)
{
	const EScurvyStage Scurvy = FBodyModel::ScurvyStage(S.ScurvySeverity);
	switch (Line)
	{
	case EInnerVoiceLine::Heatstroke: return S.BodyTemperature > HeatstrokeAbove;
	case EInnerVoiceLine::Hypothermia: return S.BodyTemperature < HypothermiaBelow;
	case EInnerVoiceLine::ThirstZero: return S.Thirst <= 0.0f;
	case EInnerVoiceLine::HungerZero: return S.Hunger <= 0.0f;
	case EInnerVoiceLine::WoundBleeding: return AnyCutBleedingAbove(S, BleedingAbove) || S.HasCondition(ECondition::Bleeding);
	case EInnerVoiceLine::ScurvyBleeding: return Scurvy >= EScurvyStage::Bleeding;
	case EInnerVoiceLine::RaySting: return S.HasCondition(ECondition::RaySting);
	case EInnerVoiceLine::JellyfishSting: return S.HasCondition(ECondition::JellyfishSting);
	case EInnerVoiceLine::Poisoned: return S.HasCondition(ECondition::Poisoned);
	case EInnerVoiceLine::WoundInfected: return S.HasCondition(ECondition::Infection);
	case EInnerVoiceLine::Fever: return S.HasCondition(ECondition::Fever);
	case EInnerVoiceLine::Sprain: return S.HasCondition(ECondition::Sprain);
	case EInnerVoiceLine::SleepZero: return S.Rest <= 0.0f;
	case EInnerVoiceLine::Hallucinating: return S.HasCondition(ECondition::Hallucinating);
	case EInnerVoiceLine::Soaked: return S.Wetness > SoakedAbove && S.BodyTemperature < ColdBelow;
	case EInnerVoiceLine::ColdLow: return S.BodyTemperature < ColdBelow;
	// La fiebre también sube la temperatura: entonces no es el sol.
	case EInnerVoiceLine::HeatHigh: return S.BodyTemperature > HeatAbove && !S.HasCondition(ECondition::Fever);
	case EInnerVoiceLine::ThirstLow: return S.Thirst < NeedLowBelow;
	case EInnerVoiceLine::HungerLow: return S.Hunger < NeedLowBelow;
	case EInnerVoiceLine::SleepLow: return S.Rest < SleepLowBelow;
	case EInnerVoiceLine::ScurvyVision: return Scurvy >= EScurvyStage::Vision;
	case EInnerVoiceLine::ScurvyGums: return Scurvy >= EScurvyStage::Gums;
	case EInnerVoiceLine::Monotony: return S.MonotonyHours >= MonotonyAtHours;
	default: return false;
	}
}

bool FInnerVoiceModel::IsCleared(EInnerVoiceLine Line, const FSurvivalState& S)
{
	switch (Line)
	{
	case EInnerVoiceLine::Heatstroke: return S.BodyTemperature < HeatstrokeAbove - TemperatureRearmMargin;
	case EInnerVoiceLine::Hypothermia: return S.BodyTemperature > HypothermiaBelow + TemperatureRearmMargin;
	case EInnerVoiceLine::ThirstZero: return S.Thirst > NeedRearmMargin;
	case EInnerVoiceLine::HungerZero: return S.Hunger > NeedRearmMargin;
	case EInnerVoiceLine::WoundBleeding: return !AnyCutBleedingAbove(S, 0.0f) && !S.HasCondition(ECondition::Bleeding);
	case EInnerVoiceLine::SleepZero: return S.Rest > NeedRearmMargin;
	case EInnerVoiceLine::Soaked: return S.Wetness < SoakedRearmBelow || S.BodyTemperature > ColdBelow + TemperatureRearmMargin;
	case EInnerVoiceLine::ColdLow: return S.BodyTemperature > ColdBelow + TemperatureRearmMargin;
	case EInnerVoiceLine::HeatHigh: return S.BodyTemperature < HeatAbove - TemperatureRearmMargin || S.HasCondition(ECondition::Fever);
	case EInnerVoiceLine::ThirstLow: return S.Thirst > NeedLowBelow + NeedRearmMargin;
	case EInnerVoiceLine::HungerLow: return S.Hunger > NeedLowBelow + NeedRearmMargin;
	case EInnerVoiceLine::SleepLow: return S.Rest > SleepLowBelow + NeedRearmMargin;
	case EInnerVoiceLine::Monotony: return S.MonotonyHours < MonotonyRearmBelowHours;
	default:
		// Estados y etapas: se rearman en cuanto dejan de estar (curarse ya es margen suficiente).
		return !IsActive(Line, S);
	}
}

void FInnerVoiceModel::Prime(FInnerVoiceState& Voice, const FSurvivalState& State)
{
	Voice = FInnerVoiceState();
	for (int32 I = 0; I < static_cast<int32>(EInnerVoiceLine::Count); ++I)
	{
		const EInnerVoiceLine Line = static_cast<EInnerVoiceLine>(I);
		if (!IsEventLine(Line) && IsActive(Line, State))
		{
			Voice.Latched |= FInnerVoiceState::Bit(Line);
		}
	}
}

void FInnerVoiceModel::ObserveBreath(FInnerVoiceState& Voice, float Oxygen01, bool bUnderwater)
{
	// Un oxígeno corrupto no dispara nada: se trata como pulmones llenos.
	const float Oxygen = FMath::IsFinite(Oxygen01) ? FMath::Clamp(Oxygen01, 0.0f, 1.0f) : 1.0f;
	const uint32 Drowning = FInnerVoiceState::Bit(EInnerVoiceLine::Drowning);
	if (bUnderwater && Oxygen <= 0.0f && !(Voice.Latched & Drowning))
	{
		Voice.Latched |= Drowning;
		Voice.Pending |= Drowning;
	}
	if (!bUnderwater)
	{
		// Ya ha salido: gritar «¡aire!» ahora llegaría tarde.
		Voice.Pending &= ~Drowning;
	}
	if (Oxygen > DrowningRearmOxygen)
	{
		Voice.Latched &= ~Drowning;
	}
	if (Voice.bWasUnderwater && !bUnderwater && Oxygen < GaspOxygenBelow)
	{
		Voice.Pending |= FInnerVoiceState::Bit(EInnerVoiceLine::OutOfBreath);
	}
	Voice.bWasUnderwater = bUnderwater;
}

bool FInnerVoiceModel::Evaluate(FInnerVoiceState& Voice, const FSurvivalState& State, const TArray<ESurvivalEvent>& Events,
	EInnerVoiceLine& OutLine)
{
	for (const ESurvivalEvent Event : Events)
	{
		const EInnerVoiceLine Line = VoiceLineForEvent(Event);
		if (IsValidVoiceLine(Line))
		{
			Voice.Pending |= FInnerVoiceState::Bit(Line);
		}
	}
	for (int32 I = 0; I < static_cast<int32>(EInnerVoiceLine::Count); ++I)
	{
		const EInnerVoiceLine Line = static_cast<EInnerVoiceLine>(I);
		if (IsEventLine(Line))
		{
			continue;
		}
		const uint32 Bit = FInnerVoiceState::Bit(Line);
		if ((Voice.Latched & Bit) && IsCleared(Line, State))
		{
			Voice.Latched &= ~Bit;
		}
		if (!(Voice.Latched & Bit) && IsActive(Line, State))
		{
			Voice.Latched |= Bit;
			Voice.Pending |= Bit;
		}
		if ((Voice.Pending & Bit) && !IsActive(Line, State))
		{
			// Pasó antes de decirse: ya no viene a cuento.
			Voice.Pending &= ~Bit;
		}
	}
	for (int32 I = 0; I < static_cast<int32>(EInnerVoiceLine::Count); ++I)
	{
		const EInnerVoiceLine Line = static_cast<EInnerVoiceLine>(I);
		const uint32 Bit = FInnerVoiceState::Bit(Line);
		if (Voice.Pending & Bit)
		{
			Voice.Pending &= ~Bit;
			OutLine = Line;
			return true;
		}
	}
	return false;
}
