#pragma once

#include "CoreMinimal.h"

#include "Interaction/ExploredInteractable.h"
#include "Weather/RainBarrelModel.h"
#include "WorldGen/ExploredPlaceholder.h"

#include "ExploredRainBarrel.generated.h"

/**
 * Barril de lluvia de la base del faro (GDD v3 §5.5): recoge la lluvia (45 s por carga, 4 como
 * mucho) y cada carga rellena la cantimplora a 4 sorbos. Muestrea el clima una vez por segundo
 * con un temporizador (sin Tick). Capa fina sobre FRainBarrelModel.
 *
 * Blueprint: «Get Charges», «Add Charges» (depuración y bot) y la interacción genérica (tecla E).
 */
UCLASS()
class EXPLORED_API AExploredRainBarrel : public AExploredPlaceholder, public IExploredInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Explored|Agua")
	int32 GetCharges() const { return State.Charges; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Agua")
	void AddCharges(int32 Charges);

	void RefreshFromIslandState();

	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	/** Segundos entre muestras del clima. */
	UPROPERTY(EditAnywhere, Category = "Explored|Agua")
	float RainSampleSeconds = 1.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

private:
	void SampleRain();
	void Store();

	FRainBarrelState State;
	FTimerHandle RainTimer;
};
