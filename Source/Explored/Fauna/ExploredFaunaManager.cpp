#include "Fauna/ExploredFaunaManager.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#include "Core/ExploredRandom.h"
#include "Fauna/ExploredMarineCreature.h"
#include "Ocean/ExploredOcean.h"
#include "Ocean/OceanCurrents.h"
#include "Player/SwimComponent.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"

/** Estado que cambia cada fotograma y que leen las consultas del mundo (sin punteros a UObject). */
struct FFaunaQueryState
{
	float TideFlow = 0.0f;
	float TideStrength = 0.7f;
	float Wind = 0.2f;
	TMap<FIntPoint, float> SeabedCache;
};

namespace FaunaManagerDetail
{
	/** WorldGen trabaja en metros; la fauna en centímetros. */
	constexpr float MetersToCm = 100.0f;
	/** Lado de la rejilla de la caché de alturas del fondo (cm). */
	constexpr float HeightCacheStepCm = 200.0f;
	constexpr int32 HeightCacheMaxEntries = 1 << 16;
	constexpr float CellRefreshSeconds = 2.0f;
	/** La sangre deja de oler pasado este tiempo (s). */
	constexpr float BloodLifetimeSeconds = 600.0f;
	constexpr float NoiseBurstDecayPerSecond = 0.8f;

	/** Variantes de peces de arrecife de Tools/Blender/animals/fish.py. */
	const TCHAR* ReefFishMeshes[] = {
		TEXT("ClownFish"), TEXT("ButterflyFish"), TEXT("ParrotFish"), TEXT("SurgeonFish"), TEXT("Wrasse"), TEXT("Grouper"),
	};

	/** Rumbo en radianes a partir de una semilla (migración, viaje de la ballena). */
	float SeedAngle(uint32 Seed)
	{
		return ExploredHash::ToUnitFloat(ExploredHash::Hash32(Seed ^ 0x7A11u)) * UE_TWO_PI;
	}

	float CullDistanceFor(EFaunaSpecies Species)
	{
		switch (Species)
		{
		case EFaunaSpecies::Gull:
		case EFaunaSpecies::Frigatebird:
			return 40000.0f;
		case EFaunaSpecies::OpenSeaFish:
			return 12000.0f;
		default:
			return 8000.0f;
		}
	}

	float HomeRadiusFor(EFaunaSpecies Species)
	{
		switch (Species)
		{
		case EFaunaSpecies::HumpbackWhale: return 60000.0f;
		case EFaunaSpecies::TigerShark: return 8000.0f;
		case EFaunaSpecies::Dolphin: return 6000.0f;
		case EFaunaSpecies::SeaTurtle: return 2500.0f;
		default: return 3000.0f;
		}
	}
}

AExploredFaunaManager::AExploredFaunaManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

void AExploredFaunaManager::BeginPlay()
{
	Super::BeginPlay();
	Density = MakeShared<FTerrainDensity>(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
	Ocean = Cast<AExploredOcean>(UGameplayStatics::GetActorOfClass(this, AExploredOcean::StaticClass()));
	BuildWorldQuery();
}

void AExploredFaunaManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const FCreatureEntry& Entry : Creatures)
	{
		if (AExploredMarineCreature* Actor = Entry.Actor.Get())
		{
			Actor->Destroy();
		}
	}
	Creatures.Reset();
	Schools.Reset();
	Flocks.Reset();
	Super::EndPlay(EndPlayReason);
}

void AExploredFaunaManager::BuildWorldQuery()
{
	using namespace FaunaManagerDetail;
	const FArchipelagoLayout& Layout = Density->GetLayout();
	TArray<FVector2D> IslandCenters;
	for (const FIslandDesc& Island : Layout.Islands)
	{
		IslandCenters.Add(Island.Center * MetersToCm);
	}
	const TArray<FOceanStrait> Straits = FOceanCurrents::BuildStraits(Layout);
	const TSharedRef<FFaunaQueryState> State = MakeShared<FFaunaQueryState>();
	const TSharedPtr<FTerrainDensity> DensityRef = Density;
	const TWeakObjectPtr<AExploredOcean> WeakOcean = Ocean;

	// Nada de UObject crudos en las lambdas: densidad y estado compartidos, océano débil.
	WorldQuery.Seabed = [DensityRef, State](const FVector2D& P)
	{
		const FIntPoint Key(FMath::FloorToInt(P.X / HeightCacheStepCm), FMath::FloorToInt(P.Y / HeightCacheStepCm));
		if (const float* Cached = State->SeabedCache.Find(Key))
		{
			return *Cached;
		}
		if (State->SeabedCache.Num() > HeightCacheMaxEntries)
		{
			State->SeabedCache.Reset();
		}
		const float X = (Key.X + 0.5f) * HeightCacheStepCm / MetersToCm;
		const float Y = (Key.Y + 0.5f) * HeightCacheStepCm / MetersToCm;
		const float Height = DensityRef->SampleColumn(X, Y).Height * MetersToCm;
		State->SeabedCache.Add(Key, Height);
		return Height;
	};
	WorldQuery.Surface = [WeakOcean](const FVector2D& P)
	{
		const AExploredOcean* OceanActor = WeakOcean.Get();
		return OceanActor ? OceanActor->GetWaterHeightAt(FVector(P.X, P.Y, 0.0)) : 0.0f;
	};
	WorldQuery.Current = [Straits, State](const FVector2D& P)
	{
		return FOceanCurrents::CurrentAt(Straits, P, State->TideFlow, State->TideStrength, State->Wind);
	};
	WorldQuery.NearestLand = [IslandCenters](const FVector2D& P)
	{
		FVector2D Best = P;
		double BestDist = TNumericLimits<double>::Max();
		for (const FVector2D& Center : IslandCenters)
		{
			const double Dist = FVector2D::DistSquared(Center, P);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = Center;
			}
		}
		return Best;
	};

	// El estado dinámico se actualiza desde Tick a través de este puntero compartido.
	DynamicState = State;
}

void AExploredFaunaManager::ReportPlayerNoise(float Noise01)
{
	NoiseBurst = FMath::Max(NoiseBurst, FMath::Clamp(Noise01, 0.0f, 1.0f));
}

void AExploredFaunaManager::AddBloodInWater(const FVector& LocationCm, float Amount01)
{
	FBloodSource Source;
	Source.OriginCm = LocationCm;
	Source.Amount01 = FMath::Clamp(Amount01, 0.0f, 1.0f);
	Blood.Add(Source);
}

void AExploredFaunaManager::SetPlayerBoatState(bool bInBoat, bool bCarriesFish)
{
	bPlayerInBoat = bInBoat;
	bPlayerCarriesFish = bCarriesFish;
}

void AExploredFaunaManager::RegisterStealableFish(AActor* Item)
{
	if (Item)
	{
		StealableFish.AddUnique(Item);
	}
}

void AExploredFaunaManager::StartTurtleNesting(const FVector& BeachCm, float RadiusCm)
{
	for (const FCreatureEntry& Entry : Creatures)
	{
		AExploredMarineCreature* Actor = Entry.Actor.Get();
		if (Actor && Actor->GetBrain().GetSpecies() == EFaunaSpecies::SeaTurtle
			&& FVector::Dist2D(Actor->GetActorLocation(), BeachCm) <= RadiusCm)
		{
			Actor->GetBrainMutable().BeginNesting(BeachCm);
		}
	}
}

FFaunaStimuli AExploredFaunaManager::GatherStimuli(float DeltaSeconds)
{
	using namespace FaunaManagerDetail;
	FFaunaStimuli S;
	S.TimeSeconds = GameSeconds;
	S.bPeaceful = bPeaceful;
	S.bPlayerInBoat = bPlayerInBoat;
	S.bPlayerCarriesFish = bPlayerCarriesFish;

	const UWorld* World = GetWorld();
	if (const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr)
	{
		S.Hours = Time->GetHours();
	}

	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		S.bHasPlayer = true;
		S.PlayerCm = Pawn->GetActorLocation();
		S.PlayerVelocityCmS = Pawn->GetVelocity();
		const USwimComponent* Swim = Pawn->FindComponentByClass<USwimComponent>();
		S.bPlayerInWater = Swim && Swim->IsInWater();
		const float Speed = static_cast<float>(S.PlayerVelocityCmS.Size());
		const float Base = S.bPlayerInWater ? FMath::Lerp(0.1f, 0.7f, FMath::Clamp(Speed / 500.0f, 0.0f, 1.0f)) : (bPlayerInBoat ? 0.3f : 0.0f);
		S.PlayerNoise01 = FMath::Max(Base, NoiseBurst);
	}
	NoiseBurst = FMath::Max(0.0f, NoiseBurst - NoiseBurstDecayPerSecond * DeltaSeconds);

	for (int32 I = Blood.Num() - 1; I >= 0; --I)
	{
		Blood[I].AgeSeconds += DeltaSeconds;
		if (Blood[I].AgeSeconds > BloodLifetimeSeconds)
		{
			Blood.RemoveAtSwap(I);
		}
	}
	S.Blood = Blood;

	StealableFish.RemoveAll([](const TWeakObjectPtr<AActor>& Item) { return !Item.IsValid(); });
	for (const TWeakObjectPtr<AActor>& Item : StealableFish)
	{
		const AActor* Actor = Item.Get();
		FStealableItem Stealable;
		Stealable.PositionCm = Actor->GetActorLocation();
		// Al aire: ni en la mano, ni en una cesta, ni guardado (oculto).
		Stealable.bExposed = Actor->GetAttachParentActor() == nullptr && !Actor->IsHidden();
		S.Stealables.Add(Stealable);
	}

	for (const FCreatureEntry& Entry : Creatures)
	{
		const AExploredMarineCreature* Actor = Entry.Actor.Get();
		if (Actor)
		{
			const EFaunaSpecies Species = Actor->GetBrain().GetSpecies();
			if (Species == EFaunaSpecies::ReefShark || Species == EFaunaSpecies::TigerShark)
			{
				S.Predators.Add(Actor->GetBrain().GetPosition());
			}
		}
	}
	return S;
}

FFaunaCellContext AExploredFaunaManager::CellContext(const FIntPoint& Cell) const
{
	using namespace FaunaManagerDetail;
	FFaunaCellContext Ctx;
	const FVector2D CenterCm = FFaunaSpawnRules::CellCenter(Cell);
	const FVector2D CenterM = CenterCm / MetersToCm;
	const FTerrainColumn Column = Density->SampleColumn(CenterM.X, CenterM.Y);
	Ctx.SeabedZCm = Column.Height * MetersToCm;

	float BestCoast = TNumericLimits<float>::Max();
	const FIslandDesc* Nearest = nullptr;
	float NearestDist = TNumericLimits<float>::Max();
	for (const FIslandDesc& Island : Density->GetLayout().Islands)
	{
		const float Dist = static_cast<float>(FVector2D::Distance(CenterM, Island.Center));
		BestCoast = FMath::Min(BestCoast, FMath::Max(0.0f, Dist - Island.Radius) * MetersToCm);
		if (Dist - Island.Radius < NearestDist)
		{
			NearestDist = Dist - Island.Radius;
			Nearest = &Island;
		}
	}
	Ctx.DistanceToCoastCm = BestCoast;
	if (Nearest)
	{
		const float Dist = static_cast<float>(FVector2D::Distance(CenterM, Nearest->Center));
		Ctx.bLagoon = Nearest->Archetype == EIslandArchetype::WhiteSands && Dist < Nearest->Radius * 0.7f && Column.Height < 0.0f;
		Ctx.bNearTeeth = Nearest->Archetype == EIslandArchetype::Teeth && Dist < Nearest->Radius + 400.0f;
	}
	Ctx.bWhalePassage = bWhalePassage;
	const UWorld* World = GetWorld();
	if (const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr)
	{
		Ctx.Hours = Time->GetHours();
	}
	return Ctx;
}

void AExploredFaunaManager::RefreshCells(const FVector& Center)
{
	const FIntPoint Origin = FFaunaSpawnRules::CellOf(FVector2D(Center.X, Center.Y));
	TSet<FIntPoint> Wanted;
	for (int32 DX = -CellRadius; DX <= CellRadius; ++DX)
	{
		for (int32 DY = -CellRadius; DY <= CellRadius; ++DY)
		{
			Wanted.Add(FIntPoint(Origin.X + DX, Origin.Y + DY));
		}
	}

	// Fuera: celdas que ya no están en el radio (con una de margen para no parpadear en el borde).
	TArray<FIntPoint> ToRemove;
	for (const FIntPoint& Cell : LoadedCells)
	{
		if (FMath::Abs(Cell.X - Origin.X) > CellRadius + 1 || FMath::Abs(Cell.Y - Origin.Y) > CellRadius + 1)
		{
			ToRemove.Add(Cell);
		}
	}
	for (const FIntPoint& Cell : ToRemove)
	{
		DespawnCell(Cell);
		LoadedCells.Remove(Cell);
	}

	for (const FIntPoint& Cell : Wanted)
	{
		if (LoadedCells.Contains(Cell))
		{
			continue;
		}
		LoadedCells.Add(Cell);
		TArray<FFaunaSpawn> Spawns;
		FFaunaSpawnRules::SpawnsForCell(FArchipelagoLayout::OfficialSeed, Cell, CellContext(Cell), Spawns);
		for (const FFaunaSpawn& Spawn : Spawns)
		{
			SpawnGroup(Spawn, Cell);
		}
	}
}

void AExploredFaunaManager::SpawnGroup(const FFaunaSpawn& Spawn, const FIntPoint& Cell)
{
	using namespace FaunaManagerDetail;
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Spawn.Species);
	const float Angle = SeedAngle(Spawn.Seed);

	if (Spawn.Species == EFaunaSpecies::ReefFish || Spawn.Species == EFaunaSpecies::OpenSeaFish)
	{
		FSchoolEntry& Entry = Schools.AddDefaulted_GetRef();
		FFishSchoolConfig Config;
		Config.Kind = Spawn.Species == EFaunaSpecies::ReefFish ? EFishSchoolKind::Reef : EFishSchoolKind::OpenSea;
		Config.HomeCm = Spawn.PositionCm;
		Config.Count = FMath::Min(Spawn.GroupSize, MaxFishPerSchool);
		Config.Seed = Spawn.Seed;
		Config.MigrationDirection = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
		Entry.Model.Init(Config, WorldQuery);
		Entry.Species = Spawn.Species;
		Entry.Cell = Cell;
		Entry.LodId = NextLodId++;
		const FName MeshName = Spawn.Species == EFaunaSpecies::ReefFish
			? FName(ReefFishMeshes[Spawn.Seed % UE_ARRAY_COUNT(ReefFishMeshes)]) : FName(Info.Mesh);
		Entry.ComponentIndex = AcquireInstancedComponent(Spawn.Species, Config.Count);
		if (UInstancedStaticMeshComponent* Component = GroupComponents.IsValidIndex(Entry.ComponentIndex) ? GroupComponents[Entry.ComponentIndex].Get() : nullptr)
		{
			Component->SetStaticMesh(LoadMesh(MeshName));
		}
		for (int32 I = 0; I < Config.Count; ++I)
		{
			Entry.Anim.Add({FFaunaAnimation::InitialPhase(Spawn.Seed + I)});
		}
		return;
	}

	if (Info.bBird)
	{
		FFlockEntry& Entry = Flocks.AddDefaulted_GetRef();
		FBirdFlockConfig Config;
		Config.Species = Spawn.Species;
		Config.HomeCm = FVector(Spawn.PositionCm.X, Spawn.PositionCm.Y, 0.0);
		Config.Count = Spawn.GroupSize;
		Config.Seed = Spawn.Seed;
		Entry.Model.Init(Config, WorldQuery);
		Entry.Species = Spawn.Species;
		Entry.Cell = Cell;
		Entry.LodId = NextLodId++;
		Entry.ComponentIndex = AcquireInstancedComponent(Spawn.Species, Config.Count);
		if (UInstancedStaticMeshComponent* Component = GroupComponents.IsValidIndex(Entry.ComponentIndex) ? GroupComponents[Entry.ComponentIndex].Get() : nullptr)
		{
			Component->SetStaticMesh(LoadMesh(FName(Info.Mesh)));
		}
		for (int32 I = 0; I < Config.Count; ++I)
		{
			Entry.Anim.Add({FFaunaAnimation::InitialPhase(Spawn.Seed + I)});
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UStaticMesh* Mesh = LoadMesh(FName(Info.Mesh));
	for (int32 I = 0; I < Spawn.GroupSize; ++I)
	{
		FMarineBrainConfig Config;
		Config.Species = Spawn.Species;
		Config.SpawnCm = Spawn.PositionCm + FVector(I * 250.0, (I % 2) * 250.0, 0.0);
		Config.HomeCm = Spawn.PositionCm;
		Config.HomeRadiusCm = HomeRadiusFor(Spawn.Species);
		Config.Seed = Spawn.Seed + static_cast<uint32>(I) * 7919u;
		Config.TravelDirection = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
		Config.EscortSide = (I % 2 == 0) ? 1.0f : -1.0f;
		Config.EscortOffsetCm = (I / 2) * 180.0f;

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AExploredMarineCreature* Actor = World->SpawnActor<AExploredMarineCreature>(AExploredMarineCreature::StaticClass(),
			Config.SpawnCm, FRotator::ZeroRotator, Params);
		if (!Actor)
		{
			continue;
		}
		Actor->InitCreature(Config, WorldQuery, Mesh);
		Actor->SpawnCell = Cell;
		FCreatureEntry& Entry = Creatures.AddDefaulted_GetRef();
		Entry.Actor = Actor;
		Entry.LodId = NextLodId++;
	}
}

void AExploredFaunaManager::DespawnCell(const FIntPoint& Cell)
{
	for (int32 I = Schools.Num() - 1; I >= 0; --I)
	{
		if (Schools[I].Cell == Cell)
		{
			ReleaseInstancedComponent(Schools[I].ComponentIndex);
			Schools.RemoveAtSwap(I);
		}
	}
	for (int32 I = Flocks.Num() - 1; I >= 0; --I)
	{
		if (Flocks[I].Cell == Cell)
		{
			ReleaseInstancedComponent(Flocks[I].ComponentIndex);
			Flocks.RemoveAtSwap(I);
		}
	}
	for (int32 I = Creatures.Num() - 1; I >= 0; --I)
	{
		AExploredMarineCreature* Actor = Creatures[I].Actor.Get();
		if (!Actor || Actor->SpawnCell == Cell)
		{
			if (Actor)
			{
				Actor->Destroy();
			}
			Creatures.RemoveAtSwap(I);
		}
	}
}

int32 AExploredFaunaManager::AcquireInstancedComponent(EFaunaSpecies Species, int32 Count)
{
	using namespace FaunaManagerDetail;
	UInstancedStaticMeshComponent* Component = nullptr;
	int32 Index = INDEX_NONE;
	if (FreeComponents.Num() > 0)
	{
		Index = FreeComponents.Pop();
		Component = GroupComponents[Index];
	}
	else
	{
		Component = NewObject<UInstancedStaticMeshComponent>(this);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(true);
		Component->SetupAttachment(RootComponent);
		Component->RegisterComponent();
		Index = GroupComponents.Add(Component);
	}
	Component->ClearInstances();
	Component->SetNumCustomDataFloats(4);
	const float Cull = CullDistanceFor(Species);
	Component->SetCullDistances(static_cast<int32>(Cull * 0.8f), static_cast<int32>(Cull));
	TArray<FTransform> Transforms;
	Transforms.Init(FTransform::Identity, Count);
	Component->AddInstances(Transforms, false, true);
	Component->SetVisibility(true);
	return Index;
}

void AExploredFaunaManager::ReleaseInstancedComponent(int32 Index)
{
	if (!GroupComponents.IsValidIndex(Index))
	{
		return;
	}
	if (UInstancedStaticMeshComponent* Component = GroupComponents[Index].Get())
	{
		Component->ClearInstances();
		Component->SetVisibility(false);
	}
	FreeComponents.Add(Index);
}

UStaticMesh* AExploredFaunaManager::LoadMesh(FName MeshName)
{
	if (const TObjectPtr<UStaticMesh>* Cached = MeshCache.Find(MeshName))
	{
		return Cached->Get();
	}
	// Ruta de import_meshes.py (familia «Fauna»): /Game/Generated/Meshes/Fauna/<Especie>/SM_<Especie>_Body.
	const FString Name = MeshName.ToString();
	const FString Path = FString::Printf(TEXT("/Game/Generated/Meshes/Fauna/%s/SM_%s_Body.SM_%s_Body"), *Name, *Name, *Name);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
	MeshCache.Add(MeshName, Mesh);
	return Mesh;
}

void AExploredFaunaManager::WriteInstances(UInstancedStaticMeshComponent* Component, const FBoidsModel& Boids, EFaunaSpecies Species,
	TArray<FFaunaAnimState>& Anim, float DeltaSeconds)
{
	if (!Component)
	{
		return;
	}
	const TArray<FBoidAgent>& Agents = Boids.GetAgents();
	if (Component->GetInstanceCount() != Agents.Num())
	{
		// Se han sacado peces del banco (pesca): se rehacen las instancias.
		Component->ClearInstances();
		TArray<FTransform> Empty;
		Empty.Init(FTransform::Identity, Agents.Num());
		Component->AddInstances(Empty, false, true);
	}
	Anim.SetNum(Agents.Num());

	TArray<FTransform> Transforms;
	Transforms.Reserve(Agents.Num());
	for (int32 I = 0; I < Agents.Num(); ++I)
	{
		const FBoidAgent& Agent = Agents[I];
		const FRotator Rotation = Agent.Velocity.IsNearlyZero() ? FRotator::ZeroRotator : Agent.Velocity.Rotation();
		Transforms.Add(FTransform(Rotation, Agent.Position));
		const FFaunaAnimParams Params = FFaunaAnimation::Advance(Species, Anim[I], static_cast<float>(Agent.Velocity.Size()),
			static_cast<float>(Agent.Velocity.Z), DeltaSeconds);
		const float Data[4] = {Params.Phase01, Params.Amplitude, Params.FrequencyHz, Params.Secondary};
		Component->SetCustomData(I, MakeArrayView(Data, 4), false);
	}
	Component->BatchUpdateInstancesTransforms(0, Transforms, true, false, true);
	Component->MarkRenderStateDirty();
}

void AExploredFaunaManager::Tick(float DeltaSeconds)
{
	using namespace FaunaManagerDetail;
	Super::Tick(DeltaSeconds);
	if (!Density)
	{
		return;
	}
	GameSeconds += DeltaSeconds;
	++FrameCounter;

	// Corrientes: marea y viento del instante.
	const UWorld* World = GetWorld();
	if (DynamicState.IsValid())
	{
		const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
		const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
		const float TotalDays = Time ? Time->GetTotalDays() : 0.0f;
		DynamicState->TideFlow = FOceanTide::Flow(TotalDays);
		DynamicState->TideStrength = FOceanTide::SpringNeapFactor(Time ? Time->GetMoonPhase() : 0.0f);
		DynamicState->Wind = Weather ? Weather->GetCurrent().Wind : 0.2f;
	}

	const FFaunaStimuli Stimuli = GatherStimuli(DeltaSeconds);
	const FVector Viewer = Stimuli.bHasPlayer ? Stimuli.PlayerCm : GetActorLocation();

	CellRefreshTimer -= DeltaSeconds;
	if (CellRefreshTimer <= 0.0f)
	{
		CellRefreshTimer = CellRefreshSeconds;
		RefreshCells(Viewer);
	}

	auto LodDelta = [this, DeltaSeconds, &Viewer](EFaunaSpecies Species, const FVector& Where, uint32 LodId, EFaunaLodTier& Tier, float& Pending)
	{
		const FFaunaLodSettings Settings = FFaunaLod::ForSpecies(Species);
		Tier = FFaunaLod::Tier(static_cast<float>(FVector::Dist(Where, Viewer)), Tier, Settings);
		if (Tier == EFaunaLodTier::Frozen)
		{
			Pending = 0.0f;
			return 0.0f;
		}
		Pending += DeltaSeconds;
		if (!FFaunaLod::ShouldTick(Tier, FrameCounter, LodId, Settings.ReducedInterval))
		{
			return 0.0f;
		}
		const float Delta = Pending;
		Pending = 0.0f;
		return Delta;
	};

	for (FSchoolEntry& Entry : Schools)
	{
		const float Delta = LodDelta(Entry.Species, Entry.Model.GetBoids().Centroid(), Entry.LodId, Entry.Tier, Entry.PendingDelta);
		if (Delta > 0.0f)
		{
			Entry.Model.Tick(Delta, Stimuli, WorldQuery);
			WriteInstances(GroupComponents.IsValidIndex(Entry.ComponentIndex) ? GroupComponents[Entry.ComponentIndex].Get() : nullptr,
				Entry.Model.GetBoids(), Entry.Species, Entry.Anim, Delta);
		}
	}

	for (FFlockEntry& Entry : Flocks)
	{
		const float Delta = LodDelta(Entry.Species, Entry.Model.GetBoids().Centroid(), Entry.LodId, Entry.Tier, Entry.PendingDelta);
		if (Delta <= 0.0f)
		{
			continue;
		}
		const FBirdFlockEvents Events = Entry.Model.Tick(Delta, Stimuli, WorldQuery);
		WriteInstances(GroupComponents.IsValidIndex(Entry.ComponentIndex) ? GroupComponents[Entry.ComponentIndex].Get() : nullptr,
			Entry.Model.GetBoids(), Entry.Species, Entry.Anim, Delta);
		// Stimuli.Stealables sigue el orden de StealableFish (ya sin inválidos).
		if (StealableFish.IsValidIndex(Events.StolenItemIndex))
		{
			AActor* Item = StealableFish[Events.StolenItemIndex].Get();
			StealableFish.RemoveAt(Events.StolenItemIndex);
			if (Item)
			{
				OnFishStolen.Broadcast(Item);
				OnFaunaMoment.Broadcast(Entry.Species, FName(TEXT("Steal")), Item->GetActorLocation());
			}
			break;  // los índices del resto de bandadas ya no valen este fotograma
		}
	}

	for (FCreatureEntry& Entry : Creatures)
	{
		AExploredMarineCreature* Actor = Entry.Actor.Get();
		if (!Actor)
		{
			continue;
		}
		const EFaunaSpecies Species = Actor->GetBrain().GetSpecies();
		const float Delta = LodDelta(Species, Actor->GetBrain().GetPosition(), Entry.LodId, Entry.Tier, Entry.PendingDelta);
		if (Delta <= 0.0f)
		{
			continue;
		}
		const FMarineBrainEvents Events = Actor->StepCreature(Delta, Stimuli, WorldQuery);
		const FVector Where = Actor->GetBrain().GetPosition();
		if (Events.bBite && Events.BiteDamage > 0.0f)
		{
			OnFaunaDamage.Broadcast(Species, Events.BiteDamage, Where);
		}
		if (Events.bSting && Events.StingDamage > 0.0f)
		{
			OnFaunaDamage.Broadcast(Species, Events.StingDamage, Where);
		}
		if (Events.bJumped)
		{
			OnFaunaMoment.Broadcast(Species, FName(TEXT("Jump")), Where);
		}
		if (Events.bBlow)
		{
			OnFaunaMoment.Broadcast(Species, FName(TEXT("Blow")), Where);
		}
		if (Events.bStartled)
		{
			OnFaunaMoment.Broadcast(Species, FName(TEXT("Startled")), Where);
		}
		if (Events.bNestingArrived)
		{
			OnTurtleNesting.Broadcast(Where);
		}
	}
}

bool UExploredFaunaSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExploredFaunaSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Manager = InWorld.SpawnActor<AExploredFaunaManager>(AExploredFaunaManager::StaticClass(), FTransform::Identity, Params);
}
