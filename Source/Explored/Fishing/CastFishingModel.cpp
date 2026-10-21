#include "Fishing/CastFishingModel.h"

bool FCastFishingModel::InRange(float Hour, const FVector2f& Range)
{
	const float H = FMath::Fmod(FMath::Fmod(Hour, 24.0f) + 24.0f, 24.0f);
	return Range.X <= Range.Y ? (H >= Range.X && H < Range.Y) : (H >= Range.X || H < Range.Y);
}

bool FCastFishingModel::IsNight(const FCastFishingConfig& Config, float Hour)
{
	return InRange(Hour, Config.Night);
}

bool FCastFishingModel::CanFish(const FCastFishingConfig& Config, float Hour, bool bStorm)
{
	return !bStorm && !IsNight(Config, Hour);
}

float FCastFishingModel::RateFactor(const FCastFishingConfig& Config, float Hour)
{
	const bool bGolden = InRange(Hour, Config.Dawn) || InRange(Hour, Config.Dusk);
	return bGolden ? FMath::Max(Config.GoldenHourFactor, 0.01f) : 1.0f;
}

float FCastFishingModel::DrawWaitSeconds(const FCastFishingConfig& Config, float Hour, uint32& InOutSeed)
{
	// LCG de Numerical Recipes: reproducible en el host y en el editor.
	InOutSeed = InOutSeed * 1664525u + 1013904223u;
	const float Unit = static_cast<float>(InOutSeed >> 8) / static_cast<float>(1u << 24);
	const float Mean = FMath::Max(Config.MeanBiteSeconds, 0.1f) / RateFactor(Config, Hour);
	const float Min = FMath::Clamp(Config.MinBiteSeconds, 0.0f, Mean);
	// Uniforme en [Min, 2·Mean − Min]: media exacta Mean.
	return Min + Unit * 2.0f * (Mean - Min);
}

FCastFishingState FCastFishingModel::Cast(const FCastFishingState& In, const FCastFishingConfig& Config, float Hour, bool bStorm,
	ECastFishingEvent& OutEvent)
{
	FCastFishingState Out = In;
	if (In.Phase != ECastFishingPhase::Idle)
	{
		OutEvent = ECastFishingEvent::None;
		return Out;
	}
	if (!CanFish(Config, Hour, bStorm))
	{
		OutEvent = ECastFishingEvent::Blocked;
		return Out;
	}
	Out.Phase = ECastFishingPhase::Waiting;
	Out.RemainingSeconds = DrawWaitSeconds(Config, Hour, Out.Seed);
	OutEvent = ECastFishingEvent::Cast;
	return Out;
}

FCastFishingState FCastFishingModel::Tick(const FCastFishingState& In, const FCastFishingConfig& Config, float DeltaSeconds, float Hour,
	bool bStorm, ECastFishingEvent& OutEvent)
{
	OutEvent = ECastFishingEvent::None;
	FCastFishingState Out = In;
	if (In.Phase == ECastFishingPhase::Idle || !(DeltaSeconds > 0.0f))
	{
		return Out;
	}
	if (!CanFish(Config, Hour, bStorm))
	{
		Out.Phase = ECastFishingPhase::Idle;
		Out.RemainingSeconds = 0.0f;
		OutEvent = ECastFishingEvent::Blocked;
		return Out;
	}
	Out.RemainingSeconds = In.RemainingSeconds - DeltaSeconds;
	if (Out.RemainingSeconds > 0.0f)
	{
		return Out;
	}
	if (In.Phase == ECastFishingPhase::Waiting)
	{
		Out.Phase = ECastFishingPhase::Biting;
		Out.RemainingSeconds = FMath::Max(Config.BiteWindowSeconds, 0.1f);
		OutEvent = ECastFishingEvent::Bite;
		return Out;
	}
	Out.Phase = ECastFishingPhase::Idle;
	Out.RemainingSeconds = 0.0f;
	OutEvent = ECastFishingEvent::Escaped;
	return Out;
}

FCastFishingState FCastFishingModel::Pull(const FCastFishingState& In, ECastFishingEvent& OutEvent)
{
	FCastFishingState Out = In;
	Out.Phase = ECastFishingPhase::Idle;
	Out.RemainingSeconds = 0.0f;
	switch (In.Phase)
	{
	case ECastFishingPhase::Biting: OutEvent = ECastFishingEvent::Caught; break;
	case ECastFishingPhase::Waiting: OutEvent = ECastFishingEvent::Reeled; break;
	default: OutEvent = ECastFishingEvent::None; break;
	}
	return Out;
}
