#include "Fishing/ExploredTrap.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

#include "Achievements/AchievementsSubsystem.h"
#include "Fishing/ExploredFishingSubsystem.h"
#include "Fishing/FishingComponent.h"

AExploredTrap::AExploredTrap()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetRelativeScale3D(FVector(0.5, 0.5, 0.35));

	// Marcador hasta que Tools/Blender genere SM_Nasa (ver meshes_pendientes.json).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
	}
}

void AExploredTrap::Configure(FName InKindName, FName InBaitItemId)
{
	KindName = InKindName;
	BaitItemId = InBaitItemId;
}

bool AExploredTrap::IsTidePool() const
{
	return KindName == FName(TEXT("poza"));
}

bool AExploredTrap::KindFromName(FName Name, ETrapKind& OutKind)
{
	if (Name == FName(TEXT("nasa")))
	{
		OutKind = ETrapKind::Nasa;
		return true;
	}
	if (Name == FName(TEXT("trampa_cangrejos")))
	{
		OutKind = ETrapKind::CrabTrap;
		return true;
	}
	if (Name == FName(TEXT("corral_piedras")))
	{
		OutKind = ETrapKind::StoneCorral;
		return true;
	}
	return false;
}

FName AExploredTrap::NameForKind(ETrapKind Kind)
{
	switch (Kind)
	{
	case ETrapKind::CrabTrap: return FName(TEXT("trampa_cangrejos"));
	case ETrapKind::StoneCorral: return FName(TEXT("corral_piedras"));
	default: return FName(TEXT("nasa"));
	}
}

AExploredTrap* AExploredTrap::SpawnRestored(UWorld* World, const FPlacedTrap& Placed)
{
	if (!World)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator::ZeroRotator, Placed.Location);
	AExploredTrap* Trap = World->SpawnActorDeferred<AExploredTrap>(AExploredTrap::StaticClass(), Transform);
	if (Trap)
	{
		Trap->Configure(NameForKind(Placed.Kind), FFishingModel::BaitItemId(Placed.Bait));
		// Con el id ya puesto, BeginPlay la encuentra en el estado y no la registra otra vez.
		Trap->TrapId = Placed.Id;
		Trap->FinishSpawning(Transform);
	}
	return Trap;
}

void AExploredTrap::BeginPlay()
{
	Super::BeginPlay();

	UExploredFishingSubsystem* Fishing = GetWorld() ? GetWorld()->GetSubsystem<UExploredFishingSubsystem>() : nullptr;
	ETrapKind Kind = ETrapKind::Nasa;
	if (!Fishing || IsTidePool() || !KindFromName(KindName, Kind))
	{
		return;
	}
	if (TrapId != 0 && Fishing->GetState().FindTrap(TrapId))
	{
		return;
	}

	// Hábitat por el agua que la cubre: en seco o casi, orilla (intermareal).
	const FVector Location = GetActorLocation();
	float WaterZ = 0.0f;
	const float CoverM = Fishing->GetWaterHeightAt(Location, WaterZ)
		? FMath::Max(0.0f, static_cast<float>(WaterZ - Location.Z) / 100.0f) : 0.0f;
	const EFishHabitat Habitat = FFishingModel::HabitatForDepth(CoverM, CoverM >= 3.0f);
	FPlacedTrap& Placed = Fishing->GetState().PlaceTrap(Kind, Habitat, Location,
		FFishingModel::BaitFromItemId(BaitItemId), Fishing->GetNowDays());
	TrapId = Placed.Id;
}

void AExploredTrap::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Solo se olvida si se destruye (recogerla); descargar el nivel la conserva.
	if (EndPlayReason == EEndPlayReason::Destroyed && TrapId != 0)
	{
		if (UExploredFishingSubsystem* Fishing = GetWorld() ? GetWorld()->GetSubsystem<UExploredFishingSubsystem>() : nullptr)
		{
			Fishing->GetState().RemoveTrap(TrapId);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AExploredTrap::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	if (IsTidePool())
	{
		OutVerbs.Add(NSLOCTEXT("Explored", "Verb_TidePool", "Marisquear"));
		return;
	}
	OutVerbs.Add(NSLOCTEXT("Explored", "Verb_CheckTrap", "Revisar la trampa"));
}

bool AExploredTrap::CanInteract_Implementation(AActor* InInstigator) const
{
	return IsTidePool() || TrapId != 0;
}

void AExploredTrap::Interact_Implementation(AActor* InInstigator)
{
	UExploredFishingSubsystem* Fishing = GetWorld() ? GetWorld()->GetSubsystem<UExploredFishingSubsystem>() : nullptr;
	if (!Fishing)
	{
		return;
	}
	const float Now = Fishing->GetNowDays();
	if (IsTidePool())
	{
		const int32 PoolId = FFishingModel::SpotKeyAt(FVector2D(GetActorLocation()));
		DropCatches(FFishingModel::GatherTidePool(Fishing->GetState(), PoolId, Now, Fishing->GetSeed()));
		return;
	}
	if (FPlacedTrap* Placed = Fishing->GetState().FindTrap(TrapId))
	{
		DropCatches(FFishingModel::CollectTrap(*Placed, Now, Fishing->GetSeed()));
	}
}

void AExploredTrap::DropCatches(const TArray<FTrapCatch>& Catches) const
{
	UWorld* World = GetWorld();
	// «fish_caught» cuenta los peces de cualquier técnica, también los de nasa (no el marisco).
	int32 Fish = 0;
	for (const FTrapCatch& Catch : Catches)
	{
		const FFishSpecies* Species = FFishingModel::FindSpecies(Catch.ItemId);
		Fish += (Species && Species->IsFish()) ? 1 : 0;
	}
	if (Fish > 0)
	{
		if (UAchievementsSubsystem* Achievements = UAchievementsSubsystem::Get(this))
		{
			Achievements->ReportStat(TEXT("fish_caught"), Fish);
		}
	}
	const FVector Base = GetActorLocation() + FVector(0.0, 0.0, 40.0);
	for (int32 Index = 0; Index < Catches.Num(); ++Index)
	{
		const FVector Offset(30.0 * (Index % 3), 30.0 * (Index / 3), 10.0 * Index);
		UFishingComponent::SpawnCatchActor(World, Catches[Index].ItemId, Base + Offset);
	}
}
