#include "UI/IslandHudModel.h"

FString FIslandHudModel::ClockText(int32 Day, float HourOfDay)
{
	const float Safe = FMath::IsFinite(HourOfDay) ? HourOfDay : 0.0f;
	float Hour = FMath::Fmod(Safe, 24.0f);
	if (Hour < 0.0f)
	{
		Hour += 24.0f;
	}
	const int32 Minutes = FMath::Clamp(FMath::FloorToInt32(Hour * 60.0f), 0, 24 * 60 - 1);
	return FString::Printf(TEXT("Día %d · %02d:%02d"), FMath::Max(1, Day), Minutes / 60, Minutes % 60);
}

FString FIslandHudModel::DayCardText(int32 Day)
{
	return FString::Printf(TEXT("Día %d"), FMath::Max(1, Day));
}

FString FIslandHudModel::BarometerText(float Hpa, EBarometerTrend Trend)
{
	const int32 Rounded = FMath::IsFinite(Hpa) ? FMath::RoundToInt32(Hpa) : 0;
	const TCHAR* Arrow = Trend == EBarometerTrend::Rising ? TEXT("↑") : Trend == EBarometerTrend::Falling ? TEXT("↓") : TEXT("→");
	return FString::Printf(TEXT("%d hPa %s"), Rounded, Arrow);
}

FString FIslandHudModel::PromptText(const FString& KeyName, const FString& VerbAndObject)
{
	if (KeyName.IsEmpty() || VerbAndObject.IsEmpty())
	{
		return FString();
	}
	return KeyName + TEXT(" · ") + VerbAndObject;
}

bool FIslandHudModel::IsBarVisible(float Value, float LowThreshold, double TimeSeconds)
{
	if (FMath::IsFinite(Value) && Value >= LowThreshold)
	{
		return true;
	}
	if (!FMath::IsFinite(TimeSeconds))
	{
		return true;
	}
	const double Phase = FMath::Fmod(FMath::Abs(TimeSeconds) * BlinkHz, 1.0);
	return Phase < 0.5;
}

TArray<FHudQuickSlot> FIslandHudModel::QuickSlots(const FBackpackState& Pack, int32 Selected)
{
	TArray<FHudQuickSlot> Slots;
	Slots.SetNum(QuickSlotCount);
	for (int32 Index = 0; Index < QuickSlotCount; ++Index)
	{
		if (Pack.Stacks.IsValidIndex(Index) && Pack.Stacks[Index].Count > 0)
		{
			Slots[Index].ItemId = Pack.Stacks[Index].Id;
			Slots[Index].Count = Pack.Stacks[Index].Count;
		}
		Slots[Index].bSelected = Index == Selected;
	}
	return Slots;
}

float FIslandHudModel::Progress01(float ElapsedSeconds, float TotalSeconds)
{
	if (!FMath::IsFinite(ElapsedSeconds) || !FMath::IsFinite(TotalSeconds) || TotalSeconds <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(ElapsedSeconds / TotalSeconds, 0.0f, 1.0f);
}
