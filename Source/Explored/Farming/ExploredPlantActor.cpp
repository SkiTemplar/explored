#include "Farming/ExploredPlantActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SoftObjectPath.h"

#include "Carry/CarryComponent.h"
#include "Farming/FarmModel.h"
#include "Farming/FarmSubsystem.h"
#include "Items/ExploredItemActor.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Items/ItemTypes.h"

namespace ExploredPlantActorDetail
{
	/** Propiedad de objeto que permite llevar agua para regar (cuenco, cáscara de coco...). */
	const FName WaterContainerProperty(TEXT("Recipiente"));
	/** Radio en que caen los frutos cosechados alrededor de la planta (cm). */
	constexpr float HarvestDropRadius = 60.0f;
	constexpr float HarvestDropHeight = 40.0f;

	FText PlantName(const UFarmSubsystem* Farm, FName PlantId)
	{
		const FPlantDef* Def = Farm ? Farm->FindPlant(PlantId) : nullptr;
		return Def ? FText::FromString(Def->NameEs) : FText::FromName(PlantId);
	}
}

AExploredPlantActor::AExploredPlantActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Pieces.Add(FName(TEXT("bancal")));

	Bed = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bed"));
	RootComponent = Bed;
	// Movable: P-BUILD la generará en tiempo de juego y un ciclón podría moverla.
	Bed->SetMobility(EComponentMobility::Movable);
	Bed->SetCollisionProfileName(TEXT("BlockAll"));
	Bed->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.2f));

	PlantMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlantMesh"));
	PlantMesh->SetupAttachment(Bed);
	PlantMesh->SetMobility(EComponentMobility::Movable);
	// La planta no bloquea al jugador, pero sí la traza de interacción (canal Visibility).
	PlantMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	PlantMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PlantMesh->SetUsingAbsoluteScale(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (CubeMesh.Succeeded())
	{
		Bed->SetStaticMesh(CubeMesh.Object);
	}
	GrowingPlaceholder = ConeMesh.Succeeded() ? ConeMesh.Object : nullptr;
	RipePlaceholder = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
}

UFarmSubsystem* AExploredPlantActor::GetFarm() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFarmSubsystem>() : nullptr;
}

void AExploredPlantActor::BeginPlay()
{
	Super::BeginPlay();
	UFarmSubsystem* Farm = GetFarm();
	if (!Farm)
	{
		RefreshVisual();
		return;
	}
	PlotId = Farm->RegisterPlot(GetActorLocation(), Pieces);
	if (bHasScarecrow)
	{
		Farm->AddScarecrow(GetActorLocation());
	}
	PlotChangedHandle = Farm->OnPlotChanged.AddUObject(this, &AExploredPlantActor::HandlePlotChanged);
	RefreshVisual();
}

void AExploredPlantActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFarmSubsystem* Farm = GetFarm())
	{
		Farm->OnPlotChanged.Remove(PlotChangedHandle);
		// Al descargar el nivel la parcela se queda en el modelo (la guarda P-SAVE);
		// solo se retira si el actor se destruye de verdad (demolición).
		if (EndPlayReason == EEndPlayReason::Destroyed && PlotId != INDEX_NONE)
		{
			if (Farm->UnregisterPlot(PlotId) == EFarmResult::NeverRemoved)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Explored] La parcela %d tiene el limonero: se queda en el huerto."), PlotId);
			}
			if (bHasScarecrow)
			{
				Farm->RemoveScarecrow(GetActorLocation());
			}
		}
	}
	PlotChangedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void AExploredPlantActor::HandlePlotChanged(int32 ChangedPlotId)
{
	if (ChangedPlotId == INDEX_NONE || ChangedPlotId == PlotId)
	{
		RefreshVisual();
	}
}

void AExploredPlantActor::RefreshVisual()
{
	const UFarmSubsystem* Farm = GetFarm();
	const FFarmStageView View = Farm ? Farm->GetStageView(PlotId) : FFarmStageView();
	if (!View.bHasPlant)
	{
		PlantMesh->SetVisibility(false);
		return;
	}

	UStaticMesh* Mesh = nullptr;
	if (!View.MeshPath.IsEmpty())
	{
		Mesh = Cast<UStaticMesh>(FSoftObjectPath(View.MeshPath).TryLoad());
	}
	const bool bFinalStage = View.StageIndex == View.StageCount - 1;
	if (!Mesh)
	{
		Mesh = bFinalStage ? RipePlaceholder.Get() : GrowingPlaceholder.Get();
	}
	PlantMesh->SetStaticMesh(Mesh);
	PlantMesh->SetVisibility(Mesh != nullptr);

	// El marcador crece con el progreso; la sed lo inclina y la muerte lo aplasta.
	const float Scale = FMath::Lerp(MinPlantScale, MaxPlantScale, View.OverallProgress);
	FVector Scale3D(Scale, Scale, Scale);
	FRotator Tilt = FRotator::ZeroRotator;
	switch (View.Health)
	{
	case EPlantHealth::Thirsty:
		Tilt.Roll = 8.0f;
		break;
	case EPlantHealth::Wilting:
		Tilt.Roll = 20.0f;
		Scale3D.Z *= 0.75f;
		break;
	case EPlantHealth::Dead:
		Tilt.Roll = 35.0f;
		Scale3D.Z *= 0.35f;
		break;
	default:
		break;
	}
	PlantMesh->SetWorldScale3D(Scale3D);
	PlantMesh->SetWorldRotation(GetActorQuat() * Tilt.Quaternion());
	// Apoyada sobre el bancal: las mallas básicas miden 100 cm de alto con el origen en el centro.
	const float BedTop = Bed->Bounds.Origin.Z + Bed->Bounds.BoxExtent.Z;
	const FVector Base = GetActorLocation();
	PlantMesh->SetWorldLocation(FVector(Base.X, Base.Y, BedTop + 50.0f * Scale3D.Z));
}

bool AExploredPlantActor::HoldsWaterContainer(const AActor* InInstigator) const
{
	const UCarryComponent* Carry = InInstigator ? InInstigator->FindComponentByClass<UCarryComponent>() : nullptr;
	const UItemRegistrySubsystem* Registry = UItemRegistrySubsystem::Resolve(this);
	if (!Carry || !Registry)
	{
		return false;
	}
	for (const EHand Hand : {EHand::Left, EHand::Right})
	{
		if (const FItemInstance* Item = Carry->GetHandItemPtr(Hand))
		{
			if (Item->IsValid() && Registry->GetEffectiveProperty(*Item, ExploredPlantActorDetail::WaterContainerProperty) > 0.0f)
			{
				return true;
			}
		}
	}
	return false;
}

void AExploredPlantActor::GatherActions(const AActor* InInstigator, TArray<EFarmPlotAction>& OutActions, FName& OutSeedItem, EHand& OutSeedHand) const
{
	OutActions.Reset();
	OutSeedItem = NAME_None;
	const UFarmSubsystem* Farm = GetFarm();
	if (!Farm || PlotId == INDEX_NONE)
	{
		return;
	}
	const FFarmStageView View = Farm->GetStageView(PlotId);
	if (View.bHasPlant)
	{
		if (View.bHarvestReady)
		{
			OutActions.Add(EFarmPlotAction::Harvest);
		}
		if (View.Health == EPlantHealth::Dead)
		{
			OutActions.Add(EFarmPlotAction::ClearDead);
		}
		else if (View.bNeedsWater && HoldsWaterContainer(InInstigator))
		{
			OutActions.Add(EFarmPlotAction::Water);
		}
		return;
	}

	const UCarryComponent* Carry = InInstigator ? InInstigator->FindComponentByClass<UCarryComponent>() : nullptr;
	if (!Carry)
	{
		return;
	}
	for (const EHand Hand : {EHand::Right, EHand::Left})
	{
		const FItemInstance* Item = Carry->GetHandItemPtr(Hand);
		if (Item && Item->IsValid() && !Farm->PlantForItem(PlotId, Item->DefinitionId).IsNone())
		{
			OutSeedItem = Item->DefinitionId;
			OutSeedHand = Hand;
			OutActions.Add(EFarmPlotAction::Plant);
			return;
		}
	}
}

void AExploredPlantActor::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	const UFarmSubsystem* Farm = GetFarm();
	const AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	TArray<EFarmPlotAction> Actions;
	FName SeedItem;
	EHand SeedHand = EHand::Right;
	GatherActions(Player, Actions, SeedItem, SeedHand);

	const FFarmStageView View = Farm ? Farm->GetStageView(PlotId) : FFarmStageView();
	for (const EFarmPlotAction Action : Actions)
	{
		switch (Action)
		{
		case EFarmPlotAction::Harvest:
			OutVerbs.Add(FText::Format(NSLOCTEXT("Explored", "Verb_Harvest", "Cosechar {0}"), ExploredPlantActorDetail::PlantName(Farm, View.PlantId)));
			break;
		case EFarmPlotAction::ClearDead:
			OutVerbs.Add(NSLOCTEXT("Explored", "Verb_ClearDeadPlant", "Arrancar la planta seca"));
			break;
		case EFarmPlotAction::Plant:
			OutVerbs.Add(FText::Format(NSLOCTEXT("Explored", "Verb_Plant", "Plantar {0}"),
				ExploredPlantActorDetail::PlantName(Farm, Farm ? Farm->PlantForItem(PlotId, SeedItem) : NAME_None)));
			break;
		case EFarmPlotAction::Water:
			OutVerbs.Add(NSLOCTEXT("Explored", "Verb_Water", "Regar"));
			break;
		}
	}
}

bool AExploredPlantActor::CanInteract_Implementation(AActor* InInstigator) const
{
	TArray<EFarmPlotAction> Actions;
	FName SeedItem;
	EHand SeedHand = EHand::Right;
	GatherActions(InInstigator, Actions, SeedItem, SeedHand);
	return Actions.Num() > 0;
}

void AExploredPlantActor::Interact_Implementation(AActor* InInstigator)
{
	UFarmSubsystem* Farm = GetFarm();
	TArray<EFarmPlotAction> Actions;
	FName SeedItem;
	EHand SeedHand = EHand::Right;
	GatherActions(InInstigator, Actions, SeedItem, SeedHand);
	if (!Farm || Actions.Num() == 0)
	{
		return;
	}

	switch (Actions[0])
	{
	case EFarmPlotAction::Harvest:
	{
		FFarmHarvest Harvest;
		if (Farm->Harvest(PlotId, Harvest) == EFarmResult::Ok)
		{
			SpawnHarvest(Harvest);
		}
		break;
	}
	case EFarmPlotAction::ClearDead:
		Farm->ClearPlot(PlotId);
		break;
	case EFarmPlotAction::Plant:
		if (Farm->Plant(PlotId, SeedItem) == EFarmResult::Ok)
		{
			// El objeto se gasta solo si el modelo aceptó la siembra.
			if (UCarryComponent* Carry = InInstigator ? InInstigator->FindComponentByClass<UCarryComponent>() : nullptr)
			{
				Carry->ConsumeOneFromHand(SeedHand);
			}
		}
		break;
	case EFarmPlotAction::Water:
		Farm->Water(PlotId);
		break;
	}
}

void AExploredPlantActor::SpawnHarvest(const FFarmHarvest& Harvest)
{
	UWorld* World = GetWorld();
	if (!World || Harvest.Item.IsNone())
	{
		return;
	}
	// Los frutos caen alrededor de la planta y se recogen como cualquier objeto del mundo.
	for (int32 I = 0; I < Harvest.Count; ++I)
	{
		const float Angle = UE_TWO_PI * static_cast<float>(I) / static_cast<float>(FMath::Max(1, Harvest.Count));
		const FVector Offset(FMath::Cos(Angle) * ExploredPlantActorDetail::HarvestDropRadius,
			FMath::Sin(Angle) * ExploredPlantActorDetail::HarvestDropRadius, ExploredPlantActorDetail::HarvestDropHeight);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AExploredItemActor* Fruit = World->SpawnActor<AExploredItemActor>(AExploredItemActor::StaticClass(),
			GetActorLocation() + Offset, FRotator::ZeroRotator, Params);
		if (Fruit)
		{
			FItemInstance Instance;
			Instance.DefinitionId = Harvest.Item;
			Fruit->InitializeFromInstance(Instance);
		}
	}
}
