#pragma once

#include "CoreMinimal.h"

#include "Cooking/CampfireModel.h"
#include "Interaction/ExploredInteractable.h"
#include "WorldGen/ExploredPlaceholder.h"

#include "ExploredFire.generated.h"

class UPointLightComponent;
class UExploredSurvivorComponent;

/**
 * Hoguera de Baliza Siete (GDD v3 §5.2): la fija del refugio de la base del faro (hoguera_fija de
 * island_props.json) o una encendida en el suelo con la receta «hoguera». Capa fina sobre
 * FCampfireModel: arde 5 min, cada leña suma 1,7 min, calienta a 4 m, asa un pez en 20 s y hierve
 * aceite en 40 s (2 peces crudos + coco vacío); solo el ciclón la apaga si no está bajo techo.
 *
 * Acciones (FFireActionsModel, en este orden): «recoger» lo cocinado, «encender» (3 leña + cuchillo),
 * «cocinar» un pez, «hervir» aceite, «avivar» con leña o un tarro de aceite. La tecla E ejecuta el
 * verbo elegido y la de combinar (C) pasa al siguiente (FVerbChoiceModel, GDD §5.6).
 *
 * Blueprint: «Ignite», «Perform Action», «Is Burning», «Get Burn Seconds», «Get Heat At».
 */
UCLASS()
class EXPLORED_API AExploredFire : public AExploredPlaceholder, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredFire();

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;
	virtual bool HasVerbChoice_Implementation(AActor* InInstigator) const override;
	virtual bool CycleVerb_Implementation(AActor* InInstigator) override;

	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	bool IsBurning() const { return State.bLit; }

	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	float GetBurnSeconds() const { return State.BurnSeconds; }

	/** true si hay algo cocinándose o listo para recoger. */
	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	bool HasJob() const { return State.Job.IsActive(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	bool IsJobReady() const { return State.Job.IsDone(); }

	/** Calor 0-1 en un punto (1 a 4 m o menos). Lo suma el cuerpo con los demás fuegos. */
	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	float GetHeatAt(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	bool IsSheltered() const { return bSheltered; }

	/** En Baliza Siete se reaparece en los refugios (GDD §5.1), no junto al fuego. */
	bool IsRespawnPoint() const { return false; }

	/** Enciende sin gastar nada (receta ya pagada, depuración o bot). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Fuego")
	void Ignite();

	/** Acciones válidas ahora para ese jugador, en orden de preferencia. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Fuego")
	TArray<FName> GetAvailableActions(AActor* InInstigator) const;

	/** Lo que haría ahora la tecla de interactuar: el verbo elegido si vale; si no, el primero. */
	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	FName GetChosenAction(AActor* InInstigator) const;

	/** «recoger», «encender», «cocinar», «hervir» o «avivar». false si no se pudo. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Fuego")
	bool PerformAction(AActor* InInstigator, FName Action);

	const FCampfireState& GetCampfireState() const { return State; }
	void RestoreCampfire(const FCampfireState& InState);

	/** Segundos entre pasos de simulación. */
	UPROPERTY(EditAnywhere, Category = "Explored|Fuego")
	float SimulationIntervalSeconds = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Explored|Fuego")
	float MaxLightIntensity = 6000.0f;

protected:
	virtual void BeginPlay() override;
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Fuego")
	TObjectPtr<UPointLightComponent> Light;

private:
	bool StartCooking(UExploredSurvivorComponent& Survivor, FName RecipeId);
	bool LightWithRecipe(UExploredSurvivorComponent& Survivor);
	bool Feed(UExploredSurvivorComponent& Survivor, FName FuelItem);
	/** Tarros de aceite de la mochila que la linterna ya no necesita (los únicos que se pueden quemar). */
	int32 SpareOilJars(const UExploredSurvivorComponent& Survivor) const;
	bool Collect(UExploredSurvivorComponent& Survivor);
	void UpdateShelter();
	void Refresh();

	FCampfireState State;
	/** Verbo elegido con la tecla de combinar (se conserva entre usos). */
	FName SelectedAction;
	bool bSheltered = false;
	bool bConfigured = false;
	float SimulationTimer = 0.0f;
};
