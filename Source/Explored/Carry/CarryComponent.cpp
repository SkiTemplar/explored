#include "Carry/CarryComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Carry/ExploredContainer.h"
#include "Core/SystemLinks.h"
#include "Carry/ExploredSledge.h"
#include "Items/ExploredItemActor.h"
#include "Items/ItemRegistrySubsystem.h"

static_assert(static_cast<uint8>(EItemSize::Pequeno) == static_cast<uint8>(EInventorySize::Pequeno)
	&& static_cast<uint8>(EItemSize::Mediano) == static_cast<uint8>(EInventorySize::Mediano)
	&& static_cast<uint8>(EItemSize::Grande) == static_cast<uint8>(EInventorySize::Grande)
	&& static_cast<uint8>(EItemSize::DosManos) == static_cast<uint8>(EInventorySize::DosManos),
	"EInventorySize tiene que ser espejo de EItemSize");

namespace CarryComponentDetail
{
	constexpr float DropDistanceCm = 100.0f;

	EInventorySlot ToModelHand(EHand Hand)
	{
		return Hand == EHand::Left ? EInventorySlot::HandLeft : EInventorySlot::HandRight;
	}

	EInventorySlot ToModelSlot(ECarrySlot Slot)
	{
		switch (Slot)
		{
		case ECarrySlot::Pocket: return EInventorySlot::Pockets;
		case ECarrySlot::Belt: return EInventorySlot::Belt;
		case ECarrySlot::Backpack: return EInventorySlot::Backpack;
		case ECarrySlot::Pouch: return EInventorySlot::Pouch;
		case ECarrySlot::Sledge: return EInventorySlot::Sledge;
		default: return EInventorySlot::None;
		}
	}

	bool ToCarrySlot(EInventorySlot Slot, ECarrySlot& OutSlot)
	{
		switch (Slot)
		{
		case EInventorySlot::Pockets: OutSlot = ECarrySlot::Pocket; return true;
		case EInventorySlot::Belt: OutSlot = ECarrySlot::Belt; return true;
		case EInventorySlot::Backpack: OutSlot = ECarrySlot::Backpack; return true;
		case EInventorySlot::Pouch: OutSlot = ECarrySlot::Pouch; return true;
		case EInventorySlot::Sledge: OutSlot = ECarrySlot::Sledge; return true;
		default: return false;
		}
	}
}

UCarryComponent::UCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCarryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bHasBackpack && !Model.HasBackpack())
	{
		EInventoryFail Ignored = EInventoryFail::None;
		Model.SetCustomBackpack(true, BackpackCapacityVolumeLiters, BackpackCapacityWeightKg, Ignored);
		SyncFromModel();
	}
}

const UItemRegistrySubsystem* UCarryComponent::GetRegistry() const
{
	return UItemRegistrySubsystem::Resolve(this);
}

FInventoryItem UCarryComponent::MakeRecord(const FItemInstance& Instance, int64 InstanceId, const UItemRegistrySubsystem* Registry)
{
	FInventoryItem Record;
	Record.InstanceId = InstanceId;
	Record.DefinitionId = Instance.DefinitionId;
	Record.LiquidLiters = Instance.LiquidLiters;
	if (!Registry)
	{
		// Sin registro no se sabe cuánto ocupa: se trata como grande (no entra
		// en bolsillos), igual que hacía la versión anterior del componente.
		Record.Size = EInventorySize::Grande;
		return Record;
	}
	Record.WeightKg = Registry->GetEffectiveWeightKg(Instance);
	Record.VolumeLiters = Registry->GetEffectiveVolumeLiters(Instance);
	Record.Size = static_cast<EInventorySize>(Registry->GetEffectiveSize(Instance));
	FItemDefinition Definition;
	if (Registry->FindDefinition(Instance.DefinitionId, Definition))
	{
		Record.Tags = Definition.Tags.Array();
		if (Definition.HasTag(TEXT("recipiente")) || Definition.HasTag(TEXT("cantimplora")))
		{
			Record.LiquidCapacityLiters = FInventoryModel::LiquidCapacityFromRecipiente(Registry->GetEffectiveProperty(Instance, TEXT("Recipiente")));
		}
	}
	Record.LiquidLiters = FMath::Min(Record.LiquidLiters, Record.LiquidCapacityLiters);
	return Record;
}

FText UCarryComponent::FailToText(EInventoryFail Fail, EInventorySlot Target)
{
	switch (Fail)
	{
	case EInventoryFail::None: return FText::GetEmpty();
	case EInventoryFail::InvalidItem: return NSLOCTEXT("Explored", "Carry_InvalidItem", "Ese objeto no se puede coger.");
	case EInventoryFail::NotFound: return NSLOCTEXT("Explored", "Carry_InvalidSlotIndex", "Ahí no hay nada.");
	case EInventoryFail::AlreadyThere: return NSLOCTEXT("Explored", "Carry_AlreadyThere", "Ya está ahí.");
	case EInventoryFail::HandOccupied: return NSLOCTEXT("Explored", "Carry_HandOccupied", "Esa mano ya está ocupada.");
	case EInventoryFail::NeedBothHands: return NSLOCTEXT("Explored", "Carry_NeedBothHands", "Necesitas las dos manos libres para esto.");
	case EInventoryFail::HandsFull: return NSLOCTEXT("Explored", "Carry_HandsFull", "Tienes las dos manos ocupadas.");
	case EInventoryFail::SameItem: return NSLOCTEXT("Explored", "Carry_SameItem", "Es un solo objeto: no se combina consigo mismo.");
	case EInventoryFail::TooBig:
		if (Target == EInventorySlot::Pockets)
		{
			return NSLOCTEXT("Explored", "Carry_PocketTooSmall", "Solo los objetos pequeños caben en los bolsillos.");
		}
		return NSLOCTEXT("Explored", "Carry_TooBig", "Es demasiado grande para guardarlo ahí.");
	case EInventoryFail::WrongKind:
		if (Target == EInventorySlot::Belt)
		{
			return NSLOCTEXT("Explored", "Carry_BeltNotTool", "El cinturón solo lleva herramientas o recipientes.");
		}
		if (Target == EInventorySlot::Sledge)
		{
			return NSLOCTEXT("Explored", "Carry_SledgeOnlyMaterial", "Las angarillas son para madera y piedra.");
		}
		return NSLOCTEXT("Explored", "Carry_WrongKind", "Eso no se guarda ahí.");
	case EInventoryFail::ContainerFull:
		if (Target == EInventorySlot::Pockets)
		{
			return NSLOCTEXT("Explored", "Carry_PocketsFull", "Los bolsillos están llenos.");
		}
		if (Target == EInventorySlot::Belt)
		{
			return NSLOCTEXT("Explored", "Carry_BeltFull", "El cinturón no tiene enganches libres.");
		}
		return NSLOCTEXT("Explored", "Carry_ContainerFull", "No queda sitio.");
	case EInventoryFail::TooHeavy:
		if (Target == EInventorySlot::Backpack)
		{
			return NSLOCTEXT("Explored", "Carry_BackpackTooHeavy", "Pesa demasiado para la mochila.");
		}
		return NSLOCTEXT("Explored", "Carry_TooHeavy", "Pesa demasiado.");
	case EInventoryFail::NoRoom:
		if (Target == EInventorySlot::Backpack)
		{
			return NSLOCTEXT("Explored", "Carry_BackpackTooBig", "No entra en la mochila.");
		}
		return NSLOCTEXT("Explored", "Carry_NoRoom", "No entra.");
	case EInventoryFail::NoBackpack: return NSLOCTEXT("Explored", "Carry_NoBackpack", "No tienes mochila.");
	case EInventoryFail::NoPouch: return NSLOCTEXT("Explored", "Carry_NoPouch", "No tienes dónde guardarlo seco.");
	case EInventoryFail::NoSledge: return NSLOCTEXT("Explored", "Carry_NoSledge", "No llevas angarillas.");
	case EInventoryFail::SledgeAttached: return NSLOCTEXT("Explored", "Carry_SledgeAttached", "Ya arrastras unas angarillas.");
	case EInventoryFail::NotEquippable: return NSLOCTEXT("Explored", "Carry_NotEquippable", "Eso no se lleva puesto.");
	case EInventoryFail::ContainerNotEmpty: return NSLOCTEXT("Explored", "Carry_ContainerNotEmpty", "Vacíalo antes.");
	case EInventoryFail::OverCarryLimit: return NSLOCTEXT("Explored", "Carry_OverCarryLimit", "No puedes con más peso.");
	case EInventoryFail::NotALiquidContainer: return NSLOCTEXT("Explored", "Carry_NotALiquidContainer", "Ahí no se lleva agua.");
	default: return NSLOCTEXT("Explored", "Carry_Generic", "No se puede.");
	}
}

void UCarryComponent::SyncFromModel()
{
	const FInventoryState& State = Model.GetState();

	// El líquido vive en el modelo mientras se lleva; se copia a la instancia
	// para que el HUD y lo que se suelte lo conserven.
	auto ToInstance = [this](const FInventoryItem& Record)
	{
		FItemInstance Instance = Payloads.FindRef(Record.InstanceId);
		Instance.LiquidLiters = Record.LiquidLiters;
		return Instance;
	};
	auto Rebuild = [&ToInstance](const FInventoryContainer& Container, TArray<FItemInstance>& Out)
	{
		Out.Reset(Container.Num());
		for (const FInventoryEntry& Entry : Container.Entries)
		{
			Out.Add(ToInstance(Entry.Item));
		}
	};

	HandLeft = State.HandLeft.IsValid() ? ToInstance(State.HandLeft) : FItemInstance();
	HandRight = State.HandRight.IsValid() ? ToInstance(State.HandRight) : FItemInstance();
	Rebuild(State.Pockets, Pockets);
	Rebuild(State.Belt, Belt);
	Rebuild(State.Backpack, BackpackItems);
	Rebuild(State.Pouch, PouchItems);
	Rebuild(State.Sledge, SledgeItems);

	if (AExploredSledge* Sledge = AttachedSledge.Get())
	{
		// La carga enganchada se ve encima de las angarillas.
		FInventoryContainer& Shown = Sledge->GetMutableContainer();
		Shown.Entries = State.Sledge.Entries;
		Sledge->RefreshStoredVisuals();
	}

	OnCarryChanged.Broadcast();
}

FItemInstance UCarryComponent::TakePayload(const FInventoryItem& Record)
{
	FItemInstance Instance;
	Payloads.RemoveAndCopyValue(Record.InstanceId, Instance);
	Instance.LiquidLiters = Record.LiquidLiters;
	return Instance;
}

bool UCarryComponent::TryPickUp(AExploredItemActor* ItemActor, FText& OutFailReason)
{
	if (!ItemActor)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_NothingToPickUp", "Aquí no hay nada que coger.");
		return false;
	}
	const FItemInstance& Instance = ItemActor->GetItemInstance();
	if (!Instance.IsValid())
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_InvalidItem", "Ese objeto no se puede coger.");
		return false;
	}

	FInventoryItem Record = MakeRecord(Instance, Model.AllocateInstanceId(), GetRegistry());
	// El actor ya resolvió su tamaño al crearse: si dice DosManos, manda (como
	// en la versión anterior, que solo miraba el actor para esta regla).
	if (ItemActor->GetEffectiveSize() == EItemSize::DosManos)
	{
		Record.Size = EInventorySize::DosManos;
	}
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.PickUp(Record, Fail))
	{
		OutFailReason = FailToText(Fail, EInventorySlot::HandLeft);
		return false;
	}

	Payloads.Add(Record.InstanceId, Instance);
	ItemActor->Destroy();
	SyncFromModel();
	return true;
}

AExploredItemActor* UCarryComponent::SpawnDropped(const FItemInstance& Instance) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}
	const FVector Location = Owner->GetActorLocation() + Owner->GetActorForwardVector() * CarryComponentDetail::DropDistanceCm;
	AExploredItemActor* NewActor = World->SpawnActor<AExploredItemActor>(AExploredItemActor::StaticClass(), Location, Owner->GetActorRotation());
	if (NewActor)
	{
		NewActor->InitializeFromInstance(Instance);
	}
	return NewActor;
}

bool UCarryComponent::Drop(EHand Hand, FText& OutFailReason)
{
	// Con un DosManos se suelta el objeto entero, se pida la mano que se pida.
	const EInventorySlot HandSlot = Model.IsHoldingTwoHanded() ? EInventorySlot::HandLeft : CarryComponentDetail::ToModelHand(Hand);
	FInventoryItem Removed;
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.RemoveFromHand(HandSlot, Removed, Fail))
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_HandEmpty", "No llevas nada en esa mano.");
		return false;
	}
	SpawnDropped(TakePayload(Removed));
	SyncFromModel();
	return true;
}

bool UCarryComponent::ConsumeOneFromHand(EHand Hand)
{
	// Un DosManos ocupa las dos manos: gastarlo las vacía.
	if (Model.IsHoldingTwoHanded())
	{
		for (const FInventoryItem& Removed : Model.ClearHands())
		{
			Payloads.Remove(Removed.InstanceId);
		}
		SyncFromModel();
		return true;
	}
	FItemInstance Discarded;
	return TakeOneFromHand(Hand, Discarded);
}

bool UCarryComponent::StoreFromHand(EHand Hand, ECarrySlot Slot, FText& OutFailReason)
{
	const FInventoryItem* Held = Model.GetHandItem(CarryComponentDetail::ToModelHand(Hand));
	if (!Held)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_HandEmpty", "No llevas nada en esa mano.");
		return false;
	}
	const EInventorySlot Target = CarryComponentDetail::ToModelSlot(Slot);
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.Move(Held->InstanceId, Target, Fail))
	{
		OutFailReason = (Held->IsTwoHanded() && Fail == EInventoryFail::TooBig)
			? NSLOCTEXT("Explored", "Carry_TwoHandedNoStore", "Eso ocupa las dos manos: no se guarda.")
			: FailToText(Fail, Target);
		return false;
	}
	SyncFromModel();
	return true;
}

bool UCarryComponent::TakeToHand(ECarrySlot Slot, int32 Index, EHand Hand, FText& OutFailReason)
{
	const FInventoryContainer* Container = Model.GetContainer(CarryComponentDetail::ToModelSlot(Slot));
	if (!Container || !Container->Entries.IsValidIndex(Index))
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_InvalidSlotIndex", "Ahí no hay nada.");
		return false;
	}
	const EInventorySlot HandSlot = CarryComponentDetail::ToModelHand(Hand);
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.Move(Container->Entries[Index].Item.InstanceId, HandSlot, Fail))
	{
		OutFailReason = FailToText(Fail, HandSlot);
		return false;
	}
	SyncFromModel();
	return true;
}

bool UCarryComponent::AutoStowFromHand(EHand Hand, ECarrySlot& OutSlot, FText& OutFailReason)
{
	EInventorySlot Where = EInventorySlot::None;
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.AutoStowFromHand(CarryComponentDetail::ToModelHand(Hand), Where, Fail))
	{
		OutFailReason = FailToText(Fail, Where);
		return false;
	}
	CarryComponentDetail::ToCarrySlot(Where, OutSlot);
	SyncFromModel();
	return true;
}

bool UCarryComponent::ReplaceHandsWithCraftResult(const FItemInstance& Result, FText& OutFailReason)
{
	for (const FInventoryItem& Removed : Model.ClearHands())
	{
		Payloads.Remove(Removed.InstanceId);
	}

	const FInventoryItem Record = MakeRecord(Result, Model.AllocateInstanceId(), GetRegistry());
	EInventoryFail Fail = EInventoryFail::None;
	const bool bPlaced = Model.PickUp(Record, Fail);
	if (bPlaced)
	{
		Payloads.Add(Record.InstanceId, Result);
	}
	else
	{
		OutFailReason = FailToText(Fail, EInventorySlot::HandLeft);
	}
	// Las manos han cambiado aunque no se haya podido colocar el resultado (M2).
	SyncFromModel();
	return bPlaced;
}

bool UCarryComponent::TakeOneFromHand(EHand Hand, FItemInstance& OutTaken)
{
	if (Model.IsHoldingTwoHanded())
	{
		return false;
	}
	const EInventorySlot HandSlot = CarryComponentDetail::ToModelHand(Hand);
	FInventoryItem Removed;
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.RemoveFromHand(HandSlot, Removed, Fail))
	{
		return false;
	}
	FItemInstance Instance = TakePayload(Removed);
	OutTaken = Instance;
	OutTaken.Count = 1;

	// Si era una pila, el resto vuelve a la misma mano como registro nuevo (el peso
	// del modelo depende de Count, así que se recalcula con MakeRecord).
	if (Instance.Count > 1)
	{
		--Instance.Count;
		const FInventoryItem Rest = MakeRecord(Instance, Model.AllocateInstanceId(), GetRegistry());
		if (Model.PlaceInHand(Rest, HandSlot, Fail))
		{
			Payloads.Add(Rest.InstanceId, Instance);
		}
	}
	SyncFromModel();
	return true;
}

bool UCarryComponent::SwapHands()
{
	if (!Model.SwapHands())
	{
		return false;
	}
	SyncFromModel();
	return true;
}

// --------------------------------------------------------------------------- equipo

bool UCarryComponent::EquipFromHand(EHand Hand, FText& OutFailReason)
{
	const EInventorySlot HandSlot = CarryComponentDetail::ToModelHand(Hand);
	const FInventoryItem* Held = Model.GetHandItem(HandSlot);
	if (!Held)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_HandEmpty", "No llevas nada en esa mano.");
		return false;
	}
	const FInventoryItem HeldCopy = *Held;
	FInventoryEquipmentSpec Spec;
	const bool bIsSledge = FInventoryModel::FindEquipmentSpec(HeldCopy, Spec) && Spec.Kind == EInventoryEquipment::Sledge;

	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.EquipFromHand(HandSlot, Fail))
	{
		OutFailReason = FailToText(Fail, HandSlot);
		return false;
	}

	if (bIsSledge)
	{
		// Las angarillas que estaban en las manos pasan a arrastrarse detrás.
		UWorld* World = GetWorld();
		AActor* Owner = GetOwner();
		if (World && Owner)
		{
			AExploredSledge* Sledge = World->SpawnActor<AExploredSledge>(AExploredSledge::StaticClass(), Owner->GetActorTransform());
			if (Sledge)
			{
				Sledge->SetSledgeItem(HeldCopy.InstanceId, Payloads.FindRef(HeldCopy.InstanceId));
				PlaceSledgeActorBehindOwner(Sledge);
				AttachedSledge = Sledge;
			}
		}
	}
	SyncFromModel();
	return true;
}

bool UCarryComponent::UnequipBackpack(EHand Hand, FText& OutFailReason)
{
	const EInventorySlot HandSlot = CarryComponentDetail::ToModelHand(Hand);
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.UnequipBackpack(HandSlot, Fail))
	{
		OutFailReason = FailToText(Fail, HandSlot);
		return false;
	}
	SyncFromModel();
	return true;
}

bool UCarryComponent::UnequipBelt(EHand Hand, FText& OutFailReason)
{
	const EInventorySlot HandSlot = CarryComponentDetail::ToModelHand(Hand);
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.UnequipBelt(HandSlot, Fail))
	{
		OutFailReason = FailToText(Fail, HandSlot);
		return false;
	}
	SyncFromModel();
	return true;
}

void UCarryComponent::PlaceSledgeActorBehindOwner(AExploredSledge* Sledge) const
{
	AActor* Owner = GetOwner();
	if (!Sledge || !Owner)
	{
		return;
	}
	const FVector Location = Owner->GetActorTransform().TransformPosition(Sledge->AttachOffsetCm);
	Sledge->SetActorLocationAndRotation(Location, Owner->GetActorRotation());
	Sledge->AttachToActor(Owner, FAttachmentTransformRules::KeepWorldTransform);
	Sledge->SetAttached(true);
}

bool UCarryComponent::AttachSledge(AExploredSledge* Sledge, FText& OutFailReason)
{
	if (!Sledge || Sledge->IsAttached())
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_NoSledge", "No llevas angarillas.");
		return false;
	}

	// Registros para el modelo: la carga conserva sus ids (salieron de este modelo).
	const UItemRegistrySubsystem* Registry = GetRegistry();
	const int64 SledgeId = Sledge->GetSledgeInstanceId() != 0 ? Sledge->GetSledgeInstanceId() : Model.AllocateInstanceId();
	const FInventoryItem SledgeRecord = MakeRecord(Sledge->GetSledgeInstance(), SledgeId, Registry);

	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.AttachSledge(SledgeRecord, Sledge->GetContainer(), Fail))
	{
		OutFailReason = FailToText(Fail, EInventorySlot::Sledge);
		return false;
	}

	Payloads.Add(SledgeId, Sledge->GetSledgeInstance());
	for (const FInventoryEntry& Entry : Model.GetState().Sledge.Entries)
	{
		FItemInstance Instance;
		if (Sledge->RemovePayload(Entry.Item.InstanceId, Instance))
		{
			Payloads.Add(Entry.Item.InstanceId, Instance);
		}
	}
	Sledge->GetMutableContainer().Entries.Reset();
	PlaceSledgeActorBehindOwner(Sledge);
	AttachedSledge = Sledge;
	SyncFromModel();
	return true;
}

bool UCarryComponent::DetachSledge()
{
	FInventorySledgeDrop Drop;
	if (!Model.DetachSledge(Drop))
	{
		return false;
	}

	AExploredSledge* Sledge = AttachedSledge.Get();
	AttachedSledge.Reset();
	if (!Sledge)
	{
		// El actor desapareció (no debería): se crea otro donde está el jugador.
		UWorld* World = GetWorld();
		AActor* Owner = GetOwner();
		Sledge = (World && Owner) ? World->SpawnActor<AExploredSledge>(AExploredSledge::StaticClass(), Owner->GetActorTransform()) : nullptr;
	}
	if (!Sledge)
	{
		return true;
	}

	Sledge->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Sledge->SetSledgeItem(Drop.SledgeItem.InstanceId, TakePayload(Drop.SledgeItem));
	FInventoryContainer& Container = Sledge->GetMutableContainer();
	Container.Entries = Drop.Load.Entries;
	for (const FInventoryEntry& Entry : Drop.Load.Entries)
	{
		Sledge->AddPayload(Entry.Item.InstanceId, TakePayload(Entry.Item));
	}
	Sledge->SetAttached(false);
	Sledge->RefreshStoredVisuals();
	SyncFromModel();
	return true;
}

void UCarryComponent::HandleEnterWater()
{
	if (Model.HasSledge())
	{
		// FInventoryModel::ApplyEnterWater: las angarillas no flotan con el jugador.
		DetachSledge();
	}
}

// --------------------------------------------------------------------------- contenedores del mundo

bool UCarryComponent::StoreInContainer(EHand Hand, AExploredContainer* Container, FText& OutFailReason)
{
	const FInventoryItem* Held = Model.GetHandItem(CarryComponentDetail::ToModelHand(Hand));
	if (!Container || !Held)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_HandEmpty", "No llevas nada en esa mano.");
		return false;
	}
	const int64 InstanceId = Held->InstanceId;
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.StoreInWorld(InstanceId, Container->GetMutableContainer(), Fail))
	{
		OutFailReason = FailToText(Fail, EInventorySlot::None);
		return false;
	}
	const FInventoryItem* Stored = Container->GetContainer().FindById(InstanceId);
	FItemInstance Instance;
	Payloads.RemoveAndCopyValue(InstanceId, Instance);
	Instance.LiquidLiters = Stored ? Stored->LiquidLiters : Instance.LiquidLiters;
	Container->AddPayload(InstanceId, Instance);
	Container->RefreshStoredVisuals();
	SyncFromModel();
	return true;
}

bool UCarryComponent::TakeFromContainer(AExploredContainer* Container, int32 SlotIndex, EHand Hand, FText& OutFailReason)
{
	const FInventoryItem* Stored = Container ? Container->GetContainer().FindBySlotIndex(SlotIndex) : nullptr;
	if (!Stored)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_InvalidSlotIndex", "Ahí no hay nada.");
		return false;
	}
	const int64 InstanceId = Stored->InstanceId;
	const EInventorySlot HandSlot = CarryComponentDetail::ToModelHand(Hand);
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.TakeFromWorld(Container->GetMutableContainer(), InstanceId, HandSlot, Fail))
	{
		OutFailReason = FailToText(Fail, HandSlot);
		return false;
	}
	FItemInstance Instance;
	Container->RemovePayload(InstanceId, Instance);
	Payloads.Add(InstanceId, Instance);
	Container->RefreshStoredVisuals();
	SyncFromModel();
	return true;
}

// --------------------------------------------------------------------------- consultas

float UCarryComponent::GetTotalWeight() const
{
	return Model.GetBodyWeightKg();
}

bool UCarryComponent::GetHandItem(EHand Hand, FItemInstance& OutItem) const
{
	const FItemInstance* Item = GetHandItemPtr(Hand);
	if (!Item)
	{
		return false;
	}
	OutItem = *Item;
	return true;
}

bool UCarryComponent::IsHandEmpty(EHand Hand) const
{
	return Model.IsHandEmpty(CarryComponentDetail::ToModelHand(Hand));
}

void UCarryComponent::SetBackpack(bool bInHasBackpack, float CapacityVolumeLiters, float CapacityWeightKg)
{
	EInventoryFail Fail = EInventoryFail::None;
	if (Model.SetCustomBackpack(bInHasBackpack, CapacityVolumeLiters, CapacityWeightKg, Fail))
	{
		bHasBackpack = bInHasBackpack;
		BackpackCapacityVolumeLiters = CapacityVolumeLiters;
		BackpackCapacityWeightKg = CapacityWeightKg;
		SyncFromModel();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] SetBackpack rechazado: %s"), LexToString(Fail));
	}
}

const FItemInstance* UCarryComponent::GetHandItemPtr(EHand Hand) const
{
	if (IsHandEmpty(Hand))
	{
		return nullptr;
	}
	return (Hand == EHand::Left) ? &HandLeft : &HandRight;
}

// --------------------------------------------------------------------------- materiales

namespace CarryComponentDetail
{
	/** Orden de gasto: lo que se arrastra o va en la espalda antes que lo que va en la mano. */
	int32 SpendPriority(EInventorySlot Slot)
	{
		switch (Slot)
		{
		case EInventorySlot::Sledge: return 0;
		case EInventorySlot::Backpack: return 1;
		case EInventorySlot::Pockets:
		case EInventorySlot::Belt:
		case EInventorySlot::Pouch: return 2;
		default: return 3;
		}
	}
}

void UCarryComponent::CountMaterials(TMap<FName, int32>& OutCounts, TSet<FName>& OutTools) const
{
	const FInventoryState& State = Model.GetState();
	auto Add = [this, &OutCounts, &OutTools](const FInventoryItem& Record)
	{
		if (!Record.IsValid())
		{
			return;
		}
		const FItemInstance* Payload = Payloads.Find(Record.InstanceId);
		OutCounts.FindOrAdd(Record.DefinitionId) += FMath::Max(Payload ? Payload->Count : 1, 1);
		OutTools.Add(Record.DefinitionId);
	};
	Add(State.HandLeft);
	// Un objeto a dos manos ocupa las dos: se cuenta una vez.
	if (!State.bHandsHoldTwoHanded)
	{
		Add(State.HandRight);
	}
	for (const FInventoryContainer* Container : {&State.Pockets, &State.Belt, &State.Pouch, &State.Backpack, &State.Sledge})
	{
		for (const FInventoryEntry& Entry : Container->Entries)
		{
			Add(Entry.Item);
		}
	}
}

bool UCarryComponent::ConsumeMaterials(const TArray<FBuildingCost>& Costs)
{
	if (Costs.Num() == 0)
	{
		return true;
	}
	const FInventoryState& State = Model.GetState();
	TArray<ExploredLinks::FMaterialStack> Stacks;
	auto AddStack = [this, &Stacks](const FInventoryItem& Record, EInventorySlot Slot)
	{
		if (!Record.IsValid())
		{
			return;
		}
		const FItemInstance* Payload = Payloads.Find(Record.InstanceId);
		Stacks.Add({Record.InstanceId, Record.DefinitionId, FMath::Max(Payload ? Payload->Count : 1, 1),
			CarryComponentDetail::SpendPriority(Slot)});
	};
	AddStack(State.HandLeft, EInventorySlot::HandLeft);
	if (!State.bHandsHoldTwoHanded)
	{
		AddStack(State.HandRight, EInventorySlot::HandRight);
	}
	const TPair<const FInventoryContainer*, EInventorySlot> Containers[] = {
		{&State.Sledge, EInventorySlot::Sledge}, {&State.Backpack, EInventorySlot::Backpack},
		{&State.Pockets, EInventorySlot::Pockets}, {&State.Belt, EInventorySlot::Belt}, {&State.Pouch, EInventorySlot::Pouch}};
	for (const TPair<const FInventoryContainer*, EInventorySlot>& Container : Containers)
	{
		for (const FInventoryEntry& Entry : Container.Key->Entries)
		{
			AddStack(Entry.Item, Container.Value);
		}
	}

	TArray<ExploredLinks::FMaterialTake> Takes;
	if (!ExploredLinks::PlanMaterialTakes(Stacks, Costs, Takes))
	{
		return false;
	}
	for (const ExploredLinks::FMaterialTake& Take : Takes)
	{
		FItemInstance* Payload = Payloads.Find(Take.InstanceId);
		const int32 Count = Payload ? FMath::Max(Payload->Count, 1) : 1;
		EInventoryFail Fail = EInventoryFail::None;
		if (Take.Count >= Count)
		{
			FInventoryItem Removed;
			if (Model.ConsumeItem(Take.InstanceId, Removed, Fail))
			{
				Payloads.Remove(Take.InstanceId);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[Explored] No se pudo gastar el objeto %lld: %s"), Take.InstanceId, LexToString(Fail));
			}
			continue;
		}
		// Pila que mengua: el registro se rehace con el nuevo Count y conserva su sitio y su líquido.
		Payload->Count = Count - Take.Count;
		FInventoryItem Record = MakeRecord(*Payload, Take.InstanceId, GetRegistry());
		if (const FInventoryItem* Current = Model.FindItemById(Take.InstanceId))
		{
			Record.LiquidLiters = Current->LiquidLiters;
		}
		if (!Model.ShrinkItem(Record, Fail))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Explored] No se pudo mermar la pila %lld: %s"), Take.InstanceId, LexToString(Fail));
		}
	}
	SyncFromModel();
	return true;
}

// --------------------------------------------------------------------------- guardado

void UCarryComponent::ExportState(FInventoryState& OutState, TMap<int64, FItemInstance>& OutInstances) const
{
	OutState = Model.GetState();
	OutInstances = Payloads;
}

bool UCarryComponent::ImportState(const FInventoryState& InState, const TMap<int64, FItemInstance>& InInstances)
{
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.LoadState(InState, Fail))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] Inventario guardado no válido: %s"), LexToString(Fail));
		return false;
	}
	Payloads = InInstances;
	// Las angarillas cargadas se vuelven a ver detrás del jugador.
	if (Model.HasSledge() && !AttachedSledge.IsValid())
	{
		UWorld* World = GetWorld();
		AActor* Owner = GetOwner();
		if (World && Owner)
		{
			if (AExploredSledge* Sledge = World->SpawnActor<AExploredSledge>(AExploredSledge::StaticClass(), Owner->GetActorTransform()))
			{
				const FInventoryItem& SledgeRecord = Model.GetState().SledgeItem;
				Sledge->SetSledgeItem(SledgeRecord.InstanceId, Payloads.FindRef(SledgeRecord.InstanceId));
				PlaceSledgeActorBehindOwner(Sledge);
				AttachedSledge = Sledge;
			}
		}
	}
	SyncFromModel();
	return true;
}
