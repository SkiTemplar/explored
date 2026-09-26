#include "Survival/BodySignals.h"

#include "Survival/BodyModel.h"

namespace BodySignalsDetail
{
	float Clamp01(float V)
	{
		return FMath::Clamp(V, 0.0f, 1.0f);
	}

	/** Cuánto falta de una necesidad por debajo de un umbral (0 por encima; 1 a cero). */
	float Below(float Need, float Threshold)
	{
		return Clamp01((Threshold - Need) / Threshold);
	}

	float Cold(const FSurvivalState& S) { return Clamp01((36.5f - S.BodyTemperature) / 2.5f); }
	float Heat(const FSurvivalState& S) { return Clamp01((S.BodyTemperature - 37.8f) / 2.0f); }
	float LowHealth(const FSurvivalState& S) { return Clamp01((50.0f - S.Health) / 50.0f); }
	float Fever(const FSurvivalState& S) { return S.HasCondition(ECondition::Fever) ? 0.5f : 0.0f; }
	float Poison(const FSurvivalState& S) { return S.HasCondition(ECondition::Poisoned) ? 0.7f : 0.0f; }

	/** Acercamiento exponencial sin pasarse (Speed en 1/s). */
	float Approach(float Current, float Target, float DeltaSeconds, float Speed)
	{
		if (DeltaSeconds <= 0.0f)
		{
			return Current;
		}
		const float Alpha = 1.0f - FMath::Exp(-Speed * DeltaSeconds);
		return Current + (Target - Current) * Clamp01(Alpha);
	}
}

using namespace BodySignalsDetail;

float FBodySignalsModel::CauseIntensity(const FSurvivalState& S, EBodyTintCause Cause)
{
	switch (Cause)
	{
	case EBodyTintCause::Cold: return Cold(S);
	case EBodyTintCause::Heat: return Clamp01(FMath::Max(Heat(S), Fever(S)));
	case EBodyTintCause::Thirst: return Below(S.Thirst, 25.0f);
	case EBodyTintCause::Hunger: return Below(S.Hunger, 20.0f) * 0.8f;
	case EBodyTintCause::Bleeding: return Clamp01(FBodyModel::TotalBleeding(S) * 1.5f);
	case EBodyTintCause::Pain: return Clamp01(FBodyModel::Pain(S) * 0.8f + LowHealth(S) * 0.6f);
	case EBodyTintCause::Poison: return Poison(S);
	case EBodyTintCause::Exhaustion: return Below(S.Rest, 15.0f);
	case EBodyTintCause::Scurvy: return Clamp01((S.ScurvySeverity - 0.3f) / 0.7f) * 0.7f;
	default: return 0.0f;
	}
}

FLinearColor FBodySignalsModel::TintFor(EBodyTintCause Cause)
{
	switch (Cause)
	{
	case EBodyTintCause::Cold: return FLinearColor(0.10f, 0.25f, 0.60f);
	case EBodyTintCause::Heat: return FLinearColor(0.70f, 0.35f, 0.05f);
	case EBodyTintCause::Thirst: return FLinearColor(0.60f, 0.55f, 0.30f);
	case EBodyTintCause::Hunger: return FLinearColor(0.25f, 0.18f, 0.10f);
	case EBodyTintCause::Bleeding: return FLinearColor(0.60f, 0.02f, 0.02f);
	case EBodyTintCause::Pain: return FLinearColor(0.30f, 0.03f, 0.05f);
	case EBodyTintCause::Poison: return FLinearColor(0.20f, 0.40f, 0.10f);
	case EBodyTintCause::Scurvy: return FLinearColor(0.30f, 0.26f, 0.35f);
	default: return FLinearColor::Black;
	}
}

FBodySignals FBodySignalsModel::Evaluate(const FSurvivalState& S, const FBodySignalContext& Context)
{
	const float Exertion = Clamp01(FMath::Max(Context.Exertion, 1.0f - Context.EnergyRatio));
	const float Breathless = 1.0f - Clamp01(Context.Oxygen01);
	const float Bleed = CauseIntensity(S, EBodyTintCause::Bleeding);
	const float Pain = FBodyModel::Pain(S);
	const float Thirst = Below(S.Thirst, 30.0f);
	const float Hunger = Below(S.Hunger, 30.0f);
	const float Exhaustion = S.SleepDeprivation();

	FBodySignals Out;
	// Respirar y latir tienen un reposo audible pero discreto.
	Out.Breathing = Clamp01(0.1f + 0.6f * Exertion + 0.4f * Breathless + 0.3f * Heat(S) + 0.2f * LowHealth(S) + 0.2f * Bleed);
	Out.Heartbeat = Clamp01(0.05f + 0.45f * Exertion + 0.5f * LowHealth(S) + 0.4f * Bleed + 0.3f * Fever(S)
		+ 0.3f * Pain + 0.3f * Breathless + 0.2f * Thirst);
	Out.Shivering = Clamp01(Cold(S) + (S.HasCondition(ECondition::Fever) ? 0.35f : 0.0f));
	Out.StomachGrowl = Clamp01(Below(S.Hunger, 45.0f) * 0.8f + FBodyModel::MonotonyFactor(S) * 0.15f + Poison(S) * 0.3f);
	Out.Blur = Clamp01(Thirst * 0.8f + Poison(S) * 0.3f + Exhaustion * 0.15f);
	Out.HandTremor = Clamp01(Cold(S) * 0.7f + Hunger * 0.4f + Exhaustion * 0.3f + Fever(S) * 0.6f
		+ Poison(S) * 0.3f + LowHealth(S) * 0.3f);
	Out.Desaturation = Clamp01(CauseIntensity(S, EBodyTintCause::Scurvy) + LowHealth(S) * 0.5f);
	Out.BleedingPulse = Bleed;
	Out.Hallucination = FBodyModel::HallucinationIntensity(S);

	// Borde de pantalla: la causa más intensa decide el tinte; nunca tapa del todo.
	float Strongest = 0.0f;
	for (int32 C = 1; C < static_cast<int32>(EBodyTintCause::Count); ++C)
	{
		const EBodyTintCause Cause = static_cast<EBodyTintCause>(C);
		const float I = CauseIntensity(S, Cause);
		if (I > Strongest)
		{
			Strongest = I;
			Out.TintCause = Cause;
		}
	}
	if (Strongest < 0.05f)
	{
		Out.TintCause = EBodyTintCause::None;
		Strongest = 0.0f;
	}
	Out.Vignette = Clamp01(Strongest * 0.75f);
	Out.VignetteTint = TintFor(Out.TintCause);
	return Out;
}

FBodySignals FBodySignalsModel::Smooth(const FBodySignals& Current, const FBodySignals& Target, float DeltaSeconds)
{
	// Velocidades (1/s): el pulso y el latido reaccionan rápido; el color, despacio.
	FBodySignals Out = Target;
	Out.Breathing = Approach(Current.Breathing, Target.Breathing, DeltaSeconds, 1.2f);
	Out.Heartbeat = Approach(Current.Heartbeat, Target.Heartbeat, DeltaSeconds, 1.5f);
	Out.Shivering = Approach(Current.Shivering, Target.Shivering, DeltaSeconds, 0.8f);
	Out.StomachGrowl = Approach(Current.StomachGrowl, Target.StomachGrowl, DeltaSeconds, 0.5f);
	Out.Vignette = Approach(Current.Vignette, Target.Vignette, DeltaSeconds, 0.6f);
	Out.Blur = Approach(Current.Blur, Target.Blur, DeltaSeconds, 0.5f);
	Out.HandTremor = Approach(Current.HandTremor, Target.HandTremor, DeltaSeconds, 0.8f);
	Out.Desaturation = Approach(Current.Desaturation, Target.Desaturation, DeltaSeconds, 0.2f);
	Out.BleedingPulse = Approach(Current.BleedingPulse, Target.BleedingPulse, DeltaSeconds, 3.0f);
	Out.Hallucination = Approach(Current.Hallucination, Target.Hallucination, DeltaSeconds, 0.3f);

	const float TintAlpha = DeltaSeconds > 0.0f ? Clamp01(1.0f - FMath::Exp(-0.6f * DeltaSeconds)) : 0.0f;
	Out.VignetteTint = Current.VignetteTint + (Target.VignetteTint - Current.VignetteTint) * TintAlpha;
	Out.TintCause = Target.TintCause;
	return Out;
}

float FBodySignalsModel::HeartRateBpm(float Heartbeat)
{
	return 60.0f + 100.0f * Clamp01(Heartbeat);
}

float FBodySignalsModel::BreathsPerMinute(float Breathing)
{
	return 12.0f + 28.0f * Clamp01(Breathing);
}

ENeedLevel FBodySignalsModel::CoarseLevel(float Need)
{
	if (Need >= 60.0f)
	{
		return ENeedLevel::Good;
	}
	if (Need >= 30.0f)
	{
		return ENeedLevel::Fair;
	}
	if (Need >= 10.0f)
	{
		return ENeedLevel::Low;
	}
	return ENeedLevel::Critical;
}

FWristWatchReadout FBodySignalsModel::WristWatch(const FSurvivalState& S, float HoursOfDay, bool bShowNeeds)
{
	FWristWatchReadout Out;
	float Hours = FMath::IsFinite(HoursOfDay) ? FMath::Fmod(HoursOfDay, 24.0f) : 0.0f;
	if (Hours < 0.0f)
	{
		Hours += 24.0f;
	}
	const int32 TotalMinutes = FMath::Clamp(FMath::FloorToInt32(Hours * 60.0f + 1.0e-3f), 0, 24 * 60 - 1);
	Out.Hour = TotalMinutes / 60;
	Out.Minute = TotalMinutes % 60;
	Out.bShowNeeds = bShowNeeds;
	Out.Thirst = CoarseLevel(S.Thirst);
	Out.Hunger = CoarseLevel(S.Hunger);
	Out.Rest = CoarseLevel(S.Rest);
	// Calor: por distancia a 37 °C, en los mismos cuatro escalones.
	const float Deviation = FMath::Abs(S.BodyTemperature - 37.0f);
	Out.Warmth = Deviation < 0.8f ? ENeedLevel::Good : Deviation < 1.6f ? ENeedLevel::Fair
		: Deviation < 2.5f ? ENeedLevel::Low : ENeedLevel::Critical;
	return Out;
}
