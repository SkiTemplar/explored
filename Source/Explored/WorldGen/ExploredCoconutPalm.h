#pragma once

#include "CoreMinimal.h"

#include "Interaction/ExploredInteractable.h"
#include "WorldGen/ExploredPlaceholder.h"

#include "ExploredCoconutPalm.generated.h"

/**
 * Palma con cocos (GDD v3 §5.3): 8 palmas × 2 cocos, sin reaparición. Cada interacción (1 s)
 * baja un coco a la mochila; lo que queda se guarda por id en UExploredIslandStateSubsystem.
 * La simulación de crecimiento y caída de v1 (FCoconutPalmModel) no se usa en Baliza Siete: el
 * GDD fija los cocos de cada palma.
 */
UCLASS()
class EXPLORED_API AExploredCoconutPalm : public AExploredPlaceholder, public IExploredInteractable
{
	GENERATED_BODY()

public:
	static constexpr float PickSeconds = 1.0f;
	static constexpr int32 DefaultCoconuts = 2;

	UFUNCTION(BlueprintPure, Category = "Explored|Recolección")
	int32 GetCoconuts() const { return Coconuts; }

	void RefreshFromIslandState();

	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

private:
	void PickOne(AActor* InInstigator);

	UPROPERTY(VisibleAnywhere, Category = "Explored|Recolección")
	int32 Coconuts = DefaultCoconuts;

	int32 InitialCoconuts = DefaultCoconuts;
};
