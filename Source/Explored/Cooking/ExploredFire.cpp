#include "Cooking/ExploredFire.h"

#include "Achievements/AchievementsSubsystem.h"
#include "Carry/CarryComponent.h"
#include "Engine/GameInstance.h"
#include "CollisionQueryParams.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Explored.h"
#include "Items/ExploredItemActor.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Save/SaveSlots.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "UI/ExploredSaveSubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"

namespace ExploredFireDetail
{
	/** Cada cuánto (s) se repite la traza de techo. */
	constexpr float ShelterCheckSeconds = 2.0f;
	/** Tope de horas de juego por paso (saltos de tiempo al dormir o cargar). */
	constexpr float MaxCatchUpHours = 72.0f;
	/** Utensilio implícito del propio fuego: un espeto sobre las brasas. */
	const FName SpitVesselId(TEXT("espeto"));
	/** Malla marcadora del hogar hasta que exista SM_Campfire en el juego. */
	const TCHAR* const HearthMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
}

AExploredFire::AExploredFire()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	Hearth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hearth"));
	RootComponent = Hearth;
	// Movible: el fuego puede aparecer en partida al construirlo.
	Hearth->SetMobility(EComponentMobility::Movable);
	Hearth->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Hearth->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.2f));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
	Light->SetLightColor(FLinearColor(1.0f, 0.55f, 0.25f));
	Light->SetAttenuationRadius(900.0f);
	Light->SetIntensity(0.0f);

	Flames = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("Flames"));
	Flames->SetupAttachment(RootComponent);
	Flames->bAutoActivate = false;

	SmokeFx = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("Smoke"));
	SmokeFx->SetupAttachment(RootComponent);
	SmokeFx->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	SmokeFx->bAutoActivate = false;
}

void AExploredFire::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMesh* Mesh = Cast<UStaticMesh>(FSoftObjectPath(ExploredFireDetail::HearthMeshPath).TryLoad()))
	{
		Hearth->SetStaticMesh(Mesh);
	}
	State.Level = static_cast<EFireLevel>(FMath::Clamp(InitialLevel, 0, static_cast<int32>(EFireLevel::Count) - 1));
	UpdateShelter();
	RefreshVisuals();
}

void AExploredFire::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ShelterTimer -= DeltaSeconds;
	if (ShelterTimer <= 0.0f)
	{
		ShelterTimer = ExploredFireDetail::ShelterCheckSeconds;
		UpdateShelter();
	}

	SimulationTimer += DeltaSeconds;
	if (SimulationTimer >= SimulationIntervalSeconds)
	{
		const float Hours = ConsumeGameHours(SimulationTimer);
		SimulationTimer = 0.0f;
		Simulate(Hours);
	}
}

float AExploredFire::ConsumeGameHours(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Clock = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	if (!Clock)
	{
		return DeltaSeconds * FallbackGameHoursPerSecond;
	}
	const float TotalDays = Clock->GetTotalDays();
	const float Hours = LastTotalDays < 0.0f ? 0.0f : (TotalDays - LastTotalDays) * 24.0f;
	LastTotalDays = TotalDays;
	return FMath::Clamp(Hours, 0.0f, ExploredFireDetail::MaxCatchUpHours);
}

void AExploredFire::Simulate(float GameHours)
{
	if (GameHours <= 0.0f)
	{
		return;
	}
	TArray<EFireEvent> Events;
	FFireModel::Tick(State, FFireData::Default(), MakeFireEnvironment(), GameHours, Events);
	if (Pot.Status == EPotStatus::Cooking || Pot.Status == EPotStatus::Done)
	{
		FCookingModel::TickPot(Pot, FCookingData::Default(), MakeCookEnvironment(), GameHours * 60.0f);
	}
	ReportEvents(Events);
	RefreshVisuals();
}

void AExploredFire::UpdateShelter()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
	const FVector End = Start + FVector(0.0f, 0.0f, ShelterTraceMeters * 100.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredFireShelter), false, this);
	FHitResult Hit;
	bSheltered = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
}

FFireEnvironment AExploredFire::MakeFireEnvironment() const
{
	FFireEnvironment Env;
	Env.bSheltered = bSheltered;
	const UWorld* World = GetWorld();
	if (const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr)
	{
		Env.Rain = Weather->GetCurrent().Rain;
		Env.Wind = Weather->GetCurrent().Wind;
	}
	return Env;
}

FCookEnvironment AExploredFire::MakeCookEnvironment() const
{
	const FFireEnvironment FireEnv = MakeFireEnvironment();
	FCookEnvironment Env;
	Env.FireHeat = State.Heat;
	Env.Smoke = State.Smoke;
	Env.FireLevel = State.Level;
	Env.Rain = FireEnv.Rain;
	Env.Wind = FireEnv.Wind;
	Env.bSheltered = FireEnv.bSheltered;
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Clock = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	if (Clock && !Clock->IsNight())
	{
		Env.Sun = Weather ? 1.0f - Weather->GetCurrent().CloudCover : 1.0f;
	}
	return Env;
}

void AExploredFire::ReportEvents(const TArray<EFireEvent>& Events) const
{
	// Sonidos y textos llegarán con el audio y la UI; aquí, estadísticas y autoguardado.
	for (const EFireEvent Event : Events)
	{
		UE_LOG(LogExplored, Verbose, TEXT("%s: suceso de fuego %d"), *GetName(), static_cast<int32>(Event));
		if (Event != EFireEvent::Ignited)
		{
			continue;
		}
		// Cada encendido cuenta uno (docs/tecnico/estadisticas.md).
		if (UAchievementsSubsystem* Achievements = UAchievementsSubsystem::Get(this))
		{
			Achievements->ReportStat(TEXT("fires_lit"));
		}
		// Encender una hoguera autoguarda, con el intervalo mínimo de FSaveSlotPolicy.
		const UWorld* World = GetWorld();
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (UExploredSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UExploredSaveSubsystem>() : nullptr)
		{
			Save->RequestAutosave(ESaveTrigger::Campfire);
		}
	}
}

void AExploredFire::RefreshVisuals()
{
	if (Light)
	{
		Light->SetIntensity(MaxLightIntensity * FMath::Clamp(State.Heat, 0.0f, 1.2f));
	}
	if (Flames)
	{
		const bool bFlames = State.Status == EFireStatus::Burning;
		if (Flames->IsActive() != bFlames)
		{
			Flames->SetActive(bFlames);
		}
	}
	if (SmokeFx)
	{
		const bool bSmoke = State.Smoke > 0.1f;
		if (SmokeFx->IsActive() != bSmoke)
		{
			SmokeFx->SetActive(bSmoke);
		}
	}
}

float AExploredFire::GetHeatAt(const FVector& WorldLocation) const
{
	return FFireModel::HeatAtDistance(State, FVector::Dist(WorldLocation, GetActorLocation()) / 100.0f);
}

bool AExploredFire::UpgradeLevel(EFireLevel NewLevel)
{
	const bool bUpgraded = FFireModel::Upgrade(State, NewLevel);
	RefreshVisuals();
	return bUpgraded;
}

void AExploredFire::RestoreState(const FFireState& InState, const FCookingPot& InPot)
{
	State = InState;
	Pot = InPot;
	PotVesselId = Pot.Status == EPotStatus::Empty ? FName(NAME_None) : Pot.VesselId;
	RefreshVisuals();
}

void AExploredFire::RestoreState(const FFireState& InState, const FCookingPot& InPot, const FItemInstance& InVesselItem)
{
	RestoreState(InState, InPot);
	PotVesselItem = InVesselItem;
	// Una olla vacía al fuego también cuenta: sin cocción, el utensilio manda.
	if (PotVesselItem.IsValid() && PotVesselId.IsNone())
	{
		PotVesselId = FCookingData::Default().VesselForItem(PotVesselItem.DefinitionId);
	}
	// Tras cargar, el tiempo se vuelve a medir desde ahora (no se simula el salto).
	LastTotalDays = -1.0f;
}

// --- Interacción -------------------------------------------------------------

void AExploredFire::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	if (Pot.Status == EPotStatus::Done || Pot.Status == EPotStatus::Burnt)
	{
		OutVerbs.Add(NSLOCTEXT("Explored", "Fire_Collect", "Retirar la comida"));
	}
	if (State.Status != EFireStatus::Burning && State.FuelHours > 0.0f)
	{
		OutVerbs.Add(State.Status == EFireStatus::Embers
			? NSLOCTEXT("Explored", "Fire_Revive", "Avivar las brasas")
			: NSLOCTEXT("Explored", "Fire_Ignite", "Encender"));
	}
	if (OutVerbs.Num() < 3)
	{
		OutVerbs.Add(NSLOCTEXT("Explored", "Fire_AddFuel", "Echar al fuego"));
	}
}

bool AExploredFire::CanInteract_Implementation(AActor* InInstigator) const
{
	return InInstigator != nullptr;
}

void AExploredFire::Interact_Implementation(AActor* InInstigator)
{
	UCarryComponent* Carry = InInstigator ? InInstigator->FindComponentByClass<UCarryComponent>() : nullptr;
	if (!Carry)
	{
		return;
	}
	if (Pot.Status == EPotStatus::Done || Pot.Status == EPotStatus::Burnt)
	{
		CollectPot();
		return;
	}

	const FFireData& FireData = FFireData::Default();
	const FCookingData& CookData = FCookingData::Default();
	for (const EHand Hand : {EHand::Right, EHand::Left})
	{
		const FItemInstance* Held = Carry->GetHandItemPtr(Hand);
		if (!Held || !Held->IsValid())
		{
			continue;
		}
		const FName ItemId = Held->DefinitionId;

		// Encender tiene prioridad: una rama seca en la mano con el fuego apagado es un arco de
		// fuego. Las brasas no se encienden: se avivan echando combustible.
		const FIgnitionDef* Ignition = FireData.FindIgnitionByTool(ItemId);
		if (Ignition && State.Status == EFireStatus::Unlit && State.FuelHours > 0.0f)
		{
			TryIgniteWith(*Carry, Hand, *Ignition);
			return;
		}
		if (FireData.FindFuel(ItemId))
		{
			TryAddFuel(*Carry, Hand, ItemId);
			return;
		}
		const FName VesselId = CookData.VesselForItem(ItemId);
		if (!VesselId.IsNone() && VesselId != ExploredFireDetail::SpitVesselId)
		{
			if (TryPlaceVessel(*Carry, Hand, VesselId))
			{
				return;
			}
		}
		if (TryAddIngredient(*Carry, Hand, ItemId))
		{
			return;
		}
	}

	// Nada que hacer con lo que lleva en las manos: si hay una olla vacía al fuego, se la devuelve.
	if (Pot.Status == EPotStatus::Empty && !PotVesselId.IsNone())
	{
		CollectPot();
	}
}

bool AExploredFire::TryIgniteWith(UCarryComponent& Carry, EHand Hand, const FIgnitionDef& Ignition)
{
	TArray<EFireEvent> Events;
	const FIgnitionResult Result = FFireModel::TryIgnite(State, FFireData::Default(), Ignition.Method, MakeFireEnvironment(), FMath::FRand(), Events);
	if (Result.bConsumedTool)
	{
		FItemInstance Used;
		Carry.TakeOneFromHand(Hand, Used);
	}
	ReportEvents(Events);
	RefreshVisuals();
	return Result.bLit;
}

bool AExploredFire::TryAddFuel(UCarryComponent& Carry, EHand Hand, FName ItemId)
{
	TArray<EFireEvent> Events;
	if (!FFireModel::AddFuel(State, FFireData::Default(), ItemId, Events))
	{
		return false;
	}
	FItemInstance Burned;
	Carry.TakeOneFromHand(Hand, Burned);
	ReportEvents(Events);
	RefreshVisuals();
	return true;
}

bool AExploredFire::TryPlaceVessel(UCarryComponent& Carry, EHand Hand, FName VesselId)
{
	if (!PotVesselId.IsNone() || Pot.Status != EPotStatus::Empty)
	{
		return false;
	}
	FItemInstance Vessel;
	if (!Carry.TakeOneFromHand(Hand, Vessel))
	{
		return false;
	}
	PotVesselId = VesselId;
	PotVesselItem = Vessel;
	return true;
}

bool AExploredFire::TryAddIngredient(UCarryComponent& Carry, EHand Hand, FName ItemId)
{
	const FCookingData& Data = FCookingData::Default();
	TArray<FName> Ids = Pot.IngredientIds;
	Ids.Add(ItemId);
	const TArray<FCookIngredient> Ingredients = BuildIngredients(Ids);

	// Técnica según el utensilio: en olla se hierve si hay receta y si no se guisa; sin olla,
	// el horno hornea y el resto asa en el espeto.
	TArray<ECookTechnique> Candidates;
	FName Vessel = PotVesselId;
	if (!Vessel.IsNone())
	{
		Candidates = {ECookTechnique::Boil, ECookTechnique::Stew};
	}
	else
	{
		Vessel = ExploredFireDetail::SpitVesselId;
		if (State.Level == EFireLevel::HornoArcilla)
		{
			Candidates.Add(ECookTechnique::Bake);
		}
		Candidates.Add(ECookTechnique::Roast);
	}

	for (const ECookTechnique Technique : Candidates)
	{
		const bool bHasRecipe = FCookingModel::FindRecipe(Data, Technique, Vessel, Ingredients, State.Level) != nullptr;
		if (!bHasRecipe && Technique != Data.Improvised.Technique)
		{
			continue;
		}
		FCookingPot NewPot;
		FString Reason;
		if (!FCookingModel::StartPot(NewPot, Data, Technique, Vessel, Ingredients, State.Level, Reason))
		{
			UE_LOG(LogExplored, Verbose, TEXT("%s: no se cocina %s (%s)"), *GetName(), *ItemId.ToString(), *Reason);
			continue;
		}
		FItemInstance Taken;
		if (!Carry.TakeOneFromHand(Hand, Taken))
		{
			return false;
		}
		Pot = NewPot;
		return true;
	}
	return false;
}

void AExploredFire::CollectPot()
{
	const FCookingData& Data = FCookingData::Default();
	FCookedFood Food;
	int32 Slot = 0;
	if (FCookingModel::Collect(Pot, Data, Food))
	{
		FItemInstance Result;
		Result.DefinitionId = Food.ItemId;
		Result.Count = Food.Count;
		SpawnNearby(Result, Slot++);
	}
	if (!PotVesselId.IsNone() && PotVesselItem.IsValid())
	{
		// La olla de coco se gasta con cada uso; la vasija de barro no. Retirarla vacía no la gasta.
		const FCookVesselDef* Vessel = Pot.Status == EPotStatus::Empty ? nullptr : Data.FindVessel(PotVesselId);
		if (Vessel)
		{
			PotVesselItem.Durability -= FCookingModel::VesselWearPerUse(*Vessel);
		}
		if (PotVesselItem.Durability > 0.0f)
		{
			SpawnNearby(PotVesselItem, Slot++);
		}
	}
	Pot = FCookingPot();
	PotVesselId = NAME_None;
	PotVesselItem = FItemInstance();
}

void AExploredFire::SpawnNearby(const FItemInstance& Instance, int32 Slot)
{
	UWorld* World = GetWorld();
	if (!World || !Instance.IsValid())
	{
		return;
	}
	const float Angle = FMath::DegreesToRadians(35.0f * static_cast<float>(Slot));
	const FVector Offset(FMath::Cos(Angle) * 90.0f, FMath::Sin(Angle) * 90.0f, 40.0f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AExploredItemActor* Spawned = World->SpawnActor<AExploredItemActor>(AExploredItemActor::StaticClass(),
		GetActorLocation() + Offset, FRotator::ZeroRotator, Params);
	if (Spawned)
	{
		Spawned->InitializeFromInstance(Instance);
	}
}

TArray<FCookIngredient> AExploredFire::BuildIngredients(const TArray<FName>& Ids) const
{
	const UItemRegistrySubsystem* Registry = UItemRegistrySubsystem::Resolve(this);
	TArray<FCookIngredient> Out;
	for (const FName& Id : Ids)
	{
		FCookIngredient& In = Out.Emplace_GetRef(Id);
		FItemDefinition Definition;
		if (Registry && Registry->FindDefinition(Id, Definition))
		{
			In.Tags = Definition.Tags.Array();
		}
	}
	return Out;
}
