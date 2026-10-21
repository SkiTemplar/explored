#include "Survival/IslandBodyModel.h"

namespace IslandBodyDetail
{
	float ClampBar(float Value)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 100.0f) : 0.0f;
	}

	bool RateOk(float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f;
	}

	bool PercentOk(float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 100.0f;
	}

	uint8 Bit(EBodyBar Bar)
	{
		return static_cast<uint8>(1u << static_cast<uint8>(Bar));
	}
}

const TCHAR* LexToString(EBodyBar Bar)
{
	switch (Bar)
	{
	case EBodyBar::Thirst: return TEXT("sed");
	case EBodyBar::Hunger: return TEXT("hambre");
	case EBodyBar::Energy: return TEXT("energia");
	case EBodyBar::Warmth: return TEXT("calor");
	default: return TEXT("?");
	}
}

bool FIslandBodyTuning::IsValid() const
{
	using namespace IslandBodyDetail;
	return RateOk(ThirstLossPerMinute) && RateOk(HungerLossPerMinute) && RateOk(EnergyLossPerMinute)
		&& RateOk(NightWarmthLossPerMinute) && RateOk(RainWarmthLossPerMinute) && RateOk(FireWarmthGainPerMinute)
		&& PercentOk(LowThreshold) && PercentOk(WarningAt) && RateOk(WarningRearm)
		&& FMath::IsFinite(LowSpeedFactor) && LowSpeedFactor > 0.0f && LowSpeedFactor <= 1.0f
		&& FMath::IsFinite(NightFromHour) && FMath::IsFinite(NightUntilHour)
		&& PercentOk(Initial.Thirst) && PercentOk(Initial.Hunger) && PercentOk(Initial.Energy) && PercentOk(Initial.Warmth)
		&& FMath::IsFinite(Events.FaintSeconds) && Events.FaintSeconds > 0.0f;
}

float FIslandBodyModel::Get(const FBodyBars& Bars, EBodyBar Bar)
{
	switch (Bar)
	{
	case EBodyBar::Thirst: return Bars.Thirst;
	case EBodyBar::Hunger: return Bars.Hunger;
	case EBodyBar::Energy: return Bars.Energy;
	case EBodyBar::Warmth: return Bars.Warmth;
	default: return 0.0f;
	}
}

FBodyBars FIslandBodyModel::With(const FBodyBars& Bars, EBodyBar Bar, float Value)
{
	FBodyBars Out = Bars;
	const float Clamped = IslandBodyDetail::ClampBar(Value);
	switch (Bar)
	{
	case EBodyBar::Thirst: Out.Thirst = Clamped; break;
	case EBodyBar::Hunger: Out.Hunger = Clamped; break;
	case EBodyBar::Energy: Out.Energy = Clamped; break;
	case EBodyBar::Warmth: Out.Warmth = Clamped; break;
	default: break;
	}
	return Out;
}

FBodyBars FIslandBodyModel::Apply(const FBodyBars& Bars, float Thirst, float Hunger, float Energy)
{
	using namespace IslandBodyDetail;
	const auto Delta = [](float Value) { return FMath::IsFinite(Value) ? Value : 0.0f; };
	FBodyBars Out = Bars;
	Out.Thirst = ClampBar(Bars.Thirst + Delta(Thirst));
	Out.Hunger = ClampBar(Bars.Hunger + Delta(Hunger));
	Out.Energy = ClampBar(Bars.Energy + Delta(Energy));
	return Out;
}

bool FIslandBodyModel::IsNight(double HourOfDay, const FIslandBodyTuning& Tuning)
{
	if (!FMath::IsFinite(HourOfDay))
	{
		return false;
	}
	const double Hour = FBodyEventsModel::HourOfDay(HourOfDay);
	return Hour >= Tuning.NightFromHour || Hour < Tuning.NightUntilHour;
}

float FIslandBodyModel::WarmthPerMinute(const FBodyEnvironment& Env, const FIslandBodyTuning& Tuning)
{
	if (Env.bNearFire)
	{
		return Tuning.FireWarmthGainPerMinute;
	}
	if (Env.bSheltered)
	{
		return 0.0f;
	}
	const float Night = IsNight(Env.HourOfDay, Tuning) ? Tuning.NightWarmthLossPerMinute : 0.0f;
	const float Rain = Env.bRaining ? Tuning.RainWarmthLossPerMinute : 0.0f;
	return -FMath::Max(Night, Rain);
}

FBodyStep FIslandBodyModel::Tick(const FBodyBars& Bars, const FBodyWarnings& Warnings, const FBodyEnvironment& Env, float RealSeconds,
	const FIslandBodyTuning& Tuning)
{
	using namespace IslandBodyDetail;
	FBodyStep Step;
	Step.Bars = Bars;
	Step.Warnings = Warnings;
	if (!FMath::IsFinite(RealSeconds) || RealSeconds <= 0.0f)
	{
		return Step;
	}
	const float Minutes = RealSeconds / 60.0f;
	Step.Bars.Thirst = ClampBar(Bars.Thirst - Tuning.ThirstLossPerMinute * Minutes);
	Step.Bars.Hunger = ClampBar(Bars.Hunger - Tuning.HungerLossPerMinute * Minutes);
	Step.Bars.Energy = ClampBar(Bars.Energy - Tuning.EnergyLossPerMinute * Minutes);
	Step.Bars.Warmth = ClampBar(Bars.Warmth + WarmthPerMinute(Env, Tuning) * Minutes);

	for (uint8 Index = 0; Index < static_cast<uint8>(EBodyBar::Count); ++Index)
	{
		const EBodyBar Bar = static_cast<EBodyBar>(Index);
		const float Value = Get(Step.Bars, Bar);
		if (Value < Tuning.WarningAt && !Step.Warnings.HasWarned(Bar))
		{
			Step.Warnings.WarnedMask |= Bit(Bar);
			Step.NewWarnings.Add(Bar);
		}
		else if (Value > Tuning.WarningAt + Tuning.WarningRearm && Step.Warnings.HasWarned(Bar))
		{
			Step.Warnings.WarnedMask &= static_cast<uint8>(~Bit(Bar));
		}
	}
	return Step;
}

bool FIslandBodyModel::IsLow(float Value, const FIslandBodyTuning& Tuning)
{
	return !FMath::IsFinite(Value) || Value < Tuning.LowThreshold;
}

bool FIslandBodyModel::AnyLow(const FBodyBars& Bars, const FIslandBodyTuning& Tuning)
{
	return IsLow(Bars.Thirst, Tuning) || IsLow(Bars.Hunger, Tuning) || IsLow(Bars.Energy, Tuning) || IsLow(Bars.Warmth, Tuning);
}

float FIslandBodyModel::SpeedFactor(const FBodyBars& Bars, const FIslandBodyTuning& Tuning)
{
	return AnyLow(Bars, Tuning) ? Tuning.LowSpeedFactor : 1.0f;
}

float FIslandBodyModel::Desaturation01(const FBodyBars& Bars, const FIslandBodyTuning& Tuning)
{
	if (!AnyLow(Bars, Tuning) || Tuning.LowThreshold <= 0.0f)
	{
		return 0.0f;
	}
	const auto Safe = [](float Value) { return FMath::IsFinite(Value) ? Value : 0.0f; };
	const float Worst = FMath::Min(FMath::Min(Safe(Bars.Thirst), Safe(Bars.Hunger)), FMath::Min(Safe(Bars.Energy), Safe(Bars.Warmth)));
	const float Depth = FMath::Clamp(1.0f - Worst / Tuning.LowThreshold, 0.0f, 1.0f);
	return FMath::Lerp(0.4f, 1.0f, Depth);
}
