#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Carry/CarryTypes.h"
#include "Cooking/CookingModel.h"
#include "Cooking/FireModel.h"
#include "Interaction/ExploredInteractable.h"
#include "Items/ItemTypes.h"

#include "ExploredFire.generated.h"

class UCarryComponent;
class UParticleSystemComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Fogata, hoguera u horno en el mundo (GDD §8.5, §8.8; biblia §5.3). Solo
 * conecta FFireModel y FCookingModel con el mundo: toda la lógica y el
 * equilibrio están en los modelos puros y en fuels.json / recipes.json.
 *
 * Interacción (la mano derecha primero, luego la izquierda):
 * - Con la comida hecha o quemada: se retira junto con el utensilio.
 * - Con cerillas, pedernal o una rama seca y el fuego apagado: se intenta encender.
 * - Con combustible: se echa al fuego (sobre brasas lo reaviva).
 * - Con una olla de coco o una vasija: se pone al fuego.
 * - Con un ingrediente: se asa en el espeto o se echa a la olla. Al echar
 *   otro ingrediente la cocción vuelve a empezar con todo lo que hay dentro.
 *
 * Luz y partículas son marcadores sin recursos propios: las plantillas de
 * partículas se asignan en el editor cuando existan.
 */
UCLASS()
class EXPLORED_API AExploredFire : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredFire();

	virtual void Tick(float DeltaSeconds) override;

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	bool IsBurning() const { return State.Status == EFireStatus::Burning; }

	/** Encendida o en brasas: punto de reaparición (GDD §8.6). */
	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	bool IsRespawnPoint() const { return FFireModel::IsRespawnPoint(State); }

	/** Hoguera con humo verde: la ve el barco del horizonte (P-EVENTS). */
	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	bool IsSignalFire() const { return FFireModel::IsSignalFire(State); }

	/** Calor (0–1) que llega a un punto del mundo; lo usará FSurvivalInputs::FireHeat. */
	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	float GetHeatAt(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Fuego")
	bool IsSheltered() const { return bSheltered; }

	/** Mejora el hogar (fogata → hoguera → horno); lo llamará la construcción (P-BUILD). */
	bool UpgradeLevel(EFireLevel NewLevel);

	/** Estado plano para el guardado (P-SAVE). */
	const FFireState& GetFireState() const { return State; }
	const FCookingPot& GetPot() const { return Pot; }
	void RestoreState(const FFireState& InState, const FCookingPot& InPot);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Fuego")
	TObjectPtr<UStaticMeshComponent> Hearth;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Fuego")
	TObjectPtr<UPointLightComponent> Light;

	/** Marcador de llamas: sin plantilla hasta que exista el efecto. */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Fuego")
	TObjectPtr<UParticleSystemComponent> Flames;

	/** Marcador de humo (más denso con leña verde o mojada). */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Fuego")
	TObjectPtr<UParticleSystemComponent> SmokeFx;

	/** 0 fogata, 1 hoguera, 2 horno de arcilla. */
	UPROPERTY(EditAnywhere, Category = "Explored|Fuego", meta = (ClampMin = "0", ClampMax = "2"))
	int32 InitialLevel = 0;

	UPROPERTY(EditAnywhere, Category = "Explored|Fuego")
	float MaxLightIntensity = 6000.0f;

	/** Altura (m) de la traza hacia arriba que decide si hay techo. */
	UPROPERTY(EditAnywhere, Category = "Explored|Fuego")
	float ShelterTraceMeters = 6.0f;

	/** Cada cuánto (s reales) se simula el modelo. */
	UPROPERTY(EditAnywhere, Category = "Explored|Fuego")
	float SimulationIntervalSeconds = 0.5f;

	/** Horas de juego por segundo real si no hay reloj del mundo (mapas de prueba). */
	UPROPERTY(EditAnywhere, Category = "Explored|Fuego")
	float FallbackGameHoursPerSecond = 1.0f / 60.0f;

private:
	void Simulate(float GameHours);
	float ConsumeGameHours(float DeltaSeconds);
	void UpdateShelter();
	FFireEnvironment MakeFireEnvironment() const;
	FCookEnvironment MakeCookEnvironment() const;
	void ReportEvents(const TArray<EFireEvent>& Events) const;
	void RefreshVisuals();

	bool TryIgniteWith(UCarryComponent& Carry, EHand Hand, const FIgnitionDef& Ignition);
	bool TryAddFuel(UCarryComponent& Carry, EHand Hand, FName ItemId);
	bool TryPlaceVessel(UCarryComponent& Carry, EHand Hand, FName VesselId);
	bool TryAddIngredient(UCarryComponent& Carry, EHand Hand, FName ItemId);
	void CollectPot();
	void SpawnNearby(const FItemInstance& Instance, int32 Slot);
	TArray<FCookIngredient> BuildIngredients(const TArray<FName>& Ids) const;

	FFireState State;
	FCookingPot Pot;

	/** Utensilio puesto al fuego (vacío = se asa en el espeto del propio fuego). */
	FName PotVesselId;
	UPROPERTY()
	FItemInstance PotVesselItem;

	bool bSheltered = false;
	float ShelterTimer = 0.0f;
	float SimulationTimer = 0.0f;
	float LastTotalDays = -1.0f;
};
