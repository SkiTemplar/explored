#pragma once

#include "CoreMinimal.h"

#include "Interaction/ExploredInteractable.h"
#include "Narrative/BeaconProgressModel.h"
#include "WorldGen/ExploredPlaceholder.h"

#include "LighthouseLamp.generated.h"

class UPointLightComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * Linterna del faro (GDD v3 §5.6, recetas 9 y 10): tres ranuras (lente, los dos tarros de
 * aceite, mecha) y encender. Cada interacción llena la siguiente ranura con lo que lleva Iria;
 * con las tres llenas, «Encender la baliza» tarda 3 s y arranca el final (UBeaconSubsystem).
 * Encendida, un foco gira sobre el mar.
 *
 * Mallas: SM_FaroLinterna_01 (K2) y las piezas de las ranuras SM_Lente_01, SM_TarroAceite_01 y
 * SM_Mecha_01 (K3); mientras no estén importadas, primitivas del motor (ExploredAssetFallback).
 * Las ranuras se leen siempre del subsistema, así que reflejan también una partida cargada o
 * el comando BalizaStage.
 */
UCLASS()
class EXPLORED_API ALighthouseLamp : public AExploredPlaceholder, public IExploredInteractable
{
	GENERATED_BODY()

public:
	ALighthouseLamp();

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	/** Ranura que llenaría ahora la interacción (Count = ninguna: toca encender o ya está encendida). */
	ELanternSlot NextSlot() const;

	UFUNCTION(BlueprintPure, Category = "Explored|Baliza")
	bool IsLit() const { return bShowingLit; }

	/** Grados por segundo que gira el haz encendido. */
	UPROPERTY(EditAnywhere, Category = "Explored|Baliza")
	float BeamDegreesPerSecond = 36.0f;

	/** Segundos que tarda «Encender la baliza» (receta 10). */
	UPROPERTY(EditAnywhere, Category = "Explored|Baliza")
	float LightSeconds = 3.0f;

protected:
	virtual void BeginPlay() override;
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

private:
	/** Muestra las piezas de las ranuras y las luces según el progreso actual. */
	void RefreshFromProgress();
	UStaticMeshComponent* MakePart(const TCHAR* Name, const TCHAR* MeshPath, const TCHAR* FallbackPath,
		const FVector& LocationCm, const FVector& FallbackScale, int32 ColorIndex);

	UPROPERTY(VisibleAnywhere, Category = "Explored|Baliza")
	TObjectPtr<USpotLightComponent> Beam;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Baliza")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LensPart;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> OilParts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WickPart;

	float LampHeightCm = 250.0f;
	float LampRadiusCm = 120.0f;
	bool bShowingLit = false;
};
