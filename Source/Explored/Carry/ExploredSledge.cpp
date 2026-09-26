#include "Carry/ExploredSledge.h"

#include "GameFramework/Actor.h"

#include "Carry/CarryComponent.h"

AExploredSledge::AExploredSledge()
{
	// Unas angarillas nuevas colocadas en el nivel ya son el objeto «angarillas».
	SledgeInstance.DefinitionId = TEXT("angarillas");
	Container.Spec = FInventoryContainerSpec::Sledge();
	SlotsPerRow = 2;
	SlotSpacingCm = FVector(40.0f, 30.0f, 25.0f);
	// Sobre el cubo marcador (100 cm) hasta que exista la malla de las angarillas.
	FirstSlotOffsetCm = FVector(-40.0f, -15.0f, 60.0f);
}

void AExploredSledge::SetSledgeItem(int64 InInstanceId, const FItemInstance& InInstance)
{
	SledgeInstanceId = InInstanceId;
	if (InInstance.IsValid())
	{
		SledgeInstance = InInstance;
	}
}

void AExploredSledge::SetAttached(bool bInAttached)
{
	bAttached = bInAttached;
	// Enganchadas no chocan con el jugador ni se pueden enfocar (van detrás).
	SetActorEnableCollision(!bAttached);
}

void AExploredSledge::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	if (bAttached)
	{
		return;
	}
	OutVerbs.Add(NSLOCTEXT("Explored", "Verb_AttachSledge", "Enganchar las angarillas"));
	OutVerbs.Add(NSLOCTEXT("Explored", "Verb_LoadSledge", "Cargar en las angarillas"));
}

void AExploredSledge::Interact_Implementation(AActor* InInstigator)
{
	if (bAttached)
	{
		return;
	}
	UCarryComponent* Carry = InInstigator ? InInstigator->FindComponentByClass<UCarryComponent>() : nullptr;
	if (!Carry)
	{
		return;
	}

	FText FailReason;
	EHand Hand = EHand::Right;
	if (FindFilledHand(*Carry, Hand))
	{
		// Con algo en las manos se carga (troncos, piedra); sin nada, se engancha.
		Carry->StoreInContainer(Hand, this, FailReason);
		return;
	}
	Carry->AttachSledge(this, FailReason);
}
