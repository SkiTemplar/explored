#include "Carry/ContainerReplicationModel.h"

namespace ContainerReplicationDetail
{
	/** Orden estable por texto del id (FName compara sin mayúsculas) y luego por cliente. */
	bool SubscriptionLess(FName ContainerA, int32 ClientA, FName ContainerB, int32 ClientB)
	{
		const FString A = ContainerA.ToString().ToLower();
		const FString B = ContainerB.ToString().ToLower();
		if (A != B)
		{
			return A < B;
		}
		return ClientA < ClientB;
	}
}

bool FContainerReplicationModel::IsWithinReach(double DistanceCm)
{
	return FMath::IsFinite(DistanceCm) && DistanceCm >= 0.0 && DistanceCm <= MaxOpenDistanceCm;
}

FContainerReplicationModel::FSubscription* FContainerReplicationModel::FindSubscription(int32 ClientId, FName ContainerId)
{
	return Subscriptions.FindByPredicate([ClientId, ContainerId](const FSubscription& S)
	{
		return S.ClientId == ClientId && S.ContainerId == ContainerId;
	});
}

const FContainerReplicationModel::FSubscription* FContainerReplicationModel::FindSubscription(int32 ClientId, FName ContainerId) const
{
	return Subscriptions.FindByPredicate([ClientId, ContainerId](const FSubscription& S)
	{
		return S.ClientId == ClientId && S.ContainerId == ContainerId;
	});
}

void FContainerReplicationModel::AddSlot(TArray<int32>& Slots, int32 SlotIndex)
{
	if (SlotIndex >= 0 && !Slots.Contains(SlotIndex))
	{
		Slots.Add(SlotIndex);
	}
}

bool FContainerReplicationModel::Subscribe(int32 ClientId, FName ContainerId, double DistanceCm)
{
	if (ClientId == INDEX_NONE || ContainerId.IsNone() || !IsWithinReach(DistanceCm) || FindSubscription(ClientId, ContainerId))
	{
		return false;
	}
	FSubscription S;
	S.ClientId = ClientId;
	S.ContainerId = ContainerId;
	Subscriptions.Add(S);
	return true;
}

bool FContainerReplicationModel::Unsubscribe(int32 ClientId, FName ContainerId)
{
	const int32 Before = Subscriptions.Num();
	Subscriptions.RemoveAll([ClientId, ContainerId](const FSubscription& S)
	{
		return S.ClientId == ClientId && S.ContainerId == ContainerId;
	});
	return Subscriptions.Num() != Before;
}

void FContainerReplicationModel::DropClient(int32 ClientId)
{
	Subscriptions.RemoveAll([ClientId](const FSubscription& S) { return S.ClientId == ClientId; });
}

void FContainerReplicationModel::RemoveContainer(FName ContainerId)
{
	Subscriptions.RemoveAll([ContainerId](const FSubscription& S) { return S.ContainerId == ContainerId; });
}

TArray<int32> FContainerReplicationModel::CloseOutOfReach(FName ContainerId, const TMap<int32, double>& DistanceByClient)
{
	TArray<int32> Closed;
	for (const int32 ClientId : Subscribers(ContainerId))
	{
		const double* Distance = DistanceByClient.Find(ClientId);
		// Sin distancia conocida (pawn perdido) también se cierra: no se manda contenido a ciegas.
		if (!Distance || !IsWithinReach(*Distance))
		{
			Unsubscribe(ClientId, ContainerId);
			Closed.Add(ClientId);
		}
	}
	return Closed;
}

bool FContainerReplicationModel::IsSubscribed(int32 ClientId, FName ContainerId) const
{
	return FindSubscription(ClientId, ContainerId) != nullptr;
}

TArray<int32> FContainerReplicationModel::Subscribers(FName ContainerId) const
{
	TArray<int32> Out;
	for (const FSubscription& S : Subscriptions)
	{
		if (S.ContainerId == ContainerId)
		{
			Out.Add(S.ClientId);
		}
	}
	Out.Sort();
	return Out;
}

void FContainerReplicationModel::MarkSlotChanged(FName ContainerId, int32 SlotIndex)
{
	for (FSubscription& S : Subscriptions)
	{
		if (S.ContainerId == ContainerId && !S.bNeedsSnapshot)
		{
			AddSlot(S.DirtySlots, SlotIndex);
		}
	}
}

void FContainerReplicationModel::MarkSlotStale(int32 ClientId, FName ContainerId, int32 SlotIndex)
{
	if (FSubscription* S = FindSubscription(ClientId, ContainerId))
	{
		if (!S->bNeedsSnapshot)
		{
			AddSlot(S->DirtySlots, SlotIndex);
		}
	}
}

void FContainerReplicationModel::CollectOutgoing(TArray<FContainerMessage>& OutMessages)
{
	OutMessages.Reset();
	for (FSubscription& S : Subscriptions)
	{
		if (S.bNeedsSnapshot)
		{
			FContainerMessage M;
			M.ClientId = S.ClientId;
			M.ContainerId = S.ContainerId;
			M.Kind = EContainerMessageKind::Snapshot;
			OutMessages.Add(M);
			S.bNeedsSnapshot = false;
			S.DirtySlots.Reset();
		}
		else if (S.DirtySlots.Num() > 0)
		{
			FContainerMessage M;
			M.ClientId = S.ClientId;
			M.ContainerId = S.ContainerId;
			M.Kind = EContainerMessageKind::Update;
			M.Slots = MoveTemp(S.DirtySlots);
			M.Slots.Sort();
			OutMessages.Add(M);
			S.DirtySlots.Reset();
		}
	}
	OutMessages.Sort([](const FContainerMessage& A, const FContainerMessage& B)
	{
		return ContainerReplicationDetail::SubscriptionLess(A.ContainerId, A.ClientId, B.ContainerId, B.ClientId);
	});
}

int32 FContainerReplicationModel::MessageBytes(const FContainerMessage& Message, const FInventoryContainer& Container)
{
	const int32 Entries = Message.Kind == EContainerMessageKind::Snapshot ? Container.Num() : Message.Slots.Num();
	return MessageHeaderBytes + Entries * EntryBytes;
}

EInventoryFail FContainerReplicationModel::ServerTake(int32 ClientId, FInventoryContainer& Container, int32 SlotIndex, int64 InstanceId, FInventoryItem& OutItem)
{
	if (!IsSubscribed(ClientId, Container.Id))
	{
		// Un cofre que no se ha abierto no existe para ese cliente: ni se ve ni se toca.
		return EInventoryFail::NotFound;
	}
	const FInventoryItem* There = Container.FindBySlotIndex(SlotIndex);
	if (!There || There->InstanceId != InstanceId)
	{
		MarkSlotStale(ClientId, Container.Id, SlotIndex);
		return EInventoryFail::NotFound;
	}
	FInventoryItem Removed;
	if (!Container.RemoveById(InstanceId, Removed))
	{
		MarkSlotStale(ClientId, Container.Id, SlotIndex);
		return EInventoryFail::NotFound;
	}
	OutItem = Removed;
	MarkSlotChanged(Container.Id, SlotIndex);
	return EInventoryFail::None;
}

EInventoryFail FContainerReplicationModel::ServerPut(int32 ClientId, FInventoryContainer& Container, const FInventoryItem& Item)
{
	if (!IsSubscribed(ClientId, Container.Id))
	{
		return EInventoryFail::NotFound;
	}
	EInventoryFail Fail = EInventoryFail::None;
	if (!Container.Add(Item, Fail))
	{
		return Fail;
	}
	MarkSlotChanged(Container.Id, Container.GetSlotIndexOf(Item.InstanceId));
	return EInventoryFail::None;
}
