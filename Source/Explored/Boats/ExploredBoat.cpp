#include "Boats/ExploredBoat.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"

#include "Achievements/AchievementsSubsystem.h"
#include "Core/SystemLinks.h"
#include "Ocean/ExploredOcean.h"
#include "Ocean/OceanWaves.h"
#include "Player/ExploredCharacter.h"
#include "Player/SwimComponent.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"

namespace ExploredBoatDetail
{
	constexpr float OceanSearchIntervalSeconds = 2.0f;
	/** Prioridad del contexto del barco: por encima del del personaje (0), que así no recibe W/A/S/D ni E. */
	constexpr int32 BoatInputPriority = 1;
	/** Altura desde la que se sondea el fondo (cm): por encima de cualquier cumbre del archipiélago. */
	constexpr float ProbeStartZ = 10000.0f;

	EBoatType ToModelType(EExploredBoatKind Kind)
	{
		return static_cast<EBoatType>(static_cast<uint8>(Kind));
	}

	/** Ruta de importación de las mallas de Tools/Blender/props (grupo «Embarcaciones» de run_props.py). */
	FSoftObjectPath MeshPath(const TCHAR* MeshName)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/Generated/Meshes/Embarcaciones/%s.%s"), MeshName, MeshName));
	}

	UInputAction* MakeBoatAction(UObject* Outer, FName Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	void MapBoatKey(UInputMappingContext* Context, UInputAction* Action, FKey Key, bool bNegate = false, bool bSwizzleYXZ = false)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bSwizzleYXZ)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
	}

	FText BoatName(EBoatType Type)
	{
		switch (Type)
		{
		case EBoatType::Raft: return NSLOCTEXT("Explored", "Boat_Raft", "la balsa");
		case EBoatType::Canoe: return NSLOCTEXT("Explored", "Boat_Canoe", "la canoa");
		case EBoatType::Outrigger: return NSLOCTEXT("Explored", "Boat_Outrigger", "la canoa con balancín");
		default: return NSLOCTEXT("Explored", "Boat_Limon", "el «Limón»");
		}
	}
}

AExploredBoat::AExploredBoat()
{
	PrimaryActorTick.bCanEverTick = true;
	// Antes de la física: el tripulante sentado se mueve con el barco en el mismo fotograma.
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	Hull->SetupAttachment(Root);
	Hull->SetMobility(EComponentMobility::Movable);
	// Cinemático: bloquea trazas (el foco de interacción) y al jugador a pie, pero no simula.
	Hull->SetSimulatePhysics(false);
	Hull->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	Sail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sail"));
	Sail->SetupAttachment(Root);
	Sail->SetMobility(EComponentMobility::Movable);
	Sail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sail->SetVisibility(false);

	Seat = CreateDefaultSubobject<USceneComponent>(TEXT("Seat"));
	Seat->SetupAttachment(Root);
}

void AExploredBoat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Model = FBoatModel(ExploredBoatDetail::ToModelType(BoatKind), Transform.GetLocation(), Transform.Rotator().Yaw);
	ApplyMeshes();
}

void AExploredBoat::BeginPlay()
{
	Super::BeginPlay();

	Model = FBoatModel(ExploredBoatDetail::ToModelType(BoatKind), GetActorLocation(), GetActorRotation().Yaw);
	ApplyMeshes();

	const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed);
	Straits = FOceanCurrents::BuildStraits(Layout);
	Ocean = Cast<AExploredOcean>(UGameplayStatics::GetActorOfClass(GetWorld(), AExploredOcean::StaticClass()));

	if (bBuiltByPlayer)
	{
		if (UAchievementsSubsystem* Achievements = UAchievementsSubsystem::Get(this))
		{
			Achievements->ReportStatItem(TEXT("boats_built"), ExploredLinks::BoatStatId(Model.GetState().Type));
		}
	}
}

void AExploredBoat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsOccupied())
	{
		Leave();
	}
	Super::EndPlay(EndPlayReason);
}

void AExploredBoat::ApplyMeshes()
{
	const FBoatDefinition& D = FBoatModel::Definition(ExploredBoatDetail::ToModelType(BoatKind));

	UStaticMesh* HullMesh = nullptr;
	if (D.MeshName && D.MeshName[0] != TEXT('\0'))
	{
		HullMesh = TSoftObjectPtr<UStaticMesh>(ExploredBoatDetail::MeshPath(D.MeshName)).LoadSynchronous();
	}
	if (HullMesh)
	{
		Hull->SetStaticMesh(HullMesh);
		Hull->SetRelativeLocation(FVector::ZeroVector);
		Hull->SetRelativeRotation(FRotator(0.0f, MeshYawOffsetDeg, 0.0f));
		Hull->SetRelativeScale3D(FVector::OneVector);
	}
	else
	{
		// Sin malla importada (o el «Limón», aún sin modelar): caja con las medidas del casco
		// y la quilla en el origen, para poder probar la navegación en PIE.
		UStaticMesh* Cube = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))).LoadSynchronous();
		Hull->SetStaticMesh(Cube);
		Hull->SetRelativeRotation(FRotator::ZeroRotator);
		Hull->SetRelativeScale3D(FVector(D.LengthCm, D.BeamCm, D.HullDepthCm) / 100.0f);
		Hull->SetRelativeLocation(FVector(0.0f, 0.0f, D.HullDepthCm * 0.5f));
	}

	if (D.HasSail())
	{
		if (UStaticMesh* SailMesh = TSoftObjectPtr<UStaticMesh>(ExploredBoatDetail::MeshPath(TEXT("SM_Sail"))).LoadSynchronous())
		{
			Sail->SetStaticMesh(SailMesh);
		}
		// Pie del mástil (boats.py: a la altura de la regala, en el centro del casco).
		Sail->SetRelativeLocation(FVector(0.0f, 0.0f, D.HullDepthCm));
	}
	Sail->SetVisibility(false);

	// El tripulante se sienta en popa, a la altura de la regala.
	Seat->SetRelativeLocation(FVector(-0.3f * D.LengthCm, 0.0f, D.HullDepthCm * 0.6f + 90.0f));
}

AExploredOcean* AExploredBoat::ResolveOcean(float DeltaSeconds)
{
	AExploredOcean* OceanActor = Ocean.Get();
	if (!OceanActor)
	{
		TimeSinceOceanSearch -= DeltaSeconds;
		if (TimeSinceOceanSearch <= 0.0f)
		{
			TimeSinceOceanSearch = ExploredBoatDetail::OceanSearchIntervalSeconds;
			OceanActor = Cast<AExploredOcean>(UGameplayStatics::GetActorOfClass(GetWorld(), AExploredOcean::StaticClass()));
			Ocean = OceanActor;
		}
	}
	return OceanActor;
}

float AExploredBoat::TraceDepthCm(const FVector2D& Position) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return MaxProbeDepthCm;
	}
	// Sondeo vertical contra lo estático del mundo (terreno por chunks, rocas): el océano no tiene colisión.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredBoatDepth), false, this);
	if (const AExploredCharacter* Character = Occupant.Get())
	{
		Params.AddIgnoredActor(Character);
	}
	FHitResult Hit;
	const FVector Start(Position.X, Position.Y, ExploredBoatDetail::ProbeStartZ);
	const FVector End(Position.X, Position.Y, -MaxProbeDepthCm);
	if (World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		// El nivel medio del mar está en Z = 0 (FArchipelagoLayout::SeaLevel).
		return static_cast<float>(-Hit.ImpactPoint.Z);
	}
	return MaxProbeDepthCm;
}

FBoatEnvironment AExploredBoat::GatherEnvironment()
{
	FBoatEnvironment Env;
	const UWorld* World = GetWorld();
	if (const AExploredOcean* OceanActor = Ocean.Get())
	{
		Env.Waves = &OceanActor->GetWaves();
		Env.WaveTimeSeconds = OceanActor->GetWaveTimeSeconds();
	}

	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	const float TotalDays = Time ? Time->GetTotalDays() : 0.0f;
	const float MoonPhase01 = Time ? Time->GetMoonPhase() : 0.0f;
	const float Wind01 = Weather ? Weather->GetCurrent().Wind : 0.2f;

	// TODO(P-BOATS): el océano aún no dibuja la marea; hasta entonces la superficie y el
	// fondo se quedan en el nivel medio (FBoatModel::TideOffsetCm ya lo calcula).
	Env.TideOffsetCm = 0.0f;

	const FVector Location = GetActorLocation();
	if (Straits.Num() > 0)
	{
		Env.CurrentCmS = FOceanCurrents::CurrentAt(Straits, FVector2D(Location.X, Location.Y),
			FOceanTide::Flow(TotalDays), FOceanTide::SpringNeapFactor(MoonPhase01), Wind01);
	}
	Env.WindCmS = FBoatWind::VelocityCmS(Wind01, FBoatWind::PrevailingFromDeg(TotalDays));

	TWeakObjectPtr<const AExploredBoat> WeakThis(this);
	const float FallbackDepth = MaxProbeDepthCm;
	Env.DepthBelowSeaLevelCm = [WeakThis, FallbackDepth](const FVector2D& Position) -> float
	{
		const AExploredBoat* Self = WeakThis.Get();
		return Self ? Self->TraceDepthCm(Position) : FallbackDepth;
	};
	return Env;
}

void AExploredBoat::DrivePaddling()
{
	if (!IsOccupied() || Model.IsStroking())
	{
		return;
	}
	// W: paladas alternas (rumbo recto). A/D sin W: paladas por la banda contraria al giro.
	if (PaddleAxis.Y > 0.5f)
	{
		if (Model.TryStroke(NextStrokeSide))
		{
			NextStrokeSide = NextStrokeSide == EBoatSide::Port ? EBoatSide::Starboard : EBoatSide::Port;
		}
	}
	else if (PaddleAxis.X < -0.5f)
	{
		Model.TryStroke(EBoatSide::Starboard);
	}
	else if (PaddleAxis.X > 0.5f)
	{
		Model.TryStroke(EBoatSide::Port);
	}
}

void AExploredBoat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ResolveOcean(DeltaSeconds);
	DrivePaddling();
	if (IsOccupied())
	{
		Controls.Rudder = FMath::Clamp(static_cast<float>(PaddleAxis.X), -1.0f, 1.0f);
	}
	const FBoatEnvironment Env = GatherEnvironment();
	Model.Step(DeltaSeconds, Controls, Env);
	ApplyModelTransform();
	UpdateSailVisual();

	// Al volcar o hundirse, el tripulante cae al agua (luego puede adrizarlo desde fuera con E).
	const EBoatCondition Condition = Model.GetState().Condition;
	if (IsOccupied() && (Condition == EBoatCondition::Capsized || Condition == EBoatCondition::Wrecked))
	{
		Leave();
	}
}

void AExploredBoat::ApplyModelTransform()
{
	const FBoatState& S = Model.GetState();
	SetActorLocationAndRotation(S.LocationCm, FRotator(S.PitchDeg, S.YawDeg, S.RollDeg), false, nullptr, ETeleportType::None);
}

void AExploredBoat::UpdateSailVisual()
{
	const FBoatState& S = Model.GetState();
	const bool bShow = S.bSailRaised && Model.GetDefinition().HasSail();
	Sail->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}
	// La botavara se abre hacia sotavento tanto como la escota.
	const float Awa = Model.GetApparentWindAngleDeg();
	const float Trim = Controls.bAutoTrim ? FBoatModel::OptimalSailTrim01(Awa) : Controls.SailTrim01;
	const float SheetDeg = 5.0f + 85.0f * Trim;
	const FVector2D Apparent = Model.GetApparentWindCmS();
	const FVector Right = GetActorRightVector();
	const float Leeward = FVector2D::DotProduct(Apparent, FVector2D(Right.X, Right.Y)) >= 0.0 ? 1.0f : -1.0f;
	Sail->SetRelativeRotation(FRotator(0.0f, 180.0f + Leeward * SheetDeg, 0.0f));
}

APlayerController* AExploredBoat::GetOccupantController() const
{
	const AExploredCharacter* Character = Occupant.Get();
	return Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
}

bool AExploredBoat::Board(AExploredCharacter* Character)
{
	const EBoatCondition Condition = Model.GetState().Condition;
	if (!Character || IsOccupied() || Condition == EBoatCondition::Capsized || Condition == EBoatCondition::Wrecked)
	{
		return false;
	}

	Occupant = Character;
	Model.SetCrewAboard(true);
	Controls = FBoatControls();
	PaddleAxis = FVector2D::ZeroVector;

	// El personaje queda sentado y quieto: sin movimiento, sin nado y sin colisión propia.
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		Movement->SetComponentTickEnabled(false);
	}
	if (USwimComponent* Swim = Character->GetSwimComponent())
	{
		Swim->SetComponentTickEnabled(false);
	}
	Character->SetActorEnableCollision(false);
	Character->AttachToComponent(Seat, FAttachmentTransformRules(EAttachmentRule::SnapToTarget,
		EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		BuildInputAssets();
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(MappingContext, ExploredBoatDetail::BoatInputPriority);
		}
		EnableInput(PC);
		BindInput(PC);
	}
	return true;
}

void AExploredBoat::Leave()
{
	AExploredCharacter* Character = Occupant.Get();
	if (APlayerController* PC = GetOccupantController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(MappingContext);
		}
		DisableInput(PC);
	}

	Occupant.Reset();
	Model.SetCrewAboard(false);
	Controls.Rudder = 0.0f;
	Controls.bBailing = false;
	PaddleAxis = FVector2D::ZeroVector;

	if (!Character)
	{
		return;
	}
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// Junto a la borda de estribor, algo por encima del agua: si no hay tierra, cae al mar y nada.
	const FBoatDefinition& D = Model.GetDefinition();
	const FVector Exit = GetActorLocation() + GetActorRightVector() * (D.BeamCm * 0.5f + 70.0f) + FVector(0.0f, 0.0f, 120.0f);
	Character->SetActorLocationAndRotation(Exit, FRotator(0.0f, GetActorRotation().Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Character->SetActorEnableCollision(true);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetComponentTickEnabled(true);
		Movement->SetMovementMode(MOVE_Falling);
	}
	if (USwimComponent* Swim = Character->GetSwimComponent())
	{
		Swim->SetComponentTickEnabled(true);
	}
}

bool AExploredBoat::PaddleStroke(bool bStarboard)
{
	return Model.TryStroke(bStarboard ? EBoatSide::Starboard : EBoatSide::Port);
}

void AExploredBoat::SetRudder(float Rudder)
{
	Controls.Rudder = FMath::Clamp(Rudder, -1.0f, 1.0f);
}

bool AExploredBoat::SetSailRaised(bool bRaised)
{
	return Model.SetSailRaised(bRaised);
}

void AExploredBoat::AdjustSailTrim(float Delta01)
{
	if (Controls.bAutoTrim)
	{
		// Parte de donde la tenía la escota automática para que el cambio no dé un salto.
		Controls.SailTrim01 = FBoatModel::OptimalSailTrim01(Model.GetApparentWindAngleDeg());
		Controls.bAutoTrim = false;
	}
	Controls.SailTrim01 = FMath::Clamp(Controls.SailTrim01 + Delta01, 0.0f, 1.0f);
}

AExploredCharacter* AExploredBoat::GetOccupant() const
{
	return Occupant.Get();
}

void AExploredBoat::RestoreFromSaveData(const FBoatSaveData& Data)
{
	if (IsOccupied())
	{
		Leave();
	}
	Model = FBoatModel::FromSaveData(Data);
	BoatKind = static_cast<EExploredBoatKind>(static_cast<uint8>(Model.GetState().Type));
	ApplyMeshes();
	ApplyModelTransform();
}

// ----------------------------------------------------------------------------- interacción

void AExploredBoat::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	const FText Name = ExploredBoatDetail::BoatName(Model.GetState().Type);
	if (Model.GetState().Condition == EBoatCondition::Capsized)
	{
		OutVerbs.Add(FText::Format(NSLOCTEXT("Explored", "Verb_RightBoat", "Adrizar {0}"), Name));
	}
	else
	{
		OutVerbs.Add(FText::Format(NSLOCTEXT("Explored", "Verb_Board", "Subir a {0}"), Name));
	}
}

bool AExploredBoat::CanInteract_Implementation(AActor* InInstigator) const
{
	return !IsOccupied() && Model.GetState().Condition != EBoatCondition::Wrecked
		&& Cast<AExploredCharacter>(InInstigator) != nullptr;
}

void AExploredBoat::Interact_Implementation(AActor* InInstigator)
{
	if (Model.GetState().Condition == EBoatCondition::Capsized)
	{
		Model.TryRight();
		return;
	}
	Board(Cast<AExploredCharacter>(InInstigator));
}

// ----------------------------------------------------------------------------- entrada

void AExploredBoat::BuildInputAssets()
{
	if (MappingContext)
	{
		return;
	}
	using namespace ExploredBoatDetail;

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Boat"));
	PaddleAction = MakeBoatAction(this, TEXT("IA_BoatPaddle"), EInputActionValueType::Axis2D);
	SailAction = MakeBoatAction(this, TEXT("IA_BoatSail"), EInputActionValueType::Boolean);
	TrimAction = MakeBoatAction(this, TEXT("IA_BoatTrim"), EInputActionValueType::Axis1D);
	AutoTrimAction = MakeBoatAction(this, TEXT("IA_BoatAutoTrim"), EInputActionValueType::Boolean);
	BailAction = MakeBoatAction(this, TEXT("IA_BoatBail"), EInputActionValueType::Boolean);
	RightAction = MakeBoatAction(this, TEXT("IA_BoatRight"), EInputActionValueType::Boolean);
	LeaveAction = MakeBoatAction(this, TEXT("IA_BoatLeave"), EInputActionValueType::Boolean);

	// Mismo esquema que el personaje: W/S en Y, A/D en X.
	MapBoatKey(MappingContext, PaddleAction, EKeys::W, false, true);
	MapBoatKey(MappingContext, PaddleAction, EKeys::S, true, true);
	MapBoatKey(MappingContext, PaddleAction, EKeys::D);
	MapBoatKey(MappingContext, PaddleAction, EKeys::A, true);
	MapBoatKey(MappingContext, PaddleAction, EKeys::Gamepad_Left2D);

	MapBoatKey(MappingContext, SailAction, EKeys::R);
	MapBoatKey(MappingContext, SailAction, EKeys::Gamepad_FaceButton_Top);
	MapBoatKey(MappingContext, TrimAction, EKeys::X);
	MapBoatKey(MappingContext, TrimAction, EKeys::Z, true);
	MapBoatKey(MappingContext, TrimAction, EKeys::Gamepad_RightTriggerAxis);
	MapBoatKey(MappingContext, TrimAction, EKeys::Gamepad_LeftTriggerAxis, true);
	MapBoatKey(MappingContext, AutoTrimAction, EKeys::T);
	MapBoatKey(MappingContext, BailAction, EKeys::B);
	MapBoatKey(MappingContext, BailAction, EKeys::Gamepad_FaceButton_Left);
	MapBoatKey(MappingContext, RightAction, EKeys::F);
	// E es también «interactuar» en el contexto del personaje; con más prioridad, aquí baja del barco.
	MapBoatKey(MappingContext, LeaveAction, EKeys::E);
	MapBoatKey(MappingContext, LeaveAction, EKeys::Gamepad_FaceButton_Right);
}

void AExploredBoat::BindInput(APlayerController* PC)
{
	if (bInputBound)
	{
		return;
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] AExploredBoat: el componente de entrada no es de Enhanced Input; usa las funciones del barco desde Blueprint."));
		return;
	}
	Input->BindAction(PaddleAction, ETriggerEvent::Triggered, this, &AExploredBoat::HandlePaddle);
	Input->BindAction(PaddleAction, ETriggerEvent::Completed, this, &AExploredBoat::HandlePaddleReleased);
	Input->BindAction(SailAction, ETriggerEvent::Started, this, &AExploredBoat::HandleToggleSail);
	Input->BindAction(TrimAction, ETriggerEvent::Triggered, this, &AExploredBoat::HandleTrim);
	Input->BindAction(AutoTrimAction, ETriggerEvent::Started, this, &AExploredBoat::HandleAutoTrim);
	Input->BindAction(BailAction, ETriggerEvent::Started, this, &AExploredBoat::HandleBailStarted);
	Input->BindAction(BailAction, ETriggerEvent::Completed, this, &AExploredBoat::HandleBailCompleted);
	Input->BindAction(RightAction, ETriggerEvent::Started, this, &AExploredBoat::HandleRight);
	Input->BindAction(LeaveAction, ETriggerEvent::Started, this, &AExploredBoat::HandleLeave);
	bInputBound = true;
}

void AExploredBoat::HandlePaddle(const FInputActionValue& Value)
{
	PaddleAxis = Value.Get<FVector2D>();
}

void AExploredBoat::HandlePaddleReleased(const FInputActionValue&)
{
	PaddleAxis = FVector2D::ZeroVector;
}

void AExploredBoat::HandleToggleSail(const FInputActionValue&)
{
	Model.SetSailRaised(!Model.GetState().bSailRaised);
}

void AExploredBoat::HandleTrim(const FInputActionValue& Value)
{
	const UWorld* World = GetWorld();
	const float Dt = World ? World->GetDeltaSeconds() : 0.0f;
	// Unos tres segundos de cazada a largada completa.
	AdjustSailTrim(Value.Get<float>() * Dt / 3.0f);
}

void AExploredBoat::HandleAutoTrim(const FInputActionValue&)
{
	Controls.bAutoTrim = !Controls.bAutoTrim;
}

void AExploredBoat::HandleBailStarted(const FInputActionValue&)
{
	Controls.bBailing = true;
}

void AExploredBoat::HandleBailCompleted(const FInputActionValue&)
{
	Controls.bBailing = false;
}

void AExploredBoat::HandleRight(const FInputActionValue&)
{
	Model.TryRight();
}

void AExploredBoat::HandleLeave(const FInputActionValue&)
{
	Leave();
}
