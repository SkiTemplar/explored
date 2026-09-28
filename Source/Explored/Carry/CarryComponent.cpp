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
	Record.Count = FMath::Max(Instance.Count, 1);
	Record.Quality = static_cast<uint8>(FMath::Clamp(Instance.Quality, 1, 5));
	if (!Registry)
	{
		// Sin registro no se sabe cuánto ocupa: se trata como grande (no entra
		// en bolsillos), igual que hacía la versión anterior del componente.
		Record.Size = EInventorySize::Grande;
		Record.MaxStack = FMath::Min(Record.Count, FInventoryModel::MaxStackSize);
		return Record;
	}
	// El modelo guarda el peso y el volumen de UNA unidad; el registro los da ya multiplicados por Count.
	Record.WeightKg = Registry->GetEffectiveWeightKg(Instance) / static_cast<float>(Record.Count);
	Record.VolumeLiters = Registry->GetEffectiveVolumeLiters(Instance) / static_cast<float>(Record.Count);
	Record.Size = static_cast<EInventorySize>(Registry->GetEffectiveSize(Instance));
	FItemDefinition Definition;
	if (Registry->FindDefinition(Instance.DefinitionId, Definition))
	{
		Record.Tags = Definition.Tags.Array();
		if (Definition.HasTag(TEXT("recipiente")) || Definition.HasTag(TEXT("cantimplora")))
		{
			Record.LiquidCapacityLiters = FInventoryModel::LiquidCapacityFromRecipiente(Registry->GetEffectiveProperty(Instance, TEXT("Recipiente")));
		}
		// Pilas (biblia 03 §1.3): recursos sin durabilidad ni líquido, hasta 10 por hueco.
		Record.MaxStack = FInventoryModel::ComputeMaxStack(Definition.MaxDurability, Record.LiquidCapacityLiters,
			Record.Size, Record.Tags, Instance.Components.Num() > 0);
	}
	Record.LiquidLiters = FMath::Min(Record.LiquidLiters, Record.LiquidCapacityLiters);
	// Una instancia que ya venía con más unidades de las que admite su pila (definición
	// desconocida, datos cambiados) se respeta hasta el tope general para no perder nada.
	Record.MaxStack = FMath::Max(Record.MaxStack, FMath::Min(Record.Count, FInventoryModel::MaxStackSize));
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
	case EInventoryFail::NotStackable: return NSLOCTEXT("Explored", "Carry_NotStackable", "No son iguales: no se juntan.");
	case EInventoryFail::StackFull: return NSLOCTEXT("Explored", "Carry_StackFull", "Ahí ya hay diez.");
	case EInventoryFail::InvalidCount: return NSLOCTEXT("Explored", "Carry_InvalidCount", "No llevas tantos.");
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
		// La cuenta de la pila vive en el modelo (se funde, se parte y se gasta ahí): se copia
		// también a Payloads para que FindInstance y el guardado (ExportState) la vean al día.
		if (FItemInstance* Payload = Payloads.Find(Record.InstanceId))
		{
			Payload->Count = Record.Count;
		}
		FItemInstance Instance = Payloads.FindRef(Record.InstanceId);
		Instance.LiquidLiters = Record.LiquidLiters;
		Instance.Count = Record.Count;
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
	Instance.Count = Record.Count;
	return Instance;
}

void UCarryComponent::ApplyStowResult(int64 SourceId, const FInventoryStowResult& Result)
{
	if (Result.NewStackId != 0)
	{
		// La parte que cupo es otra pila con la misma instancia (la cuenta la pone SyncFromModel).
		Payloads.Add(Result.NewStackId, Payloads.FindRef(SourceId));
	}
	if (Result.bSourceRemoved)
	{
		Payloads.Remove(SourceId);
	}
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
		// Lo que va a dos manos nunca apila (biblia 03 §1.3).
		Record.MaxStack = 1;
	}
	EInventoryFail Fail = EInventoryFail::None;
	int64 MergedInto = 0;
	if (!Model.PickUpMerging(Record, MergedInto, Fail))
	{
		OutFailReason = FailToText(Fail, EInventorySlot::HandLeft);
		return false;
	}

	// Si se ha fundido con la pila de una mano, esa pila ya tiene su instancia.
	if (MergedInto == 0)
	{
		Payloads.Add(Record.InstanceId, Instance);
	}
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
	const int64 HeldId = Held->InstanceId;
	const bool bTwoHanded = Held->IsTwoHanded();
	EInventoryFail Fail = EInventoryFail::None;
	FInventoryStowResult Result;
	// Completa primero las pilas iguales que ya hay ahí; si no cabe todo, el resto sigue en la mano.
	if (!Model.StowMerging(HeldId, Target, Result, Fail))
	{
		OutFailReason = (bTwoHanded && Fail == EInventoryFail::TooBig)
			? NSLOCTEXT("Explored", "Carry_TwoHandedNoStore", "Eso ocupa las dos manos: no se guarda.")
			: FailToText(Fail, Target);
		return false;
	}
	ApplyStowResult(HeldId, Result);
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
	const FInventoryItem* Held = Model.GetHandItem(CarryComponentDetail::ToModelHand(Hand));
	const int64 HeldId = Held ? Held->InstanceId : 0;
	EInventorySlot Where = EInventorySlot::None;
	EInventoryFail Fail = EInventoryFail::None;
	FInventoryStowResult Result;
	if (!Model.AutoStowMergingFromHand(CarryComponentDetail::ToModelHand(Hand), Where, Result, Fail))
	{
		OutFailReason = FailToText(Fail, Where);
		return false;
	}
	ApplyStowResult(HeldId, Result);
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
	const FInventoryItem* Held = Model.GetHandItem(HandSlot);
	if (!Held)
	{
		return false;
	}
	EInventoryFail Fail = EInventoryFail::None;
	if (Held->Count > 1)
	{
		// De una pila sale una unidad: la pila se queda en la mano, con su id y su sitio.
		const int64 HeldId = Held->InstanceId;
		OutTaken = Payloads.FindRef(HeldId);
		OutTaken.LiquidLiters = 0.0f;
		OutTaken.Count = 1;
		if (!Model.RemoveUnits(HeldId, 1, Fail))
		{
			return false;
		}
		SyncFromModel();
		return true;
	}
	FInventoryItem Removed;
	if (!Model.RemoveFromHand(HandSlot, Removed, Fail))
	{
		return false;
	}
	OutTaken = TakePayload(Removed);
	OutTaken.Count = 1;
	SyncFromModel();
	return true;
}

bool UCarryComponent::SplitFromHand(EHand Hand, int32 Count, FText& OutFailReason)
{
	const EInventorySlot From = CarryComponentDetail::ToModelHand(Hand);
	const EInventorySlot To = Hand == EHand::Left ? EInventorySlot::HandRight : EInventorySlot::HandLeft;
	const FInventoryItem* Held = Model.GetHandItem(From);
	if (!Held)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_HandEmpty", "No llevas nada en esa mano.");
		return false;
	}
	const int64 HeldId = Held->InstanceId;
	int64 NewId = 0;
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.SplitStack(HeldId, Count, To, NewId, Fail))
	{
		OutFailReason = FailToText(Fail, To);
		return false;
	}
	Payloads.Add(NewId, Payloads.FindRef(HeldId));
	SyncFromModel();
	return true;
}

bool UCarryComponent::MergeHands(FText& OutFailReason)
{
	const FInventoryItem* Left = Model.GetHandItem(EInventorySlot::HandLeft);
	const FInventoryItem* Right = Model.GetHandItem(EInventorySlot::HandRight);
	if (!Left || !Right)
	{
		OutFailReason = NSLOCTEXT("Explored", "Carry_HandEmpty", "No llevas nada en esa mano.");
		return false;
	}
	const int64 FromId = Right->InstanceId;
	const int64 IntoId = Left->InstanceId;
	int32 Moved = 0;
	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.MergeStacks(FromId, IntoId, Moved, Fail))
	{
		OutFailReason = FailToText(Fail, EInventorySlot::HandLeft);
		return false;
	}
	if (Model.FindItem(FromId) == EInventorySlot::None)
	{
		Payloads.Remove(FromId);
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
	Instance.Count = Stored ? Stored->Count : Instance.Count;
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
		OutCounts.FindOrAdd(Record.DefinitionId) += FMath::Max(Record.Count, 1);
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
	auto AddStack = [&Stacks](const FInventoryItem& Record, EInventorySlot Slot)
	{
		if (!Record.IsValid())
		{
			return;
		}
		Stacks.Add({Record.InstanceId, Record.DefinitionId, FMath::Max(Record.Count, 1),
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
		const FInventoryItem* Record = Model.FindItemById(Take.InstanceId);
		const int32 Count = Record ? FMath::Max(Record->Count, 1) : 1;
		EInventoryFail Fail = EInventoryFail::None;
		// Pila que mengua: conserva su sitio; con la última unidad, desaparece.
		if (!Model.RemoveUnits(Take.InstanceId, FMath::Min(Take.Count, Count), Fail))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Explored] No se pudo gastar el objeto %lld: %s"), Take.InstanceId, LexToString(Fail));
			continue;
		}
		if (Model.FindItem(Take.InstanceId) == EInventorySlot::None)
		{
			Payloads.Remove(Take.InstanceId);
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

namespace CarryComponentDetail
{
	/** Todos los registros del estado que se pueden tocar en su sitio (un DosManos, una vez). */
	template <typename TVisitor>
	void ForEachRecord(FInventoryState& State, TVisitor&& Visitor)
	{
		for (FInventoryItem* Item : { &State.HandLeft, &State.HandRight, &State.BackpackItem, &State.BeltItem, &State.SledgeItem })
		{
			if (Item->IsValid() && !(Item == &State.HandRight && State.bHandsHoldTwoHanded))
			{
				Visitor(*Item);
			}
		}
		if (State.bHandsHoldTwoHanded)
		{
			State.HandRight = State.HandLeft;
		}
		for (FInventoryContainer* Container : { &State.Pockets, &State.Belt, &State.Pouch, &State.Backpack, &State.Sledge })
		{
			for (FInventoryEntry& Entry : Container->Entries)
			{
				Visitor(Entry.Item);
			}
		}
	}
}

bool UCarryComponent::ImportState(const FInventoryState& InState, const TMap<int64, FItemInstance>& InInstances)
{
	FInventoryState State = InState;
	TMap<int64, FItemInstance> Instances = InInstances;
	TArray<FItemInstance> ToDrop;

	if (const UItemRegistrySubsystem* Registry = GetRegistry())
	{
		// 1) Objetos cuya definición ya no existe (contenido retirado en una actualización):
		//    fuera, y lo conocido que se queda sin sitio cae a los pies (FInventoryLoadReport).
		FInventoryLoadReport Report;
		FInventoryModel::SanitizeUnknownDefinitions(State, [Registry](FName Id)
		{
			FItemDefinition Ignored;
			return Registry->FindDefinition(Id, Ignored);
		}, Report);
		for (const FInventoryItem& Unknown : Report.Unknown)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Explored] La partida guardaba «%s» (%lld), que ya no existe: se descarta."),
				*Unknown.DefinitionId.ToString(), Unknown.InstanceId);
			Instances.Remove(Unknown.InstanceId);
		}
		for (const FInventoryItem& Orphan : Report.Orphaned)
		{
			FItemInstance Instance;
			if (Instances.RemoveAndCopyValue(Orphan.InstanceId, Instance))
			{
				// Un registro anterior a las pilas (tope 1 y cuenta 1) deja la cuenta en la instancia.
				if (Orphan.MaxStack > 1 || Orphan.Count > 1)
				{
					Instance.Count = Orphan.Count;
				}
				Instance.LiquidLiters = Orphan.LiquidLiters;
				ToDrop.Add(Instance);
			}
		}

		// 2) Partidas anteriores a las pilas: el registro pesaba la pila entera con Count 1
		//    y la cuenta estaba solo en la instancia. Se pasa a unidades y se fija el tope.
		CarryComponentDetail::ForEachRecord(State, [&Instances, &ToDrop, Registry](FInventoryItem& Record)
		{
			FItemInstance* Instance = Instances.Find(Record.InstanceId);
			FItemDefinition Definition;
			if (!Instance || !Registry->FindDefinition(Record.DefinitionId, Definition))
			{
				return;
			}
			const int32 MaxStack = FInventoryModel::ComputeMaxStack(Definition.MaxDurability, Record.LiquidCapacityLiters,
				Record.Size, Record.Tags, Instance->Components.Num() > 0);
			// Solo un registro anterior a las pilas tiene tope 1 y cuenta 1 con una instancia de más.
			if (Record.MaxStack == 1 && Record.Count == 1 && Instance->Count > 1)
			{
				const int32 Legacy = Instance->Count;
				Record.WeightKg /= static_cast<float>(Legacy);
				Record.VolumeLiters /= static_cast<float>(Legacy);
				Record.Count = FMath::Min(Legacy, MaxStack);
				if (Legacy > Record.Count)
				{
					// Lo que no cabe en una pila de hoy sale a los pies como otra pila.
					FItemInstance Rest = *Instance;
					Rest.Count = Legacy - Record.Count;
					ToDrop.Add(Rest);
				}
			}
			if (Record.Count <= MaxStack)
			{
				Record.MaxStack = MaxStack;
			}
			Record.Quality = static_cast<uint8>(FMath::Clamp(Instance->Quality, 1, 5));
			Instance->Count = Record.Count;
		});
	}

	EInventoryFail Fail = EInventoryFail::None;
	if (!Model.LoadState(State, Fail))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] Inventario guardado no válido: %s"), LexToString(Fail));
		return false;
	}
	Payloads = Instances;
	for (const FItemInstance& Dropped : ToDrop)
	{
		SpawnDropped(Dropped);
	}
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
