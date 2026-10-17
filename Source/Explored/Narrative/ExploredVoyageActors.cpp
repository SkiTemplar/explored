#include "Narrative/ExploredVoyageActors.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"

#include "Explored.h"
#include "Narrative/ExploredVoyageSubsystem.h"

namespace ExploredVoyageActorsDetail
{
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
	Mesh = ExploredVoyageActorsDetail::MakeMesh(*this);
	RootComponent = Mesh;
}

void AExploredShipyard::BeginPlay()
{
	Super::BeginPlay();
	RefreshMesh();
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
