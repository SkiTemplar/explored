#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "Carry/CarryTypes.h"

#include "ExploredCharacter.generated.h"

class UBuildPreviewComponent;
class UBodySignalsComponent;
class UCameraComponent;
class UCarryComponent;
class UCartographyComponent;
class UExploredInputSettingsSubsystem;
class UFishingComponent;
class UInputAction;
class UInputMappingContext;
class UInteractionComponent;
class USwimComponent;
class UStaticMeshComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBackpackToggled, bool, bOpen);

/**
 * Personaje en primera persona. Las acciones de Enhanced Input se construyen
 * por código para no depender de assets de datos.
 *
 * Manos, mochila y fabricación (M2, GDD §4.2/§4.5) viven en UCarryComponent y
 * UCraftingLibrary; este personaje solo traduce la entrada a esas llamadas.
 *
 * Ajustes del jugador (H7): FOV, sensibilidad de ratón y mando, invertir Y,
 * balanceo de cámara y agacharse mantenido/alterno se leen de
 * UExploredGameUserSettings (y se refrescan con OnSettingsApplied). Las teclas
 * de las acciones remapeables salen de UExploredInputSettingsSubsystem con la
 * tabla ExploredSettingsLogic::GetRemappableActions, y el contexto se
 * reconstruye al remapear.
 */
UCLASS()
class EXPLORED_API AExploredCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AExploredCharacter();

	UCameraComponent* GetCamera() const { return Camera; }

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	UCarryComponent* GetCarryComponent() const { return Carry; }

	UFUNCTION(BlueprintPure, Category = "Explored|Interacción")
	UInteractionComponent* GetInteractionComponent() const { return Interaction; }

	UFUNCTION(BlueprintPure, Category = "Explored|Nado")
	USwimComponent* GetSwimComponent() const { return Swim; }

	UFUNCTION(BlueprintPure, Category = "Explored|Mapa")
	UCartographyComponent* GetCartographyComponent() const { return Cartography; }
	UFUNCTION(BlueprintPure, Category = "Explored|Construcción")
	UBuildPreviewComponent* GetBuildPreviewComponent() const { return BuildPreview; }
	UFUNCTION(BlueprintPure, Category = "Explored|Cuerpo")
	UBodySignalsComponent* GetBodySignalsComponent() const { return Body; }
	UFUNCTION(BlueprintPure, Category = "Explored|Pesca")
	UFishingComponent* GetFishingComponent() const { return Fishing; }

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool IsBackpackOpen() const { return bBackpackOpen; }

	UPROPERTY(BlueprintAssignable, Category = "Explored|Carga")
	FOnBackpackToggled OnBackpackToggled;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	/** L8: el contexto de entrada se añade al cambiar de controlador, no solo en BeginPlay. */
	virtual void NotifyControllerChanged() override;

private:
	void BuildInputAssets();
	/** Rehace todos los mapeos del contexto con las teclas efectivas (remapeo incluido). */
	void RebuildKeyMappings();
	/** RebuildKeyMappings y pide a Enhanced Input que recalcule los mapeos activos. */
	void RefreshKeyMappings();
	UInputAction* FindActionByName(FName ActionName) const;
	UExploredInputSettingsSubsystem* GetInputSettings() const;
	/** Se suscribe a los ajustes y al remapeo del jugador local actual (idempotente). */
	void BindToPlayerSettings();
	void UnbindFromPlayerSettings();
	void HandleBindingsChanged(FName ActionName);
	/** Aplica FOV (y lo que no se lee en cada uso) desde UExploredGameUserSettings. */
	void ApplyPlayerSettings();
	void SetDebugMappingActive(bool bActive);
	void UpdateTickRate(bool bNeedsEveryFrame);

	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleLookGamepad(const FInputActionValue& Value);
	void ApplyLookInput(FVector2D Axis, float Sensitivity);
	void HandleSprintStarted(const FInputActionValue& Value);
	void HandleSprintCompleted(const FInputActionValue& Value);
	void HandleToggleFly(const FInputActionValue& Value);
	void HandleVertical(const FInputActionValue& Value);

	void HandleInteract(const FInputActionValue& Value);
	void HandleUsePrimary(const FInputActionValue& Value);
	void HandleUseSecondary(const FInputActionValue& Value);
	void HandleDrop(const FInputActionValue& Value);
	void HandleCombine(const FInputActionValue& Value);
	void HandleToggleBackpack(const FInputActionValue& Value);
	void HandleDiveStarted(const FInputActionValue& Value);
	void HandleDiveCompleted(const FInputActionValue& Value);
	void HandleWatchStarted(const FInputActionValue& Value);
	void HandleWatchCompleted(const FInputActionValue& Value);
	void HandleFish(const FInputActionValue& Value);
	void HandleReel(const FInputActionValue& Value);
	void HandleReelCompleted(const FInputActionValue& Value);

	void UseHand(EHand Hand);
	/** Andar o correr, por el multiplicador de carga de UCarryComponent. */
	void ApplyWalkSpeed();

	UFUNCTION()
	void RefreshHandMeshes();

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cámara")
	TObjectPtr<UCameraComponent> Camera;

	/** Mallas en primer plano de lo que se lleva en cada mano (GDD, punto 6 del encargo). */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Manos")
	TObjectPtr<UStaticMeshComponent> HandMeshLeft;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Manos")
	TObjectPtr<UStaticMeshComponent> HandMeshRight;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Carga")
	TObjectPtr<UCarryComponent> Carry;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Interacción")
	TObjectPtr<UInteractionComponent> Interaction;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Nado")
	TObjectPtr<USwimComponent> Swim;

	/** Mapa dibujado a mano (GDD §5). */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Mapa")
	TObjectPtr<UCartographyComponent> Cartography;
	/** Modo construcción: fantasma, giro y confirmación (GDD §8.6). */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Construcción")
	TObjectPtr<UBuildPreviewComponent> BuildPreview;
	/** Cuerpo como HUD: supervivencia, señales corporales y reloj de pulsera (GDD §8.3). */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Cuerpo")
	TObjectPtr<UBodySignalsComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Explored|Pesca")
	TObjectPtr<UFishingComponent> Fishing;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	/** Mirar con el stick derecho: acción aparte para aplicarle la sensibilidad del mando. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookGamepadAction;

	/** Contexto de depuración (vuelo): prioridad mayor que el principal y solo mientras se vuela. */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DebugMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlyAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> VerticalAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> UsePrimaryAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> UseSecondaryAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DropAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CombineAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ToggleBackpackAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DiveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> WatchAction;
	TObjectPtr<UInputAction> FishAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ReelAction;

	UPROPERTY(EditDefaultsOnly, Category = "Explored|Movimiento")
	float WalkSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Explored|Movimiento")
	float SprintSpeed = 750.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Explored|Movimiento")
	float DebugFlySpeed = 4000.0f;

	/** Amplitud del balanceo cosmético de las manos al andar (GDD, punto 6 del encargo). */
	UPROPERTY(EditDefaultsOnly, Category = "Explored|Manos")
	float HandSwayAmount = 1.2f;

	/** Amplitud de la brazada procedural al nadar; mucho mayor que el balanceo al andar. */
	UPROPERTY(EditDefaultsOnly, Category = "Explored|Manos")
	float SwimStrokeAmount = 14.0f;

	/** Amplitud vertical del balanceo de cámara al andar (Ajustes > Controles). */
	UPROPERTY(EditDefaultsOnly, Category = "Explored|Cámara")
	float CameraBobAmount = 1.2f;

	bool bIsDebugFlying = false;
	bool bSprintHeld = false;
	bool bBackpackOpen = false;
	bool bTickEveryFrame = false;
	float HandSwayPhase = 0.0f;
	FVector HandRestLocationLeft = FVector::ZeroVector;
	FVector HandRestLocationRight = FVector::ZeroVector;
	FVector CameraRestLocation = FVector::ZeroVector;

	/** Suscripciones a ajustes y remapeo; se retiran en EndPlay o al cambiar de controlador. */
	TWeakObjectPtr<UExploredInputSettingsSubsystem> BoundInputSettings;
	FDelegateHandle BindingsChangedHandle;
	FDelegateHandle SettingsAppliedHandle;
};
