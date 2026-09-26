#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Fishing/FishingModel.h"
#include "Interaction/ExploredInteractable.h"

#include "ExploredTrap.generated.h"

class UStaticMeshComponent;

/**
 * Trampa colocada (nasa, trampa de cangrejos, corral de piedras) o poza de
 * marea natural (GDD §8.9 y §10, «sin criatura visible»). No hay animal que
 * perseguir: al revisarla, el modelo dice qué ha caído desde la última vez
 * y se suelta como objetos. El estado vive en UExploredFishingSubsystem.
 */
UCLASS()
class EXPLORED_API AExploredTrap : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredTrap();

	/** Tipo y cebo antes de FinishSpawning (UFishingComponent::PlaceTrap). */
	void Configure(FName InKindName, FName InBaitItemId);

	UFUNCTION(BlueprintPure, Category = "Explored|Pesca")
	int32 GetTrapId() const { return TrapId; }

	/** Es una poza de marea natural (no una trampa colocada). */
	bool IsTidePool() const;

	/** Vuelve a crear el actor de una trampa guardada (sección «fishing»). */
	static AExploredTrap* SpawnRestored(UWorld* World, const FPlacedTrap& Placed);

	/** «nasa», «trampa_cangrejos» o «corral_piedras». */
	static FName NameForKind(ETrapKind Kind);

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** «nasa», «trampa_cangrejos», «corral_piedras» (como en fish.json) o «poza» para una poza de marea. */
	UPROPERTY(EditAnywhere, Category = "Explored|Pesca")
	FName KindName = TEXT("nasa");

	/** Cebo (id de items.json) con que se coloca; vacío = sin cebo. */
	UPROPERTY(EditAnywhere, Category = "Explored|Pesca")
	FName BaitItemId;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Pesca")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Id en FFishingSaveState (0 = aún sin registrar). */
	UPROPERTY(VisibleAnywhere, SaveGame, Category = "Explored|Pesca")
	int32 TrapId = 0;

private:
	static bool KindFromName(FName Name, ETrapKind& OutKind);
	void DropCatches(const TArray<FTrapCatch>& Catches) const;
};
