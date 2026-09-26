#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Building/BuildingTypes.h"

#include "ExploredBuildingPiece.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * Representación en el mundo de una pieza de FBuildingModel. No tiene lógica:
 * UBuildingSubsystem la crea, la coloca y la destruye; el estado vive en el modelo.
 */
UCLASS()
class EXPLORED_API AExploredBuildingPiece : public AActor
{
	GENERATED_BODY()

public:
	AExploredBuildingPiece();

	/** Enlaza el actor con su pieza y pone la malla (o una forma básica con las medidas del encaje). */
	void InitializePiece(int32 InPieceId, FName InDefId, EBuildSocket Socket, const FString& MeshPath);

	UFUNCTION(BlueprintPure, Category = "Explored|Construcción")
	int32 GetPieceId() const { return PieceId; }

	UFUNCTION(BlueprintPure, Category = "Explored|Construcción")
	FName GetDefId() const { return DefId; }

	UStaticMeshComponent* GetMeshComponent() const { return Mesh; }

	/**
	 * Malla para un encaje: la de MeshPath si existe; si no, un cubo de /Engine/BasicShapes
	 * con OutScale ajustado a las medidas del kit (suelo 2 × 2 m, pared 2 × 2,5 m...).
	 */
	static UStaticMesh* LoadMeshForSocket(EBuildSocket Socket, const FString& MeshPath, FVector& OutScale, FVector& OutOffset);

private:
	UPROPERTY(VisibleAnywhere, Category = "Explored|Construcción")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Construcción")
	int32 PieceId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Construcción")
	FName DefId;
};
