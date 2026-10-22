#include "UI/ExploredTutorialSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Stats/Stats.h"

#include "Boats/ExploredRaft.h"
#include "Explored.h"
#include "Interaction/InteractionComponent.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/BeaconSubsystem.h"
#include "Player/ExploredCharacter.h"
#include "Save/SaveArchive.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/ExploredHUD.h"
#include "UI/ExploredPlayerController.h"
#include "UI/ExploredSaveSubsystem.h"

#define LOCTEXT_NAMESPACE "ExploredTutorial"

namespace ExploredTutorialDetail
{
	const TCHAR* const Section = TEXT("tutorial");
	constexpr float SampleSeconds = 0.5f;
	const FName CoconutItem(TEXT("coco"));
	const FName FibreItem(TEXT("fibra"));
	const FName BarometerItem(TEXT("barometro"));
}

bool UExploredTutorialSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExploredTutorialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterSections();
}

void UExploredTutorialSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RegisterSections();
}

void UExploredTutorialSubsystem::Deinitialize()
{
	if (bSectionsRegistered)
	{
		if (UExploredSaveSubsystem* Save = FindSave())
		{
			Save->UnregisterSection(ExploredTutorialDetail::Section);
		}
		bSectionsRegistered = false;
	}
	Super::Deinitialize();
}

TStatId UExploredTutorialSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredTutorialSubsystem, STATGROUP_Tickables);
}

UExploredTutorialSubsystem* UExploredTutorialSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = (GEngine && WorldContextObject)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<UExploredTutorialSubsystem>() : nullptr;
}

void UExploredTutorialSubsystem::BeginNewRun()
{
	State = FTutorialHintState();
	PlayingSeconds = 0.0f;
	BusySeconds = 0.0f;
}

void UExploredTutorialSubsystem::Tick(float DeltaTime)
{
	BusySeconds = FMath::Max(0.0f, BusySeconds - DeltaTime);
	SampleTimer -= DeltaTime;
	if (SampleTimer > 0.0f)
	{
		return;
	}
	const float Elapsed = ExploredTutorialDetail::SampleSeconds - SampleTimer;
	SampleTimer = ExploredTutorialDetail::SampleSeconds;
	if (FTutorialHintModel::AllShown(State))
	{
		return;
	}
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AExploredCharacter* Character = PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
	AExploredHUD* HUD = PC ? Cast<AExploredHUD>(PC->GetHUD()) : nullptr;
	if (!Character || !HUD || !IsPlaying(Character))
	{
		return;
	}
	PlayingSeconds += Elapsed;
	State = FTutorialHintModel::RequestTriggered(State, MeasureFacts(*Character), Tuning);
	if (BusySeconds <= 0.0f)
	{
		ShowNext(*HUD);
	}
}

bool UExploredTutorialSubsystem::IsPlaying(const AExploredCharacter* Character) const
{
	const AExploredPlayerController* PC = Character ? Cast<AExploredPlayerController>(Character->GetController()) : nullptr;
	if (!PC || PC->GetUIMode() != EExploredUIMode::Playing || PC->IsCutsceneActive() || PC->IsShowingEnding())
	{
		return false;
	}
	const UIslandBodyComponent* Body = Character->GetIslandBodyComponent();
	return !Body || !Body->IsFainting();
}

FTutorialFacts UExploredTutorialSubsystem::MeasureFacts(const AExploredCharacter& Character) const
{
	using namespace ExploredTutorialDetail;
	FTutorialFacts Facts;
	Facts.PlayingSeconds = PlayingSeconds;
	const UInteractionComponent* Interaction = Character.GetInteractionComponent();
	Facts.bHasFocus = Interaction && Interaction->GetFocusedActor() != nullptr;
	if (const UIslandBodyComponent* Body = Character.GetIslandBodyComponent())
	{
		Facts.Thirst = Body->GetBars().Thirst;
		Facts.bSleepHour = Body->IsSleepHour();
	}
	if (const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(&Character))
	{
		Facts.bCarriesDrink = Survivor->GetItemCount(CoconutItem) > 0;
		Facts.Fibres = Survivor->GetItemCount(FibreItem);
		Facts.bHasBarometer = Survivor->GetItemCount(BarometerItem) > 0;
	}
	if (const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(&Character))
	{
		Facts.PagesRead = Beacon->GetPagesRead();
	}
	for (TActorIterator<AExploredRaft> It(GetWorld()); It; ++It)
	{
		if (It->GetOccupant() == &Character)
		{
			Facts.bOnRaft = true;
			break;
		}
	}
	return Facts;
}

void UExploredTutorialSubsystem::ShowNext(AExploredHUD& HUD)
{
	const FTutorialHintResult Next = FTutorialHintModel::PopNext(State);
	if (!Next.bOk)
	{
		return;
	}
	State = Next.State;
	HUD.PushNotification(HintText(Next.Hint, HUD), FLinearColor::White, HintSeconds);
	BusySeconds = HintSeconds + GapSeconds;
	UE_LOG(LogExplored, Display, TEXT("[Pistas] %s (%d de %d)"), *FTutorialHintModel::TextId(Next.Hint).ToString(),
		FTutorialHintModel::ShownCount(State), FTutorialHintModel::HintCount);
}

FText UExploredTutorialSubsystem::HintText(ETutorialHint Hint, const AExploredHUD& HUD) const
{
	auto Key = [&HUD](const TCHAR* Action) { return FText::FromString(HUD.KeyNameFor(FName(Action))); };
	switch (Hint)
	{
	case ETutorialHint::Move:
		return FText::Format(LOCTEXT("TutMove", "W A S D moverse · {0} correr · {1} saltar · Esc pausa"),
			Key(TEXT("IA_Sprint")), Key(TEXT("IA_Jump")));
	case ETutorialHint::Pickup:
		return FText::Format(LOCTEXT("TutPickup", "{0} coge o usa lo que miras; lo que coges va a la mochila"),
			Key(TEXT("IA_Interact")));
	case ETutorialHint::Drink:
		return FText::Format(LOCTEXT("TutDrink", "Para beber: {0} y «Usar» en el coco, o elígelo con 1-3 y {1}"),
			Key(TEXT("IA_ToggleBackpack")), Key(TEXT("IA_UsePrimary")));
	case ETutorialHint::Craft:
		return FText::Format(LOCTEXT("TutCraft", "Con 2 fibras se trenza una cuerda: {0} y el botón «Trenzar»"),
			Key(TEXT("IA_ToggleBackpack")));
	case ETutorialHint::Sleep:
		return FText::Format(LOCTEXT("TutSleep", "Ya puedes dormir: bajo techo y sin mirar nada, {0}"),
			Key(TEXT("IA_Interact")));
	case ETutorialHint::Notebook:
		return LOCTEXT("TutNotebook", "J abre el cuaderno: las páginas de Roig y tus notas");
	case ETutorialHint::Barometer:
		return LOCTEXT("TutBarometer", "Arriba a la derecha, el barómetro: si la flecha baja, viene temporal");
	case ETutorialHint::Paddle:
		return LOCTEXT("TutPaddle", "En la balsa: W rema (con el remo), A y D giran, E para bajar");
	default:
		return FText::GetEmpty();
	}
}

// ---------------------------------------------------------------------------
// Guardado
// ---------------------------------------------------------------------------

UExploredSaveSubsystem* UExploredTutorialSubsystem::FindSave() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
}

void UExploredTutorialSubsystem::RegisterSections()
{
	UExploredSaveSubsystem* Save = bSectionsRegistered ? nullptr : FindSave();
	if (!Save)
	{
		return;
	}
	const TWeakObjectPtr<UExploredTutorialSubsystem> WeakThis(this);
	const bool bOk = Save->RegisterSection(ExploredTutorialDetail::Section,
		[WeakThis](FSaveArchive& Ar) { if (const UExploredTutorialSubsystem* Self = WeakThis.Get()) { Self->SaveTutorial(Ar); } },
		[WeakThis](const FSaveArchive& Ar) { if (UExploredTutorialSubsystem* Self = WeakThis.Get()) { Self->LoadTutorial(Ar); } });
	if (!bOk)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Pistas] La sección «tutorial» ya la tenía otro sistema"));
	}
	bSectionsRegistered = true;
}

void UExploredTutorialSubsystem::SaveTutorial(FSaveArchive& Ar) const
{
	const FTutorialHintRecord Record = FTutorialHintModel::ToRecord(State);
	TArray<int32> Queue;
	for (const uint8 Raw : Record.Queue)
	{
		Queue.Add(Raw);
	}
	Ar.Write(TEXT("shown"), static_cast<int32>(Record.ShownMask));
	Ar.Write(TEXT("queue"), Queue);
}

void UExploredTutorialSubsystem::LoadTutorial(const FSaveArchive& Ar)
{
	PlayingSeconds = 0.0f;
	BusySeconds = 0.0f;
	int32 Shown = 0;
	if (Ar.IsEmpty() || !Ar.Read(TEXT("shown"), Shown))
	{
		// Partida guardada antes de que existieran las pistas: quien la jugó ya sabe moverse.
		State = FTutorialHintModel::AllShownState();
		return;
	}
	TArray<int32> Queue;
	Ar.Read(TEXT("queue"), Queue);
	FTutorialHintRecord Record;
	bool bInRange = Shown >= 0 && Shown <= MAX_uint8 && Queue.Num() <= FTutorialHintModel::HintCount;
	Record.ShownMask = static_cast<uint8>(FMath::Clamp(Shown, 0, static_cast<int32>(MAX_uint8)));
	for (const int32 Raw : Queue)
	{
		bInRange &= Raw >= 0 && Raw <= MAX_uint8;
		Record.Queue.Add(static_cast<uint8>(FMath::Clamp(Raw, 0, static_cast<int32>(MAX_uint8))));
	}
	const FTutorialHintResult Loaded = FTutorialHintModel::FromRecord(Record);
	if (!bInRange || !Loaded.bOk)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Pistas] La sección «tutorial» no es coherente: se dan todas por vistas"));
		State = FTutorialHintModel::AllShownState();
		return;
	}
	State = Loaded.State;
}

#undef LOCTEXT_NAMESPACE
