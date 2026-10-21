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

	/** Se puede pescar: hay caña o se puede atar una con lo que hay (caña de bambú y cuerda o 2 fibras). */
	bool CanGetRod(const TMap<FName, int32>& Inventory, const TMap<FName, int32>& World)
	{
		if (Count(Inventory, TEXT("cana_pescar")) > 0)
		{
			return true;
		}
		const int32 Bamboo = Count(Inventory, TEXT("cana_bambu")) + Count(World, TEXT("cana_bambu"));
		const int32 Fibers = Count(Inventory, TEXT("fibra")) + Count(World, TEXT("fibra"));
		return Bamboo > 0 && (Count(Inventory, TEXT("cuerda")) > 0 || Fibers >= FSupplyCrateModel::FibersPerRope);
	}

	TMap<FName, int32> Compute(const TMap<FName, int32>& Inventory, const FBeaconProgressState& Progress,
		const TMap<FName, int32>* World);

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

FName FSupplyCrateModel::WoodId()
{
	return FName(TEXT("lena"));
}

TMap<FName, int32> SupplyCrateDetail::Compute(const TMap<FName, int32>& Inventory, const FBeaconProgressState& Progress,
	const TMap<FName, int32>* World)
{
	using FCrate = FSupplyCrateModel;
	const TMap<FName, int32> NoWorld;
	const TMap<FName, int32>& Island = World ? *World : NoWorld;
	const int32 FishForOil = FishNeeded(Progress);
	// Con la isla conocida, el pescado no falta mientras se pueda pescar (reaparece).
	const bool bFishObtainable = World && CanGetRod(Inventory, Island);
	const int32 FishDeficit = bFishObtainable ? 0 : FMath::Max(FishForOil - Count(Inventory, TEXT("pez_crudo")), 0);
	// La caña de pescar pide su cuerda si aún no existe y hacen falta peces.
	const bool bRodRope = bFishObtainable && FishForOil > Count(Inventory, TEXT("pez_crudo")) && Count(Inventory, TEXT("cana_pescar")) == 0;
	const int32 RopesWanted = RopesNeeded(Inventory, Progress, FishDeficit) + (bRodRope ? 1 : 0);
	const int32 RopeDeficit = FMath::Max(RopesWanted - Count(Inventory, TEXT("cuerda")), 0);
	const int32 FiberNeed = FibersForWick(Inventory, Progress) + RopeDeficit * FCrate::FibersPerRope;
	const int32 FiberHave = Count(Inventory, TEXT("fibra")) + Count(Island, TEXT("fibra"));
	// Las fibras que faltan se cubren primero con la cuerda de la caja (vale por 2 fibras).
	const int32 FiberShort = FMath::Max(FiberNeed - FiberHave, 0);
	const int32 RopeGiven = FMath::Min(World ? FMath::DivideAndRoundUp(FiberShort, FCrate::FibersPerRope) : RopeDeficit,
		FMath::Min(RopeDeficit, FCrate::MaxRope));
	const int32 FiberDeficit = FMath::Max(FiberNeed - RopeGiven * FCrate::FibersPerRope - FiberHave, 0);
	const int32 WoodNeed = FishForOil > 0 ? FCrate::WoodPerFire : 0;
	const int32 WoodDeficit = FMath::Max(WoodNeed - Count(Inventory, TEXT("lena")) - Count(Island, TEXT("lena")), 0);

	TMap<FName, int32> Missing;
	if (RopeGiven > 0)
	{
		Missing.Add(FCrate::RopeId(), RopeGiven);
	}
	if (FiberDeficit > 0)
	{
		Missing.Add(FCrate::FiberId(), FMath::Min(FiberDeficit, FCrate::MaxFiber));
	}
	if (FishDeficit > 0)
	{
		Missing.Add(FCrate::RawFishId(), FMath::Min(FishDeficit, FCrate::MaxRawFish));
	}
	if (WoodDeficit > 0)
	{
		Missing.Add(FCrate::WoodId(), FMath::Min(WoodDeficit, FCrate::MaxWood));
	}
	return Missing;
}

TMap<FName, int32> FSupplyCrateModel::ComputeMissing(const TMap<FName, int32>& Inventory, const FBeaconProgressState& Progress)
{
	return SupplyCrateDetail::Compute(Inventory, Progress, nullptr);
}

TMap<FName, int32> FSupplyCrateModel::ComputeMissing(const TMap<FName, int32>& Inventory, const FBeaconProgressState& Progress,
	const TMap<FName, int32>& WorldRemaining)
{
	return SupplyCrateDetail::Compute(Inventory, Progress, &WorldRemaining);
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

FSupplyCrateOpenResult FSupplyCrateModel::Open(const FSupplyCrateState& State, const TMap<FName, int32>& Inventory,
	const FBeaconProgressState& Progress, const TMap<FName, int32>& WorldRemaining)
{
	FSupplyCrateOpenResult Result;
	Result.State = State;
	if (State.bDelivered)
	{
		return Result;
	}
	TMap<FName, int32> Missing = ComputeMissing(Inventory, Progress, WorldRemaining);
	if (Missing.IsEmpty())
	{
		return Result;
	}
	Result.Items = Missing;
	Result.State.bDelivered = true;
	Result.bDelivered = true;
	return Result;
}
