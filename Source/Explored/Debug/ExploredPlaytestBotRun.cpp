// Partida completa de Baliza Siete por bot (UExploredPlaytestBot con «-BalizaRun»). Ruta competente
// del GDD §3: coco, fuego, pesca y cocina, faro y página 1, dormir, cuerdas/remo/balsa, carguero y
// lente, volver, refugio en el ciclón, aceite ×2, mecha, montar la linterna, esperar la noche y
// encender. Teletransporte entre puntos de island_props.json con el tiempo del trayecto a pie
// esperado con el reloj acelerado; acciones a ×1. Ver FPlaytestRunModel para la conversión a 1×.

#include "Debug/ExploredPlaytestBot.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"

#include "Boats/ExploredRaft.h"
#include "Cooking/ExploredFire.h"
#include "Debug/ExploredPlaytestAuditor.h"
#include "Explored.h"
#include "Fishing/ExploredFishingSpot.h"
#include "Fishing/CastFishingModel.h"
#include "Interaction/ExploredInteractable.h"
#include "Items/ExploredPickup.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/BeaconDirectorModel.h"
#include "Narrative/BeaconSubsystem.h"
#include "Narrative/ExploredBeaconActors.h"
#include "Player/ExploredCharacter.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/ExploredPlayerController.h"
#include "UI/ExploredSaveSubsystem.h"
#include "Weather/StormClockModel.h"
#include "WorldGen/ExploredIslandPopulator.h"
#include "WorldGen/ExploredShelterVolume.h"

/** Cuenta los errores y avisos del log mientras dura la partida del bot. */
class FBalizaRunLogCapture : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
		FScopeLock Guard(&Lock);
		if (Level == ELogVerbosity::Error || Level == ELogVerbosity::Fatal)
		{
			Errors.Add(FString::Printf(TEXT("%s: %s"), *Category.ToString(), Message));
		}
		else if (Level == ELogVerbosity::Warning)
		{
			++Warnings;
		}
	}

	TArray<FString> TakeErrors()
	{
		FScopeLock Guard(&Lock);
		TArray<FString> Out = MoveTemp(Errors);
		Errors.Reset();
		return Out;
	}

	int32 GetWarnings()
	{
		FScopeLock Guard(&Lock);
		return Warnings;
	}

private:
	FCriticalSection Lock;
	TArray<FString> Errors;
	int32 Warnings = 0;
};

namespace BalizaRunDetail
{
	constexpr float WarmUpSeconds = 4.0f;
	/** Tope real de toda la prueba (s): con ×6 la partida dura unos 15 min reales. */
	constexpr double MaxRealSeconds = 45.0 * 60.0;
	const TCHAR* const SaveSlot = TEXT("manual3");

	FVector2D Flat(const FVector& V)
	{
		return FVector2D(V.X, V.Y);
	}

	double At(int32 Day, double Hour)
	{
		return FStormClockModel::ToAbsoluteHours(Day, Hour);
	}
}

// ---------------------------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------------------------

AActor* UExploredPlaytestBot::Prop(const FString& PropId) const
{
	for (TActorIterator<AExploredIslandPopulator> It(GetWorld()); It; ++It)
	{
		if (AActor* Found = It->FindSpawned(FName(*PropId)))
		{
			return Found;
		}
	}
	return nullptr;
}

AActor* UExploredPlaytestBot::NearestFire() const
{
	const AActor* Pawn = Character.Get();
	AActor* Best = nullptr;
	double BestDist = TNumericLimits<double>::Max();
	for (TActorIterator<AExploredFire> It(GetWorld()); It && Pawn; ++It)
	{
		const double Dist = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
		if (Dist < BestDist)
		{
			Best = *It;
			BestDist = Dist;
		}
	}
	return Best;
}

UExploredSurvivorComponent* UExploredPlaytestBot::Survivor() const
{
	return UExploredSurvivorComponent::FindOn(Character.Get());
}

bool UExploredPlaytestBot::UseItem(const TCHAR* ItemId) const
{
	AExploredCharacter* Player = Character.Get();
	return Player && Player->UseBackpackItem(FName(ItemId));
}

UIslandBodyComponent* UExploredPlaytestBot::Body() const
{
	return UIslandBodyComponent::FindOn(Character.Get());
}

int32 UExploredPlaytestBot::Count(const TCHAR* ItemId) const
{
	const UExploredSurvivorComponent* S = Survivor();
	return S ? S->GetItemCount(FName(ItemId)) : 0;
}

bool UExploredPlaytestBot::Idle() const
{
	const UExploredSurvivorComponent* S = Survivor();
	const UIslandBodyComponent* B = Body();
	return S && !S->IsBusy() && (!B || !B->IsFainting());
}

bool UExploredPlaytestBot::InteractWith(AActor* Target) const
{
	AExploredCharacter* Pawn = Character.Get();
	if (!Target || !Pawn || !Target->Implements<UExploredInteractable>())
	{
		Note(FString::Printf(TEXT("sin objetivo interactuable (%s)"), Target ? *Target->GetName() : TEXT("nulo")));
		return false;
	}
	if (!IExploredInteractable::Execute_CanInteract(Target, Pawn))
	{
		Note(FString::Printf(TEXT("%s no admite interacción ahora"), *Target->GetName()));
		return false;
	}
	IExploredInteractable::Execute_Interact(Target, Pawn);
	return true;
}

bool UExploredPlaytestBot::TeleportNear(AActor* Target, bool bCountTravel)
{
	AExploredCharacter* Pawn = Character.Get();
	if (!Target || !Pawn)
	{
		return false;
	}
	const FVector From = Pawn->GetActorLocation();
	FVector Goal = Target->GetActorLocation() + FVector(-150.0, 0.0, 120.0);
	if (const AExploredShelterVolume* Shelter = Cast<AExploredShelterVolume>(Target))
	{
		Goal = Shelter->GetRespawnLocation() + FVector(0.0, 0.0, 30.0);
	}
	if (bCountTravel)
	{
		TravelLeft1x += FPlaytestRunModel::TravelSeconds1x(FVector2D::Distance(BalizaRunDetail::Flat(From), BalizaRunDetail::Flat(Goal)) / 100.0,
			RunTuning);
	}
	Pawn->SetActorLocation(Goal, false, nullptr, ETeleportType::TeleportPhysics);
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		const FVector Look = Target->GetActorLocation() - (Goal + FVector(0.0, 0.0, 60.0));
		PC->SetControlRotation(FRotator(0.0, Look.Rotation().Yaw, 0.0));
	}
	return true;
}

double UExploredPlaytestBot::AbsHours() const
{
	const UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>();
	return Time ? FStormClockModel::ToAbsoluteHours(Time->GetDay(), Time->GetHours()) : -1.0;
}

void UExploredPlaytestBot::SetClockScale(float Scale) const
{
	if (UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Time->SetTimeScale(Scale);
	}
}

void UExploredPlaytestBot::Note(const FString& Text) const
{
	UE_LOG(LogExplored, Display, TEXT("[BalizaBot] %s"), *Text);
}

// ---------------------------------------------------------------------------------------------
// Pasos genéricos
// ---------------------------------------------------------------------------------------------

void UExploredPlaytestBot::AddStep(const FString& Name, TFunction<bool()> Start, TFunction<bool()> IsDone, double Timeout1x, bool bFast)
{
	RunSteps.Add({ Name, MoveTemp(Start), MoveTemp(IsDone), Timeout1x, bFast });
}

void UExploredPlaytestBot::AddGoTo(const FString& PropId)
{
	AddGoToActor(PropId, [this, PropId]() { return Prop(PropId); });
	if (IsSlowRoute())
	{
		AddDawdle(PropId);
	}
}

void UExploredPlaytestBot::AddGoToActor(const FString& Label, TFunction<AActor*()> Target)
{
	AddStep(TEXT("ir a ") + Label, [this, Target]() { return TeleportNear(Target(), true); }, [this]() { return TravelLeft1x <= 0.0; }, // loc: ignorar (paso del bot, solo en el log)
		900.0, true);
}

void UExploredPlaytestBot::AddUse(const FString& PropId, int32 Times, bool bOptional)
{
	for (int32 Index = 0; Index < Times; ++Index)
	{
		// Opcional: si no admite interacción (cantimplora llena, página ya leída) se sigue sin fallar.
		AddStep(TEXT("usar ") + PropId, [this, PropId, bOptional]() { return InteractWith(Prop(PropId)) || bOptional; },
			[this]() { return Idle() && Run.Clock.Est1xSeconds - StepStart1x > 0.3; }, 30.0);
	}
}

void UExploredPlaytestBot::AddCraft(const FString& RecipeId)
{
	AddStep(TEXT("craftear ") + RecipeId, [this, RecipeId]()
	{
		// Como el botón de receta del inventario.
		AExploredCharacter* Player = Character.Get();
		return Player && Player->CraftFromBackpack(FName(*RecipeId));
	}, [this]() { return Idle() && Run.Clock.Est1xSeconds - StepStart1x > 0.3; }, 60.0);
}

void UExploredPlaytestBot::AddWaitUntil(int32 Day, double Hour, const FString& Why)
{
	const double Target = BalizaRunDetail::At(Day, Hour);
	AddStep(FString::Printf(TEXT("esperar a D%d %05.2f (%s)"), Day, Hour, *Why), [] { return true; },
		[this, Target]() { return AbsHours() >= Target; }, 3600.0, true);
}

void UExploredPlaytestBot::AddFish(int32 Target)
{
	AddStep(FString::Printf(TEXT("pescar hasta %d peces"), Target), [this]() { return InteractWith(Prop(TEXT("punto_pesca_1"))); },
		[this, Target]()
		{
			AExploredFishingSpot* Spot = Cast<AExploredFishingSpot>(Prop(TEXT("punto_pesca_1")));
			if (!Spot || Count(TEXT("pez_crudo")) >= Target)
			{
				return Spot != nullptr;
			}
			const ECastFishingPhase Phase = static_cast<ECastFishingPhase>(Spot->GetPhase());
			if (Phase == ECastFishingPhase::Biting)
			{
				IExploredInteractable::Execute_Interact(Spot, Character.Get());
			}
			else if (Phase == ECastFishingPhase::Idle && Idle())
			{
				IExploredInteractable::Execute_Interact(Spot, Character.Get());
			}
			return false;
		}, 70.0 * Target + 60.0);
}

void UExploredPlaytestBot::AddFireJob(TFunction<AActor*()> FireGetter, const FString& Action, const FString& Product)
{
	AddStep(FString::Printf(TEXT("hoguera: %s"), *Action), [this, FireGetter, Action, Product]()
	{
		AExploredFire* Fire = Cast<AExploredFire>(FireGetter());
		AExploredCharacter* Player = Character.Get();
		MarkCount = Product.IsEmpty() ? 0 : Count(*Product);
		if (!Fire || !Player)
		{
			return false;
		}
		// Como el jugador: C (CycleVerb) hasta el verbo y E (Interact) para hacerlo.
		const FName Wanted(*Action);
		for (int32 Tries = 0; Tries < 4 && Fire->GetChosenAction(Player) != Wanted; ++Tries)
		{
			IExploredInteractable::Execute_CycleVerb(Fire, Player);
		}
		if (Fire->GetChosenAction(Player) != Wanted)
		{
			return false;
		}
		IExploredInteractable::Execute_Interact(Fire, Player);
		return true;
	}, [this, FireGetter, Product]()
	{
		AExploredFire* Fire = Cast<AExploredFire>(FireGetter());
		if (!Fire)
		{
			return false;
		}
		if (Product.IsEmpty())
		{
			return Idle() && Fire->IsBurning();
		}
		if (Fire->IsJobReady() && Idle() && Fire->GetChosenAction(Character.Get()) == FName(TEXT("recoger")))
		{
			IExploredInteractable::Execute_Interact(Fire, Character.Get());
		}
		return Count(*Product) > MarkCount;
	}, 90.0);
}

void UExploredPlaytestBot::AddRow(const FString& Label, TFunction<FVector()> Target, double ArriveMeters)
{
	auto Raft = [this]() -> AExploredRaft*
	{
		TActorIterator<AExploredRaftSite> It(GetWorld());
		return It ? It->GetRaft() : nullptr;
	};
	AddStep(TEXT("remar hasta ") + Label, [Raft]() { return Raft() && Raft()->IsOccupied(); }, [this, Raft, Target, ArriveMeters]() // loc: ignorar (paso del bot, solo en el log)
	{
		AExploredRaft* R = Raft();
		if (!R || !R->IsOccupied())
		{
			return false;
		}
		const FRaftMotion& Motion = R->GetMotion();
		const FVector2D Goal = BalizaRunDetail::Flat(Target()) / 100.0;
		const FVector2D ToGoal = Goal - Motion.PositionM;
		const double Distance = ToGoal.Size();
		const bool bSettled = Run.Clock.Est1xSeconds - StepStart1x > 5.0;
		if (Distance <= ArriveMeters || (bSettled && R->IsGrounded()))
		{
			R->SetControls(0.0f, 0.0f);
			Note(FString::Printf(TEXT("balsa a %.1f m del destino (encallada %d)"), Distance, R->IsGrounded() ? 1 : 0));
			return true;
		}
		const double Desired = FMath::RadiansToDegrees(FMath::Atan2(ToGoal.Y, ToGoal.X));
		const double Error = FMath::FindDeltaAngleDegrees(static_cast<double>(Motion.YawDeg), Desired);
		R->SetControls(FMath::Abs(Error) < 60.0 ? 1.0f : 0.3f, static_cast<float>(FMath::Clamp(Error / 15.0, -1.0, 1.0)));
		return false;
	}, 260.0);
}


// ---------------------------------------------------------------------------------------------
// Bucle
// ---------------------------------------------------------------------------------------------

void UExploredPlaytestBot::BeginRun()
{
	bRunMode = true;
	RouteName = TEXT("competente");
	FParse::Value(FCommandLine::Get(), TEXT("BalizaRun="), RouteName);
	FParse::Value(FCommandLine::Get(), TEXT("BalizaClock="), FastScale);
	FastScale = FMath::Clamp(FastScale, 1.0f, 12.0f);
	Run.Route = RouteName;
	LogCapture = MakeShared<FBalizaRunLogCapture>();
	GLog->AddOutputDevice(LogCapture.Get());
	bActive = true;
	Note(FString::Printf(TEXT("Partida completa, ruta %s, reloj ×%.0f en esperas"), *RouteName, FastScale));
}

void UExploredPlaytestBot::Deinitialize()
{
	if (LogCapture.IsValid() && GLog)
	{
		GLog->RemoveOutputDevice(LogCapture.Get());
	}
	LogCapture.Reset();
	Super::Deinitialize();
}

void UExploredPlaytestBot::StartRunStep()
{
	StepStart1x = Run.Clock.Est1xSeconds;
	TravelLeft1x = 0.0;
	const FRunStep& Step = RunSteps[RunIndex];
	SetClockScale(Step.bFast ? FastScale : 1.0f);
	Note(FString::Printf(TEXT("%s · %s"), *FPlaytestRunModel::FormatGameClock(AbsHours()), *Step.Name));
	if (!Step.Start())
	{
		FinishRunStep(false);
	}
}

void UExploredPlaytestBot::FinishRunStep(bool bOk)
{
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(GetWorld());
	FPlaytestRunStep Record;
	Record.Name = RunSteps[RunIndex].Name;
	Record.bOk = bOk;
	Record.Est1xSeconds = Run.Clock.Est1xSeconds;
	Record.GameClock = FPlaytestRunModel::FormatGameClock(AbsHours());
	Record.Stage = Beacon ? FBeaconDirectorModel::StageName(Beacon->GetStage()) : TEXT("?");
	Run.Steps.Add(Record);
	if (!bOk)
	{
		UE_LOG(LogExplored, Warning, TEXT("[BalizaBot] FALLO: %s"), *Record.Name);
		EndRun(Record.Name);
		return;
	}
	// Guardar y cargar en un punto tranquilo (no en la balsa ni en un fundido) tras cada etapa nueva.
	const AExploredRaftSite* Site = TActorIterator<AExploredRaftSite>(GetWorld()) ? *TActorIterator<AExploredRaftSite>(GetWorld()) : nullptr;
	const bool bOnRaft = Site && Site->GetRaft() && Site->GetRaft()->IsOccupied();
	while (PendingSaveChecks.Num() > 0 && !bOnRaft && Idle())
	{
		CheckSaveLoad(PendingSaveChecks[0]);
		PendingSaveChecks.RemoveAt(0);
	}
	++RunIndex;
	if (RunIndex >= RunSteps.Num())
	{
		Run.bReachedEpilogue = true;
		EndRun(FString());
		return;
	}
	StartRunStep();
}

bool UExploredPlaytestBot::CheckSaveLoad(const FString& StageName)
{
	UGameInstance* Game = GetWorld()->GetGameInstance();
	UExploredSaveSubsystem* Save = Game ? Game->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(GetWorld());
	UExploredSurvivorComponent* S = Survivor();
	if (!Save || !Beacon || !S || !Body())
	{
		return false;
	}
	++Run.SaveLoadChecks;
	const FBeaconRunState BeaconBefore = Beacon->GetRunState();
	const FBackpackState PackBefore = S->GetBackpack();
	const FBodyBars BarsBefore = Body()->GetBars();
	const double HoursBefore = AbsHours();
	FString Error;
	const bool bSaved = Save->SaveToSlot(BalizaRunDetail::SaveSlot, Error);
	const bool bLoaded = bSaved && Save->LoadFromSlot(BalizaRunDetail::SaveSlot, Error) == ESaveLoadResult::Ok;
	const FBeaconRunState BeaconAfter = Beacon->GetRunState();
	const bool bSameStory = BeaconAfter.Progress == BeaconBefore.Progress && BeaconAfter.Notebook == BeaconBefore.Notebook;
	const bool bSamePack = Survivor() && Survivor()->GetBackpack() == PackBefore;
	const FBodyBars BarsAfter = Body() ? Body()->GetBars() : FBodyBars();
	const bool bSameBody = FMath::IsNearlyEqual(BarsAfter.Thirst, BarsBefore.Thirst, 0.5f) && FMath::IsNearlyEqual(BarsAfter.Hunger, BarsBefore.Hunger, 0.5f)
		&& FMath::IsNearlyEqual(BarsAfter.Energy, BarsBefore.Energy, 0.5f) && FMath::IsNearlyEqual(BarsAfter.Warmth, BarsBefore.Warmth, 0.5f);
	const bool bSameClock = FMath::Abs(AbsHours() - HoursBefore) < 0.05;
	const bool bOk = bLoaded && bSameStory && bSamePack && bSameBody && bSameClock;
	if (!bOk)
	{
		++Run.SaveLoadFailures;
	}
	Run.SaveLoadNotes.Add(FString::Printf(TEXT("%s: %s%s%s%s%s"), *StageName, bOk ? TEXT("ok") : TEXT("FALLO"),
		bLoaded ? TEXT("") : *(TEXT(" carga: ") + Error), bSameStory ? TEXT("") : TEXT(" historia distinta"),
		bSamePack ? TEXT("") : TEXT(" mochila distinta"), bSameBody && bSameClock ? TEXT("") : TEXT(" cuerpo o reloj distinto")));
	return bOk;
}

void UExploredPlaytestBot::TrackStageAndSaves()
{
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(GetWorld());
	if (!Beacon)
	{
		return;
	}
	const int32 Stage = Beacon->GetStageIndex();
	if (Stage != LastStageIndex)
	{
		LastStageIndex = Stage;
		const FString Name = FBeaconDirectorModel::StageName(Beacon->GetStage());
		Run = FPlaytestRunModel::MarkStage(Run, Stage, Name);
		Note(FString::Printf(TEXT("etapa %s (%.1f min a 1×)"), *Name, Run.Clock.Est1xSeconds / 60.0));
		PendingSaveChecks.Add(Name);
	}
}

void UExploredPlaytestBot::Upkeep()
{
	UExploredSurvivorComponent* S = Survivor();
	const UIslandBodyComponent* B = Body();
	if (!S || !B || !Idle())
	{
		return;
	}
	const FBodyBars& Bars = B->GetBars();
	if (Bars.Thirst < 45.0f)
	{
		// La cantimplora primero (se rellena en el barril); los cocos dan los cuencos del aceite.
		if (S->GetCanteenSips() > 0)
		{
			UseItem(TEXT("cantimplora"));
		}
		else if (S->GetItemCount(TEXT("coco")) > 0)
		{
			UseItem(TEXT("coco"));
		}
	}
	if (Bars.Hunger < 40.0f && S->GetItemCount(TEXT("pez_asado")) > 0)
	{
		UseItem(TEXT("pez_asado"));
	}
}

void UExploredPlaytestBot::EndRun(const FString& FailedStep)
{
	SetClockScale(1.0f);
	Run.bRan = true;
	Run.FailedStep = FailedStep;
	if (const UIslandBodyComponent* B = Body())
	{
		Run.Faints = B->GetFaintCount();
		Run.Sleeps = B->GetSleepCount();
	}
	if (const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(GetWorld()))
	{
		Run.PagesRead = Beacon->GetPagesRead();
	}
	if (Run.bReachedEpilogue)
	{
		Run.CreditsSeconds = RunTuning.CreditsSeconds;
	}
	if (LogCapture.IsValid())
	{
		for (const FString& Line : LogCapture->TakeErrors())
		{
			Run = FPlaytestRunModel::AddLogError(Run, Line);
		}
		Run.LogWarnings = LogCapture->GetWarnings();
		GLog->RemoveOutputDevice(LogCapture.Get());
		LogCapture.Reset();
	}
	if (UExploredPlaytestAuditor* Auditor = GetWorld()->GetSubsystem<UExploredPlaytestAuditor>())
	{
		Auditor->SetRunSummary(Run);
	}
	Note(FString::Printf(TEXT("RESULTADO: %s · %.1f min a 1× · %d desmayos · %d/10 etapas · %d errores de log%s"),
		FPlaytestRunModel::Passed(Run, RunTuning) ? TEXT("OK") : TEXT("FALLO"), FPlaytestRunModel::EstimatedMinutes(Run), Run.Faints,
		Run.StagesReached.Num(), Run.LogErrors, FailedStep.IsEmpty() ? TEXT("") : *(TEXT(" · falló: ") + FailedStep)));
	bDone = true;
	bActive = false;
}

void UExploredPlaytestBot::TickRun(float DeltaTime)
{
	if (RunIndex == INDEX_NONE)
	{
		WarmupTimer += DeltaTime;
		const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(GetWorld());
		if (WarmupTimer < BalizaRunDetail::WarmUpSeconds || !Character.IsValid() || !Beacon || !Prop(TEXT("faro_linterna")) || !Survivor())
		{
			return;
		}
		BuildRunScript();
		RunIndex = 0;
		LastStageIndex = -1;
		TrackStageAndSaves();
		StartRunStep();
		return;
	}
	const UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>();
	const double Scale = Time ? Time->GetTimeScale() : 1.0;
	Run.Clock = FPlaytestRunModel::Accumulate(Run.Clock, DeltaTime, Scale);
	TravelLeft1x -= DeltaTime * Scale;
	if (LogCapture.IsValid())
	{
		for (const FString& Line : LogCapture->TakeErrors())
		{
			Run = FPlaytestRunModel::AddLogError(Run, Line);
		}
	}
	TrackStageAndSaves();
	Upkeep();
	if (Run.Clock.RealSeconds > BalizaRunDetail::MaxRealSeconds)
	{
		EndRun(TEXT("tope de tiempo real"));
		return;
	}
	if (Body() && Body()->IsFainting())
	{
		return;
	}
	const FRunStep& Step = RunSteps[RunIndex];
	if (Step.IsDone())
	{
		FinishRunStep(true);
		return;
	}
	if (Run.Clock.Est1xSeconds - StepStart1x > Step.Timeout1x)
	{
		FinishRunStep(false);
	}
}
