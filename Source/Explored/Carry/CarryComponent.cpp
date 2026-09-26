#include "Carry/CarryComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Items/ExploredItemActor.h"
#include "Items/ItemRegistrySubsystem.h"

namespace
{
	constexpr float DropDistanceCm = 100.0f;
}

UCarryComponent::UCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const UItemRegistrySubsystem* UCarryComponent::GetRegistry() const
{
	return UItemRegistrySubsystem::Resolve(this);
}

bool UCarryComponent::PlaceInFreeHand(const FItemInstance& Instance, bool bTwoHandedItem, FText& OutFailReason)
{
	if (bTwoHandedItem)
	{
		if (bHandLeftFilled || bHandRightFilled)
		{
			OutFailReason = NSLOCTEXT("Explored", "Carry_NeedBothHands", "Necesitas las dos manos libres para esto.");
			return false;
		}
		HandLeft = Instance;
		HandRight = Instance;
		bHandLeftFilled = true;
		bHandRightFilled = true;
		bHandsHoldTwoHandedItem = true;
		return true;
	}

	if (!bHandLeftFilled)
	{
		HandLeft = Instance;
		bHandLeftFilled = true;
		return true;
	}
	if (!bHandRightFilled)
	{
		HandRight = Instance;
		bHandRightFilled = true;
		return true;
	}
	OutFailReason = NSLOCTEXT("Explored", "Carry_HandsFull", "Tienes las dos manos ocupadas.");
	return false;
}

bool UCarryComponent::TryPickUp(AExploredItemActor* ItemActor, FText& OutFailReason)
{
	if (!ItemActor)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_NothingToPickUp", "Aquí no hay nada que coger.");
		return false;
	}
	const FItemInstance& Instance = ItemActor->GetItemInstance();
	if (!Instance.IsValid())
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_InvalidItem", "Ese objeto no se puede coger.");
		return false;
	}

	const bool bTwoHanded = ItemActor->GetEffectiveSize() == EItemSize::DosManos;
	if (!PlaceInFreeHand(Instance, bTwoHanded, OutFailReason))
	{
		return false;
	}

	ItemActor->Destroy();
	OnCarryChanged.Broadcast();
	return true;
}

AExploredItemActor* UCarryComponent::SpawnDropped(const FItemInstance& Instance) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}
	const FVector Location = Owner->GetActorLocation() + Owner->GetActorForwardVector() * DropDistanceCm;
	AExploredItemActor* NewActor = World->SpawnActor<AExploredItemActor>(AExploredItemActor::StaticClass(), Location, Owner->GetActorRotation());
	if (NewActor)
	{
		NewActor->InitializeFromInstance(Instance);
	}
	return NewActor;
}

bool UCarryComponent::Drop(EHand Hand, FText& OutFailReason)
{
	if (bHandsHoldTwoHandedItem)
	{
		SpawnDropped(HandLeft);
		HandLeft = FItemInstance();
		HandRight = FItemInstance();
		bHandLeftFilled = false;
		bHandRightFilled = false;
		bHandsHoldTwoHandedItem = false;
		OnCarryChanged.Broadcast();
		return true;
	}

	const bool bFilled = (Hand == EHand::Left) ? bHandLeftFilled : bHandRightFilled;
	if (!bFilled)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_HandEmpty", "No llevas nada en esa mano.");
		return false;
	}

	FItemInstance& HandRef = (Hand == EHand::Left) ? HandLeft : HandRight;
	SpawnDropped(HandRef);
	HandRef = FItemInstance();
	if (Hand == EHand::Left) { bHandLeftFilled = false; } else { bHandRightFilled = false; }
	OnCarryChanged.Broadcast();
	return true;
}

bool UCarryComponent::StoreFromHand(EHand Hand, ECarrySlot Slot, FText& OutFailReason)
{
	if (bHandsHoldTwoHandedItem)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_TwoHandedNoStore", "Eso ocupa las dos manos: no se guarda.");
		return false;
	}
	const bool bFilled = (Hand == EHand::Left) ? bHandLeftFilled : bHandRightFilled;
	if (!bFilled)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_HandEmpty", "No llevas nada en esa mano.");
		return false;
	}
	const FItemInstance& Instance = (Hand == EHand::Left) ? HandLeft : HandRight;
	const UItemRegistrySubsystem* Registry = GetRegistry();

	switch (Slot)
	{
	case ECarrySlot::Pocket:
	{
		const EItemSize Size = Registry ? Registry->GetEffectiveSize(Instance) : EItemSize::Grande;
		if (Size != EItemSize::Pequeno)
		{
			OutFailReason = NSLOCTEXT("Explored", "Carry_PocketTooSmall", "Solo los objetos pequeños caben en los bolsillos.");
			return false;
		}
		if (Pockets.Num() >= MaxPocketSlots)
		{
			OutFailReason = NSLOCTEXT("Explored", "Carry_PocketsFull", "Los bolsillos están llenos.");
			return false;
		}
		Pockets.Add(Instance);
		break;
	}
	case ECarrySlot::Belt:
	{
		bool bIsBeltWorthy = false;
		if (Registry)
		{
			const TMap<FName, FItemDefinition>& Items = Registry->GetItems();
			bIsBeltWorthy = ItemEffective::HasTag(Instance, TEXT("herramienta"), Items)
				|| ItemEffective::HasTag(Instance, TEXT("contenedor"), Items)
				|| ItemEffective::HasTag(Instance, TEXT("recipiente"), Items);
		}
		if (!bIsBeltWorthy)
		{
			OutFailReason = NSLOCTEXT("Explored", "Carry_BeltNotTool", "El cinturón solo lleva herramientas o recipientes.");
			return false;
		}
		if (Belt.Num() >= MaxBeltSlots)
		{
			OutFailReason = NSLOCTEXT("Explored", "Carry_BeltFull", "El cinturón no tiene enganches libres.");
			return false;
		}
		Belt.Add(Instance);
		break;
	}
	case ECarrySlot::Backpack:
	{
		if (!bHasBackpack)
		{
			OutFailReason = NSLOCTEXT("Explored", "Carry_NoBackpack", "No tienes mochila.");
			return false;
		}
		if (!Registry)
		{
			OutFailReason = NSLOCTEXT("Explored", "Carry_NoRegistry", "El registro de objetos no está disponible.");
			return false;
		}
		float UsedWeight = 0.0f, UsedVolume = 0.0f;
		for (const FItemInstance& Existing : BackpackItems)
		{
			UsedWeight += Registry->GetEffectiveWeightKg(Existing);
			UsedVolume += Registry->GetEffectiveVolumeLiters(Existing);
		}
		const float AddedWeight = Registry->GetEffectiveWeightKg(Instance);
		const float AddedVolume = Registry->GetEffectiveVolumeLiters(Instance);
		if (UsedWeight + AddedWeight > BackpackCapacityWeightKg)
		{
			OutFailReason = NSLOCTEXT("Explored", "Carry_BackpackTooHeavy", "Pesa demasiado para la mochila.");
			return false;
		}
		if (UsedVolume + AddedVolume > BackpackCapacityVolumeLiters)
		{
			OutFailReason = NSLOCTEXT("Explored", "Carry_BackpackTooBig", "No entra en la mochila.");
			return false;
		}
		BackpackItems.Add(Instance);
		break;
	}
	}

	if (Hand == EHand::Left) { HandLeft = FItemInstance(); bHandLeftFilled = false; }
	else { HandRight = FItemInstance(); bHandRightFilled = false; }

	OnCarryChanged.Broadcast();
	return true;
}

bool UCarryComponent::TakeToHand(ECarrySlot Slot, int32 Index, EHand Hand, FText& OutFailReason)
{
	const bool bHandFilled = (Hand == EHand::Left) ? bHandLeftFilled : bHandRightFilled;
	if (bHandFilled)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_HandOccupied", "Esa mano ya está ocupada.");
		return false;
	}

	TArray<FItemInstance>* Container = nullptr;
	switch (Slot)
	{
	case ECarrySlot::Pocket: Container = &Pockets; break;
	case ECarrySlot::Belt: Container = &Belt; break;
	case ECarrySlot::Backpack: Container = &BackpackItems; break;
	}
	if (!Container || !Container->IsValidIndex(Index))
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_InvalidSlotIndex", "Ahí no hay nada.");
		return false;
	}

	const FItemInstance Instance = (*Container)[Index];
	Container->RemoveAt(Index);
	if (Hand == EHand::Left) { HandLeft = Instance; bHandLeftFilled = true; }
	else { HandRight = Instance; bHandRightFilled = true; }

	OnCarryChanged.Broadcast();
	return true;
}

bool UCarryComponent::ReplaceHandsWithCraftResult(const FItemInstance& Result, FText& OutFailReason)
{
	HandLeft = FItemInstance();
	HandRight = FItemInstance();
	bHandLeftFilled = false;
	bHandRightFilled = false;
	bHandsHoldTwoHandedItem = false;

	const UItemRegistrySubsystem* Registry = GetRegistry();
	const bool bTwoHanded = Registry && Registry->GetEffectiveSize(Result) == EItemSize::DosManos;
	return PlaceInFreeHand(Result, bTwoHanded, OutFailReason);
}

bool UCarryComponent::SwapHands()
{
	if (bHandsHoldTwoHandedItem)
	{
		return false;
	}
	Swap(HandLeft, HandRight);
	Swap(bHandLeftFilled, bHandRightFilled);
	OnCarryChanged.Broadcast();
	return true;
}

float UCarryComponent::GetTotalWeight() const
{
	const UItemRegistrySubsystem* Registry = GetRegistry();
	if (!Registry)
	{
		return 0.0f;
	}
	float Total = 0.0f;
	if (bHandLeftFilled) { Total += Registry->GetEffectiveWeightKg(HandLeft); }
	if (bHandRightFilled && !bHandsHoldTwoHandedItem) { Total += Registry->GetEffectiveWeightKg(HandRight); }
	for (const FItemInstance& Item : Pockets) { Total += Registry->GetEffectiveWeightKg(Item); }
	for (const FItemInstance& Item : Belt) { Total += Registry->GetEffectiveWeightKg(Item); }
	for (const FItemInstance& Item : BackpackItems) { Total += Registry->GetEffectiveWeightKg(Item); }
	return Total;
}

bool UCarryComponent::GetHandItem(EHand Hand, FItemInstance& OutItem) const
{
	const bool bFilled = (Hand == EHand::Left) ? bHandLeftFilled : bHandRightFilled;
	if (!bFilled)
	{
		return false;
	}
	OutItem = (Hand == EHand::Left) ? HandLeft : HandRight;
	return true;
}

bool UCarryComponent::IsHandEmpty(EHand Hand) const
{
	return !((Hand == EHand::Left) ? bHandLeftFilled : bHandRightFilled);
}

void UCarryComponent::SetBackpack(bool bInHasBackpack, float CapacityVolumeLiters, float CapacityWeightKg)
{
	bHasBackpack = bInHasBackpack;
	BackpackCapacityVolumeLiters = CapacityVolumeLiters;
	BackpackCapacityWeightKg = CapacityWeightKg;
}

const FItemInstance* UCarryComponent::GetHandItemPtr(EHand Hand) const
{
	const bool bFilled = (Hand == EHand::Left) ? bHandLeftFilled : bHandRightFilled;
	if (!bFilled)
	{
		return nullptr;
	}
	return (Hand == EHand::Left) ? &HandLeft : &HandRight;
}
