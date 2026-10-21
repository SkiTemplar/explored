#include "Narrative/SupplyCrateModel.h"

namespace SupplyCrateDetail
{
	int32 Count(const TMap<FName, int32>& Inventory, const TCHAR* Id)
	{
		const int32* Found = Inventory.Find(FName(Id));
		return Found ? FMath::Max(*Found, 0) : 0;
	}

	/** Peces crudos que hacen falta para los tarros que quedan. */
	int32 FishNeeded(const FBeaconProgressState& Progress)
	{
		if (FBeaconProgressModel::IsSlotFilled(Progress, ELanternSlot::Oil))
		{
			return 0;
		}
		const int32 JarsMissing = FMath::Max(FBeaconProgressModel::RequiredOilJars - FMath::Max(Progress.OilJars, 0), 0);
		return JarsMissing * FSupplyCrateModel::FishPerOilJar;
	}

	int32 FibersForWick(const TMap<FName, int32>& Inventory, const FBeaconProgressState& Progress)
	{
		const bool bHasWick = Progress.bWickMade || FBeaconProgressModel::IsSlotFilled(Progress, ELanternSlot::Wick)
			|| Count(Inventory, TEXT("mecha")) > 0;
		return bHasWick ? 0 : FSupplyCrateModel::FibersPerWick;
	}

	int32 RopesNeeded(const TMap<FName, int32>& Inventory, const FBeaconProgressState& Progress, int32 FishDeficit)
	{
		int32 Ropes = 0;
		if (!Progress.bLensCollected && !Progress.bRaftBuilt)
		{
			Ropes += 2;
			Ropes += Count(Inventory, TEXT("remo")) > 0 ? 0 : 1;
		}
		if (FishDeficit > FSupplyCrateModel::MaxRawFish && Count(Inventory, TEXT("cana_pescar")) == 0)
		{
			Ropes += 1;
		}
		return Ropes;
	}
}

FName FSupplyCrateModel::RopeId()
{
	return FName(TEXT("cuerda"));
}

FName FSupplyCrateModel::FiberId()
{
	return FName(TEXT("fibra"));
}

FName FSupplyCrateModel::RawFishId()
{
	return FName(TEXT("pez_crudo"));
}

TMap<FName, int32> FSupplyCrateModel::ComputeMissing(const TMap<FName, int32>& Inventory, const FBeaconProgressState& Progress)
{
	using namespace SupplyCrateDetail;
	const int32 FishDeficit = FMath::Max(FishNeeded(Progress) - Count(Inventory, TEXT("pez_crudo")), 0);
	const int32 RopeDeficit = FMath::Max(RopesNeeded(Inventory, Progress, FishDeficit) - Count(Inventory, TEXT("cuerda")), 0);
	const int32 RopeGiven = FMath::Min(RopeDeficit, MaxRope);
	const int32 FiberNeed = FibersForWick(Inventory, Progress) + (RopeDeficit - RopeGiven) * FibersPerRope;
	const int32 FiberDeficit = FMath::Max(FiberNeed - Count(Inventory, TEXT("fibra")), 0);

	TMap<FName, int32> Missing;
	if (RopeGiven > 0)
	{
		Missing.Add(RopeId(), RopeGiven);
	}
	if (FiberDeficit > 0)
	{
		Missing.Add(FiberId(), FMath::Min(FiberDeficit, MaxFiber));
	}
	if (FishDeficit > 0)
	{
		Missing.Add(RawFishId(), FMath::Min(FishDeficit, MaxRawFish));
	}
	return Missing;
}

FSupplyCrateOpenResult FSupplyCrateModel::Open(const FSupplyCrateState& State, const TMap<FName, int32>& Inventory,
	const FBeaconProgressState& Progress)
{
	FSupplyCrateOpenResult Result;
	Result.State = State;
	if (State.bDelivered)
	{
		return Result;
	}
	TMap<FName, int32> Missing = ComputeMissing(Inventory, Progress);
	if (Missing.IsEmpty())
	{
		return Result;
	}
	Result.Items = Missing;
	Result.State.bDelivered = true;
	Result.bDelivered = true;
	return Result;
}
