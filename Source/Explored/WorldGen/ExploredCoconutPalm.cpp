#include "WorldGen/ExploredCoconutPalm.h"

#include "Explored.h"
#include "Items/BackpackModel.h"
#include "Items/ExploredIslandStateSubsystem.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"
#include "WorldGen/IslandPropsModel.h"

#define LOCTEXT_NAMESPACE "ExploredIsland"

namespace CoconutPalmDetail
{
	const FName Coconut(TEXT("coco"));
}

void AExploredCoconutPalm::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	InitialCoconuts = Prop.Cocos > 0 ? Prop.Cocos : DefaultCoconuts;
	Coconuts = InitialCoconuts;
	// BeginPlay llega antes que Configure (el populador crea y luego configura): aquí ya hay id.
	RefreshFromIslandState();
}

void AExploredCoconutPalm::BeginPlay()
{
	Super::BeginPlay();
	RefreshFromIslandState();
}

void AExploredCoconutPalm::RefreshFromIslandState()
{
	if (const UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this))
	{
		// Una partida editada o corrupta no da más cocos de los que tiene la palma.
		Coconuts = FMath::Clamp(Island->GetPalmCoconuts(GetPropId(), InitialCoconuts), 0, InitialCoconuts);
	}
}

void AExploredCoconutPalm::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	if (Coconuts > 0)
	{
		OutVerbs.Add(FText::Format(LOCTEXT("PickCoconut", "Bajar un coco ({0})"), FText::AsNumber(Coconuts)));
	}
}

bool AExploredCoconutPalm::CanInteract_Implementation(AActor* InInstigator) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	return Survivor && !Survivor->IsBusy() && Coconuts > 0;
}

void AExploredCoconutPalm::Interact_Implementation(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	if (!Survivor || Coconuts <= 0)
	{
		return;
	}
	const EBackpackResult CanAdd = FBackpackModel::CanAdd(Survivor->GetBackpack(), ExploredIslandData::Get().Backpack, CoconutPalmDetail::Coconut, 1);
	if (CanAdd != EBackpackResult::Ok)
	{
		Survivor->ReportResult(CanAdd);
		return;
	}
	const TWeakObjectPtr<AExploredCoconutPalm> WeakThis(this);
	const TWeakObjectPtr<AActor> WeakInstigator(InInstigator);
	Survivor->StartTimedAction(PickSeconds, [WeakThis, WeakInstigator]()
	{
		if (AExploredCoconutPalm* Self = WeakThis.Get())
		{
			Self->PickOne(WeakInstigator.Get());
		}
	});
}

void AExploredCoconutPalm::PickOne(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	if (!Survivor || Coconuts <= 0 || !Survivor->TryAddItem(CoconutPalmDetail::Coconut, 1))
	{
		return;
	}
	--Coconuts;
	if (UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this))
	{
		Island->SetPalmCoconuts(GetPropId(), Coconuts);
	}
	UE_LOG(LogExplored, Display, TEXT("[Isla] Coco de %s (quedan %d)"), *GetPropId().ToString(), Coconuts);
}

#undef LOCTEXT_NAMESPACE
