#include "Narrative/ExploredVoyageActors.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"

#include "Explored.h"
#include "Narrative/ExploredVoyageSubsystem.h"

namespace ExploredVoyageActorsDetail
{
	/** Altura a la que se sube el Limón terminado para que la quilla quede sobre la arena (cm). */
	constexpr double BeachedShipLiftCm = 55.0;

	UExploredVoyageSubsystem* GetVoyage(const UWorld* World)
	{
		return World ? World->GetSubsystem<UExploredVoyageSubsystem>() : nullptr;
	}

	const TCHAR* WreckMeshFor(EShipPart Part)
	{
		switch (Part)
		{
		case EShipPart::Fuselage: return TEXT("/Game/Generated/Meshes/Albatros/SM_Albatros_Fuselage_Wreck.SM_Albatros_Fuselage_Wreck");
		case EShipPart::Wing: return TEXT("/Game/Generated/Meshes/Albatros/SM_Albatros_Wing_Wreck.SM_Albatros_Wing_Wreck");
		case EShipPart::Tail: return TEXT("/Game/Generated/Meshes/Albatros/SM_Albatros_Tail_Wreck.SM_Albatros_Tail_Wreck");
		default: return TEXT("/Game/Generated/Meshes/Albatros/SM_Albatros_Engine_Wreck.SM_Albatros_Engine_Wreck");
		}
	}

	/** Primera malla de la lista que exista (el kit del Limón puede no estar importado aún). */
	UStaticMesh* LoadFirst(std::initializer_list<const TCHAR*> Paths)
	{
		for (const TCHAR* Path : Paths)
		{
			if (UStaticMesh* Mesh = Cast<UStaticMesh>(FSoftObjectPath(Path).TryLoad()))
			{
				return Mesh;
			}
		}
		return nullptr;
	}

	UStaticMeshComponent* MakeMesh(AActor& Owner)
	{
		UStaticMeshComponent* Mesh = Owner.CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		return Mesh;
	}
}

AExploredSalvageSite::AExploredSalvageSite()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = ExploredVoyageActorsDetail::MakeMesh(*this);
	RootComponent = Mesh;
}

void AExploredSalvageSite::Configure(EShipPart InPart)
{
	Part = InPart;
}

void AExploredSalvageSite::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* Wreck = Cast<UStaticMesh>(FSoftObjectPath(ExploredVoyageActorsDetail::WreckMeshFor(Part)).TryLoad()))
	{
		Mesh->SetStaticMesh(Wreck);
		ExploredInteraction::AddFocusBox(*this, *Mesh);
	}
	else
	{
		UE_LOG(LogExplored, Warning, TEXT("%s: falta la malla del pecio %s"), *GetName(), ExploredVoyageActorsDetail::WreckMeshFor(Part));
	}
}

bool AExploredSalvageSite::IsRecovered() const
{
	const UExploredVoyageSubsystem* Voyage = ExploredVoyageActorsDetail::GetVoyage(GetWorld());
	return Voyage && Voyage->HasPart(Part);
}

void AExploredSalvageSite::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	if (!IsRecovered())
	{
		OutVerbs.Add(FText::Format(NSLOCTEXT("ExploredVoyage", "VerbSalvage", "Rescatar: {0} del Albatros"),
			UExploredVoyageSubsystem::PartName(Part)));
	}
}

bool AExploredSalvageSite::CanInteract_Implementation(AActor* InInstigator) const
{
	return !IsRecovered();
}

void AExploredSalvageSite::Interact_Implementation(AActor* InInstigator)
{
	if (UExploredVoyageSubsystem* Voyage = ExploredVoyageActorsDetail::GetVoyage(GetWorld()))
	{
		Voyage->RecoverPart(Part);
	}
}

AExploredShipyard::AExploredShipyard()
{
	PrimaryActorTick.bCanEverTick = false;
	// Raíz aparte: el barco terminado se sube sobre la arena sin mover el actor.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = ExploredVoyageActorsDetail::MakeMesh(*this);
	Mesh->SetupAttachment(RootComponent);
}

void AExploredShipyard::BeginPlay()
{
	Super::BeginPlay();
	RefreshMesh();
	// El armazón y el barco ocupan casi lo mismo: basta con la caja del primero.
	ExploredInteraction::AddFocusBox(*this, *Mesh);
}

void AExploredShipyard::RefreshMesh()
{
	const UExploredVoyageSubsystem* Voyage = ExploredVoyageActorsDetail::GetVoyage(GetWorld());
	const EVoyageStage Stage = Voyage ? Voyage->GetStage() : EVoyageStage::Salvage;
	const bool bShip = Stage == EVoyageStage::SetSail || Stage == EVoyageStage::Departed;
	if (bHasMesh && bShip == bShowingShip)
	{
		return;
	}
	UStaticMesh* NewMesh = bShip
		? ExploredVoyageActorsDetail::LoadFirst({
			TEXT("/Game/Generated/Meshes/Limon/SM_Limon.SM_Limon"),
			TEXT("/Game/Generated/Meshes/Embarcaciones/SM_Canoe_Outrigger.SM_Canoe_Outrigger")})
		: ExploredVoyageActorsDetail::LoadFirst({
			TEXT("/Game/Generated/Meshes/Limon/SM_Limon_Frame.SM_Limon_Frame"),
			TEXT("/Game/Generated/Meshes/Embarcaciones/SM_Raft.SM_Raft")});
	if (!NewMesh)
	{
		UE_LOG(LogExplored, Warning, TEXT("%s: no hay malla para la grada del Limón"), *GetName());
		return;
	}
	Mesh->SetStaticMesh(NewMesh);
	// SM_Limon tiene el origen en la línea de flotación y ~0,5 m de calado: varado en la arena
	// se apoya en la quilla. El armazón (SM_Limon_Frame) ya tiene el origen en el suelo.
	Mesh->SetRelativeLocation(FVector(0.0, 0.0, bShip ? ExploredVoyageActorsDetail::BeachedShipLiftCm : 0.0));
	bShowingShip = bShip;
	bHasMesh = true;
}

void AExploredShipyard::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	const UExploredVoyageSubsystem* Voyage = ExploredVoyageActorsDetail::GetVoyage(GetWorld());
	if (!Voyage)
	{
		return;
	}
	switch (Voyage->GetStage())
	{
	case EVoyageStage::Salvage:
		OutVerbs.Add(FText::Format(NSLOCTEXT("ExploredVoyage", "VerbYardMissing", "Grada del Limón: faltan piezas del Albatros ({0}/4)"),
			Voyage->CountParts()));
		break;
	case EVoyageStage::Build:
		OutVerbs.Add(NSLOCTEXT("ExploredVoyage", "VerbYardBuild", "Construir el Limón"));
		break;
	case EVoyageStage::SetSail:
		OutVerbs.Add(Voyage->IsStarNightNow()
			? NSLOCTEXT("ExploredVoyage", "VerbYardSail", "Zarpar guiándote por las estrellas")
			: NSLOCTEXT("ExploredVoyage", "VerbYardWait", "Zarpar (solo de noche)"));
		break;
	default:
		break;
	}
}

bool AExploredShipyard::CanInteract_Implementation(AActor* InInstigator) const
{
	const UExploredVoyageSubsystem* Voyage = ExploredVoyageActorsDetail::GetVoyage(GetWorld());
	return Voyage && Voyage->GetStage() != EVoyageStage::Departed;
}

void AExploredShipyard::Interact_Implementation(AActor* InInstigator)
{
	UExploredVoyageSubsystem* Voyage = ExploredVoyageActorsDetail::GetVoyage(GetWorld());
	if (!Voyage)
	{
		return;
	}
	switch (Voyage->GetStage())
	{
	case EVoyageStage::Build:
		Voyage->TryBuildShip();
		break;
	case EVoyageStage::SetSail:
		Voyage->TryDepart();
		break;
	default:
		break;
	}
}

AExploredDepartingShip::AExploredDepartingShip()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Mesh;
}

void AExploredDepartingShip::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* Ship = ExploredVoyageActorsDetail::LoadFirst({
		TEXT("/Game/Generated/Meshes/Limon/SM_Limon.SM_Limon"),
		TEXT("/Game/Generated/Meshes/Embarcaciones/SM_Canoe_Outrigger.SM_Canoe_Outrigger")}))
	{
		Mesh->SetStaticMesh(Ship);
	}
	BaseYaw = GetActorRotation().Yaw;
	BaseZ = GetActorLocation().Z;
}

void AExploredDepartingShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	// Arranca suave y cabecea/escora poco: se lee como un barco pequeño con marejada.
	const float Speed = SpeedCmPerSecond * FMath::Clamp(Age / 3.0f, 0.15f, 1.0f);
	FVector Location = GetActorLocation() + GetActorForwardVector() * Speed * DeltaSeconds;
	Location.Z = BaseZ + FMath::Sin(Age * 1.3f) * 12.0f;
	const FRotator Rotation(FMath::Sin(Age * 1.1f) * 2.5f, BaseYaw, FMath::Sin(Age * 0.8f + 1.0f) * 4.0f);
	SetActorLocationAndRotation(Location, Rotation);
}
