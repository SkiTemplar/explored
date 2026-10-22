#include "Core/ExploredWiringSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Stats/Stats.h"
#include "Subsystems/SubsystemCollection.h"

#include "Audio/ExploredMusicSubsystem.h"
#include "Boats/ExploredRaft.h"
#include "Carry/CarryComponent.h"
#include "Cooking/ExploredFire.h"
#include "Events/WorldEventsSubsystem.h"
#include "ExploredGameMode.h"
#include "Fauna/ExploredFaunaManager.h"
#include "Fishing/ExploredFishingSubsystem.h"
#include "Player/ExploredCharacter.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/BodySignalsComponent.h"
#include "UI/ExploredSaveSubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"

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

	if (Events)
	{
		EventStartedHandle = Events->OnEventStarted.AddUObject(this, &UExploredWiringSubsystem::HandleEventStarted);
		EventEndedHandle = Events->OnEventEnded.AddUObject(this, &UExploredWiringSubsystem::HandleEventEnded);
	}
	if (UExploredSaveSubsystem* Save = FindSaveSubsystem())
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
		if (UExploredSaveSubsystem* Save = FindSaveSubsystem())
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
	if (UExploredSaveSubsystem* Save = FindSaveSubsystem())
	{
		UnregisterSections(*Save);
	}
	Super::Deinitialize();
}

TStatId UExploredWiringSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredWiringSubsystem, STATGROUP_Tickables);
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

void UExploredWiringSubsystem::BeginNewRun()
{
	// Una carga anterior que no llegó a aplicarse no puede pisar la mochila de la partida nueva.
	PendingInventory.Reset();
	PendingBody.Reset();
}

void UExploredWiringSubsystem::Tick(float DeltaTime)
{
	SampleTimer -= DeltaTime;
	if (SampleTimer > 0.0f)
	{
		return;
	}
	SampleTimer = FMath::Max(SampleIntervalSeconds, 0.05f);
	Sample();
}

// ---------------------------------------------------------------------------
// Muestreo
// ---------------------------------------------------------------------------

void UExploredWiringSubsystem::Sample()
{
	AExploredCharacter* Character = GetPlayerCharacter();
	if (Character != BoundPawn.Get())
	{
		UnbindPawn();
		BindPawn(Character);
	}
	BindFaunaManager();
	ApplyPendingPawnSections();
	if (!Character)
	{
		return;
	}
	UpdateBody(*Character);
	UpdateFaunaAboard(*Character);
}

void UExploredWiringSubsystem::UpdateBody(AExploredCharacter& Character)
{
	UBodySignalsComponent* Body = Character.GetBodySignalsComponent();
	if (!Body)
	{
		return;
	}
	// Calor de los fuegos cercanos (FFireModel::HeatAtDistance en cada uno).
	const FVector Location = Character.GetActorLocation();
	TArray<float> Heats;
	for (TActorIterator<AExploredFire> It(GetWorld()); It; ++It)
	{
		if (FVector::DistSquared(It->GetActorLocation(), Location) <= FMath::Square(FireSearchRadiusCm))
		{
			Heats.Add(It->GetHeatAt(Location));
		}
	}
	const UCarryComponent* Carry = Character.GetCarryComponent();
	ExploredLinks::FSurvivalLinkInputs Links;
	Links.FireHeat = ExploredLinks::CombineFireHeat(Heats);
	Links.CarriedWeightRatio = Carry ? Carry->GetCarriedWeightRatio() : 0.0f;
	Body->SetLinkInputs(Links);
}

void UExploredWiringSubsystem::UpdateFaunaAboard(AExploredCharacter& Character)
{
	AExploredFaunaManager* Fauna = BoundFauna.Get();
	if (!Fauna)
	{
		return;
	}
	bool bAboard = false;
	for (TActorIterator<AExploredRaft> It(GetWorld()); It; ++It)
	{
		if (It->GetOccupant() == &Character)
		{
			bAboard = true;
			break;
		}
	}
	// Delfines junto a la balsa y gaviotas detrás del pescado.
	const UCarryComponent* Carry = Character.GetCarryComponent();
	Fauna->SetPlayerBoatState(bAboard, Carry && Carry->HasItemWithTag(TEXT("pescado")));
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
