#include "Core/ExploredWiringSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Optional.h"
#include "Stats/Stats.h"
#include "Subsystems/SubsystemCollection.h"

#include "Audio/ExploredMusicSubsystem.h"
#include "Boats/ExploredBoat.h"
#include "Carry/CarryComponent.h"
#include "Cooking/ExploredFire.h"
#include "Events/WorldEventsSubsystem.h"
#include "ExploredGameMode.h"
#include "Fauna/ExploredFaunaManager.h"
#include "Fishing/ExploredFishingSubsystem.h"
#include "Fishing/FishingComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Items/ExploredItemActor.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Items/ItemTypes.h"
#include "Player/ExploredCharacter.h"
#include "Player/SwimComponent.h"
#include "Save/SaveSystemStates.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/BodySignalsComponent.h"
#include "UI/ExploredPlayerController.h"
#include "UI/ExploredSaveSubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/IslandLayoutLoader.h"

namespace ExploredWiringDetail
{
	// Secciones de la partida que registra este subsistema (docs/tecnico/guardado.md).
	const TCHAR* const SectionInventory = TEXT("inventory");
	const TCHAR* const SectionBody = TEXT("body");
	const TCHAR* const SectionFishing = TEXT("fishing");
	const TCHAR* const SectionCooking = TEXT("cooking");
	const TCHAR* const SectionBoats = TEXT("boats");
	const TCHAR* const SectionTime = TEXT("time");
	const TCHAR* const SectionProgress = TEXT("progress");
	const TCHAR* const SectionWorld = TEXT("world");
	const TCHAR* const SectionWiring = TEXT("wiring");

	/** Un fuego guardado se reconoce por su sitio con esta tolerancia (cm). */
	constexpr double FireMatchToleranceCm = 100.0;
	/** Lo que trae el paquete del barco del horizonte (ids de items.json). */
	const TCHAR* const ShipPackageItems[] = { TEXT("cerillas"), TEXT("cantimplora"), TEXT("cuerda"), TEXT("bolsa_impermeable") };
	/** Velocidad mínima (cm/s) para que la vela cuente como navegar (música de travesía). */
	constexpr float SailingMinSpeedCmS = 50.0f;

	const FName DangerBody(TEXT("body"));
	const FName DangerStorm(TEXT("storm"));
	const FName DangerFauna(TEXT("fauna"));
	const FName DangerEruption(TEXT("eruption"));

	UExploredSaveSubsystem* FindSave(const UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
	}

	ExploredSaveStates::FSavedItemNode ToNode(const FItemInstance& Item)
	{
		ExploredSaveStates::FSavedItemNode Node;
		Node.DefinitionId = Item.DefinitionId;
		Node.Quality = Item.Quality;
		Node.Durability = Item.Durability;
		Node.Count = Item.Count;
		Node.LiquidLiters = Item.LiquidLiters;
		Node.GeneratedName = Item.GeneratedName.ToString();
		return Node;
	}

	FItemInstance FromNode(const ExploredSaveStates::FSavedItemNode& Node)
	{
		FItemInstance Item;
		Item.DefinitionId = Node.DefinitionId;
		Item.Quality = Node.Quality;
		Item.Durability = Node.Durability;
		Item.Count = Node.Count;
		Item.LiquidLiters = Node.LiquidLiters;
		// Texto ya formateado al fabricar (ver docs/tecnico/localizacion.md, nombres de los datos).
		Item.GeneratedName = Node.GeneratedName.IsEmpty() ? FText::GetEmpty() : FText::FromString(Node.GeneratedName);
		return Item;
	}

	/** Un objeto con sus piezas como lista aplanada. */
	void WriteItem(FSaveArchive& Ar, const TCHAR* Key, const FItemInstance& Item)
	{
		TArray<ExploredSaveStates::FSavedItemNode> Nodes;
		if (Item.IsValid())
		{
			ExploredSaveStates::FlattenItemTree(Item, &ToNode, Nodes);
		}
		ExploredSaveStates::SaveItemNodes(Ar, Key, Nodes);
	}

	FItemInstance ReadItem(const FSaveArchive& Ar, const TCHAR* Key)
	{
		TArray<ExploredSaveStates::FSavedItemNode> Nodes;
		ExploredSaveStates::LoadItemNodes(Ar, Key, Nodes);
		TArray<FItemInstance> Roots = ExploredSaveStates::RebuildItemTrees<FItemInstance>(Nodes, &FromNode);
		return Roots.Num() > 0 ? Roots[0] : FItemInstance();
	}

	const TCHAR* EndingName(EExploredEnding Ending)
	{
		switch (Ending)
		{
		case EExploredEnding::Departed: return TEXT("Departed");
		case EExploredEnding::Stayed: return TEXT("Stayed");
		default: return TEXT("None");
		}
	}
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

bool UExploredWiringSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExploredWiringSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTimeOfDaySubsystem>();
	Collection.InitializeDependency<UExploredWeatherSubsystem>();
	UWorldEventsSubsystem* Events = Collection.InitializeDependency<UWorldEventsSubsystem>();
	Collection.InitializeDependency<UExploredFishingSubsystem>();
	Collection.InitializeDependency<UExploredMusicSubsystem>();

	Layout = *ExploredIsland::Get();
	WorldDeltas.Seed = FIslandLayout::OfficialSeed;

	if (Events)
	{
		EventStartedHandle = Events->OnEventStarted.AddUObject(this, &UExploredWiringSubsystem::HandleEventStarted);
		EventEndedHandle = Events->OnEventEnded.AddUObject(this, &UExploredWiringSubsystem::HandleEventEnded);
	}
	if (UExploredSaveSubsystem* Save = ExploredWiringDetail::FindSave(GetWorld()))
	{
		RegisterSections(*Save);
	}
}

void UExploredWiringSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Si el mundo aún no tenía GameInstance al inicializarse, se registra ahora.
	if (!bSectionsRegistered)
	{
		if (UExploredSaveSubsystem* Save = ExploredWiringDetail::FindSave(&InWorld))
		{
			RegisterSections(*Save);
		}
	}
}

void UExploredWiringSubsystem::Deinitialize()
{
	UnbindPawn();
	if (AExploredFaunaManager* Fauna = BoundFauna.Get())
	{
		Fauna->OnFaunaDamage.Remove(FaunaDamageHandle);
	}
	UWorld* World = GetWorld();
	if (UWorldEventsSubsystem* Events = World ? World->GetSubsystem<UWorldEventsSubsystem>() : nullptr)
	{
		Events->OnEventStarted.Remove(EventStartedHandle);
		Events->OnEventEnded.Remove(EventEndedHandle);
	}
	if (UExploredSaveSubsystem* Save = ExploredWiringDetail::FindSave(World))
	{
		UnregisterSections(*Save);
	}
	Super::Deinitialize();
}

TStatId UExploredWiringSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredWiringSubsystem, STATGROUP_Tickables);
}

float UExploredWiringSubsystem::GetTotalDays() const
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	return Time ? Time->GetTotalDays() : 0.0f;
}

UExploredWiringSubsystem* UExploredWiringSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = (GEngine && WorldContextObject)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<UExploredWiringSubsystem>() : nullptr;
}

AExploredCharacter* UExploredWiringSubsystem::GetPlayerCharacter() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
}

void UExploredWiringSubsystem::Tick(float DeltaTime)
{
	// El rebrote de sesión necesita granularidad fina para no notarse a saltos; el resto del
	// muestreo (cuerpo, clima, descubrimientos...) va a la cadencia más gorda de siempre.

	SampleTimer -= DeltaTime;
	if (SampleTimer > 0.0f)
	{
		return;
	}
	const float Elapsed = FMath::Max(SampleIntervalSeconds, 0.05f) - SampleTimer;
	SampleTimer = FMath::Max(SampleIntervalSeconds, 0.05f);
	Sample(Elapsed);
}

// ---------------------------------------------------------------------------
// Muestreo
// ---------------------------------------------------------------------------

void UExploredWiringSubsystem::Sample(float DeltaSeconds)
{
	AExploredCharacter* Character = GetPlayerCharacter();
	if (Character != BoundPawn.Get())
	{
		UnbindPawn();
		BindPawn(Character);
	}
	BindFaunaManager();
	ApplyPendingPawnSections();
	SpawnLandingStarterKitIfNeeded();

	const float Days = GetTotalDays();
	if (RunStartDays < 0.0f)
	{
		// Partida sin «Nueva partida» (-SkipMenu, PIE): empieza ahora.
		RunStartDays = Days;
	}
	const float GameHours = LastSampleDays < 0.0f ? 0.0f : FMath::Clamp((Days - LastSampleDays) * 24.0f, 0.0f, 48.0f);
	LastSampleDays = Days;

	UpdateStorms(GameHours);
	if (!Character)
	{
		return;
	}
	const FVector Location = Character->GetActorLocation();
	const FVector2D PositionMeters(Location.X / 100.0, Location.Y / 100.0);
	UpdateBody(*Character, GameHours);
	UpdateBoat(*Character, PositionMeters);
	UpdateDanger(*Character);
	UpdateWorldEvents(PositionMeters);
}

void UExploredWiringSubsystem::NotifyDiscovery(bool bMorale) const
{
	UWorld* World = GetWorld();
	if (UExploredMusicSubsystem* Music = World ? World->GetSubsystem<UExploredMusicSubsystem>() : nullptr)
	{
		Music->NotifyDiscovery();
	}
	if (!bMorale)
	{
		return;
	}
	if (AExploredCharacter* Character = GetPlayerCharacter())
	{
		if (UBodySignalsComponent* Body = Character->GetBodySignalsComponent())
		{
			Body->ApplyMoraleEvent(EMoraleEvent::Discovery);
		}
	}
}

void UExploredWiringSubsystem::UpdateBody(AExploredCharacter& Character, float GameHours)
{
	UWorld* World = GetWorld();
	UBodySignalsComponent* Body = Character.GetBodySignalsComponent();
	UCarryComponent* Carry = Character.GetCarryComponent();
	UExploredMusicSubsystem* Music = World ? World->GetSubsystem<UExploredMusicSubsystem>() : nullptr;

	// Calor de los fuegos cercanos (FFireModel::HeatAtDistance en cada uno).
	const FVector Location = Character.GetActorLocation();
	TArray<float> Heats;
	for (TActorIterator<AExploredFire> It(World); It; ++It)
	{
		if (FVector::DistSquared(It->GetActorLocation(), Location) <= FMath::Square(FireSearchRadiusCm))
		{
			Heats.Add(It->GetHeatAt(Location));
		}
	}
	const float FireHeat = ExploredLinks::CombineFireHeat(Heats);
	const bool bPerforming = Music && Music->IsFlutePerforming();

	if (Body)
	{
		ExploredLinks::FSurvivalLinkInputs Links;
		Links.FireHeat = FireHeat;
		Links.CarriedWeightRatio = Carry ? Carry->GetCarriedWeightRatio() : 0.0f;
		Links.bPlayingMusic = bPerforming;
		Body->SetLinkInputs(Links);
		// Tocar la flauta junto al fuego sube el ánimo (FFluteModel::MoraleRatePerHour).
		if (Music && GameHours > 0.0f)
		{
			Body->AddMorale(Music->GetFluteMoraleRatePerHour(FireHeat) * GameHours);
		}
	}
}

void UExploredWiringSubsystem::UpdateBoat(AExploredCharacter& Character, const FVector2D& PositionMeters)
{
	UWorld* World = GetWorld();
	AExploredBoat* Boat = nullptr;
	for (TActorIterator<AExploredBoat> It(World); It; ++It)
	{
		if (It->GetOccupant() == &Character)
		{
			Boat = *It;
			break;
		}
	}
	const bool bAboard = Boat != nullptr;
	const bool bUnderSail = bAboard && Boat->IsSailRaised() && Boat->GetSpeedCmS() >= ExploredWiringDetail::SailingMinSpeedCmS;

	// Música de travesía: a vela y lejos de tierra.
	if (UExploredMusicSubsystem* Music = World ? World->GetSubsystem<UExploredMusicSubsystem>() : nullptr)
	{
		float CoastDistance = 0.0f;
		ExploredLinks::NearestIsland(Layout, PositionMeters, CoastDistance);
		Music->SetSailing(bUnderSail && CoastDistance > 100.0f);
	}

	// Delfines junto a la canoa y gaviotas detrás del pescado (P-FAUNA).
	UCarryComponent* Carry = Character.GetCarryComponent();
	if (AExploredFaunaManager* Fauna = BoundFauna.Get())
	{
		Fauna->SetPlayerBoatState(bAboard, Carry && Carry->HasItemWithTag(TEXT("pescado")));
	}
	// Legendarias que solo pican desde la barca (el Rey de Plata).
	if (UFishingComponent* Fishing = Character.GetFishingComponent())
	{
		Fishing->SetFromBoat(bAboard);
	}
}

void UExploredWiringSubsystem::UpdateDanger(AExploredCharacter& Character)
{
	UWorld* World = GetWorld();
	UExploredMusicSubsystem* Music = World ? World->GetSubsystem<UExploredMusicSubsystem>() : nullptr;
	if (!Music)
	{
		return;
	}
	const UBodySignalsComponent* Body = Character.GetBodySignalsComponent();
	if (Body)
	{
		Music->SetDanger(ExploredLinks::BodyDanger01(Body->GetSurvivalState()), ExploredWiringDetail::DangerBody);
	}
	if (const UExploredWeatherSubsystem* Weather = World->GetSubsystem<UExploredWeatherSubsystem>())
	{
		Music->SetDanger(ExploredLinks::StormDanger01(Weather->GetState(), Body && Body->GetInputs().bSheltered),
			ExploredWiringDetail::DangerStorm);
	}
	if (const AExploredFaunaManager* Fauna = BoundFauna.Get())
	{
		Music->SetDanger(Fauna->GetPlayerThreat01(Character.GetActorLocation()), ExploredWiringDetail::DangerFauna);
	}
	if (const UWorldEventsSubsystem* Events = World->GetSubsystem<UWorldEventsSubsystem>())
	{
		Music->SetDanger(Events->GetEruption().Tremor * 0.5f, ExploredWiringDetail::DangerEruption);
	}
}

void UExploredWiringSubsystem::UpdateWorldEvents(const FVector2D& PositionMeters)
{
	UWorld* World = GetWorld();
	UWorldEventsSubsystem* Events = World ? World->GetSubsystem<UWorldEventsSubsystem>() : nullptr;
	if (!Events)
	{
		return;
	}
	// Presenciar: los activos cuentan en cuanto el jugador está donde se ven.
	for (const FWorldEvent& Event : Events->GetActiveEvents())
	{
		if (WitnessedEvents.Contains(Event.Id) || !ExploredLinks::IsEventWitnessed(Event, Layout, PositionMeters))
		{
			continue;
		}
		WitnessedEvents.Add(Event.Id);
		NotifyDiscovery(/*bMorale*/ false);
	}

	// Hoguera de señal → el barco del horizonte suelta un paquete una sola vez.
	const AExploredFire* SignalFire = nullptr;
	for (TActorIterator<AExploredFire> It(World); It; ++It)
	{
		if (It->IsSignalFire())
		{
			SignalFire = *It;
			break;
		}
	}
	FWorldEvent Ship;
	if (SignalFire && Events->TryDropShipPackage(true, Ship))
	{
		// El paquete llega a la orilla junto a la hoguera (la deriva real queda para el océano).
		for (const TCHAR* ItemId : ExploredWiringDetail::ShipPackageItems)
		{
			SpawnItemsAround(SignalFire->GetActorLocation() + FVector(Ship.Direction.X, Ship.Direction.Y, 0.0) * 400.0, FName(ItemId), 1, 150.0f);
		}
	}
}

void UExploredWiringSubsystem::UpdateStorms(float GameHours)
{
	UWorld* World = GetWorld();
	const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	if (!Weather)
	{
		return;
	}
	// El ciclón también castiga a las embarcaciones (varadas, la mitad).
	const int32 Category = Weather->GetCycloneCategory();
	if (Category > 0 && GameHours > 0.0f)
	{
		for (TActorIterator<AExploredBoat> It(World); It; ++It)
		{
			It->ApplyExternalDamage(ExploredLinks::BoatCycloneDamagePerHour(Category, It->IsGrounded()) * GameHours);
		}
	}
}

void UExploredWiringSubsystem::SpawnLandingStarterKitIfNeeded()
{
	if (bStarterKitSpawned)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	APlayerStart* Start = nullptr;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Start = *It;
		break;
	}
	if (!Start)
	{
		// El nivel horneado todavía no ha terminado de aparecer; se reintenta en el próximo Sample.
		return;
	}
	// El mapa de juego es también el del menú principal y el menú no pausa: sin esto el kit
	// aparecía a los 0,25 s de estar en el menú, antes de que «Continuar»/«Cargar» leyera
	// starterKitSpawned, y cada carga dejaba un cuchillo y una cantimplora más en Landing.
	if (const AExploredPlayerController* PC = Cast<AExploredPlayerController>(World->GetFirstPlayerController()))
	{
		if (PC->GetUIMode() == EExploredUIMode::Menu)
		{
			return;
		}
	}
	bStarterKitSpawned = true;

	const FVector WreckCenter = Start->GetActorLocation() + Start->GetActorForwardVector() * 300.0;
	SpawnItemsAround(WreckCenter, TEXT("cuchillo"), 1, 80.0f);
	SpawnItemsAround(WreckCenter, TEXT("cantimplora"), 1, 80.0f);

	const FVector AroundSpawn = Start->GetActorLocation();
	SpawnItemsAround(AroundSpawn, TEXT("coco_maduro"), 3, 450.0f);
	SpawnItemsAround(AroundSpawn, TEXT("palo_recto"), 4, 450.0f);
	SpawnItemsAround(AroundSpawn, TEXT("rama_seca"), 3, 450.0f);
	SpawnItemsAround(AroundSpawn, TEXT("canto_rodado"), 3, 450.0f);
	SpawnItemsAround(AroundSpawn, TEXT("piedra_plana"), 2, 450.0f);
}

void UExploredWiringSubsystem::SpawnItemsAround(const FVector& Center, FName ItemId, int32 Count, float RadiusCm) const
{
	UWorld* World = GetWorld();
	if (!World || ItemId.IsNone())
	{
		return;
	}
	for (int32 I = 0; I < Count; ++I)
	{
		const float Angle = UE_TWO_PI * (static_cast<float>(I) + FMath::FRand()) / static_cast<float>(FMath::Max(Count, 1));
		const FVector Location = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * RadiusCm * FMath::FRandRange(0.3f, 1.0f)
			+ FVector(0.0, 0.0, 100.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AExploredItemActor* Item = World->SpawnActor<AExploredItemActor>(AExploredItemActor::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			FItemInstance Instance;
			Instance.DefinitionId = ItemId;
			Item->InitializeFromInstance(Instance);
		}
	}
}

// ---------------------------------------------------------------------------
// Delegados
// ---------------------------------------------------------------------------

void UExploredWiringSubsystem::BindPawn(AExploredCharacter* Character)
{
	BoundPawn = Character;
	if (!Character)
	{
		return;
	}
	if (UBodySignalsComponent* Body = Character->GetBodySignalsComponent())
	{
		BodyEventHandle = Body->OnSurvivalEvent.AddUObject(this, &UExploredWiringSubsystem::HandleSurvivalEvent);
	}
}

void UExploredWiringSubsystem::UnbindPawn()
{
	if (AExploredCharacter* Character = BoundPawn.Get())
	{
		if (UBodySignalsComponent* Body = Character->GetBodySignalsComponent())
		{
			Body->OnSurvivalEvent.Remove(BodyEventHandle);
		}
	}
	BodyEventHandle.Reset();
	BoundPawn.Reset();
}

void UExploredWiringSubsystem::BindFaunaManager()
{
	if (BoundFauna.IsValid())
	{
		return;
	}
	const UWorld* World = GetWorld();
	const UExploredFaunaSubsystem* FaunaSubsystem = World ? World->GetSubsystem<UExploredFaunaSubsystem>() : nullptr;
	if (AExploredFaunaManager* Manager = FaunaSubsystem ? FaunaSubsystem->GetManager() : nullptr)
	{
		BoundFauna = Manager;
		FaunaDamageHandle = Manager->OnFaunaDamage.AddUObject(this, &UExploredWiringSubsystem::HandleFaunaDamage);
		// Modo Explorador: fauna pacífica (GDD §11).
		const AExploredCharacter* Character = GetPlayerCharacter();
		const UBodySignalsComponent* Body = Character ? Character->GetBodySignalsComponent() : nullptr;
		Manager->SetPeaceful(Body && Body->GetModeSettings().Mode == ESurvivalMode::Explorer);
	}
}

void UExploredWiringSubsystem::HandleSurvivalEvent(ESurvivalEvent Event)
{
	if (Event != ESurvivalEvent::Died)
	{
		return;
	}
	UWorld* World = GetWorld();
	AExploredGameMode* GameMode = World ? World->GetAuthGameMode<AExploredGameMode>() : nullptr;
	if (GameMode && BoundPawn.IsValid())
	{
		GameMode->HandlePlayerDeath(BoundPawn.Get());
	}
}

void UExploredWiringSubsystem::HandleEventStarted(const FWorldEvent& Event)
{
	AExploredFaunaManager* Fauna = BoundFauna.Get();
	if (Fauna && Event.Type == EWorldEventType::WhalePassage)
	{
		Fauna->SetWhalePassage(true);
	}
}

void UExploredWiringSubsystem::HandleEventEnded(const FWorldEvent& Event)
{
	AExploredFaunaManager* Fauna = BoundFauna.Get();
	if (Fauna && Event.Type == EWorldEventType::WhalePassage)
	{
		Fauna->SetWhalePassage(false);
	}
}

void UExploredWiringSubsystem::HandleFaunaDamage(EFaunaSpecies Species, float Damage, const FVector& LocationCm)
{
	const AExploredCharacter* Character = BoundPawn.Get();
	UBodySignalsComponent* Body = Character ? Character->GetBodySignalsComponent() : nullptr;
	if (!Body)
	{
		return;
	}
	const ExploredLinks::FFaunaHarm Harm = ExploredLinks::HarmFromFauna(Species, Damage);
	if (Harm.bSting)
	{
		Body->ApplySting(Harm.Sting);
	}
	if (Harm.CutDepth > 0.0f)
	{
		Body->AddCut(Harm.CutDepth);
		// La sangre en el agua atrae a más tiburones.
		if (AExploredFaunaManager* Fauna = BoundFauna.Get())
		{
			Fauna->AddBloodInWater(LocationCm, Harm.CutDepth);
		}
	}
}

void UExploredWiringSubsystem::BeginNewRun()
{
	RunStartDays = GetTotalDays();
	WitnessedEvents.Reset();
	Progress = FExploredProgress();
}

// ---------------------------------------------------------------------------
// Guardado
// ---------------------------------------------------------------------------

void UExploredWiringSubsystem::RegisterSections(UExploredSaveSubsystem& Save)
{
	using namespace ExploredWiringDetail;
	const TWeakObjectPtr<UExploredWiringSubsystem> WeakThis(this);
	auto Register = [&Save, WeakThis](const TCHAR* Name, void (UExploredWiringSubsystem::*SaveFn)(FSaveArchive&) const,
		void (UExploredWiringSubsystem::*LoadFn)(const FSaveArchive&))
	{
		Save.RegisterSection(Name,
			[WeakThis, SaveFn](FSaveArchive& Ar)
			{
				if (const UExploredWiringSubsystem* Self = WeakThis.Get())
				{
					(Self->*SaveFn)(Ar);
				}
			},
			[WeakThis, LoadFn](const FSaveArchive& Ar)
			{
				if (UExploredWiringSubsystem* Self = WeakThis.Get())
				{
					(Self->*LoadFn)(Ar);
				}
			});
	};
	// El reloj primero: los demás sistemas miden el tiempo desde la hora cargada.
	Register(SectionTime, &UExploredWiringSubsystem::SaveClock, &UExploredWiringSubsystem::LoadClock);
	Register(SectionWiring, &UExploredWiringSubsystem::SaveWiring, &UExploredWiringSubsystem::LoadWiring);
	Register(SectionProgress, &UExploredWiringSubsystem::SaveProgress, &UExploredWiringSubsystem::LoadProgress);
	Register(SectionWorld, &UExploredWiringSubsystem::SaveWorld, &UExploredWiringSubsystem::LoadWorld);
	Register(SectionInventory, &UExploredWiringSubsystem::SaveInventory, &UExploredWiringSubsystem::LoadInventory);
	Register(SectionBody, &UExploredWiringSubsystem::SaveBody, &UExploredWiringSubsystem::LoadBody);
	Register(SectionFishing, &UExploredWiringSubsystem::SaveFishing, &UExploredWiringSubsystem::LoadFishing);
	Register(SectionCooking, &UExploredWiringSubsystem::SaveFires, &UExploredWiringSubsystem::LoadFires);
	Register(SectionBoats, &UExploredWiringSubsystem::SaveBoats, &UExploredWiringSubsystem::LoadBoats);
	bSectionsRegistered = true;
}

void UExploredWiringSubsystem::UnregisterSections(UExploredSaveSubsystem& Save)
{
	using namespace ExploredWiringDetail;
	for (const TCHAR* Name : {SectionInventory, SectionBody, SectionFishing, SectionCooking, SectionBoats,
		SectionTime, SectionProgress, SectionWorld, SectionWiring})
	{
		Save.UnregisterSection(Name);
	}
}

// --- Reloj, progreso, mundo y estado propio ------------------------------------

void UExploredWiringSubsystem::SaveClock(FSaveArchive& Ar) const
{
	const UWorld* World = GetWorld();
	ExploredSaveStates::FSavedClock Clock;
	if (const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr)
	{
		Clock.Day = Time->GetDay();
		Clock.Hours = Time->GetHours();
	}
	if (const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr)
	{
		Clock.ForcedWeather = Weather->GetForcedState(Clock.ForcedUntilDays);
	}
	ExploredSaveStates::SaveClock(Ar, Clock);
}

void UExploredWiringSubsystem::LoadClock(const FSaveArchive& Ar)
{
	if (Ar.IsEmpty())
	{
		return;
	}
	UWorld* World = GetWorld();
	ExploredSaveStates::FSavedClock Clock;
	ExploredSaveStates::LoadClock(Ar, Clock);
	if (UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr)
	{
		Time->SetTime(Clock.Day, Clock.Hours);
	}
	if (UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr)
	{
		Weather->RestoreForcedState(Clock.ForcedWeather, Clock.ForcedUntilDays);
	}
	// El salto de reloj no cuenta como horas vividas por los enlaces.
	LastSampleDays = -1.0f;
}

void UExploredWiringSubsystem::SaveWiring(FSaveArchive& Ar) const
{
	Ar.Write(TEXT("runStartDays"), RunStartDays);
	Ar.Write(TEXT("starterKitSpawned"), bStarterKitSpawned);
}

void UExploredWiringSubsystem::LoadWiring(const FSaveArchive& Ar)
{
	RunStartDays = -1.0f;
	Ar.Read(TEXT("runStartDays"), RunStartDays);
	// Sin la clave (partida nueva, o guardada antes de que existiera este reparto) se entrega
	// el salvamento de Landing: en una partida ya empezada es una vez de más junto al
	// PlayerStart, no una regresión.
	bStarterKitSpawned = false;
	Ar.Read(TEXT("starterKitSpawned"), bStarterKitSpawned);
	WitnessedEvents.Reset();
}

void UExploredWiringSubsystem::SaveProgress(FSaveArchive& Ar) const
{
	TArray<FName> Discovered = Progress.Discovered.Array();
	Discovered.Sort(FNameLexicalLess());
	Ar.Write(TEXT("discovered"), Discovered);
	Ar.Write(TEXT("shipParts"), Progress.ShipParts);
	Ar.Write(TEXT("shipBuilt"), Progress.bShipBuilt);
	Ar.Write(TEXT("reachedHiddenIsland"), Progress.bReachedHiddenIsland);
	TArray<FString> Endings;
	for (const EExploredEnding Ending : Progress.EndingsSeen)
	{
		Endings.Add(ExploredWiringDetail::EndingName(Ending));
	}
	Ar.Write(TEXT("endingsSeen"), Endings);
}

void UExploredWiringSubsystem::LoadProgress(const FSaveArchive& Ar)
{
	Progress = FExploredProgress();
	TArray<FName> Discovered;
	Ar.Read(TEXT("discovered"), Discovered);
	Progress.Discovered.Append(Discovered);
	Ar.Read(TEXT("shipParts"), Progress.ShipParts);
	Ar.Read(TEXT("shipBuilt"), Progress.bShipBuilt);
	Ar.Read(TEXT("reachedHiddenIsland"), Progress.bReachedHiddenIsland);
	TArray<FString> Endings;
	Ar.Read(TEXT("endingsSeen"), Endings);
	for (const FString& Ending : Endings)
	{
		if (Ending == TEXT("Departed"))
		{
			Progress.EndingsSeen.Add(EExploredEnding::Departed);
		}
		else if (Ending == TEXT("Stayed"))
		{
			Progress.EndingsSeen.Add(EExploredEnding::Stayed);
		}
	}
}

void UExploredWiringSubsystem::SaveWorld(FSaveArchive& Ar) const
{
	WorldDeltas.Save(Ar);
}

void UExploredWiringSubsystem::LoadWorld(const FSaveArchive& Ar)
{
	WorldDeltas.Load(Ar);
	if (WorldDeltas.Seed == 0)
	{
		WorldDeltas.Seed = FIslandLayout::OfficialSeed;
	}
}

// --- Personaje: inventario y cuerpo ---------------------------------------

void UExploredWiringSubsystem::SaveInventory(FSaveArchive& Ar) const
{
	const AExploredCharacter* Character = GetPlayerCharacter();
	const UCarryComponent* Carry = Character ? Character->GetCarryComponent() : nullptr;
	if (!Carry)
	{
		// Sin personaje se conserva lo que llegó de la partida.
		if (PendingInventory.IsSet())
		{
			Ar = PendingInventory.GetValue();
		}
		return;
	}
	FInventoryState State;
	TMap<int64, FItemInstance> Instances;
	Carry->ExportState(State, Instances);
	FSaveArchive StateAr;
	ExploredSaveStates::SaveInventory(StateAr, State);
	Ar.Write(TEXT("state"), StateAr);
	TArray<FSaveArchive> Payloads;
	for (const TPair<int64, FItemInstance>& Entry : Instances)
	{
		FSaveArchive& Payload = Payloads.AddDefaulted_GetRef();
		Payload.Write(TEXT("id"), Entry.Key);
		ExploredWiringDetail::WriteItem(Payload, TEXT("item"), Entry.Value);
	}
	Ar.Write(TEXT("payloads"), Payloads);
}

void UExploredWiringSubsystem::LoadInventory(const FSaveArchive& Ar)
{
	PendingInventory.Reset();
	if (Ar.IsEmpty())
	{
		return;
	}
	PendingInventory = Ar;
	ApplyPendingPawnSections();
}

void UExploredWiringSubsystem::SaveBody(FSaveArchive& Ar) const
{
	const AExploredCharacter* Character = GetPlayerCharacter();
	const UBodySignalsComponent* Body = Character ? Character->GetBodySignalsComponent() : nullptr;
	if (!Body)
	{
		if (PendingBody.IsSet())
		{
			Ar = PendingBody.GetValue();
		}
		return;
	}
	ExploredSaveStates::SaveSurvival(Ar, Body->GetSurvivalState(), Body->GetModeSettings().Mode);
}

void UExploredWiringSubsystem::LoadBody(const FSaveArchive& Ar)
{
	PendingBody.Reset();
	if (Ar.IsEmpty())
	{
		return;
	}
	PendingBody = Ar;
	ApplyPendingPawnSections();
}

void UExploredWiringSubsystem::ApplyPendingPawnSections()
{
	AExploredCharacter* Character = GetPlayerCharacter();
	if (!Character)
	{
		return;
	}
	if (PendingInventory.IsSet())
	{
		if (UCarryComponent* Carry = Character->GetCarryComponent())
		{
			const FSaveArchive& Ar = PendingInventory.GetValue();
			FInventoryState State;
			FSaveArchive StateAr;
			if (Ar.Read(TEXT("state"), StateAr))
			{
				ExploredSaveStates::LoadInventory(StateAr, State);
			}
			TMap<int64, FItemInstance> Instances;
			TArray<FSaveArchive> Payloads;
			Ar.Read(TEXT("payloads"), Payloads);
			for (const FSaveArchive& Payload : Payloads)
			{
				int64 Id = 0;
				const FItemInstance Item = ExploredWiringDetail::ReadItem(Payload, TEXT("item"));
				if (Payload.Read(TEXT("id"), Id) && Id != 0 && Item.IsValid())
				{
					Instances.Add(Id, Item);
				}
			}
			Carry->ImportState(State, Instances);
		}
		PendingInventory.Reset();
	}
	if (PendingBody.IsSet())
	{
		if (UBodySignalsComponent* Body = Character->GetBodySignalsComponent())
		{
			FSurvivalState State;
			ESurvivalMode Mode = Body->GetModeSettings().Mode;
			ExploredSaveStates::LoadSurvival(PendingBody.GetValue(), State, Mode);
			Body->RestoreSurvival(State, Mode);
			// La fauna se enlazó con el modo por defecto: el cargado manda (GDD §11).
			if (AExploredFaunaManager* Fauna = BoundFauna.Get())
			{
				Fauna->SetPeaceful(Body->GetModeSettings().Mode == ESurvivalMode::Explorer);
			}
		}
		PendingBody.Reset();
	}
}

// --- Pesca, fuegos y barcos ---------------------------------------------------------

void UExploredWiringSubsystem::SaveFishing(FSaveArchive& Ar) const
{
	const UWorld* World = GetWorld();
	if (const UExploredFishingSubsystem* Fishing = World ? World->GetSubsystem<UExploredFishingSubsystem>() : nullptr)
	{
		ExploredSaveStates::SaveFishing(Ar, Fishing->GetState());
	}
}

void UExploredWiringSubsystem::LoadFishing(const FSaveArchive& Ar)
{
	UWorld* World = GetWorld();
	UExploredFishingSubsystem* Fishing = World ? World->GetSubsystem<UExploredFishingSubsystem>() : nullptr;
	if (!Fishing || Ar.IsEmpty())
	{
		return;
	}
	ExploredSaveStates::LoadFishing(Ar, Fishing->GetState());
}

void UExploredWiringSubsystem::SaveFires(FSaveArchive& Ar) const
{
	TArray<FSaveArchive> Fires;
	for (TActorIterator<AExploredFire> It(GetWorld()); It; ++It)
	{
		ExploredSaveStates::FSavedFire Fire;
		Fire.Location = It->GetActorLocation();
		Fire.Fire = It->GetFireState();
		Fire.Pot = It->GetPot();
		FSaveArchive& Entry = Fires.AddDefaulted_GetRef();
		ExploredSaveStates::SaveFire(Entry, Fire);
		ExploredWiringDetail::WriteItem(Entry, TEXT("vessel"), It->GetPotVesselItem());
	}
	Ar.Write(TEXT("fires"), Fires);
}

void UExploredWiringSubsystem::LoadFires(const FSaveArchive& Ar)
{
	UWorld* World = GetWorld();
	TArray<FSaveArchive> Fires;
	if (!World || !Ar.Read(TEXT("fires"), Fires))
	{
		return;
	}
	TArray<AExploredFire*> Available;
	for (TActorIterator<AExploredFire> It(World); It; ++It)
	{
		Available.Add(*It);
	}
	for (const FSaveArchive& Entry : Fires)
	{
		ExploredSaveStates::FSavedFire Fire;
		ExploredSaveStates::LoadFire(Entry, Fire);
		// El fuego del mapa que está en ese sitio; si no hay (se construyó en partida), se crea.
		AExploredFire* Target = nullptr;
		for (int32 I = 0; I < Available.Num(); ++I)
		{
			if (FVector::Dist(Available[I]->GetActorLocation(), Fire.Location) <= ExploredWiringDetail::FireMatchToleranceCm)
			{
				Target = Available[I];
				Available.RemoveAt(I);
				break;
			}
		}
		if (!Target)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Target = World->SpawnActor<AExploredFire>(AExploredFire::StaticClass(), Fire.Location, FRotator::ZeroRotator, Params);
		}
		if (Target)
		{
			Target->RestoreState(Fire.Fire, Fire.Pot, ExploredWiringDetail::ReadItem(Entry, TEXT("vessel")));
		}
	}
}

void UExploredWiringSubsystem::SaveBoats(FSaveArchive& Ar) const
{
	TArray<FSaveArchive> Boats;
	for (TActorIterator<AExploredBoat> It(GetWorld()); It; ++It)
	{
		FSaveArchive& Entry = Boats.AddDefaulted_GetRef();
		// Las del mapa se reconocen por el nombre del actor; las construidas se vuelven a crear.
		Entry.Write(TEXT("name"), It->GetFName());
		Entry.Write(TEXT("builtByPlayer"), It->bBuiltByPlayer);
		FSaveArchive Data;
		ExploredSaveStates::SaveBoat(Data, It->GetSaveData());
		Entry.Write(TEXT("data"), Data);
	}
	Ar.Write(TEXT("boats"), Boats);
}

void UExploredWiringSubsystem::LoadBoats(const FSaveArchive& Ar)
{
	UWorld* World = GetWorld();
	TArray<FSaveArchive> Boats;
	if (!World || !Ar.Read(TEXT("boats"), Boats))
	{
		return;
	}
	// Las construidas en partida se retiran: la partida trae las suyas.
	TArray<AExploredBoat*> Built;
	for (TActorIterator<AExploredBoat> It(World); It; ++It)
	{
		if (It->bBuiltByPlayer)
		{
			Built.Add(*It);
		}
	}
	for (AExploredBoat* Boat : Built)
	{
		Boat->Destroy();
	}
	for (const FSaveArchive& Entry : Boats)
	{
		FName Name;
		bool bBuilt = false;
		FSaveArchive DataAr;
		Entry.Read(TEXT("name"), Name);
		Entry.Read(TEXT("builtByPlayer"), bBuilt);
		if (!Entry.Read(TEXT("data"), DataAr))
		{
			continue;
		}
		FBoatSaveData Data;
		ExploredSaveStates::LoadBoat(DataAr, Data);
		if (!bBuilt)
		{
			for (TActorIterator<AExploredBoat> It(World); It; ++It)
			{
				if (!It->bBuiltByPlayer && It->GetFName() == Name)
				{
					It->RestoreFromSaveData(Data);
					break;
				}
			}
			continue;
		}
		const FTransform Transform(FRotator(0.0, Data.YawDeg, 0.0), Data.LocationCm);
		AExploredBoat* Boat = World->SpawnActorDeferred<AExploredBoat>(AExploredBoat::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Boat)
		{
			Boat->bBuiltByPlayer = true;
			// El tipo se fija antes de empezar (BeginPlay rehace el modelo con él) y el estado, después.
			Boat->RestoreFromSaveData(Data);
			Boat->FinishSpawning(Transform);
			Boat->RestoreFromSaveData(Data);
		}
	}
}
