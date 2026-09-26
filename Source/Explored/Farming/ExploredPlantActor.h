#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "GameFramework/Actor.h"

#include "Carry/CarryTypes.h"
#include "Interaction/ExploredInteractable.h"

#include "ExploredPlantActor.generated.h"

class UFarmSubsystem;
class UStaticMesh;
class UStaticMeshComponent;
struct FFarmHarvest;

/** Lo que el jugador puede hacer ahora con una parcela (sin reflexión: solo C++). */
enum class EFarmPlotAction : uint8
{
	Harvest,
	ClearDead,
	Plant,
	Water,
};

/**
 * Parcela del huerto en el mundo (bancal, espaldera o arriate del limonero):
 * registra su parcela en UFarmSubsystem, muestra la etapa del cultivo (malla de
 * los datos o un marcador de /Engine/BasicShapes escalado por el progreso) y
 * ofrece plantar, regar y cosechar con la interacción de siempre (biblia §7.1–7.2).
 * Hasta que exista el sistema de construcción (P-BUILD) se coloca a mano en el
 * nivel indicando qué piezas forma.
 */
UCLASS()
class EXPLORED_API AExploredPlantActor : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredPlantActor();

	int32 GetPlotId() const { return PlotId; }

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	/** Piezas de construcción que forma esta parcela (plants.json → requiresPiece). */
	UPROPERTY(EditAnywhere, Category = "Explored|Huerto")
	TArray<FName> Pieces;

	/** Un espantapájaros junto a la parcela (protege en FFarmModel::ScarecrowRadius). */
	UPROPERTY(EditAnywhere, Category = "Explored|Huerto")
	bool bHasScarecrow = false;

	/** Escala del marcador recién plantado y en la etapa final. */
	UPROPERTY(EditAnywhere, Category = "Explored|Huerto")
	float MinPlantScale = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Explored|Huerto")
	float MaxPlantScale = 1.2f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Huerto")
	TObjectPtr<UStaticMeshComponent> Bed;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Huerto")
	TObjectPtr<UStaticMeshComponent> PlantMesh;

	/** Marcadores mientras la etapa no tenga malla propia (meshes_pendientes.json). */
	UPROPERTY(EditAnywhere, Category = "Explored|Huerto")
	TObjectPtr<UStaticMesh> GrowingPlaceholder;

	UPROPERTY(EditAnywhere, Category = "Explored|Huerto")
	TObjectPtr<UStaticMesh> RipePlaceholder;

private:
	/** Acciones posibles ahora, en orden de prioridad (la primera es la que hace Interact). */
	void GatherActions(const AActor* InInstigator, TArray<EFarmPlotAction>& OutActions, FName& OutSeedItem, EHand& OutSeedHand) const;
	bool HoldsWaterContainer(const AActor* InInstigator) const;
	UFarmSubsystem* GetFarm() const;
	void HandlePlotChanged(int32 ChangedPlotId);
	void RefreshVisual();
	void SpawnHarvest(const FFarmHarvest& Harvest);

	int32 PlotId = INDEX_NONE;
	FDelegateHandle PlotChangedHandle;
};
