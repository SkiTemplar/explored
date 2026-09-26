#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Carry/CarryTypes.h"
#include "Carry/InventoryModel.h"
#include "Interaction/ExploredInteractable.h"
#include "Items/ItemTypes.h"

#include "ExploredContainer.generated.h"

class UCarryComponent;
class UStaticMeshComponent;

/**
 * Contenedor del mundo: cesta, estante o arcón (GDD §8.2, biblia §3.9).
 * Lo guardado se ve colocado: cada objeto ocupa un hueco visible estable
 * (FInventoryEntry::SlotIndex) que GetSlotTransform traduce a una posición.
 *
 * Las reglas de capacidad son las de FInventoryContainer (modelo puro); el
 * actor guarda además la instancia completa de cada objeto por su id.
 *
 * Interactuar con él: si el jugador lleva algo en una mano, lo guarda; si no,
 * coge a la mano lo último que se guardó.
 */
UCLASS()
class EXPLORED_API AExploredContainer : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredContainer();

	/** Capacidad según la clase (cesta, estante, arcón). Solo si está vacío. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Contenedor")
	void SetKind(EWorldContainerKind InKind);

	UFUNCTION(BlueprintPure, Category = "Explored|Contenedor")
	int32 GetNumStored() const { return Container.Num(); }

	/** Posición local del hueco visible SlotIndex (rejilla de baldas o fondo de la cesta). */
	UFUNCTION(BlueprintPure, Category = "Explored|Contenedor")
	FTransform GetSlotTransform(int32 SlotIndex) const;

	const FInventoryContainer& GetContainer() const { return Container; }
	const FItemInstance* FindInstance(int64 InstanceId) const { return Payloads.Find(InstanceId); }

	/**
	 * Lo usa UCarryComponent: el modelo del jugador mueve el registro y aquí
	 * se mueve la instancia completa. No llamar desde otro sitio.
	 */
	FInventoryContainer& GetMutableContainer() { return Container; }
	void AddPayload(int64 InstanceId, const FItemInstance& Instance) { Payloads.Add(InstanceId, Instance); }
	bool RemovePayload(int64 InstanceId, FItemInstance& OutInstance);
	/** Rehace las mallas de lo guardado; llamar tras cualquier cambio. */
	void RefreshStoredVisuals();

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Capacidad del contenedor; las angarillas la sustituyen por la suya. */
	virtual FInventoryContainerSpec MakeSpec() const;

	/** Mano que tiene algo (derecha antes que izquierda) o false si están vacías. */
	static bool FindFilledHand(const UCarryComponent& Carry, EHand& OutHand);

	UPROPERTY(VisibleAnywhere, Category = "Explored|Contenedor")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "Explored|Contenedor")
	EWorldContainerKind Kind = EWorldContainerKind::Cesta;

	/** Nombre estable para el guardado (P-SAVE), p. ej. «arcon_base_01». */
	UPROPERTY(EditAnywhere, Category = "Explored|Contenedor")
	FName ContainerId;

	/** Huecos por fila y separación de la rejilla donde se colocan los objetos. */
	UPROPERTY(EditAnywhere, Category = "Explored|Contenedor")
	int32 SlotsPerRow = 3;

	UPROPERTY(EditAnywhere, Category = "Explored|Contenedor")
	FVector SlotSpacingCm = FVector(25.0f, 25.0f, 35.0f);

	UPROPERTY(EditAnywhere, Category = "Explored|Contenedor")
	FVector FirstSlotOffsetCm = FVector(-25.0f, -25.0f, 60.0f);

	UPROPERTY(EditAnywhere, Category = "Explored|Contenedor")
	float StoredItemScale = 0.25f;

	/** Registros (modelo puro): no es USTRUCT, lo persiste P-SAVE junto con Payloads. */
	FInventoryContainer Container;

	UPROPERTY()
	TMap<int64, FItemInstance> Payloads;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> StoredMeshes;
};
