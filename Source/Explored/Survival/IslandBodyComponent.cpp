#include "Survival/IslandBodyComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Boats/ExploredRaft.h"
#include "Cooking/ExploredFire.h"
#include "Explored.h"
#include "Items/IslandDataLoader.h"
#include "Narrative/BeaconSubsystem.h"
#include "Player/ExploredCharacter.h"
#include "Player/SwimComponent.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/BodySignalsComponent.h"
#include "UI/ExploredHUD.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "Weather/RainBarrelModel.h"
#include "WorldGen/ExploredShelterVolume.h"

#define LOCTEXT_NAMESPACE "ExploredBody"

namespace IslandBodyComponentDetail
{
	constexpr float StepSeconds = 0.25f;
	/** Refugio por defecto del desmayo (GDD §5.1: Cala Varada). */
	const TCHAR* const DefaultShelterHint = TEXT("cala");

	AExploredHUD* FindHUD(const APawn* Pawn)
	{
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		return PC ? Cast<AExploredHUD>(PC->GetHUD()) : nullptr;
	}
}

UIslandBodyComponent::UIslandBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;
}

UIslandBodyComponent* UIslandBodyComponent::FindOn(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UIslandBodyComponent>() : nullptr;
}

void UIslandBodyComponent::BeginPlay()
{
	Super::BeginPlay();
	Tuning = ExploredIslandData::Get().Body;
	if (!bRestored)
	{
		Bars = Tuning.Initial;
	}
	ApplyBodyOutputs();
}

void UIslandBodyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FadeTimer);
		World->GetTimerManager().ClearTimer(FadeBackTimer);
	}
	Super::EndPlay(EndPlayReason);
}

float UIslandBodyComponent::GetBar(int32 BarIndex) const
{
	if (BarIndex < 0 || BarIndex >= static_cast<int32>(EBodyBar::Count))
	{
		return 0.0f;
	}
	return FIslandBodyModel::Get(Bars, static_cast<EBodyBar>(BarIndex));
}

void UIslandBodyComponent::ApplyDelta(float Thirst, float Hunger, float Energy)
{
	Bars = FIslandBodyModel::Apply(Bars, Thirst, Hunger, Energy);
	ApplyBodyOutputs();
}

void UIslandBodyComponent::RestoreBars(const FBodyBars& InBars, FName InLastShelter)
{
	FBodyBars Clamped;
	for (uint8 Index = 0; Index < static_cast<uint8>(EBodyBar::Count); ++Index)
	{
		const EBodyBar Bar = static_cast<EBodyBar>(Index);
		Clamped = FIslandBodyModel::With(Clamped, Bar, FIslandBodyModel::Get(InBars, Bar));
	}
	Bars = Clamped;
	LastShelterId = InLastShelter;
	Warnings = FBodyWarnings();
	FaintTracker = FFaintTracker();
	bRestored = true;
	ApplyBodyOutputs();
}

void UIslandBodyComponent::ResetForNewRun()
{
	Tuning = ExploredIslandData::Get().Body;
	RestoreBars(Tuning.Initial, NAME_None);
}

void UIslandBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bInFade)
	{
		return;
	}
	StepAccumulator += DeltaTime;
	if (StepAccumulator < IslandBodyComponentDetail::StepSeconds)
	{
		return;
	}
	const float RealSeconds = StepAccumulator;
	StepAccumulator = 0.0f;
	Step(RealSeconds);
}

void UIslandBodyComponent::Step(float RealSeconds)
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	const float Scale = Time ? Time->GetTimeScale() : 1.0f;

	const FBodyEnvironment Env = MeasureEnvironment();
	const FBodyStep Result = FIslandBodyModel::Tick(Bars, Warnings, Env, RealSeconds * Scale, Tuning);
	Bars = Result.Bars;
	Warnings = Result.Warnings;
	for (const EBodyBar Bar : Result.NewWarnings)
	{
		ShowWarning(Bar);
	}
	ApplyBodyOutputs();

	const FFaintTick Faint = FBodyEventsModel::TickFaint(FaintTracker, Bars, RealSeconds, Tuning.Events);
	FaintTracker = Faint.Tracker;
	if (Faint.bFainted)
	{
		BeginFaint();
	}
}

FBodyEnvironment UIslandBodyComponent::MeasureEnvironment()
{
	FBodyEnvironment Env;
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return Env;
	}
	const FVector Here = Owner->GetActorLocation();
	if (const UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Env.HourOfDay = Time->GetHours();
	}
	bSheltered = false;
	for (TActorIterator<AExploredShelterVolume> It(World); It; ++It)
	{
		if (It->IsActorSheltered(Owner))
		{
			bSheltered = true;
			if (It->IsRespawnPoint())
			{
				LastShelterId = It->ShelterId;
			}
			break;
		}
	}
	Env.bSheltered = bSheltered;
	for (TActorIterator<AExploredFire> It(World); It && !Env.bNearFire; ++It)
	{
		Env.bNearFire = It->IsBurning() && FVector::DistSquared(It->GetActorLocation(), Here) <= FMath::Square(FireRadiusCm);
	}
	if (const UExploredWeatherSubsystem* Weather = World->GetSubsystem<UExploredWeatherSubsystem>())
	{
		Env.bRaining = Weather->GetCurrent().Rain >= FRainBarrelModel::RainThreshold;
	}
	return Env;
}

void UIslandBodyComponent::ApplyBodyOutputs()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	if (UBodySignalsComponent* Signals = Owner->FindComponentByClass<UBodySignalsComponent>())
	{
		Signals->SetIslandDesaturation(FIslandBodyModel::Desaturation01(Bars, Tuning));
	}
	const float Factor = GetSpeedFactor();
	if (!FMath::IsNearlyEqual(Factor, LastSpeedFactor))
	{
		LastSpeedFactor = Factor;
		if (AExploredCharacter* Character = Cast<AExploredCharacter>(Owner))
		{
			Character->RefreshWalkSpeed();
		}
	}
}

void UIslandBodyComponent::ShowWarning(EBodyBar Bar) const
{
	switch (Bar)
	{
	case EBodyBar::Thirst: ShowLine(LOCTEXT("WarnThirst", "Tengo sed.")); break;
	case EBodyBar::Hunger: ShowLine(LOCTEXT("WarnHunger", "Tengo hambre.")); break;
	case EBodyBar::Energy: ShowLine(LOCTEXT("WarnEnergy", "Estoy agotada.")); break;
	case EBodyBar::Warmth: ShowLine(LOCTEXT("WarnWarmth", "Tengo frío.")); break;
	default: break;
	}
	UE_LOG(LogExplored, Display, TEXT("[Cuerpo] Aviso: %s bajo %.0f"), LexToString(Bar), Tuning.WarningAt);
}

void UIslandBodyComponent::ShowLine(const FText& Text) const
{
	if (AExploredHUD* HUD = IslandBodyComponentDetail::FindHUD(Cast<APawn>(GetOwner())))
	{
		HUD->ShowSubtitle(Text, 4.0f);
	}
}

double UIslandBodyComponent::GetAbsHours() const
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	return Time ? FStormClockModel::ToAbsoluteHours(Time->GetDay(), Time->GetHours()) : 12.0;
}

void UIslandBodyComponent::SetAbsHours(double AbsHours) const
{
	const UWorld* World = GetWorld();
	UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	if (!Time || !FMath::IsFinite(AbsHours) || AbsHours < 0.0)
	{
		return;
	}
	const int32 Day = FMath::FloorToInt32(AbsHours / 24.0) + 1;
	Time->SetTime(Day, static_cast<float>(AbsHours - (Day - 1) * 24.0));
}

bool UIslandBodyComponent::CanSleepNow() const
{
	return !bInFade && FBodyEventsModel::CanSleep(GetAbsHours(), bSheltered, Tuning.Events, IsCycloneNow()) == ESleepCheck::Allowed;
}

bool UIslandBodyComponent::IsCycloneNow() const
{
	const UWorld* World = GetWorld();
	const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	return Weather && Weather->GetStorm().IsCyclone(GetAbsHours());
}

bool UIslandBodyComponent::TrySleep()
{
	if (bInFade)
	{
		return false;
	}
	const FSleepOutcome Outcome = FBodyEventsModel::Sleep(Bars, GetAbsHours(), bSheltered, Tuning.Events, IsCycloneNow());
	if (!Outcome.bSlept)
	{
		ShowLine(Outcome.Check == ESleepCheck::NotSheltered ? LOCTEXT("SleepNoRoof", "Solo puedo dormir bajo techo.")
			: Outcome.Check == ESleepCheck::Storm ? LOCTEXT("SleepStorm", "Con este temporal no hay quien duerma.")
			: LOCTEXT("SleepWrongHour", "Aún no es hora de dormir."));
		return false;
	}
	if (Outcome.bThirstWarning)
	{
		ShowLine(LOCTEXT("SleepThirsty", "Tienes demasiada sed para dormir"));
	}
	const FBodyBars NewBars = Outcome.Bars;
	const double WakeHour = Outcome.WakeHour;
	const TWeakObjectPtr<UIslandBodyComponent> WeakThis(this);
	PlayBlackout([WeakThis, NewBars, WakeHour]()
	{
		UIslandBodyComponent* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		Self->SetAbsHours(WakeHour);
		Self->Bars = NewBars;
		Self->FaintTracker = FFaintTracker();
		++Self->SleepCount;
		if (UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(Self))
		{
			Beacon->TriggerThought(EIriaThought::FirstNight);
		}
		UE_LOG(LogExplored, Display, TEXT("[Cuerpo] Duerme hasta %.2f h: sed %.0f, hambre %.0f, energía %.0f"), WakeHour,
			NewBars.Thirst, NewBars.Hunger, NewBars.Energy);
	}, FText::GetEmpty());
	return true;
}

void UIslandBodyComponent::ForceFaint()
{
	if (!bInFade)
	{
		BeginFaint();
	}
}

bool UIslandBodyComponent::IsAtSea() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	if (Cast<AExploredRaft>(Owner->GetAttachParentActor()))
	{
		return true;
	}
	const USwimComponent* Swim = Owner->FindComponentByClass<USwimComponent>();
	return Swim && Swim->IsInWater();
}

FVector UIslandBodyComponent::FaintRespawnLocation(bool bAtSea) const
{
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	FVector Fallback = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	if (!World)
	{
		return Fallback;
	}
	if (bAtSea)
	{
		for (TActorIterator<AExploredRaftSite> It(World); It; ++It)
		{
			return It->GetShoreLocation();
		}
	}
	const AExploredShelterVolume* Chosen = nullptr;
	for (TActorIterator<AExploredShelterVolume> It(World); It; ++It)
	{
		if (!It->IsRespawnPoint())
		{
			continue;
		}
		const bool bLast = !LastShelterId.IsNone() && It->ShelterId == LastShelterId;
		const bool bDefault = LastShelterId.IsNone() && It->ShelterId.ToString().Contains(IslandBodyComponentDetail::DefaultShelterHint);
		if (bLast || bDefault)
		{
			return It->GetRespawnLocation();
		}
		Chosen = Chosen ? Chosen : *It;
	}
	return Chosen ? Chosen->GetRespawnLocation() : Fallback;
}

void UIslandBodyComponent::BeginFaint()
{
	const bool bAtSea = IsAtSea();
	const double WakeHour = FBodyEventsModel::Faint(GetAbsHours(), Tuning.Events).WakeHour;
	const FBodyBars WakeBars = Tuning.Events.WakeAfterFaint;
	UE_LOG(LogExplored, Display, TEXT("[Cuerpo] Desmayo (%s): despierta a las %.2f h"), bAtSea ? TEXT("en el mar") : TEXT("en tierra"), WakeHour);
	const TWeakObjectPtr<UIslandBodyComponent> WeakThis(this);
	const FText Text = bAtSea ? LOCTEXT("FaintShore", "Despiertas en Playa Norte…") : LOCTEXT("FaintShelter", "Despiertas en el refugio…");
	PlayBlackout([WeakThis, bAtSea, WakeHour, WakeBars]()
	{
		UIslandBodyComponent* Self = WeakThis.Get();
		AActor* Owner = Self ? Self->GetOwner() : nullptr;
		if (!Owner)
		{
			return;
		}
		if (AExploredRaft* Raft = Cast<AExploredRaft>(Owner->GetAttachParentActor()))
		{
			Raft->Leave();
		}
		const FVector Where = Self->FaintRespawnLocation(bAtSea);
		Owner->TeleportTo(Where, Owner->GetActorRotation(), false, true);
		Self->SetAbsHours(WakeHour);
		Self->Bars = WakeBars;
		Self->Warnings = FBodyWarnings();
		Self->FaintTracker = FFaintTracker();
		++Self->FaintCount;
		UE_LOG(LogExplored, Display, TEXT("[Cuerpo] Despierta en %s"), *Where.ToCompactString());
	}, Text);
}

void UIslandBodyComponent::PlayBlackout(TFunction<void()> OnBlack, const FText& Text)
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!World || bInFade)
	{
		return;
	}
	bInFade = true;
	PendingOnBlack = MoveTemp(OnBlack);
	PendingText = Text;
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC)
	{
		Pawn->DisableInput(PC);
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeSeconds, FLinearColor::Black, true, true);
		}
	}
	const TWeakObjectPtr<UIslandBodyComponent> WeakThis(this);
	World->GetTimerManager().SetTimer(FadeTimer, FTimerDelegate::CreateLambda([WeakThis]()
	{
		UIslandBodyComponent* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (Self->PendingOnBlack)
		{
			Self->PendingOnBlack();
			Self->PendingOnBlack = nullptr;
		}
		Self->ApplyBodyOutputs();
		if (UWorld* InnerWorld = Self->GetWorld())
		{
			InnerWorld->GetTimerManager().SetTimer(Self->FadeBackTimer, FTimerDelegate::CreateUObject(Self, &UIslandBodyComponent::FinishFade),
				FMath::Max(0.1f, Self->BlackSeconds), false);
		}
	}), FMath::Max(0.1f, FadeSeconds), false);
}

void UIslandBodyComponent::FinishFade()
{
	bInFade = false;
	StepAccumulator = 0.0f;
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC)
	{
		Pawn->EnableInput(PC);
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FadeSeconds, FLinearColor::Black, true, false);
		}
	}
	if (!PendingText.IsEmpty())
	{
		if (AExploredHUD* HUD = IslandBodyComponentDetail::FindHUD(Pawn))
		{
			HUD->ShowCard(PendingText, 3.0f);
		}
		PendingText = FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE
