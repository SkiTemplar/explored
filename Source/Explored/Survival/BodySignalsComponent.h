#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPtr.h"

#include "Core/SystemLinks.h"
#include "Survival/BodyModel.h"
#include "Survival/BodySignals.h"
#include "Survival/InnerVoiceModel.h"
#include "Survival/SurvivalModel.h"

#include "BodySignalsComponent.generated.h"

class SExploredWristWatch;
class UAudioComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USoundBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBodySurvivalEvent, ESurvivalEvent);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBodyInnerVoice, EInnerVoiceLine);

/**
 * «El cuerpo como HUD» (GDD §8.3). Capa fina de Unreal sobre los modelos puros:
 *
 * - Lleva el FSurvivalState del jugador y lo avanza con FSurvivalModel::Tick en
 *   horas de juego (UTimeOfDaySubsystem), con el entorno leído del clima, el sol
 *   (traza hacia el Sol para la sombra), el agua (USwimComponent) y el movimiento.
 *   Nadie más tickea el modelo de supervivencia: si mañana existe un componente
 *   de supervivencia propio, este solo debería leer su estado.
 * - Detecta caídas (altura desde el punto más alto del salto) y aplica
 *   FBodyModel::ApplyFall.
 * - Traduce FBodySignalsModel en: parámetros de un material de postproceso
 *   (nombres en docs/tecnico/cuerpo.md) o, si aún no existe, en los ajustes de
 *   postproceso de la cámara; temblor de cámara y de manos (el personaje lee
 *   GetShiverRotation / GetHandTremorOffset); latido y respiración audibles.
 * - Muestra el reloj de pulsera (SExploredWristWatch) mientras se levanta la muñeca.
 *
 * Sin iconos: el jugador diagnostica por sensaciones.
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UBodySignalsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBodySignalsComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Estado y modo (C++) ------------------------------------------------------

	const FSurvivalState& GetSurvivalState() const { return State; }
	FSurvivalState& GetMutableSurvivalState() { return State; }
	const FBodySignals& GetSignals() const { return Signals; }
	const FSurvivalModeSettings& GetModeSettings() const { return ModeSettings; }
	void SetModeSettings(const FSurvivalModeSettings& InSettings) { ModeSettings = InSettings; }
	void SetMode(ESurvivalMode Mode) { ModeSettings = FSurvivalModeSettings::FromMode(Mode); }

	/**
	 * Lo que aportan otros sistemas al entorno del cuerpo (calor de los fuegos,
	 * carga, música...). Lo refresca UExploredWiringSubsystem y se mezcla con lo que
	 * mide el propio componente (ExploredLinks::ApplySurvivalLinks).
	 */
	void SetLinkInputs(const ExploredLinks::FSurvivalLinkInputs& InLinks) { Links = InLinks; }
	const FSurvivalInputs& GetInputs() const { return Inputs; }

	/** Estado de una partida guardada (sección «body»). */
	void RestoreSurvival(const FSurvivalState& InState, ESurvivalMode Mode);

	/** Reaparición (el modo de juego decide dónde): vivo, sin heridas, con el cuerpo tocado. */
	void ApplyRespawn();

	/** Ánimo que suman otros sistemas por su cuenta (la flauta junto al fuego), en puntos. */
	void AddMorale(float Points);

	/** Sucesos del cuerpo (intoxicación, infección, esguince…) para sonidos y textos. */
	FOnBodySurvivalEvent OnSurvivalEvent;

	/**
	 * Aviso interior que el personaje piensa ahora (biblia 01 §6.0; texto ES/EN en
	 * FInnerVoiceModel::Text). Como mucho uno por paso del cuerpo; la UI lo muestra como
	 * subtítulo opcional y el audio, si existe, como voz baja.
	 */
	FOnBodyInnerVoice OnInnerVoice;

	// --- Acciones del juego -------------------------------------------------------

	void Consume(const FConsumable& Item);
	void AddCut(float Depth);
	void TreatWounds(EWoundTreatment Treatment);
	void ApplySting(EStingKind Kind);

	/** Contacto con fuego, brasas o líquido hirviendo (biblia 01 §6.8). */
	void ApplyContactBurn();

	/** Usa una medicina de items.json (FMedicineModel); false si ese objeto no lo es. */
	bool ApplyMedicine(FName ItemId);
	void ApplyMoraleEvent(EMoraleEvent Event);

	/** Caída aplicada a mano (la detección automática ya llama a esto al aterrizar). */
	void ApplyFall(float HeightM, ELandingSurface Surface);

	UFUNCTION(BlueprintPure, Category = "Explored|Cuerpo")
	float GetHealth() const { return State.Health; }

	UFUNCTION(BlueprintPure, Category = "Explored|Cuerpo")
	bool IsDead() const { return State.IsDead(); }

	/** Torpeza actual (0–1) para fallos de manos y puntería. */
	UFUNCTION(BlueprintPure, Category = "Explored|Cuerpo")
	float GetClumsiness() const { return FBodyModel::Clumsiness(State); }

	// --- Salidas para el personaje --------------------------------------------------

	/** Desplazamiento de las manos en primer plano por temblor (cm, espacio de cámara). */
	FVector GetHandTremorOffset(bool bRightHand) const;

	/** Giro que se suma a la cámara por tiritona (grados). */
	FRotator GetShiverRotation() const;

	// --- Reloj de pulsera ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Explored|Cuerpo")
	void SetWristWatchRaised(bool bRaised) { bWristWatchRaised = bRaised; }

	UFUNCTION(BlueprintPure, Category = "Explored|Cuerpo")
	bool IsWristWatchRaised() const { return bWristWatchRaised; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Cuerpo")
	void SetShowWatchNeeds(bool bShow) { bShowWatchNeeds = bShow; }

	const FWristWatchReadout& GetWristWatchReadout() const { return WatchReadout; }

protected:
	/** Material de postproceso del cuerpo (ver docs/tecnico/cuerpo.md). Sin él se usan los ajustes de la cámara. */
	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo")
	TSoftObjectPtr<UMaterialInterface> BodyPostProcessMaterial;

	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo|Audio")
	TSoftObjectPtr<USoundBase> HeartbeatLoop;

	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo|Audio")
	TSoftObjectPtr<USoundBase> BreathSound;

	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo|Audio")
	TSoftObjectPtr<USoundBase> StomachGrowlSound;

	/** Grados máximos de tiritona en la cámara. */
	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo")
	float ShiverMaxDegrees = 0.8f;

	/** Centímetros máximos de temblor de las manos. */
	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo")
	float HandTremorMaxCm = 0.7f;

	/** Duración del día si no hay UTimeOfDaySubsystem (minutos reales). */
	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo")
	float FallbackDayLengthMinutes = 24.0f;

	/** Cada cuánto (s reales) se avanza el modelo de supervivencia. */
	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo")
	float SurvivalStepSeconds = 0.5f;

private:
	void StepSurvival(float RealSeconds);
	void SampleEnvironment();
	void UpdateEnergy(float DeltaSeconds);
	void UpdateFallDetection();
	void ApplyPostProcess();
	void ApplyAudio(float DeltaSeconds);
	void EnsureWristWatch();
	void RemoveWristWatch();
	void Broadcast(const TArray<ESurvivalEvent>& Events);
	/** Avisos interiores tras cualquier cambio del cuerpo (paso, caída, picadura…). */
	void SpeakInnerVoice(const TArray<ESurvivalEvent>& Events);
	float GameHoursPerSecond() const;
	EActivity CurrentActivity() const;
	UCameraComponent* FindCamera() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyPostProcessMID;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HeartbeatAudio;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedBreathSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedStomachSound;

	FSurvivalState State;
	FSurvivalModeSettings ModeSettings;
	FSurvivalInputs Inputs;
	ExploredLinks::FSurvivalLinkInputs Links;
	FBodySignals Signals;
	FBodySignals TargetSignals;
	FWristWatchReadout WatchReadout;
	FInnerVoiceState InnerVoice;
	TSharedPtr<SExploredWristWatch> WristWatchWidget;

	float StepAccumulator = 0.0f;
	float EnvironmentTimer = 0.0f;
	float BreathTimer = 0.0f;
	float TremorTime = 0.0f;
	float FallApexZ = 0.0f;
	bool bWasFalling = false;
	bool bFallFromFlight = false;
	bool bWristWatchRaised = false;
	bool bShowWatchNeeds = true;
};
