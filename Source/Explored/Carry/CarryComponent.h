#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Carry/CarryTypes.h"
#include "Items/ItemTypes.h"

#include "CarryComponent.generated.h"

class AExploredItemActor;
class UItemRegistrySubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCarryChanged);

/**
 * Manos, bolsillos, cinturón y mochila del jugador (GDD §4.2, biblia §2).
 * No sabe nada de fabricación: solo transporta FItemInstance y valida
 * capacidad. UCraftingLibrary y UInteractionComponent son quienes lo usan.
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCarryComponent();

	/** Va a la primera mano libre; si el objeto es DosManos, exige las dos vacías. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool TryPickUp(AExploredItemActor* ItemActor, FText& OutFailReason);

	/** Genera el actor en el mundo, delante del jugador. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool Drop(EHand Hand, FText& OutFailReason);

	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool StoreFromHand(EHand Hand, ECarrySlot Slot, FText& OutFailReason);

	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool TakeToHand(ECarrySlot Slot, int32 Index, EHand Hand, FText& OutFailReason);

	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool SwapHands();

	/**
	 * Vacía las dos manos (las piezas ya se consumieron al fabricar) y coloca
	 * el resultado en la mano libre, o en las dos si es DosManos. Lo usa
	 * AExploredCharacter::HandleCombine tras UCraftingLibrary::Apply.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool ReplaceHandsWithCraftResult(const FItemInstance& Result, FText& OutFailReason);

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	float GetTotalWeight() const;

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool GetHandItem(EHand Hand, FItemInstance& OutItem) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool IsHandEmpty(EHand Hand) const;

	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	void SetBackpack(bool bInHasBackpack, float CapacityVolumeLiters, float CapacityWeightKg);

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool HasBackpack() const { return bHasBackpack; }

	/** Directo, sin comprobar registro: lo usan las manos-en-pantalla y el HUD. */
	const FItemInstance* GetHandItemPtr(EHand Hand) const;

	const TArray<FItemInstance>& GetPocketItems() const { return Pockets; }
	const TArray<FItemInstance>& GetBeltItems() const { return Belt; }
	const TArray<FItemInstance>& GetBackpackItems() const { return BackpackItems; }

	UPROPERTY(BlueprintAssignable, Category = "Explored|Carga")
	FOnCarryChanged OnCarryChanged;

	static constexpr int32 MaxPocketSlots = 4;
	static constexpr int32 MaxBeltSlots = 3;

private:
	const UItemRegistrySubsystem* GetRegistry() const;
	bool PlaceInFreeHand(const FItemInstance& Instance, bool bTwoHandedItem, FText& OutFailReason);
	AExploredItemActor* SpawnDropped(const FItemInstance& Instance) const;

	UPROPERTY()
	FItemInstance HandLeft;
	UPROPERTY()
	bool bHandLeftFilled = false;

	UPROPERTY()
	FItemInstance HandRight;
	UPROPERTY()
	bool bHandRightFilled = false;

	/** Si es true, HandLeft y HandRight contienen el mismo objeto DosManos (ver Drop). */
	UPROPERTY()
	bool bHandsHoldTwoHandedItem = false;

	UPROPERTY()
	TArray<FItemInstance> Pockets;

	UPROPERTY()
	TArray<FItemInstance> Belt;

	UPROPERTY()
	TArray<FItemInstance> BackpackItems;

	UPROPERTY(EditAnywhere, Category = "Explored|Carga")
	bool bHasBackpack = false;

	UPROPERTY(EditAnywhere, Category = "Explored|Carga")
	float BackpackCapacityVolumeLiters = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Carga")
	float BackpackCapacityWeightKg = 0.0f;
};
