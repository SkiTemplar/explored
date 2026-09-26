#include "Player/ExploredCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

#include "Carry/CarryComponent.h"
#include "Crafting/CraftingLibrary.h"
#include "Interaction/InteractionComponent.h"
#include "Items/ExploredItemActor.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Player/SwimComponent.h"

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

namespace ExploredCharacterDetail
{
	// En tierra basta con 30 Hz para el balanceo de las manos; nadando la cámara
	// se orienta a mano en Tick y tiene que ir a la frecuencia de pantalla (M15).
	constexpr float LandTickInterval = 1.0f / 30.0f;
	constexpr float SwimTickInterval = 0.0f;
}

AExploredCharacter::AExploredCharacter()
{
	// Excepción deliberada: el balanceo de las manos (punto 6 del encargo de
	// M2) necesita un seno por fotograma. Es la única razón para tener tick
	// en el personaje; todo lo demás sigue dirigido por eventos y delegados.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = ExploredCharacterDetail::LandTickInterval;

	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(85.0f);

	HandRestLocationLeft = FVector(30.0f, -12.0f, -10.0f);
	HandRestLocationRight = FVector(30.0f, 12.0f, -10.0f);

	HandMeshLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandMeshLeft"));
	HandMeshLeft->SetupAttachment(Camera);
	HandMeshLeft->SetRelativeLocation(HandRestLocationLeft);
	HandMeshLeft->SetRelativeScale3D(FVector(0.3f));
	HandMeshLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandMeshLeft->SetVisibility(false);
	HandMeshLeft->SetCastShadow(false);

	HandMeshRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandMeshRight"));
	HandMeshRight->SetupAttachment(Camera);
	HandMeshRight->SetRelativeLocation(HandRestLocationRight);
	HandMeshRight->SetRelativeScale3D(FVector(0.3f));
	HandMeshRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandMeshRight->SetVisibility(false);
	HandMeshRight->SetCastShadow(false);

	Carry = CreateDefaultSubobject<UCarryComponent>(TEXT("Carry"));
	Interaction = CreateDefaultSubobject<UInteractionComponent>(TEXT("Interaction"));
	Swim = CreateDefaultSubobject<USwimComponent>(TEXT("Swim"));

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

	if (Carry)
	{
		Carry->OnCarryChanged.AddDynamic(this, &AExploredCharacter::RefreshHandMeshes);
	}
}

void AExploredCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const EWaterState WaterState = Swim ? Swim->GetWaterState() : EWaterState::OnLand;
	const bool bSwimming = WaterState == EWaterState::Swimming || WaterState == EWaterState::Diving;

	// Nadando, la rotación de la cámara se fija aquí cada fotograma: a 30 Hz se nota
	// a tirones con FPS altos (M15). Se cambia solo en la transición.
	const float DesiredTickInterval = bSwimming
		? ExploredCharacterDetail::SwimTickInterval
		: ExploredCharacterDetail::LandTickInterval;
	if (!FMath::IsNearlyEqual(GetActorTickInterval(), DesiredTickInterval))
	{
		SetActorTickInterval(DesiredTickInterval);
	}

	if (bSwimming)
	{
		// Brazadas: la fase la lleva USwimComponent (avanza con la velocidad de nado);
		// aquí solo se traduce en el vaivén de las manos, mucho más amplio que al andar.
		const float Phase = Swim->GetStrokePhase() * UE_TWO_PI;
		const float SwingL = FMath::Sin(Phase) * SwimStrokeAmount;
		const float SwingR = FMath::Sin(Phase + UE_PI) * SwimStrokeAmount;
		if (HandMeshLeft)
		{
			HandMeshLeft->SetRelativeLocation(HandRestLocationLeft + FVector(SwingL * 0.6f, 0.0f, SwingL));
		}
		if (HandMeshRight)
		{
			HandMeshRight->SetRelativeLocation(HandRestLocationRight + FVector(SwingR * 0.6f, 0.0f, SwingR));
		}

		// La cámara deja de seguir solo el control del jugador: se le suma el balanceo de la ola.
		Camera->bUsePawnControlRotation = false;
		const FRotator ControlRot = GetControlRotation();
		const FRotator Tilt = Swim->GetWaveTilt();
		Camera->SetWorldRotation(FRotator(ControlRot.Pitch + Tilt.Pitch, ControlRot.Yaw, Tilt.Roll));
		return;
	}
	Camera->bUsePawnControlRotation = true;

	const float SpeedRatio = FMath::Clamp(GetVelocity().Size2D() / FMath::Max(WalkSpeed, 1.0f), 0.0f, 1.0f);
	if (SpeedRatio > KINDA_SMALL_NUMBER)
	{
		HandSwayPhase += DeltaSeconds * SpeedRatio * 8.0f;
	}
	const float Offset = FMath::Sin(HandSwayPhase) * HandSwayAmount * SpeedRatio;
	if (HandMeshLeft)
	{
		HandMeshLeft->SetRelativeLocation(HandRestLocationLeft + FVector(0.0f, 0.0f, Offset));
	}
	if (HandMeshRight)
	{
		HandMeshRight->SetRelativeLocation(HandRestLocationRight + FVector(0.0f, 0.0f, -Offset));
	}
}

void AExploredCharacter::RefreshHandMeshes()
{
	const UItemRegistrySubsystem* Registry = UItemRegistrySubsystem::Resolve(this);
	if (!Registry || !Carry)
	{
		return;
	}

	auto UpdateOne = [Registry](UStaticMeshComponent* MeshComp, UCarryComponent* CarryComp, EHand Hand)
	{
		const FItemInstance* Item = CarryComp->GetHandItemPtr(Hand);
		if (!Item)
		{
			MeshComp->SetVisibility(false);
			return;
		}
		FItemDefinition Definition;
		if (Registry->FindDefinition(Item->DefinitionId, Definition))
		{
			if (UObject* Loaded = Definition.MeshPath.TryLoad())
			{
				if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Loaded))
				{
					MeshComp->SetStaticMesh(StaticMesh);
				}
			}
		}
		MeshComp->SetVisibility(true);
	};

	UpdateOne(HandMeshLeft, Carry, EHand::Left);
	UpdateOne(HandMeshRight, Carry, EHand::Right);
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

	InteractAction = MakeAction(this, TEXT("IA_Interact"), EInputActionValueType::Boolean);
	UsePrimaryAction = MakeAction(this, TEXT("IA_UsePrimary"), EInputActionValueType::Boolean);
	UseSecondaryAction = MakeAction(this, TEXT("IA_UseSecondary"), EInputActionValueType::Boolean);
	DropAction = MakeAction(this, TEXT("IA_Drop"), EInputActionValueType::Boolean);
	CombineAction = MakeAction(this, TEXT("IA_Combine"), EInputActionValueType::Boolean);
	ToggleBackpackAction = MakeAction(this, TEXT("IA_ToggleBackpack"), EInputActionValueType::Boolean);
	DiveAction = MakeAction(this, TEXT("IA_Dive"), EInputActionValueType::Boolean);

	// E también sube en vuelo de depuración (VerticalAction); en juego normal
	// solo importa como interacción, así que conviven en la misma tecla.
	MapKey(MappingContext, InteractAction, EKeys::E);
	MapKey(MappingContext, UsePrimaryAction, EKeys::LeftMouseButton);
	MapKey(MappingContext, UseSecondaryAction, EKeys::RightMouseButton);
	MapKey(MappingContext, DropAction, EKeys::G);
	MapKey(MappingContext, CombineAction, EKeys::C);
	MapKey(MappingContext, ToggleBackpackAction, EKeys::Tab);
	// Bucear: mantener para bajar; soltar deja que el pulmón empuje de vuelta a la superficie.
	MapKey(MappingContext, DiveAction, EKeys::LeftControl);
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

	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleInteract);
	Input->BindAction(UsePrimaryAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleUsePrimary);
	Input->BindAction(UseSecondaryAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleUseSecondary);
	Input->BindAction(DropAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleDrop);
	Input->BindAction(CombineAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleCombine);
	Input->BindAction(ToggleBackpackAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleToggleBackpack);
	Input->BindAction(DiveAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleDiveStarted);
	Input->BindAction(DiveAction, ETriggerEvent::Completed, this, &AExploredCharacter::HandleDiveCompleted);
}

void AExploredCharacter::HandleMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator Yaw(0.0f, GetControlRotation().Yaw, 0.0f);
	// Buceando (sumergido de verdad, no solo a flote) se nada libremente hacia
	// donde mira la cámara, como en vuelo de depuración; a flote W/S siguen
	// moviendo por el plano horizontal, igual que andando.
	const bool bFreeSwim = Swim && Swim->GetWaterState() == EWaterState::Diving;
	if (bIsDebugFlying || bFreeSwim)
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

void AExploredCharacter::HandleInteract(const FInputActionValue&)
{
	if (!Interaction)
	{
		return;
	}

	// Coger un objeto del mundo es responsabilidad de UCarryComponent, no de
	// IExploredInteractable::Interact (que se reserva para lo que no son
	// objetos transportables: puertas, palancas...). Con foco en un
	// AExploredItemActor, E coge; en cualquier otro interactuable, usa el
	// evento genérico.
	if (AExploredItemActor* ItemActor = Cast<AExploredItemActor>(Interaction->GetFocusedActor()))
	{
		if (Carry)
		{
			FText FailReason;
			Carry->TryPickUp(ItemActor, FailReason);
		}
		return;
	}

	Interaction->InteractWithFocus();
}

void AExploredCharacter::UseHand(EHand Hand)
{
	if (!Carry)
	{
		return;
	}
	FItemInstance Item;
	if (!Carry->GetHandItem(Hand, Item))
	{
		return;
	}
	// El efecto concreto de usar cada objeto (cortar leña, beber, encender una
	// antorcha...) lo aportan los módulos de supervivencia y recolección
	// (fuera del alcance de M2); de momento solo se deja constancia de la
	// acción para que esos sistemas puedan enganchar aquí más adelante.
	UE_LOG(LogTemp, Verbose, TEXT("[Explored] Usar mano %s: %s"),
		Hand == EHand::Left ? TEXT("izquierda") : TEXT("derecha"), *Item.DefinitionId.ToString());
}

void AExploredCharacter::HandleUsePrimary(const FInputActionValue&)
{
	// «Clic izquierdo = usar la mano derecha» (encargo, punto 5).
	UseHand(EHand::Right);
}

void AExploredCharacter::HandleUseSecondary(const FInputActionValue&)
{
	// «Clic derecho = usar la mano izquierda» (encargo, punto 5).
	UseHand(EHand::Left);
}

void AExploredCharacter::HandleDrop(const FInputActionValue&)
{
	if (!Carry)
	{
		return;
	}
	FText FailReason;
	// «La mano activa»: preferimos soltar la derecha si tiene algo; si no, la izquierda.
	if (!Carry->IsHandEmpty(EHand::Right))
	{
		Carry->Drop(EHand::Right, FailReason);
	}
	else if (!Carry->IsHandEmpty(EHand::Left))
	{
		Carry->Drop(EHand::Left, FailReason);
	}
}

void AExploredCharacter::HandleCombine(const FInputActionValue&)
{
	// Un objeto DosManos ocupa las dos manos con la misma instancia: no se
	// combina consigo mismo (duplicaria su material en el resultado).
	if (!Carry || Carry->IsHoldingTwoHandedItem())
	{
		return;
	}

	FItemInstance Left, Right;
	if (!Carry->GetHandItem(EHand::Left, Left) || !Carry->GetHandItem(EHand::Right, Right))
	{
		return;
	}

	const TArray<FName> Verbs = UCraftingLibrary::FindActions(Left, Right);
	if (Verbs.Num() == 0)
	{
		return;
	}

	// Con un único verbo posible se aplica directamente. Con varios (hasta 3,
	// ver UCraftingLibrary::MaxActions), «abrir la lista» de verdad es una
	// pantalla de selección que corresponde a un hito de interfaz posterior;
	// aquí se aplica el primero para no bloquear el bucle de fabricación, y
	// FindActions ya deja listos los candidatos para esa pantalla futura.
	FItemInstance Result;
	FText FailReason;
	if (UCraftingLibrary::Apply(Left, Right, Verbs[0], Result, FailReason))
	{
		Carry->ReplaceHandsWithCraftResult(Result, FailReason);
	}
}

void AExploredCharacter::HandleToggleBackpack(const FInputActionValue&)
{
	bBackpackOpen = !bBackpackOpen;
	OnBackpackToggled.Broadcast(bBackpackOpen);
}

void AExploredCharacter::HandleDiveStarted(const FInputActionValue&)
{
	if (Swim)
	{
		Swim->SetDiveHeld(true);
	}
}

void AExploredCharacter::HandleDiveCompleted(const FInputActionValue&)
{
	if (Swim)
	{
		Swim->SetDiveHeld(false);
	}
}
