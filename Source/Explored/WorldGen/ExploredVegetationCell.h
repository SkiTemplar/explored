#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Interaction/ExploredInteractable.h"

#include "ExploredVegetationCell.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UPrimitiveComponent;
class UStaticMesh;

/**
 * Celda de vegetación horneada: un HISM por malla con las instancias de una
 * región del mundo. Las instancias recolectadas se eliminan en tiempo de
 * juego y se registran para el guardado (UExploredVegetationHarvestSubsystem).
 *
 * Una celda entera no es «interactuable» en bloque: implementa
 * IExploredInteractable pero responde sobre la instancia exacta que fijó
 * SetInteractionFocus (UInteractionComponent la llama con el resultado de la
 * traza antes de preguntar CanInteract), porque un único actor aquí agrupa
 * cientos de plantas o rocas distintas.
 */
UCLASS()
class EXPLORED_API AExploredVegetationCell : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredVegetationCell();

	/** Crea (o reutiliza) el HISM de una malla. Solo en horneado. */
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateComponent(UStaticMesh* Mesh, FName Species,
		bool bCollision, float CullDistanceMeters, bool bCastShadow = true);

	/** Especie de un componente de esta celda. */
	FName GetSpecies(const UHierarchicalInstancedStaticMeshComponent* Component) const;

	/** Todos los componentes HISM de esta celda con su especie (para el subsistema de recolección y el guardado). */
	const TMap<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>, FName>& GetComponentSpecies() const { return ComponentSpecies; }

	/** La instancia exacta que golpeó la última traza de interacción (ver UInteractionComponent::UpdateFocus). */
	void SetInteractionFocus(UPrimitiveComponent* Component, int32 InstanceIndex);

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Vegetación")
	FIntPoint CellCoord = FIntPoint::ZeroValue;

private:
	UPROPERTY()
	TMap<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>, FName> ComponentSpecies;

	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> FocusedComponent;
	int32 FocusedInstanceIndex = INDEX_NONE;
};
