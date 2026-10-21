#include "WorldGen/ExploredIslandPopulator.h"

#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

#include "Cooking/ExploredFire.h"
#include "Explored.h"
#include "Fishing/ExploredFishingSpot.h"
#include "Items/ExploredIslandStateSubsystem.h"
#include "Items/ExploredPickup.h"
#include "Narrative/ExploredBeaconActors.h"
#include "Boats/ExploredRaft.h"
#include "Narrative/ExploredSupplyCrate.h"
#include "Narrative/LighthouseLamp.h"
#include "Weather/ExploredRainBarrel.h"
#include "WorldGen/ExploredCoconutPalm.h"
#include "WorldGen/ExploredPlaceholder.h"
#include "WorldGen/ExploredShelterVolume.h"
#include "WorldGen/IslandLayout.h"
#include "WorldGen/IslandLayoutLoader.h"
#include "WorldGen/IslandPropsLoader.h"

namespace ExploredIslandPopulatorDetail
{
	constexpr double TraceTopCm = 20000.0;
	constexpr double TraceBottomCm = -5000.0;
	const FName PropTag(TEXT("IslandProp"));
}

AExploredIslandPopulator::AExploredIslandPopulator()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	ToonMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/M_Toon.M_Toon")));
	// Tipos que ya tienen actor de juego; el resto es AExploredPlaceholder hasta su tramo de F3.
	ClassRegistry.Add(TEXT("hoguera_fija"), AExploredFire::StaticClass());
	ClassRegistry.Add(TEXT("shelter_volume"), AExploredShelterVolume::StaticClass());
	// F3b: recursos, pesca, agua y caja de suministros (GDD v3 §5.3-5.5).
	ClassRegistry.Add(TEXT("palma"), AExploredCoconutPalm::StaticClass());
	for (const TCHAR* Resource : { TEXT("lena"), TEXT("fibra"), TEXT("cana_bambu"), TEXT("tronco"), TEXT("lente") })
	{
		ClassRegistry.Add(Resource, AExploredPickup::StaticClass());
	}
	ClassRegistry.Add(TEXT("punto_pesca"), AExploredFishingSpot::StaticClass());
	ClassRegistry.Add(TEXT("barril_lluvia"), AExploredRainBarrel::StaticClass());
	ClassRegistry.Add(TEXT("caja_suministros"), AExploredSupplyCrate::StaticClass());
	ClassRegistry.Add(TEXT("balsa_sitio"), AExploredRaftSite::StaticClass());
	// F3c: historia, linterna y final (GDD v3 §2, §5.6 y §5.9).
	ClassRegistry.Add(TEXT("pagina"), AExploredNotebookPage::StaticClass());
	ClassRegistry.Add(TEXT("faro_torre"), AExploredLighthouseTower::StaticClass());
	ClassRegistry.Add(TEXT("faro_linterna"), ALighthouseLamp::StaticClass());
}

void AExploredIslandPopulator::BeginPlay()
{
	Super::BeginPlay();
	if (bPopulateOnBeginPlay && HasAuthority())
	{
		Populate();
	}
}

void AExploredIslandPopulator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Clear();
	Super::EndPlay(EndPlayReason);
}

int32 AExploredIslandPopulator::Populate()
{
	Clear();
	const TSharedRef<const FIslandProps> Props = ExploredIslandProps::Get();
	UMaterialInterface* Material = ToonMaterial.LoadSynchronous();
	if (!Material)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Isla] Sin %s: las primitivas de reserva usan el material por defecto"), *ToonMaterial.ToString());
	}
	TMap<FName, float> GroundById;
	for (const FIslandProp& Prop : Props->Props)
	{
		const float* AnchorGround = Prop.Anchor.IsNone() ? nullptr : GroundById.Find(Prop.Anchor);
		const float Ground = AnchorGround ? *AnchorGround : TraceGroundMeters(Prop.Location);
		GroundById.Add(Prop.Id, Ground);
		if (AActor* Actor = SpawnProp(*Props, Prop, Ground, Material))
		{
			Spawned.Add(Actor);
			ById.Add(Prop.Id, Actor);
		}
	}
	if (UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this))
	{
		Island->ApplyToWorld();
	}
	UE_LOG(LogExplored, Display, TEXT("[Isla] %d colocaciones creadas (%d de reserva, %d con actor de juego)"),
		Spawned.Num(), PlaceholderCount, Spawned.Num() - PlaceholderCount);
	return Spawned.Num();
}

void AExploredIslandPopulator::Clear()
{
	for (AActor* Actor : Spawned)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Spawned.Reset();
	ById.Reset();
	PlaceholderCount = 0;
}

AActor* AExploredIslandPopulator::FindSpawned(FName PropId) const
{
	const TWeakObjectPtr<AActor>* Found = ById.Find(PropId);
	return Found ? Found->Get() : nullptr;
}

float AExploredIslandPopulator::TraceGroundMeters(const FVector2D& LocationMeters) const
{
	using namespace ExploredIslandPopulatorDetail;
	const FVector XY(LocationMeters.X * 100.0, LocationMeters.Y * 100.0, 0.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IslandPopulatorGround), true, this);
	// Solo el terreno: lo ya colocado (casco, torre...) no es suelo para lo siguiente.
	for (const AActor* Actor : Spawned)
	{
		Params.AddIgnoredActor(Actor);
	}
	FHitResult Hit;
	const UWorld* World = GetWorld();
	if (World && World->LineTraceSingleByObjectType(Hit, XY + FVector(0.0, 0.0, TraceTopCm), XY + FVector(0.0, 0.0, TraceBottomCm),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return static_cast<float>(Hit.ImpactPoint.Z / 100.0);
	}
	return ExploredIsland::Get()->SampleColumn(static_cast<float>(LocationMeters.X), static_cast<float>(LocationMeters.Y)).Height;
}

AActor* AExploredIslandPopulator::SpawnProp(const FIslandProps& Props, const FIslandProp& Prop, float GroundMeters, UMaterialInterface* Material)
{
	const FIslandPropType* Type = Props.FindType(Prop.Type);
	UWorld* World = GetWorld();
	if (!Type || !World)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator(Prop.Pitch, Prop.Yaw, 0.0f), IslandPropsPlacement::WorldLocation(Prop, GroundMeters));
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const TSubclassOf<AActor>* Registered = ClassRegistry.Find(Prop.Type);
	AActor* Actor = nullptr;
	if (Registered && *Registered)
	{
		Actor = World->SpawnActor<AActor>(*Registered, Transform, Params);
		if (AExploredPlaceholder* GameplayProp = Cast<AExploredPlaceholder>(Actor))
		{
			GameplayProp->Configure(Prop, *Type, Props.MeshPathFor(Prop), Material);
		}
		if (AExploredShelterVolume* Shelter = Cast<AExploredShelterVolume>(Actor))
		{
			Shelter->ShelterId = Prop.Id;
			Shelter->bRespawnPoint = Prop.bRespawn;
			Shelter->SetExtentMeters(Prop.Extent);
		}
	}
	else
	{
		AExploredPlaceholder* Placeholder = World->SpawnActor<AExploredPlaceholder>(AExploredPlaceholder::StaticClass(), Transform, Params);
		if (Placeholder)
		{
			Placeholder->Configure(Prop, *Type, Props.MeshPathFor(Prop), Material);
			++PlaceholderCount;
		}
		Actor = Placeholder;
	}
	if (!Actor)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Isla] No se pudo crear %s (%s)"), *Prop.Id.ToString(), *Prop.Type.ToString());
		return nullptr;
	}
	Actor->Tags.AddUnique(ExploredIslandPopulatorDetail::PropTag);
	Actor->Tags.AddUnique(Prop.Id);
#if WITH_EDITOR
	Actor->SetActorLabel(Prop.Id.ToString());
#endif
	return Actor;
}
