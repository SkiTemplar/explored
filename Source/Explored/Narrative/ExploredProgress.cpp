#include "Narrative/ExploredProgress.h"

bool FExploredProgress::Discover(FName Id)
{
	if (Id.IsNone() || Discovered.Contains(Id))
	{
		return false;
	}
	Discovered.Add(Id);
	return true;
}

int32 FExploredProgress::CountWithPrefix(const FString& Prefix) const
{
	int32 Count = 0;
	for (const FName& Id : Discovered)
	{
		Count += Id.ToString().StartsWith(Prefix) ? 1 : 0;
	}
	return Count;
}

bool FExploredProgress::HasAllShipParts() const
{
	for (uint8 P = 0; P < static_cast<uint8>(EShipPart::Count); ++P)
	{
		if (!HasShipPart(static_cast<EShipPart>(P)))
		{
			return false;
		}
	}
	return true;
}

int32 FExploredProgress::CountShipParts() const
{
	int32 Count = 0;
	for (uint8 P = 0; P < static_cast<uint8>(EShipPart::Count); ++P)
	{
		Count += HasShipPart(static_cast<EShipPart>(P)) ? 1 : 0;
	}
	return Count;
}

bool FExploredProgress::BuildShip()
{
	if (!CanBuildShip())
	{
		return false;
	}
	bShipBuilt = true;
	return true;
}

bool FExploredProgress::IsStarNight(float Hours)
{
	const float Wrapped = FMath::Fmod(FMath::Fmod(Hours, 24.0f) + 24.0f, 24.0f);
	return Wrapped >= NightStartHours || Wrapped < NightEndHours;
}

bool FExploredProgress::Depart(float Hours)
{
	if (!CanDepart() || HasDeparted() || !IsStarNight(Hours))
	{
		return false;
	}
	EndingsSeen.Add(EExploredEnding::Departed);
	return true;
}

EVoyageStage FExploredProgress::GetVoyageStage() const
{
	if (HasDeparted())
	{
		return EVoyageStage::Departed;
	}
	if (bShipBuilt)
	{
		return EVoyageStage::SetSail;
	}
	return HasAllShipParts() ? EVoyageStage::Build : EVoyageStage::Salvage;
}

float FExploredProgress::MapCompletion() const
{
	struct FCategory
	{
		const TCHAR* Prefix;
		int32 Total;
	};
	const FCategory Categories[] = {
		{TEXT("petro_"), TotalPetroglyphs},
		{TEXT("bottle_"), TotalBottles},
		{TEXT("view_"), TotalViewpoints},
	};
	int32 Found = 0;
	int32 Total = 0;
	for (const FCategory& C : Categories)
	{
		Found += FMath::Min(CountWithPrefix(C.Prefix), C.Total);
		Total += C.Total;
	}
	return Total > 0 ? 100.0f * Found / Total : 0.0f;
}

bool FExploredProgress::IsMapComplete() const
{
	return MapCompletion() >= 100.0f - KINDA_SMALL_NUMBER;
}
