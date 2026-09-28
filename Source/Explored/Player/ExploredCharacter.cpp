#include "Player/ExploredCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

#include "Achievements/AchievementsSubsystem.h"
#include "Building/BuildPreviewComponent.h"
#include "Carry/CarryComponent.h"
#include "Cartography/CartographyComponent.h"
#include "Cooking/CookingModel.h"
#include "Crafting/CraftingLibrary.h"
#include "Fishing/FishingComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Items/ExploredItemActor.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Mining/TerrainToolComponent.h"
#include "Player/SwimComponent.h"
#include "UI/ExploredGameUserSettings.h"
#include "UI/ExploredInputSettingsSubsystem.h"
#include "UI/SettingsLogic.h"
#include "Survival/BodySignalsComponent.h"

// Espacio de nombres con nombre (no anónimo): MakeAction/MapKey son nombres
// demasiado genéricos para el Unity build.
namespace ExploredCharacterInput
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

	UEnhancedInputLocalPlayerSubsystem* EnhancedInputFor(const AController* Controller)
	{
		const APlayerController* PC = Cast<APlayerController>(Controller);
		return PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	}

	constexpr float TickIntervalIdle = 1.0f / 30.0f;
}

using namespace ExploredCharacterInput;

AExploredCharacter::AExploredCharacter()
{
	// Excepción deliberada: el balanceo de las manos (punto 6 del encargo de
	// M2) y su temblor (UBodySignalsComponent) necesitan un valor por
	// fotograma. Es la única razón para tener tick en el personaje; todo lo
	// demás sigue dirigido por eventos y delegados.
	PrimaryActorTick.bCanEverTick = true;
	// 30 Hz basta para el vaivén de las manos; al nadar (la cámara sigue a la
	// ola) o con balanceo de cámara en marcha se pasa a cada fotograma (ver
	// UpdateTickRate), porque a 30 Hz la cámara da tirones (M15).
	PrimaryActorTick.TickInterval = TickIntervalIdle;

	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Camera->bUsePawnControlRotation = true;
	CameraRestLocation = FVector(0.0f, 0.0f, 70.0f);
	Camera->SetFieldOfView(ExploredSettingsLogic::FieldOfViewRange.Default);

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
	Cartography = CreateDefaultSubobject<UCartographyComponent>(TEXT("Cartography"));
	BuildPreview = CreateDefaultSubobject<UBuildPreviewComponent>(TEXT("BuildPreview"));
	Body = CreateDefaultSubobject<UBodySignalsComponent>(TEXT("Body"));
	Fishing = CreateDefaultSubobject<UFishingComponent>(TEXT("Fishing"));
	TerrainTool = CreateDefaultSubobject<UTerrainToolComponent>(TEXT("TerrainTool"));

	bUseControllerRotationYaw = true;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->JumpZVelocity = 480.0f;
	Movement->AirControl = 0.3f;
	Movement->MaxFlySpeed = DebugFlySpeed;
	Movement->BrakingDecelerationFlying = 8000.0f;
	Movement->SetWalkableFloorAngle(50.0f);
	// Agacharse (Ctrl en tierra; en el agua la misma tecla bucea).
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	Movement->SetCrouchedHalfHeight(60.0f);
}

void AExploredCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Carry)
	{
		Carry->OnCarryChanged.AddDynamic(this, &AExploredCharacter::RefreshHandMeshes);
	}

	BindToPlayerSettings();
	ApplyPlayerSettings();
}

void AExploredCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromPlayerSettings();
	Super::EndPlay(EndPlayReason);
}

void AExploredCharacter::NotifyControllerChanged()
{
	// El contexto sale del jugador local anterior (si lo había) y entra en el
	// nuevo. Antes se añadía solo en BeginPlay, así que un pawn poseído más
	// tarde se quedaba sin entrada (L8).
	if (UEnhancedInputLocalPlayerSubsystem* Previous = EnhancedInputFor(PreviousController))
	{
		if (MappingContext)
		{
			Previous->RemoveMappingContext(MappingContext);
		}
		if (DebugMappingContext)
		{
			Previous->RemoveMappingContext(DebugMappingContext);
		}
	}

	Super::NotifyControllerChanged();

	// Los mapeos dependen del remapeo del jugador local: se construyen aquí
	// también, porque la posesión llega antes que SetupPlayerInputComponent.
	UnbindFromPlayerSettings();
	BindToPlayerSettings();
	BuildInputAssets();
	RebuildKeyMappings();
	if (UEnhancedInputLocalPlayerSubsystem* Current = EnhancedInputFor(GetController()))
	{
		Current->AddMappingContext(MappingContext, 0);
		if (bIsDebugFlying && DebugMappingContext)
		{
			Current->AddMappingContext(DebugMappingContext, 1);
		}
	}
}

UExploredInputSettingsSubsystem* AExploredCharacter::GetInputSettings() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UExploredInputSettingsSubsystem>() : nullptr;
}

void AExploredCharacter::BindToPlayerSettings()
{
	if (!SettingsAppliedHandle.IsValid())
	{
		if (UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get())
		{
			SettingsAppliedHandle = Settings->OnSettingsApplied.AddUObject(this, &AExploredCharacter::ApplyPlayerSettings);
		}
	}
	if (!BindingsChangedHandle.IsValid())
	{
		if (UExploredInputSettingsSubsystem* InputSettings = GetInputSettings())
		{
			BoundInputSettings = InputSettings;
			BindingsChangedHandle = InputSettings->OnBindingsChanged.AddUObject(this, &AExploredCharacter::HandleBindingsChanged);
		}
	}
}

void AExploredCharacter::UnbindFromPlayerSettings()
{
	if (SettingsAppliedHandle.IsValid())
	{
		if (UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get())
		{
			Settings->OnSettingsApplied.Remove(SettingsAppliedHandle);
		}
		SettingsAppliedHandle.Reset();
	}
	if (UExploredInputSettingsSubsystem* InputSettings = BoundInputSettings.Get())
	{
		InputSettings->OnBindingsChanged.Remove(BindingsChangedHandle);
	}
	BoundInputSettings.Reset();
	BindingsChangedHandle.Reset();
}

void AExploredCharacter::HandleBindingsChanged(FName)
{
	RefreshKeyMappings();
}

void AExploredCharacter::RefreshKeyMappings()
{
	RebuildKeyMappings();
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = EnhancedInputFor(GetController()))
	{
		// Enhanced Input cachea los mapeos activos: hay que pedirle que los recalcule.
		Subsystem->RequestRebuildControlMappings();
	}
}

void AExploredCharacter::ApplyPlayerSettings()
{
	// Sensibilidad, invertir Y, balanceo y agacharse se leen en cada uso;
	// el FOV es estado de la cámara y se fija aquí.
	if (const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get())
	{
		if (Camera)
		{
			Camera->SetFieldOfView(Settings->GetFOV());
		}
	}
}

void AExploredCharacter::UpdateTickRate(bool bNeedsEveryFrame)
{
	if (bNeedsEveryFrame != bTickEveryFrame)
	{
		bTickEveryFrame = bNeedsEveryFrame;
		SetActorTickInterval(bNeedsEveryFrame ? 0.0f : TickIntervalIdle);
	}
}

void AExploredCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get();
	// «Reducir movimiento» (accesibilidad) anula el balanceo aunque esté activado.
	const bool bCameraMotion = !Settings || (Settings->GetCameraBobEnabled() && !Settings->GetReduceMotion());
	// Señales del cuerpo: temblor de manos (frío, hambre, fiebre, sueño) y tiritona de cámara.
	const FVector TremorL = Body ? Body->GetHandTremorOffset(false) : FVector::ZeroVector;
	const FVector TremorR = Body ? Body->GetHandTremorOffset(true) : FVector::ZeroVector;
	const FRotator Shiver = Body ? Body->GetShiverRotation() : FRotator::ZeroRotator;

	const EWaterState WaterState = Swim ? Swim->GetWaterState() : EWaterState::OnLand;
	if (WaterState == EWaterState::Swimming || WaterState == EWaterState::Diving)
	{
		UpdateTickRate(true);
		// No se puede nadar agachado: en vuelo el movimiento ya lo deshace, pero
		// así no se vuelve a agachar solo al salir del agua.
		if (bIsCrouched || GetCharacterMovement()->bWantsToCrouch)
		{
			UnCrouch();
		}
		// Con angarillas no se nada: se quedan en la orilla con su carga.
		if (Carry && Carry->HasSledge())
		{
			Carry->HandleEnterWater();
		}

		// Brazadas: la fase la lleva USwimComponent (avanza con la velocidad de nado);
		// aquí solo se traduce en el vaivén de las manos, mucho más amplio que al andar.
		const float Phase = Swim->GetStrokePhase() * UE_TWO_PI;
		const float SwingL = FMath::Sin(Phase) * SwimStrokeAmount;
		const float SwingR = FMath::Sin(Phase + UE_PI) * SwimStrokeAmount;
		if (HandMeshLeft)
		{
			HandMeshLeft->SetRelativeLocation(HandRestLocationLeft + FVector(SwingL * 0.6f, 0.0f, SwingL) + TremorL);
		}
		if (HandMeshRight)
		{
			HandMeshRight->SetRelativeLocation(HandRestLocationRight + FVector(SwingR * 0.6f, 0.0f, SwingR) + TremorR);
		}

		// La cámara deja de seguir solo el control del jugador: se le suma el
		// balanceo de la ola, salvo que el jugador lo haya desactivado.
		Camera->SetRelativeLocation(CameraRestLocation);
		Camera->bUsePawnControlRotation = false;
		const FRotator ControlRot = GetControlRotation();
		const FRotator Tilt = bCameraMotion ? Swim->GetWaveTilt() : FRotator::ZeroRotator;
		// Tiritona (UBodySignalsComponent) si el agua enfría.
		Camera->SetWorldRotation(FRotator(ControlRot.Pitch + Tilt.Pitch, ControlRot.Yaw, Tilt.Roll) + Shiver);
		return;
	}
	// Tiritona (GDD §8.3): mismo mecanismo que el balanceo de la ola, sin tocar la rotación de control.
	if (!Shiver.IsNearlyZero())
	{
		Camera->bUsePawnControlRotation = false;
		Camera->SetWorldRotation(GetControlRotation() + Shiver);
	}
	else
	{
		Camera->bUsePawnControlRotation = true;
	}

	const float SpeedRatio = FMath::Clamp(GetVelocity().Size2D() / FMath::Max(WalkSpeed, 1.0f), 0.0f, 1.0f);
	const bool bMoving = SpeedRatio > KINDA_SMALL_NUMBER;
	if (bMoving)
	{
		HandSwayPhase += DeltaSeconds * SpeedRatio * 8.0f;
	}
	const float Offset = FMath::Sin(HandSwayPhase) * HandSwayAmount * SpeedRatio;
	if (HandMeshLeft)
	{
		HandMeshLeft->SetRelativeLocation(HandRestLocationLeft + FVector(0.0f, 0.0f, Offset) + TremorL);
	}
	if (HandMeshRight)
	{
		HandMeshRight->SetRelativeLocation(HandRestLocationRight + FVector(0.0f, 0.0f, -Offset) + TremorR);
	}

	// Balanceo de cámara: un rebote por paso (el doble de la frecuencia del vaivén de las manos).
	const bool bBob = bCameraMotion && bMoving && !bIsDebugFlying && GetCharacterMovement()->IsMovingOnGround();
	const float Bob = bBob ? FMath::Abs(FMath::Sin(HandSwayPhase)) * CameraBobAmount * SpeedRatio : 0.0f;
	Camera->SetRelativeLocation(CameraRestLocation + FVector(0.0f, 0.0f, Bob));
	UpdateTickRate(bBob);
}

void AExploredCharacter::RefreshHandMeshes()
{
	// Cualquier cambio de carga puede cambiar el peso y, con él, la velocidad.
	ApplyWalkSpeed();

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
		// L6: sin esto, un objeto sin malla (o cuya malla no carga) seguía
		// mostrando la del objeto anterior.
		MeshComp->SetStaticMesh(nullptr);
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
	LookGamepadAction = MakeAction(this, TEXT("IA_LookGamepad"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	SprintAction = MakeAction(this, TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	FlyAction = MakeAction(this, TEXT("IA_DebugFly"), EInputActionValueType::Boolean);
	VerticalAction = MakeAction(this, TEXT("IA_DebugVertical"), EInputActionValueType::Axis1D);

	InteractAction = MakeAction(this, TEXT("IA_Interact"), EInputActionValueType::Boolean);
	UsePrimaryAction = MakeAction(this, TEXT("IA_UsePrimary"), EInputActionValueType::Boolean);
	UseSecondaryAction = MakeAction(this, TEXT("IA_UseSecondary"), EInputActionValueType::Boolean);
	DropAction = MakeAction(this, TEXT("IA_Drop"), EInputActionValueType::Boolean);
	CombineAction = MakeAction(this, TEXT("IA_Combine"), EInputActionValueType::Boolean);
	ToggleBackpackAction = MakeAction(this, TEXT("IA_ToggleBackpack"), EInputActionValueType::Boolean);
	// Bucear (en el agua) o agacharse (en tierra): mantener para bajar; al soltar,
	// el pulmón empuja de vuelta a la superficie.
	DiveAction = MakeAction(this, TEXT("IA_Dive"), EInputActionValueType::Boolean);
	// Reloj de pulsera: mantener para levantar la muñeca y ver la hora (GDD §8.3).
	WatchAction = MakeAction(this, TEXT("IA_Watch"), EInputActionValueType::Boolean);
	// Pesca (GDD §8.9): lanzar o recoger, y eje de sedal (recoger / soltar) en la pelea.
	FishAction = MakeAction(this, TEXT("IA_Fish"), EInputActionValueType::Boolean);
	ReelAction = MakeAction(this, TEXT("IA_Reel"), EInputActionValueType::Axis1D);

#if !UE_BUILD_SHIPPING
	// M12: E/Q de subir y bajar en vuelo viven en un contexto aparte que solo se
	// añade mientras se vuela (SetDebugMappingActive), con más prioridad que el
	// principal y consumiendo la tecla. Antes E estaba mapeada a la vez a
	// IA_DebugVertical e IA_Interact en el contexto principal.
	DebugMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_ExploredDebug"));
	MapKey(DebugMappingContext, VerticalAction, EKeys::E);
	MapKey(DebugMappingContext, VerticalAction, EKeys::Q, true);
#endif
}

UInputAction* AExploredCharacter::FindActionByName(FName ActionName) const
{
	const TObjectPtr<UInputAction> Remappable[] = {
		JumpAction, SprintAction, DiveAction, InteractAction, UsePrimaryAction,
		UseSecondaryAction, DropAction, CombineAction, ToggleBackpackAction
	};
	for (const TObjectPtr<UInputAction>& Action : Remappable)
	{
		if (Action && Action->GetFName() == ActionName)
		{
			return Action;
		}
	}
	return nullptr;
}

void AExploredCharacter::RebuildKeyMappings()
{
	if (!MappingContext)
	{
		return;
	}
	MappingContext->UnmapAll();

	// Fijos: movimiento con WASD (reservadas en ExploredSettingsLogic::IsReservedKey) y mirar.
	// Movimiento: W/S en Y del vector (adelante), A/D en X (lateral).
	MapKey(MappingContext, MoveAction, EKeys::W, false, true);
	MapKey(MappingContext, MoveAction, EKeys::S, true, true);
	MapKey(MappingContext, MoveAction, EKeys::D);
	MapKey(MappingContext, MoveAction, EKeys::A, true);
	MapKey(MappingContext, MoveAction, EKeys::Gamepad_Left2D);

	MapKey(MappingContext, LookAction, EKeys::Mouse2D);
	MapKey(MappingContext, LookGamepadAction, EKeys::Gamepad_Right2D);

	// Remapeables: la tecla efectiva (remapeo o por defecto) de la tabla común.
	const UExploredInputSettingsSubsystem* InputSettings = GetInputSettings();
	for (const ExploredSettingsLogic::FRemappableAction& Entry : ExploredSettingsLogic::GetRemappableActions())
	{
		if (UInputAction* Action = FindActionByName(Entry.ActionName))
		{
			const FKey DefaultKey(Entry.DefaultKey);
			MapKey(MappingContext, Action, InputSettings ? InputSettings->GetKeyFor(Entry.ActionName, DefaultKey) : DefaultKey);
		}
	}

	// Mando: fijo (no se remapea; ver UExploredInputSettingsSubsystem::SetKeyFor).
	// B queda para agacharse/bucear en juego y para «Volver» en los menús.
	MapKey(MappingContext, JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	MapKey(MappingContext, SprintAction, EKeys::Gamepad_LeftThumbstick);
	MapKey(MappingContext, DiveAction, EKeys::Gamepad_FaceButton_Right);
	MapKey(MappingContext, InteractAction, EKeys::Gamepad_FaceButton_Left);
	MapKey(MappingContext, UsePrimaryAction, EKeys::Gamepad_RightTrigger);
	MapKey(MappingContext, UseSecondaryAction, EKeys::Gamepad_LeftTrigger);
	MapKey(MappingContext, DropAction, EKeys::Gamepad_DPad_Down);
	MapKey(MappingContext, CombineAction, EKeys::Gamepad_FaceButton_Top);
	// View (Special_Left) saca el mapa en las manos (P-UI2, AExploredPlayerController): la mochila va a la cruceta.
	MapKey(MappingContext, ToggleBackpackAction, EKeys::Gamepad_DPad_Right);

	// Reloj de pulsera (fijo, fuera de la tabla de remapeo). H de «hora»: la T es
	// para soltar sedal al pescar.
	MapKey(MappingContext, WatchAction, EKeys::H);
	MapKey(MappingContext, WatchAction, EKeys::Gamepad_DPad_Up);

	// Pesca (fijo): F lanza o recoge; R recoge sedal y T lo suelta en la pelea. El
	// contexto de construcción también usa R, pero no la consume.
	MapKey(MappingContext, FishAction, EKeys::F);
	MapKey(MappingContext, ReelAction, EKeys::R);
	MapKey(MappingContext, ReelAction, EKeys::T, true);
	MapKey(MappingContext, ReelAction, EKeys::Gamepad_RightTriggerAxis);
	MapKey(MappingContext, ReelAction, EKeys::Gamepad_LeftTriggerAxis, true);

#if !UE_BUILD_SHIPPING
	MapKey(MappingContext, FlyAction, EKeys::F8);
#endif
}

void AExploredCharacter::SetDebugMappingActive(bool bActive)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = EnhancedInputFor(GetController());
	if (!Subsystem || !DebugMappingContext)
	{
		return;
	}
	if (bActive)
	{
		Subsystem->AddMappingContext(DebugMappingContext, 1);
	}
	else
	{
		Subsystem->RemoveMappingContext(DebugMappingContext);
	}
}

void AExploredCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInputAssets();
	BindToPlayerSettings();
	RefreshKeyMappings();

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AExploredCharacter::HandleMove);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AExploredCharacter::HandleLook);
	Input->BindAction(LookGamepadAction, ETriggerEvent::Triggered, this, &AExploredCharacter::HandleLookGamepad);
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

	// Modo construcción: contexto propio (B, R, Z/X y clic izquierdo) con prioridad 1.
	if (BuildPreview)
	{
		BuildPreview->SetupInput(Input);
	}
	Input->BindAction(WatchAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleWatchStarted);
	Input->BindAction(WatchAction, ETriggerEvent::Completed, this, &AExploredCharacter::HandleWatchCompleted);
	Input->BindAction(FishAction, ETriggerEvent::Started, this, &AExploredCharacter::HandleFish);
	Input->BindAction(ReelAction, ETriggerEvent::Triggered, this, &AExploredCharacter::HandleReel);
	Input->BindAction(ReelAction, ETriggerEvent::Completed, this, &AExploredCharacter::HandleReelCompleted);
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
	const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get();
	ApplyLookInput(Value.Get<FVector2D>(), Settings ? Settings->GetMouseSensitivity() : ExploredSettingsLogic::SensitivityRange.Default);
}

void AExploredCharacter::HandleLookGamepad(const FInputActionValue& Value)
{
	const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get();
	ApplyLookInput(Value.Get<FVector2D>(), Settings ? Settings->GetGamepadSensitivity() : ExploredSettingsLogic::SensitivityRange.Default);
}

void AExploredCharacter::ApplyLookInput(FVector2D Axis, float Sensitivity)
{
	const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get();
	const bool bInvertY = Settings && Settings->GetInvertY();
	AddControllerYawInput(Axis.X * Sensitivity);
	// Por defecto, subir el ratón/stick mira hacia arriba (de ahí el signo menos).
	AddControllerPitchInput((bInvertY ? Axis.Y : -Axis.Y) * Sensitivity);
}

void AExploredCharacter::HandleSprintStarted(const FInputActionValue&)
{
	bSprintHeld = true;
	ApplyWalkSpeed();
}

void AExploredCharacter::HandleSprintCompleted(const FInputActionValue&)
{
	bSprintHeld = false;
	ApplyWalkSpeed();
}

void AExploredCharacter::ApplyWalkSpeed()
{
	// La carga frena: sobrepeso y angarillas (FInventoryModel::GetMoveSpeedMultiplier).
	const float Base = bSprintHeld ? SprintSpeed : WalkSpeed;
	const float Multiplier = Carry ? Carry->GetMoveSpeedMultiplier() : 1.0f;
	GetCharacterMovement()->MaxWalkSpeed = Base * Multiplier;
}

void AExploredCharacter::HandleToggleFly(const FInputActionValue&)
{
#if !UE_BUILD_SHIPPING
	bIsDebugFlying = !bIsDebugFlying;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	// L7: el nado reutiliza MOVE_Flying y deja su velocidad y su gravedad; el vuelo de
	// depuración las vuelve a poner al activarse y la gravedad normal al soltarlo.
	Movement->MaxFlySpeed = DebugFlySpeed;
	Movement->GravityScale = bIsDebugFlying ? 0.0f : 1.0f;
	Movement->SetMovementMode(bIsDebugFlying ? MOVE_Flying : MOVE_Falling);
	SetActorEnableCollision(!bIsDebugFlying);
	SetDebugMappingActive(bIsDebugFlying);
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
	// Comer (P-WIRE): lo que tiene ficha de comida en recipes.json alimenta al cuerpo.
	const FFoodDef* Food = FCookingData::Default().FindFood(Item.DefinitionId);
	if (Food && Body && !Carry->IsHoldingTwoHandedItem() && Carry->ConsumeOneFromHand(Hand))
	{
		Body->Consume(Food->Effects);
		if (UAchievementsSubsystem* Achievements = UAchievementsSubsystem::Get(this))
		{
			Achievements->ReportStatItem(TEXT("foods_eaten"), Item.DefinitionId);
			const UItemRegistrySubsystem* Registry = UItemRegistrySubsystem::Resolve(this);
			FItemDefinition Definition;
			if (Registry && Registry->FindDefinition(Item.DefinitionId, Definition) && Definition.HasTag(TEXT("coco")))
			{
				Achievements->ReportStat(TEXT("coconuts_opened"));
			}
		}
		return;
	}
	// El resto de usos (cortar leña, beber, encender una antorcha...) llegarán con
	// sus módulos; de momento solo se deja constancia de la acción.
	UE_LOG(LogTemp, Verbose, TEXT("[Explored] Usar mano %s: %s"),
		Hand == EHand::Left ? TEXT("izquierda") : TEXT("derecha"), *Item.DefinitionId.ToString());
}

void AExploredCharacter::HandleUsePrimary(const FInputActionValue&)
{
	// En modo construcción el clic izquierdo coloca la pieza (UBuildPreviewComponent).
	if (BuildPreview && BuildPreview->IsBuildModeActive())
	{
		return;
	}
	// Pescando, los gatillos del mando son el eje del sedal (IA_Reel): no usar la mano.
	if (Fishing && Fishing->GetSessionState() != EFishingSessionState::Idle)
	{
		return;
	}
	// Pico o pala en la mano: el clic cava (UTerrainToolComponent).
	if (TerrainTool && TerrainTool->TryUseFromHands(Carry, false))
	{
		return;
	}
	// «Clic izquierdo = usar la mano derecha» (encargo, punto 5).
	UseHand(EHand::Right);
}

void AExploredCharacter::HandleUseSecondary(const FInputActionValue&)
{
	if (Fishing && Fishing->GetSessionState() != EFishingSessionState::Idle)
	{
		return;
	}
	if (TerrainTool && TerrainTool->TryUseFromHands(Carry, true))
	{
		return;
	}
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

	const TArray<FName> Verbs = UCraftingLibrary::FindActionsInWorld(this, Left, Right);
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
	if (UCraftingLibrary::ApplyInWorld(this, Left, Right, Verbs[0], Result, FailReason))
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

	// En tierra la misma tecla agacha, manteniendo o alternando según Ajustes.
	const bool bOnLand = !Swim || Swim->GetWaterState() == EWaterState::OnLand;
	if (!bOnLand || bIsDebugFlying)
	{
		return;
	}
	const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get();
	const bool bHold = !Settings || Settings->GetHoldToCrouch();
	if (bHold || !GetCharacterMovement()->bWantsToCrouch)
	{
		Crouch();
	}
	else
	{
		UnCrouch();
	}
}

void AExploredCharacter::HandleDiveCompleted(const FInputActionValue&)
{
	if (Swim)
	{
		Swim->SetDiveHeld(false);
	}

	const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get();
	const bool bHold = !Settings || Settings->GetHoldToCrouch();
	if (bHold && GetCharacterMovement()->bWantsToCrouch)
	{
		UnCrouch();
	}
}

void AExploredCharacter::HandleWatchStarted(const FInputActionValue&)
{
	if (Body)
	{
		Body->SetWristWatchRaised(true);
	}
}

void AExploredCharacter::HandleWatchCompleted(const FInputActionValue&)
{
	if (Body)
	{
		Body->SetWristWatchRaised(false);
	}
}

void AExploredCharacter::HandleFish(const FInputActionValue&)
{
	if (!Fishing)
	{
		return;
	}
	if (Fishing->GetSessionState() == EFishingSessionState::Idle)
	{
		Fishing->StartCast();
	}
	else
	{
		Fishing->Cancel();
	}
}

void AExploredCharacter::HandleReel(const FInputActionValue& Value)
{
	if (Fishing)
	{
		Fishing->SetReelInput(Value.Get<float>());
	}
}

void AExploredCharacter::HandleReelCompleted(const FInputActionValue&)
{
	if (Fishing)
	{
		Fishing->SetReelInput(0.0f);
	}
}
