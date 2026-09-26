#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "ExploredInteractable.generated.h"

UINTERFACE(BlueprintType)
class EXPLORED_API UExploredInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Cualquier actor que UInteractionComponent pueda enfocar y con el que se
 * pueda interactuar (coger, usar, abrir...). GDD §12.2 (módulo Interaction).
 */
class EXPLORED_API IExploredInteractable
{
	GENERATED_BODY()

public:
	/** Como máximo 3 verbos contextuales (biblia §8.3), en español, para el HUD. */
	UFUNCTION(BlueprintNativeEvent, Category = "Explored|Interacción")
	void GetContextVerbs(TArray<FText>& OutVerbs) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Explored|Interacción")
	bool CanInteract(AActor* Instigator) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Explored|Interacción")
	void Interact(AActor* Instigator);
};
