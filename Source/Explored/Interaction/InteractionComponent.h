#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "InteractionComponent.generated.h"

class UCameraComponent;

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

	UFUNCTION(BlueprintPure, Category = "Explored|Interacción")
	TArray<FText> GetContextVerbs() const { return CurrentVerbs; }

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

	UPROPERTY()
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY()
	TWeakObjectPtr<AActor> FocusedActor;

	/** true si el último OnFocusChanged emitido fue con un actor (ver L5 en UpdateFocus). */
	bool bHasFocus = false;

	UPROPERTY()
	TArray<FText> CurrentVerbs;
};
