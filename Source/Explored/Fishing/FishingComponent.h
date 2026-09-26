#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Misc/Optional.h"

#include "Fishing/FishingModel.h"
#include "Fishing/FishingTension.h"

#include "FishingComponent.generated.h"

class AExploredItemActor;
class AExploredTrap;
class UExploredFishingSubsystem;

/** Fase de la sesión con la caña. */
UENUM(BlueprintType)
enum class EFishingSessionState : uint8
{
	Idle,
	Waiting,
	Fighting,
};

/** Cómo acabó una sesión (para sonidos, textos y logros). */
UENUM(BlueprintType)
enum class EFishingResult : uint8
{
	Caught,
	Escaped,
	LineSnapped,
	NoBite,
	Cancelled,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFishingFinished, EFishingResult, Result, FName, CatchId);

/**
 * Pesca con caña del personaje (GDD §8.9). Capa fina sobre FFishingModel y
 * FFishFight: lanza hacia donde mira la cámara, espera la picada que decide
 * el modelo, reenvía la entrada de recoger/soltar al minijuego de tensión y,
 * si sale bien, suelta la captura en el mundo como AExploredItemActor.
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UFishingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFishingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Lanza el anzuelo al agua a la que apunta la cámara. False si no hay agua a tiro. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Pesca")
	bool StartCast();

	/** Recoge el sedal y termina la sesión. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Pesca")
	void Cancel();

	/** Entrada del minijuego: 1 recoger, 0 aguantar, -1 soltar sedal. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Pesca")
	void SetReelInput(float Input) { ReelInput = FMath::Clamp(Input, -1.0f, 1.0f); }

	/** Cebo puesto en el anzuelo (id de items.json; vacío = sin cebo). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Pesca")
	void SetBaitItem(FName ItemId) { Bait = FFishingModel::BaitFromItemId(ItemId); }

	/** Sitio con nombre en el que se está (lo fijan volúmenes del mundo; legendarias, biblia §4.6). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Pesca")
	void SetSpotTag(FName Tag) { SpotTag = Tag; }

	/** Coloca una trampa («nasa», «trampa_cangrejos», «corral_piedras») donde mira el jugador. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Pesca")
	AExploredTrap* PlaceTrap(FName KindName, FName BaitItemId);

	UFUNCTION(BlueprintPure, Category = "Explored|Pesca")
	EFishingSessionState GetSessionState() const { return SessionState; }

	/** Tensión del sedal (0–1, 1 = se parte) para el HUD. */
	UFUNCTION(BlueprintPure, Category = "Explored|Pesca")
	float GetTension01() const;

	/** Metros de sedal fuera. */
	UFUNCTION(BlueprintPure, Category = "Explored|Pesca")
	float GetLineOutM() const;

	void SetTackle(const FFishingTackle& InTackle) { Tackle = InTackle; }
	const FFishingTackle& GetTackle() const { return Tackle; }

	/** Suelta un objeto de captura en el mundo (lo usan también las trampas). */
	static AExploredItemActor* SpawnCatchActor(UWorld* World, FName ItemId, const FVector& Location);

	UPROPERTY(BlueprintAssignable, Category = "Explored|Pesca")
	FOnFishingFinished OnFishingFinished;

protected:
	virtual void BeginPlay() override;

private:
	UExploredFishingSubsystem* GetFishing() const;
	bool TraceWaterPoint(FVector& OutSurfacePoint) const;
	float ComputeNoise() const;
	void Finish(EFishingResult Result, FName CatchId);
	void HandleCatch();

	/** Alcance máximo del lance (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Explored|Pesca")
	float MaxCastDistanceCm = 2500.0f;

	/** Si no pica en este tiempo (s reales), se recoge solo. */
	UPROPERTY(EditDefaultsOnly, Category = "Explored|Pesca")
	float MaxBiteWaitSeconds = 90.0f;

	/** Peso cómodo (kg) para el ruido por carga; el mismo que usa USwimComponent. */
	UPROPERTY(EditDefaultsOnly, Category = "Explored|Pesca")
	float ComfortableWeightKg = 15.0f;

	/** Velocidad (cm/s) que cuenta como moverse del todo al calcular el ruido. */
	UPROPERTY(EditDefaultsOnly, Category = "Explored|Pesca")
	float NoisySpeedCmS = 450.0f;

	FFishingTackle Tackle;
	EFishBait Bait = EFishBait::None;
	FName SpotTag;

	EFishingSessionState SessionState = EFishingSessionState::Idle;
	FVector CastPoint = FVector::ZeroVector;
	float BiteTimer = 0.0f;
	bool bHasBite = false;
	FFishBite PendingBite;
	TOptional<FFishFight> Fight;
	float ReelInput = 0.0f;
};
