#include "Building/BuildPreviewComponent.h"

#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/SoftObjectPath.h"

#include "Building/BuildingModel.h"
#include "Building/BuildingSubsystem.h"
#include "Building/ExploredBuildingPiece.h"
#include "Carry/CarryComponent.h"
#include "Carry/CarryTypes.h"
#include "Core/SystemLinks.h"

namespace BuildPreviewDetail
{
	/** Material de las formas básicas: tiene el parámetro vectorial «Color». */
	const TCHAR* GhostMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	UInputAction* MakeBuildAction(UObject* Outer, FName Name)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = EInputActionValueType::Boolean;
		return Action;
	}
}

UBuildPreviewComponent::UBuildPreviewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UBuildPreviewComponent::SetupInput(UEnhancedInputComponent* Input)
{
	if (!Input)
	{
		return;
	}
	if (!BuildContext)
	{
		BuildContext = NewObject<UInputMappingContext>(this, TEXT("IMC_ExploredBuild"));
		ToggleAction = BuildPreviewDetail::MakeBuildAction(this, TEXT("IA_BuildToggle"));
		RotateAction = BuildPreviewDetail::MakeBuildAction(this, TEXT("IA_BuildRotate"));
		ConfirmAction = BuildPreviewDetail::MakeBuildAction(this, TEXT("IA_BuildConfirm"));
		NextAction = BuildPreviewDetail::MakeBuildAction(this, TEXT("IA_BuildNext"));
		PreviousAction = BuildPreviewDetail::MakeBuildAction(this, TEXT("IA_BuildPrevious"));
		BuildContext->MapKey(ToggleAction, EKeys::B);
		BuildContext->MapKey(RotateAction, EKeys::R);
		// El clic izquierdo también es «usar la mano derecha» en IMC_Explored: este contexto tiene más
		// prioridad, así que no debe consumir la tecla; AExploredCharacter la ignora en modo construcción.
		ConfirmAction->bConsumeInput = false;
		BuildContext->MapKey(ConfirmAction, EKeys::LeftMouseButton);
		BuildContext->MapKey(NextAction, EKeys::X);
		BuildContext->MapKey(PreviousAction, EKeys::Z);
		// El contexto está siempre activo: fuera del modo construcción R, Z y X no
		// deben tapar otras acciones (R recoge sedal al pescar). Sus manejadores ya
		// no hacen nada si el modo está apagado.
		RotateAction->bConsumeInput = false;
		NextAction->bConsumeInput = false;
		PreviousAction->bConsumeInput = false;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = PC && PC->GetLocalPlayer()
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr)
	{
		Subsystem->AddMappingContext(BuildContext, 1);
	}

	Input->BindAction(ToggleAction, ETriggerEvent::Started, this, &UBuildPreviewComponent::HandleToggle);
	Input->BindAction(RotateAction, ETriggerEvent::Started, this, &UBuildPreviewComponent::HandleRotate);
	Input->BindAction(ConfirmAction, ETriggerEvent::Started, this, &UBuildPreviewComponent::HandleConfirm);
	Input->BindAction(NextAction, ETriggerEvent::Started, this, &UBuildPreviewComponent::HandleNext);
	Input->BindAction(PreviousAction, ETriggerEvent::Started, this, &UBuildPreviewComponent::HandlePrevious);
}

void UBuildPreviewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Ghost)
	{
		Ghost->DestroyComponent();
		Ghost = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

UBuildingSubsystem* UBuildPreviewComponent::GetBuildingSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UBuildingSubsystem>() : nullptr;
}

void UBuildPreviewComponent::SetBuildModeActive(bool bActive)
{
	bBuildModeActive = bActive;
	SetComponentTickEnabled(bActive);
	if (bActive)
	{
		if (SelectedPiece.IsNone())
		{
			CyclePiece(0);
		}
		EnsureGhost();
		RefreshGhostMesh();
	}
	if (Ghost)
	{
		Ghost->SetVisibility(bActive && bHasAim);
	}
}

void UBuildPreviewComponent::SelectPiece(FName DefId)
{
	SelectedPiece = DefId;
	RefreshGhostMesh();
}

void UBuildPreviewComponent::CyclePiece(int32 Direction)
{
	const UBuildingSubsystem* Building = GetBuildingSubsystem();
	const FBuildingModel* Model = Building ? Building->GetModel() : nullptr;
	if (!Model || Model->GetCatalog().Pieces.Num() == 0)
	{
		return;
	}
	const TArray<FBuildingPieceDef>& Pieces = Model->GetCatalog().Pieces;
	const int32 Current = Pieces.IndexOfByPredicate([this](const FBuildingPieceDef& Def) { return Def.Id == SelectedPiece; });
	const int32 Next = Current == INDEX_NONE ? 0 : ((Current + Direction) % Pieces.Num() + Pieces.Num()) % Pieces.Num();
	SelectPiece(Pieces[Next].Id);
}

void UBuildPreviewComponent::RotatePreview()
{
	Rotation = (Rotation + 1) % 4;
}

FText UBuildPreviewComponent::GetPreviewReason() const
{
	return UBuildingSubsystem::GetReasonText(PreviewReason);
}

void UBuildPreviewComponent::EnsureGhost()
{
	AActor* Owner = GetOwner();
	if (Ghost || !Owner)
	{
		return;
	}
	Ghost = NewObject<UStaticMeshComponent>(Owner, TEXT("BuildGhost"));
	Ghost->SetupAttachment(Owner->GetRootComponent());
	Ghost->SetUsingAbsoluteLocation(true);
	Ghost->SetUsingAbsoluteRotation(true);
	Ghost->SetUsingAbsoluteScale(true);
	Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ghost->SetCastShadow(false);
	Ghost->SetVisibility(false);
	Ghost->RegisterComponent();

	if (UMaterialInterface* Base = Cast<UMaterialInterface>(FSoftObjectPath(BuildPreviewDetail::GhostMaterialPath).TryLoad()))
	{
		GhostMaterial = UMaterialInstanceDynamic::Create(Base, this);
	}
}

void UBuildPreviewComponent::RefreshGhostMesh()
{
	const UBuildingSubsystem* Building = GetBuildingSubsystem();
	const FBuildingModel* Model = Building ? Building->GetModel() : nullptr;
	const FBuildingPieceDef* Def = Model ? Model->GetCatalog().FindPiece(SelectedPiece) : nullptr;
	if (!Ghost || !Def)
	{
		return;
	}
	FVector Scale;
	Ghost->SetStaticMesh(AExploredBuildingPiece::LoadMeshForSocket(Def->Socket, Model->ResolveMeshPath(*Def), Scale, GhostOffset));
	Ghost->SetWorldScale3D(Scale);
	if (GhostMaterial)
	{
		for (int32 Slot = 0; Slot < Ghost->GetNumMaterials(); ++Slot)
		{
			Ghost->SetMaterial(Slot, GhostMaterial);
		}
	}
}

void UBuildPreviewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bBuildModeActive)
	{
		UpdatePreview();
	}
}

bool UBuildPreviewComponent::TraceAim(FVector& OutAimPoint, bool& bOutGroundContact) const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return false;
	}
	FVector EyeLocation;
	FRotator EyeRotation;
	Owner->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredBuildAim), false, Owner);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, EyeLocation, EyeLocation + EyeRotation.Vector() * MaxReachCm, ECC_Visibility, Params))
	{
		return false;
	}
	OutAimPoint = Hit.ImpactPoint;
	bOutGroundContact = !Cast<AExploredBuildingPiece>(Hit.GetActor());
	return true;
}

void UBuildPreviewComponent::UpdatePreview()
{
	UBuildingSubsystem* Building = GetBuildingSubsystem();
	bool bAimGround = false;
	bHasAim = Building && !SelectedPiece.IsNone() && TraceAim(AimPoint, bAimGround);
	if (!bHasAim)
	{
		if (Ghost)
		{
			Ghost->SetVisibility(false);
		}
		return;
	}

	// Primero dónde encaja; luego si la base de la pieza encajada toca el terreno de verdad.
	FVector Location;
	float Yaw = 0.0f;
	Building->PreviewPlacement(SelectedPiece, AimPoint, Rotation, false, Location, Yaw);
	bAimOnGround = false;
	if (const UWorld* World = GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredBuildGround), false, GetOwner());
		FHitResult Hit;
		const FVector Start = Location + FVector(0.0, 0.0, 50.0);
		const FVector End = Location - FVector(0.0, 0.0, FBuildingModel::FoundationHeightCm + 50.0);
		bAimOnGround = World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params) &&
			!Cast<AExploredBuildingPiece>(Hit.GetActor());
	}
	// Sin base todavía, la primera pieza se funda donde se apunta si eso es terreno.
	bAimOnGround |= bAimGround && Building->GetModel() && Building->GetModel()->FindBaseAt(AimPoint) == INDEX_NONE;
	PreviewReason = Building->PreviewPlacement(SelectedPiece, AimPoint, Rotation, bAimOnGround, Location, Yaw);

	EnsureGhost();
	if (!Ghost)
	{
		return;
	}
	const FRotator Rotator(0.0f, Yaw, 0.0f);
	Ghost->SetWorldLocationAndRotation(Location + Rotator.RotateVector(GhostOffset), Rotator);
	Ghost->SetVisibility(true);
	if (GhostMaterial)
	{
		GhostMaterial->SetVectorParameterValue(TEXT("Color"),
			PreviewReason == EBuildFailReason::None ? ValidColor : InvalidColor);
	}
}

void UBuildPreviewComponent::GatherCarried(TMap<FName, int32>& OutInventory, TSet<FName>& OutTools) const
{
	// Todo lo que se lleva encima, angarillas incluidas (la madera y la piedra van ahí).
	if (const UCarryComponent* Carry = GetOwner() ? GetOwner()->FindComponentByClass<UCarryComponent>() : nullptr)
	{
		Carry->CountMaterials(OutInventory, OutTools);
	}
}

bool UBuildPreviewComponent::ConfirmPlacement(FText& OutReason)
{
	OutReason = FText::GetEmpty();
	UBuildingSubsystem* Building = GetBuildingSubsystem();
	if (!Building || !bBuildModeActive || !bHasAim)
	{
		return false;
	}

	TMap<FName, int32> Inventory;
	TSet<FName> Tools;
	if (bFreeBuild)
	{
		// Depuración: todo lo que pida la pieza y sus herramientas.
		if (const FBuildingPieceDef* Def = Building->GetModel() ? Building->GetModel()->GetCatalog().FindPiece(SelectedPiece) : nullptr)
		{
			for (const FBuildingCost& Cost : Def->Cost)
			{
				Inventory.Add(Cost.Item, Cost.Count);
			}
			for (const FName& Tool : Def->Tools)
			{
				Tools.Add(Tool);
			}
			if (const FBuildingTierDef* Tier = Building->GetModel()->GetCatalog().FindTier(Def->Tier))
			{
				for (const FName& Tool : Tier->RequiresTools)
				{
					Tools.Add(Tool);
				}
			}
		}
	}
	else
	{
		GatherCarried(Inventory, Tools);
	}

	const TMap<FName, int32> Before = Inventory;
	EBuildFailReason Reason = EBuildFailReason::None;
	const int32 PieceId = Building->TryPlacePiece(SelectedPiece, AimPoint, Rotation, bAimOnGround, Inventory, Tools, Reason);
	OutReason = UBuildingSubsystem::GetReasonText(Reason);
	if (PieceId != INDEX_NONE && !bFreeBuild)
	{
		// Lo que el modelo ha descontado del recuento sale ahora del inventario de verdad.
		UCarryComponent* Carry = GetOwner() ? GetOwner()->FindComponentByClass<UCarryComponent>() : nullptr;
		if (Carry && !Carry->ConsumeMaterials(ExploredLinks::SpentMaterials(Before, Inventory)))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Explored] Construcción: el inventario no tenía lo que el recuento decía"));
		}
	}
	// Pendiente: gastar GetBuildMinutes(SelectedPiece) de trabajo (actividad Working del FSurvivalModel).
	return PieceId != INDEX_NONE;
}

void UBuildPreviewComponent::HandleToggle(const FInputActionValue&)
{
	SetBuildModeActive(!bBuildModeActive);
}

void UBuildPreviewComponent::HandleRotate(const FInputActionValue&)
{
	if (bBuildModeActive)
	{
		RotatePreview();
	}
}

void UBuildPreviewComponent::HandleConfirm(const FInputActionValue&)
{
	if (bBuildModeActive)
	{
		FText Reason;
		ConfirmPlacement(Reason);
	}
}

void UBuildPreviewComponent::HandleNext(const FInputActionValue&)
{
	if (bBuildModeActive)
	{
		CyclePiece(1);
	}
}

void UBuildPreviewComponent::HandlePrevious(const FInputActionValue&)
{
	if (bBuildModeActive)
	{
		CyclePiece(-1);
	}
}
