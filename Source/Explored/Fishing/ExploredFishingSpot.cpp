#include "Fishing/ExploredFishingSpot.h"

#include "Components/StaticMeshComponent.h"

#include "Explored.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"
#include "Weather/IslandWeatherQuery.h"

#define LOCTEXT_NAMESPACE "ExploredIsland"

AExploredFishingSpot::AExploredFishingSpot()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AExploredFishingSpot::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	// island_props.json no le da colisión (no estorba al andar), pero la interacción traza visibilidad.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	State.Seed = GetTypeHash(GetPropId()) | 1u;
}

void AExploredFishingSpot::SetFishing(bool bActive)
{
	SetActorTickEnabled(bActive);
	if (!bActive)
	{
		Angler.Reset();
	}
}

void AExploredFishingSpot::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	switch (State.Phase)
	{
	case ECastFishingPhase::Idle: OutVerbs.Add(LOCTEXT("CastLine", "Lanzar el sedal")); break;
	case ECastFishingPhase::Waiting: OutVerbs.Add(LOCTEXT("ReelIn", "Recoger el sedal")); break;
	default: OutVerbs.Add(LOCTEXT("PullFish", "Sacar el pez")); break;
	}
}

bool AExploredFishingSpot::CanInteract_Implementation(AActor* InInstigator) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	return Survivor && !Survivor->IsBusy();
}

void AExploredFishingSpot::Interact_Implementation(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	if (!Survivor)
	{
		return;
	}
	const FCastFishingConfig& Config = ExploredIslandData::Get().Fishing;
	ECastFishingEvent Event = ECastFishingEvent::None;
	if (State.Phase == ECastFishingPhase::Idle)
	{
		if (Survivor->GetItemCount(Config.Rod) <= 0)
		{
			Survivor->ShowMessage(LOCTEXT("NeedRod", "Necesitas una caña de pescar"));
			return;
		}
		State = FCastFishingModel::Cast(State, Config, ExploredIslandWeather::GetHour(this), ExploredIslandWeather::IsCycloneWall(this), Event);
		if (Event == ECastFishingEvent::Cast)
		{
			Angler = InInstigator;
			SetFishing(true);
		}
	}
	else
	{
		State = FCastFishingModel::Pull(State, Event);
		SetFishing(false);
	}
	Report(Event, Survivor);
}

void AExploredFishingSpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ECastFishingEvent Event = ECastFishingEvent::None;
	State = FCastFishingModel::Tick(State, ExploredIslandData::Get().Fishing, DeltaSeconds, ExploredIslandWeather::GetHour(this),
		ExploredIslandWeather::IsCycloneWall(this), Event);
	if (Event == ECastFishingEvent::None)
	{
		return;
	}
	Report(Event, UExploredSurvivorComponent::FindOn(Angler.Get()));
	if (State.Phase == ECastFishingPhase::Idle)
	{
		SetFishing(false);
	}
}

void AExploredFishingSpot::Report(ECastFishingEvent Event, UExploredSurvivorComponent* Survivor)
{
	UE_LOG(LogExplored, Display, TEXT("[Isla] Pesca en %s: evento %d"), *GetPropId().ToString(), static_cast<int32>(Event));
	if (!Survivor)
	{
		return;
	}
	switch (Event)
	{
	case ECastFishingEvent::Bite:
		Survivor->ShowMessage(LOCTEXT("FishBite", "Pica: pulsa ahora"));
		break;
	case ECastFishingEvent::Escaped:
		Survivor->ShowMessage(LOCTEXT("FishEscaped", "Se ha escapado"));
		break;
	case ECastFishingEvent::Blocked:
		Survivor->ShowMessage(LOCTEXT("NoFishing", "Ahora no pican"));
		break;
	case ECastFishingEvent::Caught:
		if (Survivor->TryAddItem(ExploredIslandData::Get().Fishing.Catch, 1))
		{
			++Catches;
		}
		break;
	default:
		break;
	}
}

#undef LOCTEXT_NAMESPACE
