#include "Narrative/ExploredSupplyCrate.h"

#include "Engine/World.h"
#include "EngineUtils.h"

#include "Explored.h"
#include "Items/BackpackModel.h"
#include "Items/ExploredPickup.h"
#include "Narrative/BeaconSubsystem.h"
#include "WorldGen/ExploredCoconutPalm.h"
#include "Items/ExploredIslandStateSubsystem.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"

#define LOCTEXT_NAMESPACE "ExploredIsland"

void AExploredSupplyCrate::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	RefreshFromIslandState();
}

TMap<FName, int32> AExploredSupplyCrate::CountIslandResources(const UWorld* World)
{
	TMap<FName, int32> Remaining;
	if (!World)
	{
		return Remaining;
	}
	for (TActorIterator<AExploredPickup> It(World); It; ++It)
	{
		if (!It->IsActorBeingDestroyed() && !It->GetItemId().IsNone() && It->GetCount() > 0)
		{
			Remaining.FindOrAdd(It->GetItemId()) += It->GetCount();
		}
	}
	for (TActorIterator<AExploredCoconutPalm> It(World); It; ++It)
	{
		if (It->GetCoconuts() > 0)
		{
			Remaining.FindOrAdd(FName(TEXT("coco"))) += It->GetCoconuts();
		}
	}
	return Remaining;
}

void AExploredSupplyCrate::RefreshFromIslandState()
{
	if (const UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this))
	{
		State = Island->GetCrate(GetPropId());
	}
}

void AExploredSupplyCrate::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	OutVerbs.Add(State.bDelivered ? LOCTEXT("CrateEmpty", "Caja vacía") : LOCTEXT("OpenCrate", "Abrir la caja"));
}

bool AExploredSupplyCrate::CanInteract_Implementation(AActor* InInstigator) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	return Survivor && !Survivor->IsBusy() && !State.bDelivered;
}

void AExploredSupplyCrate::Interact_Implementation(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	if (!Survivor || State.bDelivered)
	{
		return;
	}
	// Progreso real de la baliza (antes se pasaba el inicial y la caja creía que faltaba todo).
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	const FBeaconProgressState Progress = Beacon ? Beacon->GetRunState().Progress : FBeaconProgressState();
	const TMap<FName, int32> Pack = FBackpackModel::ToCounts(Survivor->GetBackpack());
	const FSupplyCrateOpenResult Opened = FSupplyCrateModel::Open(State, Pack, Progress, CountIslandResources(GetWorld()));
	if (!Opened.bDelivered)
	{
		Survivor->ShowMessage(LOCTEXT("CrateNothing", "No te hace falta nada de la caja"));
		return;
	}
	FBackpackState Next;
	const EBackpackResult Result = FBackpackModel::Exchange(Survivor->GetBackpack(), ExploredIslandData::Get().Backpack, {}, Opened.Items, Next);
	if (Result != EBackpackResult::Ok)
	{
		Survivor->ReportResult(Result);
		return;
	}
	Survivor->ApplyBackpack(Next);
	State = Opened.State;
	if (UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this))
	{
		Island->SetCrate(GetPropId(), State);
	}
	for (const TPair<FName, int32>& Pair : Opened.Items)
	{
		UE_LOG(LogExplored, Display, TEXT("[Isla] Caja de suministros: +%d %s"), Pair.Value, *Pair.Key.ToString());
	}
}

#undef LOCTEXT_NAMESPACE
