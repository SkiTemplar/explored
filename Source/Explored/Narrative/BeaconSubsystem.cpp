#include "Narrative/BeaconSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"
#include "UObject/SoftObjectPath.h"

#include "Audio/ExploredMusicSubsystem.h"
#include "Boats/ExploredRaft.h"
#include "Cooking/ExploredFire.h"
#include "Explored.h"
#include "Items/BackpackModel.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Items/ExploredIslandStateSubsystem.h"
#include "Narrative/BeaconTexts.h"
#include "Narrative/ExploredSupplyCrate.h"
#include "Narrative/SupplyCrateModel.h"
#include "Narrative/ExploredBeaconActors.h"
#include "Narrative/FinaleSequenceModel.h"
#include "Narrative/LighthouseLamp.h"
#include "Save/SaveIslandSections.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/ExploredHUD.h"
#include "UI/ExploredPlayerController.h"
#include "UI/ExploredSaveSubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "Weather/StormClockModel.h"
#include "WorldGen/ExploredShelterVolume.h"
#include "WorldGen/IslandPropsLoader.h"
#include "WorldGen/IslandPropsModel.h"

#define LOCTEXT_NAMESPACE "ExploredBeacon"

namespace BeaconSubsystemDetail
{
	constexpr float SampleSeconds = 0.5f;
	constexpr int32 StartDay = 1;
	constexpr float StartHour = 7.0f;
	constexpr float CardSeconds = 6.0f;
	constexpr float PageSeconds = 16.0f;
	/** La bocina suena cuando el barco ya está cerca, al 80 % del acercamiento. */
	constexpr float HornAtApproachFraction = 0.8f;
	/** El Mistral entra desde mar abierto, al noroeste del faro, y se para a ~500 m de la costa. */
	const FVector MistralFromOffsetCm(160000.0, -70000.0, 0.0);
	const FVector MistralToOffsetCm(48000.0, -20000.0, 0.0);

	/** Bocina del Mistral (Tools/Audio, generators/beacon.py). */
	const TCHAR* const HornSound = TEXT("/Game/Generated/Audio/Efectos/sfx_ship_horn.sfx_ship_horn");
	/** Tono de la bocina (1 = el de la grabación). */
	constexpr float HornPitch = 1.0f;
	/** Plano del final (cm, respecto a la linterna): detrás del faro, al lado contrario de la casa
	 *  (al oeste) y algo por encima, para ver el haz sobre el mar y el Mistral. */
	constexpr double FinaleCamBackCm = 3200.0;
	constexpr double FinaleCamSideCm = -2200.0;
	constexpr double FinaleCamDropCm = -500.0;
	/** Punto al que mira: mar adentro hacia el Mistral, cerca del agua. */
	constexpr double FinaleLookAheadCm = 14000.0;
	constexpr double FinaleLookZCm = 300.0;
	constexpr float FinaleBlendSeconds = 2.5f;
	const TCHAR* const PageSound = TEXT("/Game/Generated/Audio/Efectos/sfx_ui_page_turn.sfx_ui_page_turn");
	const TCHAR* const IgniteSound = TEXT("/Game/Generated/Audio/Efectos/sfx_fire_ignite.sfx_fire_ignite");

	const FName LensItem(TEXT("lente"));
	const FName OilItem(TEXT("tarro_aceite"));
	const FName WickItem(TEXT("mecha"));
	const FName CookedFishItem(TEXT("pez_asado"));
	const FName EmptyCoconutItem(TEXT("coco_vacio"));
	const FName BarometerItem(TEXT("barometro"));

	const FName TowerProp(TEXT("faro_torre"));
	const FName HouseProp(TEXT("faro_casa"));
	const FName FreighterProp(TEXT("carguero_casco"));

	float HorizontalMeters(const FVector& A, const FVector& B)
	{
		return static_cast<float>(FVector::Dist2D(A, B) / 100.0);
	}

	AExploredHUD* FindHUD(const UObject* Context)
	{
		const APlayerController* PC = UGameplayStatics::GetPlayerController(Context, 0);
		return PC ? Cast<AExploredHUD>(PC->GetHUD()) : nullptr;
	}

	/** Lo que hay que llevar para cada ranura de la linterna (GDD §5.6, receta 9). */
	TMap<FName, int32> SlotCost(ELanternSlot Slot)
	{
		switch (Slot)
		{
		case ELanternSlot::Lens: return { { LensItem, 1 } };
		case ELanternSlot::Oil: return { { OilItem, FBeaconProgressModel::RequiredOilJars } };
		case ELanternSlot::Wick: return { { WickItem, 1 } };
		default: return {};
		}
	}

	FText MissingForSlot(ELanternSlot Slot)
	{
		switch (Slot)
		{
		case ELanternSlot::Lens: return LOCTEXT("MissingLens", "Falta la lente: está en el «Sáhara»");
		case ELanternSlot::Oil: return LOCTEXT("MissingOil", "Hacen falta dos tarros de aceite");
		case ELanternSlot::Wick: return LOCTEXT("MissingWick", "Falta la mecha");
		default: return FText::GetEmpty();
		}
	}

#if !UE_BUILD_SHIPPING
	UBeaconSubsystem* FromWorld(UWorld* World)
	{
		UBeaconSubsystem* Beacon = World ? World->GetSubsystem<UBeaconSubsystem>() : nullptr;
		if (!Beacon)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Baliza] No hay partida en marcha"));
		}
		return Beacon;
	}

	void CmdStage(const TArray<FString>& Args, UWorld* World)
	{
		EBeaconStage Stage = EBeaconStage::Prologo;
		if (Args.Num() < 1 || !FBeaconDirectorModel::ParseStage(Args[0], Stage))
		{
			UE_LOG(LogExplored, Warning, TEXT("[Baliza] Uso: BalizaStage <Prologo|Orilla|Faro|Balsa|Carguero|LenteEnMano|Ciclon|Preparacion|LinternaMontada|Final|0-9>"));
			return;
		}
		if (UBeaconSubsystem* Beacon = FromWorld(World))
		{
			Beacon->DebugSetStage(Stage);
		}
	}

	void CmdGive(const TArray<FString>& Args, UWorld* World)
	{
		const int32 Count = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : 1;
		const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(Pawn);
		if (Args.Num() < 1 || Count <= 0 || !Survivor)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Baliza] Uso: BalizaGive <id de items.json> <n> (con Iria en el mundo)"));
			return;
		}
		const bool bOk = Survivor->TryAddItem(FName(*Args[0]), Count);
		UE_LOG(LogExplored, Display, TEXT("[Baliza] BalizaGive %s %d: %s"), *Args[0], Count, bOk ? TEXT("ok") : TEXT("no cabe o no existe"));
	}

	void CmdTime(const TArray<FString>& Args, UWorld* World)
	{
		UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
		if (Args.Num() < 2 || !Time)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Baliza] Uso: BalizaTime <día> <hora>"));
			return;
		}
		Time->SetTime(FMath::Max(1, FCString::Atoi(*Args[0])), FMath::Clamp(FCString::Atof(*Args[1]), 0.0f, 23.99f));
		UE_LOG(LogExplored, Display, TEXT("[Baliza] Reloj: día %d, %.2f h"), Time->GetDay(), Time->GetHours());
	}

	FAutoConsoleCommandWithWorldAndArgs StageCommand(TEXT("BalizaStage"),
		TEXT("Pone la historia de Baliza Siete en una etapa: BalizaStage <nombre o 0-9>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdStage));
	FAutoConsoleCommandWithWorldAndArgs GiveCommand(TEXT("BalizaGive"),
		TEXT("Mete objetos en la mochila de Iria: BalizaGive <id> <n>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdGive));
	FAutoConsoleCommandWithWorldAndArgs TimeCommand(TEXT("BalizaTime"),
		TEXT("Pone el reloj: BalizaTime <día> <hora>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdTime));
#endif
}

bool UBeaconSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UBeaconSubsystem* UBeaconSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBeaconSubsystem>() : nullptr;
}

TStatId UBeaconSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBeaconSubsystem, STATGROUP_Tickables);
}

void UBeaconSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterSections();
}

void UBeaconSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RegisterSections();
	CacheLandmarks();
	// Sin menú (pruebas de humo, bot) no hay «Nueva partida»: la partida empieza ya.
	if (FParse::Param(FCommandLine::Get(), TEXT("SkipMenu")))
	{
		BeginNewRun();
	}
}

void UBeaconSubsystem::Deinitialize()
{
	if (bSectionsRegistered)
	{
		if (UExploredSaveSubsystem* Save = FindSave())
		{
			Save->UnregisterSection(ExploredSaveStates::SectionProgress);
			Save->UnregisterSection(ExploredSaveStates::SectionPages);
		}
		bSectionsRegistered = false;
	}
	Super::Deinitialize();
}

UExploredSaveSubsystem* UBeaconSubsystem::FindSave() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
}

void UBeaconSubsystem::RegisterSections()
{
	UExploredSaveSubsystem* Save = bSectionsRegistered ? nullptr : FindSave();
	if (!Save)
	{
		return;
	}
	const TWeakObjectPtr<UBeaconSubsystem> WeakThis(this);
	const bool bProgress = Save->RegisterSection(ExploredSaveStates::SectionProgress,
		[WeakThis](FSaveArchive& Ar) { if (const UBeaconSubsystem* Self = WeakThis.Get()) { Self->SaveProgress(Ar); } },
		[WeakThis](const FSaveArchive& Ar) { if (UBeaconSubsystem* Self = WeakThis.Get()) { Self->LoadProgress(Ar); } });
	const bool bPages = Save->RegisterSection(ExploredSaveStates::SectionPages,
		[WeakThis](FSaveArchive& Ar) { if (const UBeaconSubsystem* Self = WeakThis.Get()) { Self->SavePages(Ar); } },
		[WeakThis](const FSaveArchive& Ar) { if (UBeaconSubsystem* Self = WeakThis.Get()) { Self->LoadPages(Ar); } });
	if (!bProgress || !bPages)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Baliza] Las secciones «progress» o «pages» ya las tenía otro sistema"));
	}
	bSectionsRegistered = true;
}

void UBeaconSubsystem::SaveProgress(FSaveArchive& Ar) const
{
	ExploredSaveStates::SaveBeaconProgress(Ar, Run);
}

void UBeaconSubsystem::LoadProgress(const FSaveArchive& Ar)
{
	if (!ExploredSaveStates::LoadBeaconProgress(Ar, Run))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Baliza] La sección «progress» de la partida no es coherente: se empieza desde el prólogo"));
	}
	// Si se guardó con el Mistral ya llegado, el final se vuelve a ver: nada del final en curso sobrevive a la carga.
	Run.bMistralArrived = false;
	bRunActive = true;
	SampleTimer = 0.0f;
	ResetEnding();
	LastDriftwoodDay = 0;
	UE_LOG(LogExplored, Display, TEXT("[Baliza] Partida cargada en la etapa %s"), FBeaconDirectorModel::StageName(Run.Progress.Stage));
}

void UBeaconSubsystem::SavePages(FSaveArchive& Ar) const
{
	ExploredSaveStates::SavePages(Ar, Run.Notebook);
}

void UBeaconSubsystem::LoadPages(const FSaveArchive& Ar)
{
	if (!ExploredSaveStates::LoadPages(Ar, Run.Notebook))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Baliza] La sección «pages» de la partida no se puede leer"));
	}
}

void UBeaconSubsystem::CacheLandmarks()
{
	using namespace BeaconSubsystemDetail;
	const TSharedRef<const FIslandProps> Props = ExploredIslandProps::Get();
	const auto Find = [&Props](FName Id, FVector& Out) -> bool
	{
		const FIslandProp* Prop = Props->FindProp(Id);
		if (Prop)
		{
			Out = IslandPropsPlacement::WorldLocation(*Prop, 0.0f);
		}
		return Prop != nullptr;
	};
	bHasTower = Find(TowerProp, TowerLocation);
	bHasHouse = Find(HouseProp, HouseLocation);
	bHasFreighter = Find(FreighterProp, FreighterLocation);
	if (!bHasTower || !bHasHouse || !bHasFreighter)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Baliza] Faltan faro_torre, faro_casa o carguero_casco en island_props.json"));
	}
}

void UBeaconSubsystem::BeginNewRun()
{
	Run = FBeaconRunState();
	bRunActive = true;
	SampleTimer = 0.0f;
	ResetEnding();
	LastDriftwoodDay = 0;
	if (UWorld* World = GetWorld())
	{
		if (UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
		{
			Time->SetTime(BeaconSubsystemDetail::StartDay, BeaconSubsystemDetail::StartHour);
		}
	}
	if (UIslandBodyComponent* Body = UIslandBodyComponent::FindOn(GetPlayerPawn()))
	{
		Body->ResetForNewRun();
	}
	if (AExploredHUD* HUD = BeaconSubsystemDetail::FindHUD(this))
	{
		HUD->ShowCard(ExploredBeaconTexts::Get().Get(TEXT("cartel_inicio")), BeaconSubsystemDetail::CardSeconds);
	}
	UE_LOG(LogExplored, Display, TEXT("[Baliza] Nueva partida: día 1, 07:00"));
}

void UBeaconSubsystem::ResetEnding()
{
	bEndingStarted = false;
	MistralStartedAt = -1.0;
	bHasStartLocation = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HornTimer);
		World->GetTimerManager().ClearTimer(FadeTimer);
	}
	if (Mistral)
	{
		Mistral->Destroy();
		Mistral = nullptr;
	}
	if (FinaleCamera)
	{
		if (AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
		{
			PC->SetCutsceneActive(false);
			PC->ResetIgnoreInputFlags();
			if (APawn* Pawn = PC->GetPawn())
			{
				PC->SetViewTarget(Pawn);
			}
		}
		FinaleCamera->Destroy();
		FinaleCamera = nullptr;
	}
	if (UExploredMusicSubsystem* Music = GetWorld() ? GetWorld()->GetSubsystem<UExploredMusicSubsystem>() : nullptr)
	{
		Music->SetFinale(false);
	}
}

APawn* UBeaconSubsystem::GetPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

double UBeaconSubsystem::GetAbsHours() const
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	return Time ? FStormClockModel::ToAbsoluteHours(Time->GetDay(), Time->GetHours()) : 0.0;
}

FBeaconFacts UBeaconSubsystem::MeasureFacts(const APawn& Pawn) const
{
	using namespace BeaconSubsystemDetail;
	FBeaconFacts Facts;
	Facts.AbsHours = GetAbsHours();
	const FVector Here = Pawn.GetActorLocation();
	Facts.MovedMeters = bHasStartLocation ? HorizontalMeters(Here, StartLocation) : 0.0f;
	Facts.MetersToTower = bHasTower ? HorizontalMeters(Here, TowerLocation) : -1.0f;
	Facts.MetersToHouse = bHasHouse ? HorizontalMeters(Here, HouseLocation) : -1.0f;
	Facts.MetersToFreighter = bHasFreighter ? HorizontalMeters(Here, FreighterLocation) : -1.0f;

	const UExploredWeatherSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	const FStormClockModel Storm = Weather ? Weather->GetStorm() : FStormClockModel();
	Facts.bCycloneEye = Storm.StateAt(Facts.AbsHours) == EStormState::CycloneEye;
	if (const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(&Pawn))
	{
		const FBackpackState& Pack = Survivor->GetBackpack();
		Facts.BarometerHpa = FBackpackModel::Count(Pack, BarometerItem) > 0 ? Storm.PressureAt(Facts.AbsHours) : 0.0f;
		Facts.bDrankCoconut = FBackpackModel::Count(Pack, EmptyCoconutItem) > 0;
		Facts.bHasCookedFish = FBackpackModel::Count(Pack, CookedFishItem) > 0;
		Facts.OilJarsInPack = FBackpackModel::Count(Pack, OilItem);
		Facts.bHasWick = FBackpackModel::Count(Pack, WickItem) > 0;
		Facts.bHasLens = FBackpackModel::Count(Pack, LensItem) > 0;
	}
	for (TActorIterator<AExploredRaftSite> It(GetWorld()); It && !Facts.bRaftBuilt; ++It)
	{
		Facts.bRaftBuilt = It->IsRaftBuilt();
	}
	for (TActorIterator<AExploredFire> It(GetWorld()); It && !Facts.bFireLit; ++It)
	{
		Facts.bFireLit = It->IsBurning();
	}
	return Facts;
}

void UBeaconSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SampleTimer -= DeltaTime;
	if (SampleTimer > 0.0f)
	{
		return;
	}
	SampleTimer = BeaconSubsystemDetail::SampleSeconds;
	Sample();
}

void UBeaconSubsystem::Sample()
{
	// En el menú principal el mundo sigue corriendo: la partida solo avanza jugando.
	const AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	const APawn* Pawn = GetPlayerPawn();
	if (!bRunActive || !Pawn || (PC && PC->GetUIMode() != EExploredUIMode::Playing))
	{
		return;
	}
	if (!bHasStartLocation)
	{
		StartLocation = Pawn->GetActorLocation();
		bHasStartLocation = true;
	}
	bSheltered = false;
	for (TActorIterator<AExploredShelterVolume> It(GetWorld()); It && !bSheltered; ++It)
	{
		bSheltered = It->IsActorSheltered(Pawn);
	}
	Apply(FBeaconDirectorModel::Step(Run, MeasureFacts(*Pawn), Tuning));
	WashUpDriftwood(*Pawn);
}

void UBeaconSubsystem::WashUpDriftwood(const APawn& Pawn)
{
	UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	if (!Time || Time->GetDay() <= 1 || Time->GetHours() < 6.0f || Time->GetDay() == LastDriftwoodDay)
	{
		return;
	}
	LastDriftwoodDay = Time->GetDay();
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(&Pawn);
	UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this);
	if (!Survivor || !Island)
	{
		return;
	}
	bool bFireBurning = false;
	for (TActorIterator<AExploredFire> It(World); It && !bFireBurning; ++It)
	{
		bFireBurning = It->IsBurning();
	}
	const int32 Wood = FSupplyCrateModel::DriftwoodAtDawn(FBackpackModel::ToCounts(Survivor->GetBackpack()), Run.Progress,
		AExploredSupplyCrate::CountIslandResources(World), bFireBurning);
	const FIslandProp* Beach = ExploredIslandProps::Get()->FindProp(FName(TEXT("lena_cala_1")));
	if (Wood <= 0 || !Beach)
	{
		return;
	}
	Island->SpawnDropped(FSupplyCrateModel::WoodId(), Wood, IslandPropsPlacement::WorldLocation(*Beach, 0.0f) + FVector(0.0, 0.0, 30.0));
	UE_LOG(LogExplored, Display, TEXT("[Baliza] Leña de deriva en la Cala Varada: %d (día %d)"), Wood, LastDriftwoodDay);
	if (AExploredHUD* HUD = BeaconSubsystemDetail::FindHUD(this))
	{
		HUD->ShowSubtitle(LOCTEXT("Driftwood", "Esta noche el mar ha dejado leña en la Cala Varada."), 5.0f);
	}
}

void UBeaconSubsystem::Apply(const FBeaconStepResult& Result)
{
	const EBeaconStage Before = Run.Progress.Stage;
	Run = Result.State;
	if (Run.Progress.Stage != Before)
	{
		UE_LOG(LogExplored, Display, TEXT("[Baliza] Etapa: %s → %s"), FBeaconDirectorModel::StageName(Before),
			FBeaconDirectorModel::StageName(Run.Progress.Stage));
	}
	for (const EIriaThought Thought : Result.Thoughts)
	{
		ShowThought(Thought);
	}
	if (Result.bAutosave)
	{
		if (UExploredSaveSubsystem* Save = FindSave())
		{
			UE_LOG(LogExplored, Display, TEXT("[Baliza] Autoguardado de la historia (%.2f h)"), Run.LastAbsHours);
			Save->RequestAutosave(ESaveTrigger::Story);
		}
	}
	if (Result.bMistralDue)
	{
		StartMistral();
	}
}

void UBeaconSubsystem::ShowThought(EIriaThought Thought) const
{
	const FName Id = FBeaconDirectorModel::ThoughtTextId(Thought);
	const FBeaconTexts& Texts = ExploredBeaconTexts::Get();
	UE_LOG(LogExplored, Display, TEXT("[Baliza] Pensamiento: %s"), *Id.ToString());
	if (AExploredHUD* HUD = BeaconSubsystemDetail::FindHUD(this))
	{
		HUD->ShowSubtitle(Texts.Get(Id), Texts.SubtitleSeconds);
	}
}

void UBeaconSubsystem::TriggerThought(EIriaThought Thought)
{
	Apply(FBeaconDirectorModel::TriggerThought(Run, Thought));
}

FText UBeaconSubsystem::GetObjectiveText() const
{
	FBeaconObjectiveContext Context;
	Context.bPageOneRead = FNotebookModel::IsRead(Run.Notebook, 1);
	Context.bSheltered = bSheltered;
	const FName Id = FBeaconProgressModel::CurrentObjective(Run.Progress, Context);
	return Id.IsNone() ? FText::GetEmpty() : ExploredBeaconTexts::Get().Get(Id);
}

bool UBeaconSubsystem::ReadPage(int32 Page)
{
	const FBeaconStepResult Result = FBeaconDirectorModel::ReadPage(Run, Page);
	if (!Result.bOk)
	{
		return false;
	}
	Apply(Result);
	const FBeaconTexts& Texts = ExploredBeaconTexts::Get();
	const FText Title = FText::Format(LOCTEXT("PageTitle", "Cuaderno de guardia · página {0} de {1}"),
		FText::AsNumber(Page), FText::AsNumber(FNotebookModel::PageCount));
	if (AExploredHUD* HUD = BeaconSubsystemDetail::FindHUD(this))
	{
		HUD->ShowPage(Title, Texts.Get(FNotebookModel::PageTextId(Page)), Texts.Get(FNotebookModel::NoteTextId(Page)),
			BeaconSubsystemDetail::PageSeconds);
	}
	PlaySound2D(BeaconSubsystemDetail::PageSound);
	UE_LOG(LogExplored, Display, TEXT("[Baliza] Página %d leída (%d/8)"), Page, FNotebookModel::ReadCount(Run.Notebook));
	return true;
}

bool UBeaconSubsystem::TryFillSlot(ELanternSlot Slot, UExploredSurvivorComponent& Survivor)
{
	using namespace BeaconSubsystemDetail;
	// Una muestra ya: lo recogido hace menos de medio segundo cuenta como hito.
	Sample();
	const TMap<FName, int32> Cost = SlotCost(Slot);
	FBackpackState Next;
	const EBackpackResult Paid = FBackpackModel::Exchange(Survivor.GetBackpack(), FBackpackRules::Default(), Cost, {}, Next);
	const FBeaconStepResult Result = FBeaconDirectorModel::FillSlot(Run, Slot);
	if (Cost.Num() == 0 || Paid != EBackpackResult::Ok || !Result.bOk)
	{
		Survivor.ShowMessage(MissingForSlot(Slot));
		return false;
	}
	Survivor.ApplyBackpack(Next);
	Apply(Result);
	UE_LOG(LogExplored, Display, TEXT("[Baliza] Ranura %d de la linterna llena (%s)"), static_cast<int32>(Slot),
		FBeaconProgressModel::IsLanternMounted(Run.Progress) ? TEXT("linterna montada") : TEXT("faltan ranuras"));
	return true;
}

bool UBeaconSubsystem::TryLight()
{
	const FBeaconStepResult Result = FBeaconDirectorModel::Light(Run, GetAbsHours());
	if (!Result.bOk)
	{
		return false;
	}
	Apply(Result);
	PlaySound2D(BeaconSubsystemDetail::IgniteSound);
	const double ArrivesAt = FBeaconDirectorModel::MistralArrivalHours(Run.LitAtHours, Tuning);
	UE_LOG(LogExplored, Display, TEXT("[Baliza] Baliza encendida a las %.2f h; el Mistral llega a las %.2f h"), Run.LitAtHours, ArrivesAt);
	// Encendida de noche, el Mistral aparece ya; de día, a las 21:00 (lo decide Step).
	Sample();
	return true;
}

void UBeaconSubsystem::StartMistral()
{
	using namespace BeaconSubsystemDetail;
	UWorld* World = GetWorld();
	if (!World || bEndingStarted)
	{
		return;
	}
	bEndingStarted = true;
	MistralStartedAt = World->GetUnpausedTimeSeconds();
	const FVector Anchor(TowerLocation.X, TowerLocation.Y, 0.0);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Mistral = World->SpawnActor<AExploredMistralLights>(AExploredMistralLights::StaticClass(), FTransform(Anchor + MistralFromOffsetCm), Params);
	if (Mistral)
	{
		Mistral->Approach(Anchor + MistralFromOffsetCm, Anchor + MistralToOffsetCm, MistralApproachSeconds);
	}
	if (UExploredMusicSubsystem* Music = World->GetSubsystem<UExploredMusicSubsystem>())
	{
		Music->SetFinale(true);
	}
	StartFinaleCamera();
	UE_LOG(LogExplored, Display, TEXT("[Baliza] Luces del Mistral en el horizonte"));
	World->GetTimerManager().SetTimer(HornTimer, FTimerDelegate::CreateUObject(this, &UBeaconSubsystem::SoundHorn),
		FFinaleSequenceModel::HornDelaySeconds(FinaleTiming()), false);
}

void UBeaconSubsystem::StartFinaleCamera()
{
	using namespace BeaconSubsystemDetail;
	UWorld* World = GetWorld();
	AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!World || !PC || FinaleCamera)
	{
		return;
	}
	// La linterna marca el encuadre; sin ella (mapa de pruebas), la torre a 30 m de altura.
	FVector Lantern(TowerLocation.X, TowerLocation.Y, TowerLocation.Z + 3000.0);
	for (TActorIterator<ALighthouseLamp> It(World); It; ++It)
	{
		Lantern = It->GetActorLocation();
		break;
	}
	const FVector Toward = MistralToOffsetCm.GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Toward);
	const FVector Eye = Lantern - Toward * FinaleCamBackCm + Side * FinaleCamSideCm - FVector(0.0, 0.0, FinaleCamDropCm);
	const FVector Look(Lantern.X + Toward.X * FinaleLookAheadCm, Lantern.Y + Toward.Y * FinaleLookAheadCm, FinaleLookZCm);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FinaleCamera = World->SpawnActor<ACameraActor>(Eye, (Look - Eye).Rotation(), Params);
	if (!FinaleCamera)
	{
		return;
	}
	FinaleCamera->GetCameraComponent()->SetFieldOfView(70.0f);
	FinaleCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	// El jugador mira el final: sin moverse ni girar la cámara hasta el epílogo.
	PC->SetCutsceneActive(true);
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	PC->SetViewTargetWithBlend(FinaleCamera, FinaleTiming().CameraBlendSeconds, VTBlend_Cubic);
}

float UBeaconSubsystem::GetSecondsSinceMistral() const
{
	const UWorld* World = GetWorld();
	return World && MistralStartedAt >= 0.0 ? static_cast<float>(World->GetUnpausedTimeSeconds() - MistralStartedAt) : -1.0f;
}

FFinaleTiming UBeaconSubsystem::FinaleTiming() const
{
	FFinaleTiming Timing;
	Timing.MistralApproachSeconds = MistralApproachSeconds;
	Timing.HornAtApproachFraction = BeaconSubsystemDetail::HornAtApproachFraction;
	Timing.HornToFadeSeconds = HornToFadeSeconds;
	Timing.CameraBlendSeconds = BeaconSubsystemDetail::FinaleBlendSeconds;
	return FFinaleSequenceModel::Sanitize(Timing);
}

void UBeaconSubsystem::SoundHorn()
{
	PlaySound2D(BeaconSubsystemDetail::HornSound, BeaconSubsystemDetail::HornPitch);
	UE_LOG(LogExplored, Display, TEXT("[Baliza] Bocina del Mistral"));
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FadeTimer, FTimerDelegate::CreateUObject(this, &UBeaconSubsystem::FinishEnding),
			FFinaleSequenceModel::FadeDelaySeconds(FinaleTiming()), false);
	}
}

void UBeaconSubsystem::FinishEnding()
{
	const FBeaconTexts& Texts = ExploredBeaconTexts::Get();
	const EBeaconEpilogue Epilogue = FFinaleSequenceModel::EpilogueForPages(FNotebookModel::ReadCount(Run.Notebook));
	UE_LOG(LogExplored, Display, TEXT("[Baliza] Epílogo %s (%d/8 páginas)"),
		Epilogue == EBeaconEpilogue::Complete ? TEXT("completo") : TEXT("parcial"), FNotebookModel::ReadCount(Run.Notebook));
	if (AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->ShowEnding(Texts.Get(FBeaconProgressModel::EpilogueTextId(Epilogue)), Texts.Get(FBeaconProgressModel::CommonEpilogueTextId()));
	}
}

void UBeaconSubsystem::PlaySound2D(const TCHAR* Path, float Pitch) const
{
	const FSoftObjectPath SoundPath(Path);
	if (!FPackageName::DoesPackageExist(SoundPath.GetLongPackageName()))
	{
		UE_LOG(LogExplored, Verbose, TEXT("[Baliza] Sin sonido %s"), Path);
		return;
	}
	if (USoundBase* Sound = Cast<USoundBase>(SoundPath.TryLoad()))
	{
		UGameplayStatics::PlaySound2D(this, Sound, 1.0f, Pitch);
	}
}

void UBeaconSubsystem::DebugSetStage(EBeaconStage Stage)
{
	FBeaconRunState Forced = FBeaconDirectorModel::StateForStage(Stage);
	// Se conservan las páginas y los pensamientos ya vistos: el atajo no los borra.
	Forced.Notebook.PagesMask |= Run.Notebook.PagesMask;
	Forced.ThoughtsShown = Run.ThoughtsShown;
	Forced.LastAbsHours = Run.LastAbsHours;
	if (Forced.Progress.bBeaconLit)
	{
		Forced.LitAtHours = GetAbsHours();
	}
	Run = Forced;
	bRunActive = true;
	bEndingStarted = false;
	UE_LOG(LogExplored, Display, TEXT("[Baliza] BalizaStage: %s"), FBeaconDirectorModel::StageName(Stage));
}

#undef LOCTEXT_NAMESPACE
