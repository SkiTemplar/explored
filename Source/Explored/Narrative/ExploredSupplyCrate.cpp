#include "Narrative/ExploredSupplyCrate.h"

#include "Explored.h"
#include "Items/BackpackModel.h"
#include "Items/ExploredIslandStateSubsystem.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"

#define LOCTEXT_NAMESPACE "ExploredIsland"

void AExploredSupplyCrate::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	RefreshFromIslandState();
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
	const FSupplyCrateOpenResult Opened = FSupplyCrateModel::Open(State, FBackpackModel::ToCounts(Survivor->GetBackpack()), FBeaconProgressState());
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
