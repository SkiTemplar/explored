#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "ExploredCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Personaje en primera persona. Las acciones de Enhanced Input se construyen
 * por código para no depender de assets de datos.
 */
UCLASS()
class EXPLORED_API AExploredCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AExploredCharacter();

	UCameraComponent* GetCamera() const { return Camera; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void BuildInputAssets();
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleSprintStarted(const FInputActionValue& Value);
	void HandleSprintCompleted(const FInputActionValue& Value);
	void HandleToggleFly(const FInputActionValue& Value);
	void HandleVertical(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cámara")
	TObjectPtr<UCameraComponent> Camera;

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

	UPROPERTY(EditDefaultsOnly, Category = "Explored|Movimiento")
	float WalkSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Explored|Movimiento")
	float SprintSpeed = 750.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Explored|Movimiento")
	float DebugFlySpeed = 4000.0f;

	bool bIsDebugFlying = false;
};
