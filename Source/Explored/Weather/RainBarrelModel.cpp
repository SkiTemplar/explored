#include "Weather/RainBarrelModel.h"

FRainBarrelState FRainBarrelModel::AddCharges(const FRainBarrelState& In, int32 Charges)
{
	FRainBarrelState Out = In;
	Out.Charges = FMath::Clamp(In.Charges + FMath::Max(0, Charges), 0, MaxCharges);
	if (Out.Charges == MaxCharges)
	{
		Out.RainSeconds = 0.0f;
	}
	return Out;
}

FRainBarrelState FRainBarrelModel::Collect(const FRainBarrelState& In, float RainSeconds)
{
	if (!(RainSeconds > 0.0f) || In.Charges >= MaxCharges)
	{
		return In;
	}
	FRainBarrelState Out = In;
	Out.RainSeconds = In.RainSeconds + RainSeconds;
	const int32 NewCharges = FMath::FloorToInt(Out.RainSeconds / RainSecondsPerCharge);
	Out.RainSeconds -= static_cast<float>(NewCharges) * RainSecondsPerCharge;
	return AddCharges(Out, NewCharges);
}

ERainBarrelResult FRainBarrelModel::RefillCanteen(const FRainBarrelState& InBarrel, const FBackpackState& InPack,
	FRainBarrelState& OutBarrel, FBackpackState& OutPack)
{
	OutBarrel = InBarrel;
	OutPack = InPack;
	if (FBackpackModel::Count(InPack, FBackpackModel::CanteenId()) <= 0)
	{
		return ERainBarrelResult::NoCanteen;
	}
	if (InPack.CanteenSips >= FBackpackModel::CanteenMaxSips)
	{
		return ERainBarrelResult::CanteenFull;
	}
	if (InBarrel.Charges <= 0)
	{
		return ERainBarrelResult::Empty;
	}
	OutBarrel.Charges = InBarrel.Charges - 1;
	OutPack.CanteenSips = FBackpackModel::CanteenMaxSips;
	return ERainBarrelResult::Ok;
}
