#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "InteractionComponent.generated.h"

class UCameraComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFocusChanged, AActor*, NewFocus);

/**
 * Traza desde la cámara y mantiene el foco de interacción (GDD §12.2). Solo
 * este componente tiene tick habilitado (a baja frecuencia): el resto del
 * personaje sigue con PrimaryActorTick.bCanEverTick = false.
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Explored|Interacción")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	/**
	 * Componente e instancia exactos que golpeó la traza (relevante cuando el
	 * foco es un UHierarchicalInstancedStaticMeshComponent, p. ej. vegetación
	 * o rocas: muchas instancias comparten un único actor «celda»). Instancia
	 * -1 si el foco no viene de un HISM.
	 */
	UPrimitiveComponent* GetFocusedComponent() const { return FocusedComponent.Get(); }
	int32 GetFocusedInstanceIndex() const { return FocusedInstanceIndex; }

	UFUNCTION(BlueprintPure, Category = "Explored|Interacción")
	TArray<FText> GetContextVerbs() const { return CurrentVerbs; }

	/** true si el foco ofrece varios verbos y la tecla de combinar elige (IExploredInteractable::HasVerbChoice). */
	UFUNCTION(BlueprintPure, Category = "Explored|Interacción")
	bool FocusHasVerbChoice() const { return bCurrentHasChoice; }

	/** Pasa al siguiente verbo del foco (tecla de combinar). false si no hay foco o no hay elección. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Interacción")
	bool CycleFocusVerb();

	/** Llama a Interact en el actor enfocado, si puede. Devuelve false si no hay foco. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Interacción")
	bool InteractWithFocus();

	UPROPERTY(EditAnywhere, Category = "Explored|Interacción")
	float TraceDistanceCm = 250.0f;

	UPROPERTY(BlueprintAssignable, Category = "Explored|Interacción")
	FOnFocusChanged OnFocusChanged;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateFocus();
	/** Vuelve a pedir al foco sus verbos (como mucho 3) y si hay elección. */
	void RefreshVerbs();

	UPROPERTY()
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY()
	TWeakObjectPtr<AActor> FocusedActor;

	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> FocusedComponent;

	int32 FocusedInstanceIndex = INDEX_NONE;

	/** true si el último OnFocusChanged emitido fue con un actor (ver L5 en UpdateFocus). */
	bool bHasFocus = false;

	UPROPERTY()
	TArray<FText> CurrentVerbs;

	bool bCurrentHasChoice = false;
};
