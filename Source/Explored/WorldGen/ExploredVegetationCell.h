#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ExploredVegetationCell.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Celda de vegetación horneada: un HISM por malla con las instancias de una
 * región del mundo. Las instancias recolectadas se eliminan en tiempo de
 * juego y se registran para el guardado.
 */
UCLASS()
class EXPLORED_API AExploredVegetationCell : public AActor
{
	GENERATED_BODY()

public:
	AExploredVegetationCell();

	/** Crea (o reutiliza) el HISM de una malla. Solo en horneado. */
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateComponent(UStaticMesh* Mesh, FName Species,
		bool bCollision, float CullDistanceMeters, bool bCastShadow = true);

	/** Especie de un componente de esta celda. */
	FName GetSpecies(const UHierarchicalInstancedStaticMeshComponent* Component) const;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Vegetación")
	FIntPoint CellCoord = FIntPoint::ZeroValue;

private:
	UPROPERTY()
	TMap<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>, FName> ComponentSpecies;
};
