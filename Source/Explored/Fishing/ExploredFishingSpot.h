#pragma once

#include "CoreMinimal.h"

#include "Fishing/CastFishingModel.h"
#include "Interaction/ExploredInteractable.h"
#include "WorldGen/ExploredPlaceholder.h"

#include "ExploredFishingSpot.generated.h"

class UExploredSurvivorComponent;

/**
 * Punto de pesca de las Rocas del Pescador (GDD v3 §5.4): con la caña de pescar en la mochila,
 * E lanza; al morder hay 3 s para volver a pulsar E y sacar un pez crudo; si no, se escapa. Más
 * picadas al amanecer y al atardecer; de noche y con el ciclón no se pesca. Capa fina sobre
 * FCastFishingModel; solo hace Tick mientras el sedal está en el agua.
 *
 * Blueprint: «Get Phase» para el HUD y la interacción genérica (tecla E).
 */
UCLASS()
class EXPLORED_API AExploredFishingSpot : public AExploredPlaceholder, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredFishingSpot();

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	/** 0 reposo, 1 esperando, 2 mordida (ECastFishingPhase). */
	UFUNCTION(BlueprintPure, Category = "Explored|Pesca")
	int32 GetPhase() const { return static_cast<int32>(State.Phase); }

	/** Peces sacados en este punto (depuración y bot). */
	UFUNCTION(BlueprintPure, Category = "Explored|Pesca")
	int32 GetCatches() const { return Catches; }

protected:
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

private:
	void Report(ECastFishingEvent Event, UExploredSurvivorComponent* Survivor);
	void SetFishing(bool bActive);

	FCastFishingState State;
	TWeakObjectPtr<AActor> Angler;
	int32 Catches = 0;
};
