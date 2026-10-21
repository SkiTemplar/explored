#pragma once

#include "CoreMinimal.h"

#include "Interaction/ExploredInteractable.h"
#include "Narrative/BeaconProgressModel.h"
#include "WorldGen/ExploredPlaceholder.h"

#include "LighthouseLamp.generated.h"

class UAudioComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProceduralMeshComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * Linterna del faro (GDD v3 §5.6, recetas 9 y 10): tres ranuras (lente, los dos tarros de
 * aceite, mecha) y encender. Cada interacción llena la siguiente ranura con lo que lleva Iria;
 * con las tres llenas, «Encender la baliza» tarda 3 s y arranca el final (UBeaconSubsystem).
 * Durante esos 3 s la llama prende y crece (luz que parpadea y sube); encendida, un foco y un
 * cono visible (M_BeaconBeam) giran sobre el mar con el sonido de la lente.
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

	/** Largo (m) y radio final (m) del cono visible del haz. */
	UPROPERTY(EditAnywhere, Category = "Explored|Baliza")
	float BeamConeLengthM = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Baliza")
	float BeamConeRadiusM = 11.0f;

	/** Capturas: prende la llama y enciende la baliza a los LightSeconds, sin superviviente. */
	void DebugIgnite();

	/** Dirección del haz en el mundo (eje del foco). */
	FVector GetBeamDirection() const;

	/** Instante (s de mundo) en que empezó a prender; negativo si no está prendiendo. */
	float GetIgniteStartedAt() const { return IgniteStartedAt; }

protected:
	virtual void BeginPlay() override;
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

private:
	/** Muestra las piezas de las ranuras y las luces según el progreso actual. */
	void RefreshFromProgress();
	UStaticMeshComponent* MakePart(const TCHAR* Name, const TCHAR* MeshPath, const TCHAR* FallbackPath,
		const FVector& LocationCm, const FVector& FallbackScale, int32 ColorIndex);
	/** Cono del haz (malla procedural abierta, punta en la lente, con M_BeaconBeam), hijo del foco. */
	void BuildBeamCone();
	/** Luz de la llama mientras prende y al quedar encendida (parpadeo suave). */
	void UpdateFlame(float Now);
	void StartLensSound();

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

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> BeamCone;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamMaterial;

	/** M_Toon de la linterna con el cristal encendido (GlowCell/GlowStrength) mientras arde la llama. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlassMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LensSound;

	float IgniteStartedAt = -1.0f;
	float LitAt = -1.0f;

	float LampHeightCm = 250.0f;
	float LampRadiusCm = 120.0f;
	bool bShowingLit = false;
};
