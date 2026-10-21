#include "Items/ExploredPickup.h"

#include "Explored.h"
#include "Items/ExploredIslandStateSubsystem.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"
#include "WorldGen/IslandPropsModel.h"

#define LOCTEXT_NAMESPACE "ExploredIsland"

FName AExploredPickup::ItemForPropType(FName PropTypeName)
{
	// Los tipos de recurso de island_props.json se llaman como su objeto (lena, fibra, tronco...).
	return PropTypeName;
}

void AExploredPickup::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	ItemId = ItemForPropType(Prop.Type);
	Count = FMath::Max(1, Prop.Amount);
	// BeginPlay llega antes que Configure (el populador crea y luego configura): aquí ya hay id.
	RefreshFromIslandState();
}

void AExploredPickup::ConfigureLoose(FName InItemId, int32 InCount)
{
	bLoose = true;
	ConfigureStandalone(NAME_None, InItemId, FString());
	ItemId = InItemId;
	Count = FMath::Max(1, InCount);
}

void AExploredPickup::BeginPlay()
{
	Super::BeginPlay();
	RefreshFromIslandState();
}

void AExploredPickup::RefreshFromIslandState()
{
	// Los recursos colocados no se destruyen al cogerlos: se ocultan, para que cargar una partida
	// anterior a la recogida los vuelva a poner en su sitio.
	const UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this);
	if (!bLoose && Island)
	{
		SetTaken(Island->IsCollected(GetPropId()));
	}
}

void AExploredPickup::SetTaken(bool bInTaken)
{
	bTaken = bInTaken;
	SetActorHiddenInGame(bTaken);
	SetActorEnableCollision(!bTaken);
}

void AExploredPickup::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	OutVerbs.Add(FText::Format(LOCTEXT("PickUpItem", "Coger {0}"), ExploredIslandData::ItemName(ItemId).ToLower()));
}

bool AExploredPickup::CanInteract_Implementation(AActor* InInstigator) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	return Survivor && !Survivor->IsBusy() && !ItemId.IsNone() && IsAvailable();
}

void AExploredPickup::Interact_Implementation(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	if (!Survivor)
	{
		return;
	}
	// Se comprueba antes de esperar el segundo: con la mochila llena no se empieza a recoger.
	const EBackpackResult CanAdd = FBackpackModel::CanAdd(Survivor->GetBackpack(), ExploredIslandData::Get().Backpack, ItemId, Count);
	if (CanAdd != EBackpackResult::Ok)
	{
		Survivor->ReportResult(CanAdd);
		return;
	}
	const TWeakObjectPtr<AExploredPickup> WeakThis(this);
	const TWeakObjectPtr<AActor> WeakInstigator(InInstigator);
	Survivor->StartTimedAction(PickupSeconds, [WeakThis, WeakInstigator]()
	{
		if (AExploredPickup* Self = WeakThis.Get())
		{
			Self->Collect(WeakInstigator.Get());
		}
	});
}

void AExploredPickup::Collect(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	if (!Survivor || !IsAvailable() || !Survivor->TryAddItem(ItemId, Count))
	{
		return;
	}
	if (UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this))
	{
		if (bLoose)
		{
			Island->ForgetDropped(this);
		}
		else
		{
			Island->MarkCollected(GetPropId());
		}
	}
	UE_LOG(LogExplored, Display, TEXT("[Isla] Recogido %s (%d %s)"), *GetPropId().ToString(), Count, *ItemId.ToString());
	if (bLoose)
	{
		Destroy();
	}
	else
	{
		SetTaken(true);
	}
}

#undef LOCTEXT_NAMESPACE
