#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"

#include "TerrainToolComponent.generated.h"

class UCarryComponent;
class USoundBase;
struct FHitResult;

/** Uso de una herramienta de terreno (espejo de ETerrainToolAction para Blueprint y red). */
UENUM(BlueprintType)
enum class EExploredTerrainAction : uint8
{
	Pick UMETA(DisplayName = "Picar"),
	ShovelFlatten UMETA(DisplayName = "Aplanar con pala"),
	PlaceSoil UMETA(DisplayName = "Echar tierra"),
};

/** Lo que se oye y se ve de un uso (espejo de EMineHitCue sin el mellado). */
UENUM(BlueprintType)
enum class EExploredTerrainCue : uint8
{
	Rebound UMETA(DisplayName = "Rebote"),
	Miss UMETA(DisplayName = "Nada"),
	Hit UMETA(DisplayName = "Golpe"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTerrainToolCue, EExploredTerrainAction, Action, EExploredTerrainCue, Cue,
	FVector, LocationCm);

/**
 * Pico y pala sobre el terreno volumétrico (GDD v2 §3.4), en el personaje.
 *
 * Cliente que controla al personaje: traza desde la cámara (3 m), predice el sonido y las
 * partículas (`OnToolCue`) y pide el uso al servidor. Servidor: valida alcance, cadencia y
 * superficie (`FTerrainToolModel::Validate`), edita con `UTerrainEditSubsystem` y lleva la
 * tierra transportada (replicada solo al dueño). Los demás jugadores oyen el golpe por
 * una multidifusión no fiable; el hueco les llega por la cola de terreno.
 *
 * Nodos de Blueprint: «Try Use Held Tool» (el clic de la mano), «Get Carried Soil M3» y el
 * evento «On Tool Cue» para enganchar partículas y sonidos por material.
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UTerrainToolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerrainToolComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Usa la herramienta que lleva el personaje en las manos. Principal: la mano derecha.
	 * Secundario: la izquierda si lleva herramienta; si no, la de la derecha en su uso
	 * secundario (pala: echar tierra). Devuelve true si el clic era de una herramienta de
	 * terreno (aunque no haya golpeado nada): el llamante no debe usar la mano para otra cosa.
	 */
	bool TryUseFromHands(const UCarryComponent* Carry, bool bSecondary);

	/** Usa el objeto como herramienta de terreno. false si no es pico ni pala. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Terreno")
	bool TryUseHeldTool(FName ItemId, bool bSecondary);

	/** Tierra que lleva el jugador (m³), para rellenar con la pala. */
	UFUNCTION(BlueprintPure, Category = "Explored|Terreno")
	float GetCarriedSoilM3() const { return CarriedSoilM3; }

	/** Cada uso, al pulsar (predicción local) y en los demás jugadores. */
	UPROPERTY(BlueprintAssignable, Category = "Explored|Terreno")
	FOnTerrainToolCue OnToolCue;

	UPROPERTY(EditAnywhere, Category = "Explored|Terreno")
	TObjectPtr<USoundBase> HitSound;

	UPROPERTY(EditAnywhere, Category = "Explored|Terreno")
	TObjectPtr<USoundBase> ReboundSound;

protected:
	UFUNCTION(Server, Reliable)
	void ServerUseTool(EExploredTerrainAction Action, uint8 Tool, FVector_NetQuantize10 ImpactCm);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastToolCue(EExploredTerrainAction Action, EExploredTerrainCue Cue, FVector_NetQuantize10 LocationCm);

private:
	bool TraceTerrain(FHitResult& OutHit) const;
	void PlayCue(EExploredTerrainAction Action, EExploredTerrainCue Cue, const FVector& LocationCm);
	/** Aplica un uso ya validado en el servidor; devuelve lo que ha pasado. */
	EExploredTerrainCue ApplyOnServer(EExploredTerrainAction Action, uint8 Tool, const FVector& ImpactMeters);

	UPROPERTY(Replicated)
	float CarriedSoilM3 = 0.0f;

	/** Último uso local (cliente) y último aceptado (servidor), en segundos del mundo. */
	double LastLocalUseSeconds = -1.0e9;
	double LastServerUseSeconds = -1.0e9;
};
