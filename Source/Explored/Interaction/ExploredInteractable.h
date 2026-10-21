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
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Explored|Interacción")
	bool CanInteract(AActor* Instigator) const;
	virtual bool CanInteract_Implementation(AActor* Instigator) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Explored|Interacción")
	void Interact(AActor* Instigator);
	virtual void Interact_Implementation(AActor* Instigator);

	/**
	 * true si ahora hay más de un verbo para este jugador y la tecla de combinar elige entre ellos
	 * (GDD v3 §5.6). El HUD enseña entonces «E · verbo   C · siguiente».
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Explored|Interacción")
	bool HasVerbChoice(AActor* Instigator) const;
	virtual bool HasVerbChoice_Implementation(AActor* Instigator) const;

	/** Pasa al siguiente verbo válido; la próxima Interact lo ejecuta. false si no hay elección. */
	UFUNCTION(BlueprintNativeEvent, Category = "Explored|Interacción")
	bool CycleVerb(AActor* Instigator);
	virtual bool CycleVerb_Implementation(AActor* Instigator);
};

class UStaticMeshComponent;

namespace ExploredInteraction
{
	/**
	 * Añade al actor una caja que solo bloquea el trazado de visibilidad de UInteractionComponent,
	 * ajustada a los límites de la malla. Las mallas importadas sin colisión (props por script)
	 * serían invisibles para la interacción; la caja no bloquea al jugador ni a la física.
	 */
	EXPLORED_API void AddFocusBox(AActor& Owner, const UStaticMeshComponent& Mesh);
}
