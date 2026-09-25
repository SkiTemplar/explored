#include "Player/ExploredCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, FName Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	void MapKey(UInputMappingContext* Context, UInputAction* Action, FKey Key,
		bool bNegate = false, bool bSwizzleYXZ = false)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		UObject* Outer = Context;
		if (bSwizzleYXZ)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Outer);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Outer));
		}
	}
}

AExploredCharacter::AExploredCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(85.0f);

	bUseControllerRotationYaw = true;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->JumpZVelocity = 480.0f;
	Movement->AirControl = 0.3f;
	Movement->MaxFlySpeed = DebugFlySpeed;
	Movement->BrakingDecelerationFlying = 8000.0f;
	Movement->SetWalkableFloorAngle(50.0f);
}

void AExploredCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}
}

void AExploredCharacter::BuildInputAssets()
{
	if (MappingContext)
	{
		return;
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Explored"));
	MoveAction = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	SprintAction = MakeAction(this, TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	FlyAction = MakeAction(this, TEXT("IA_DebugFly"), EInputActionValueType::Boolean);
	VerticalAction = MakeAction(this, TEXT("IA_DebugVertical"), EInputActionValueType::Axis1D);

	// Movimiento: W/S en Y del vector (adelante), A/D en X (lateral).
	MapKey(MappingContext, MoveAction, EKeys::W, false, true);
	MapKey(MappingContext, MoveAction, EKeys::S, true, true);
	MapKey(MappingContext, MoveAction, EKeys::D);
	MapKey(MappingContext, MoveAction, EKeys::A, true);
	MapKey(MappingContext, MoveAction, EKeys::Gamepad_Left2D);

	MapKey(MappingContext, LookAction, EKeys::Mouse2D);
	MapKey(MappingContext, LookAction, EKeys::Gamepad_Right2D);

	MapKey(MappingContext, JumpAction, EKeys::SpaceBar);
	MapKey(MappingContext, JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	MapKey(MappingContext, SprintAction, EKeys::LeftShift);
	MapKey(MappingContext, SprintAction, EKeys::Gamepad_LeftThumbstick);

	MapKey(MappingContext, FlyAction, EKeys::F8);
	MapKey(MappingContext, VerticalAction, EKeys::E);
	MapKey(MappingContext, VerticalAction, EKeys::Q, true);
}

void AExploredCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInputAssets();

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AExploredCharacter::HandleMove);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AExploredCharacter::HandleLook);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleSprintStarted);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AExploredCharacter::HandleSprintCompleted);
	Input->BindAction(FlyAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleToggleFly);
	Input->BindAction(VerticalAction, ETriggerEvent::Triggered, this, &AExploredCharacter::HandleVertical);
}

void AExploredCharacter::HandleMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator Yaw(0.0f, GetControlRotation().Yaw, 0.0f);
	if (bIsDebugFlying)
	{
		AddMovementInput(GetControlRotation().Vector(), Axis.Y);
	}
	else
	{
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	}
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void AExploredCharacter::HandleLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void AExploredCharacter::HandleSprintStarted(const FInputActionValue&)
{
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void AExploredCharacter::HandleSprintCompleted(const FInputActionValue&)
{
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void AExploredCharacter::HandleToggleFly(const FInputActionValue&)
{
#if !UE_BUILD_SHIPPING
	bIsDebugFlying = !bIsDebugFlying;
	GetCharacterMovement()->SetMovementMode(bIsDebugFlying ? MOVE_Flying : MOVE_Falling);
	SetActorEnableCollision(!bIsDebugFlying);
#endif
}

void AExploredCharacter::HandleVertical(const FInputActionValue& Value)
{
	if (bIsDebugFlying)
	{
		AddMovementInput(FVector::UpVector, Value.Get<float>());
	}
}
