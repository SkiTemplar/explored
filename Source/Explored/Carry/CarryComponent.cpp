#include "Carry/CarryComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Carry/ExploredContainer.h"
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
	if (const UItemRegistrySubsystem* Registry = GetRegistry())
	{
		OnItemPickedUp.Broadcast(Registry->GetDisplayName(Instance));
	}
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
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.EquipFromHand(HandSlot, Fail))
	{
		OutFailReason = FailToText(Fail, HandSlot);
		return false;
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

bool UCarryComponent::DetachSledge()
{
	FInventorySledgeDrop Drop;
	if (!Model.DetachSledge(Drop))
	{
		return false;
	}
	// Sin actor de angarillas: el bastidor y su carga quedan sueltos en el suelo.
	SpawnDropped(TakePayload(Drop.SledgeItem));
	for (const FInventoryEntry& Entry : Drop.Load.Entries)
	{
		SpawnDropped(TakePayload(Entry.Item));
	}
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
	SyncFromModel();
	return true;
}
