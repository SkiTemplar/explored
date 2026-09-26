#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "Carry/CarryTypes.h"

#include "ExploredCharacter.generated.h"

class UBuildPreviewComponent;
class UCameraComponent;
class UCarryComponent;
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

	UFUNCTION(BlueprintPure, Category = "Explored|Construcción")
	UBuildPreviewComponent* GetBuildPreviewComponent() const { return BuildPreview; }

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool IsBackpackOpen() const { return bBackpackOpen; }

	UPROPERTY(BlueprintAssignable, Category = "Explored|Carga")
	FOnBackpackToggled OnBackpackToggled;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void BuildInputAssets();
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
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

	void UseHand(EHand Hand);

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

	/** Modo construcción: fantasma, giro y confirmación (GDD §8.6). */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Construcción")
	TObjectPtr<UBuildPreviewComponent> BuildPreview;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

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

	bool bIsDebugFlying = false;
	bool bBackpackOpen = false;
	float HandSwayPhase = 0.0f;
	FVector HandRestLocationLeft = FVector::ZeroVector;
	FVector HandRestLocationRight = FVector::ZeroVector;
};
