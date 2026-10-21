#include "Weather/ExploredRainBarrel.h"

#include "Engine/World.h"
#include "TimerManager.h"

#include "Explored.h"
#include "Items/ExploredIslandStateSubsystem.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Weather/IslandWeatherQuery.h"

#define LOCTEXT_NAMESPACE "ExploredIsland"

void AExploredRainBarrel::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RainTimer, this, &AExploredRainBarrel::SampleRain, FMath::Max(RainSampleSeconds, 0.1f), true);
	}
}

void AExploredRainBarrel::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RainTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AExploredRainBarrel::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	RefreshFromIslandState();
}

void AExploredRainBarrel::RefreshFromIslandState()
{
	if (const UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this))
	{
		State = Island->GetBarrel(GetPropId());
	}
}

void AExploredRainBarrel::Store()
{
	if (UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this))
	{
		Island->SetBarrel(GetPropId(), State);
	}
}

void AExploredRainBarrel::SampleRain()
{
	if (ExploredIslandWeather::GetRain(this) < FRainBarrelModel::RainThreshold)
	{
		return;
	}
	const FRainBarrelState Next = FRainBarrelModel::Collect(State, FMath::Max(RainSampleSeconds, 0.1f));
	if (!(Next == State))
	{
		State = Next;
		Store();
	}
}

void AExploredRainBarrel::AddCharges(int32 Charges)
{
	State = FRainBarrelModel::AddCharges(State, Charges);
	Store();
	UE_LOG(LogExplored, Display, TEXT("[Isla] Barril %s: %d cargas"), *GetPropId().ToString(), State.Charges);
}

void AExploredRainBarrel::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	OutVerbs.Add(FText::Format(LOCTEXT("FillCanteen", "Llenar la cantimplora ({0}/{1})"),
		FText::AsNumber(State.Charges), FText::AsNumber(FRainBarrelModel::MaxCharges)));
}

bool AExploredRainBarrel::CanInteract_Implementation(AActor* InInstigator) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	return Survivor && !Survivor->IsBusy();
}

void AExploredRainBarrel::Interact_Implementation(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	if (!Survivor)
	{
		return;
	}
	FRainBarrelState NextBarrel;
	FBackpackState NextPack;
	switch (FRainBarrelModel::RefillCanteen(State, Survivor->GetBackpack(), NextBarrel, NextPack))
	{
	case ERainBarrelResult::Ok:
		State = NextBarrel;
		Store();
		Survivor->ApplyBackpack(NextPack);
		Survivor->ShowMessage(LOCTEXT("CanteenFilled", "Cantimplora llena"));
		break;
	case ERainBarrelResult::Empty:
		Survivor->ShowMessage(LOCTEXT("BarrelEmpty", "El barril está vacío"));
		break;
	case ERainBarrelResult::NoCanteen:
		Survivor->ShowMessage(LOCTEXT("NoCanteen", "No llevas la cantimplora"));
		break;
	case ERainBarrelResult::CanteenFull:
		Survivor->ShowMessage(LOCTEXT("CanteenAlreadyFull", "La cantimplora ya está llena"));
		break;
	}
}

#undef LOCTEXT_NAMESPACE
