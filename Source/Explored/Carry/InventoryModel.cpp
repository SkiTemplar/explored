#include "Carry/InventoryModel.h"

namespace InventoryModelDetail
{
	/** Holgura para que un objeto que llena justo el límite quepa pese al redondeo de float. */
	constexpr float CapacityTolerance = 1.0e-4f;

	// Mochilas (biblia §3.8): la del Albatros trae bolsillo impermeable; la de
	// fibra gana volumen; la de cuero con armazón de bambú gana peso y comodidad.
	constexpr float AlbatrosBackpackLiters = 20.0f;
	constexpr float AlbatrosBackpackKg = 10.0f;
	constexpr float FibreBackpackLiters = 30.0f;
	constexpr float FibreBackpackKg = 12.0f;
	constexpr float LeatherBackpackLiters = 34.0f;
	constexpr float LeatherBackpackKg = 20.0f;
	constexpr float BackpackComfortKg = 2.0f;
	constexpr float FramedBackpackComfortKg = 6.0f;

	/** Cinturón de cuero: +2 enganches (biblia §3.8). */
	constexpr int32 LeatherBeltHooks = 5;

	constexpr int32 SledgeSlots = 8;
	constexpr float SledgeMaxKg = 120.0f;

	/** Ruido y velocidad (P-FISH y movimiento). */
	constexpr float NoiseFromLoad = 0.35f;
	constexpr float NoisePerExtraBeltItem = 0.06f;
	constexpr float NoiseFromSledge = 0.35f;
	constexpr float NoiseFromTwoHanded = 0.1f;
	constexpr float OverloadSpeedPenalty = 0.35f;
	constexpr float SledgeBaseSpeed = 0.85f;
	constexpr float SledgeFullPenalty = 0.3f;
	constexpr float MinSpeedMultiplier = 0.4f;

	FName Tag(const TCHAR* Name) { return FName(Name); }

	bool IsHand(EInventorySlot Slot)
	{
		return Slot == EInventorySlot::HandLeft || Slot == EInventorySlot::HandRight;
	}

	bool IsBodySlot(EInventorySlot Slot)
	{
		return Slot != EInventorySlot::None && Slot != EInventorySlot::Sledge && Slot != EInventorySlot::Count;
	}

	/** El objeto abre la bolsa estanca cuando cuelga del cinturón. */
	bool ProvidesPouch(const FInventoryItem& Item)
	{
		return Item.HasTag(Tag(TEXT("impermeable")));
	}

	/** Lo que se estropea con el agua y conviene llevar en la bolsa estanca. */
	bool NeedsDryStorage(const FInventoryItem& Item)
	{
		return Item.HasTag(Tag(TEXT("mapa"))) || Item.HasTag(Tag(TEXT("papel")))
			|| Item.HasTag(Tag(TEXT("fuego"))) || Item.HasTag(Tag(TEXT("yesca")));
	}

	bool BelongsOnBelt(const FInventoryItem& Item)
	{
		return Item.HasTag(Tag(TEXT("herramienta"))) || Item.HasTag(Tag(TEXT("instrumento")))
			|| Item.HasTag(Tag(TEXT("cantimplora")));
	}

	bool IsHaulingMaterial(const FInventoryItem& Item)
	{
		return Item.HasTag(Tag(TEXT("madera"))) || Item.HasTag(Tag(TEXT("piedra")));
	}

	struct FBodyContainerRef
	{
		const FInventoryContainer* Container;
		EInventorySlot Slot;
	};

	/**
	 * Recorre todo lo que lleva el jugador una sola vez (un DosManos cuenta una
	 * vez aunque ocupe las dos manos). Visitor(const FInventoryItem&, EInventorySlot).
	 */
	template <typename TVisitor>
	void ForEachCarried(const FInventoryState& S, bool bIncludeSledge, TVisitor&& Visitor)
	{
		if (S.HandLeft.IsValid())
		{
			Visitor(S.HandLeft, EInventorySlot::HandLeft);
		}
		if (S.HandRight.IsValid() && !S.bHandsHoldTwoHanded)
		{
			Visitor(S.HandRight, EInventorySlot::HandRight);
		}
		const FBodyContainerRef Containers[] = {
			{ &S.Pockets, EInventorySlot::Pockets },
			{ &S.Belt, EInventorySlot::Belt },
			{ &S.Pouch, EInventorySlot::Pouch },
			{ &S.Backpack, EInventorySlot::Backpack },
		};
		for (const FBodyContainerRef& Ref : Containers)
		{
			for (const FInventoryEntry& Entry : Ref.Container->Entries)
			{
				Visitor(Entry.Item, Ref.Slot);
			}
		}
		if (bIncludeSledge)
		{
			for (const FInventoryEntry& Entry : S.Sledge.Entries)
			{
				Visitor(Entry.Item, EInventorySlot::Sledge);
			}
		}
	}

	/** Equipo puesto (mochila, cinturón de cuero y, si se pide, angarillas). */
	template <typename TVisitor>
	void ForEachEquipped(const FInventoryState& S, bool bIncludeSledge, TVisitor&& Visitor)
	{
		if (S.bHasBackpack && S.BackpackItem.IsValid())
		{
			Visitor(S.BackpackItem);
		}
		if (S.BeltItem.IsValid())
		{
			Visitor(S.BeltItem);
		}
		if (bIncludeSledge && S.bHasSledge && S.SledgeItem.IsValid())
		{
			Visitor(S.SledgeItem);
		}
	}

	/** Reordena los huecos visibles para que ocupen 0..N-1 (al quitar enganches del cinturón). */
	void CompactSlots(FInventoryContainer& Container)
	{
		Container.Entries.StableSort([](const FInventoryEntry& A, const FInventoryEntry& B) { return A.SlotIndex < B.SlotIndex; });
		for (int32 Index = 0; Index < Container.Entries.Num(); ++Index)
		{
			Container.Entries[Index].SlotIndex = Index;
		}
	}

	/** ¿Cabe todo el contenido en otra capacidad? Conserva los huecos si puede. */
	bool RefillInto(const FInventoryContainer& From, const FInventoryContainerSpec& NewSpec, FInventoryContainer& OutTo, EInventoryFail& OutFail)
	{
		OutTo = FInventoryContainer();
		OutTo.Id = From.Id;
		OutTo.Spec = NewSpec;
		for (const FInventoryEntry& Entry : From.Entries)
		{
			if (!OutTo.Add(Entry.Item, OutFail))
			{
				return false;
			}
		}
		OutFail = EInventoryFail::None;
		return true;
	}
}

const TCHAR* LexToString(EInventoryFail Fail)
{
	switch (Fail)
	{
	case EInventoryFail::None: return TEXT("Sin error");
	case EInventoryFail::InvalidItem: return TEXT("Objeto no válido");
	case EInventoryFail::NotFound: return TEXT("No se encuentra");
	case EInventoryFail::AlreadyThere: return TEXT("Ya está ahí");
	case EInventoryFail::HandOccupied: return TEXT("Esa mano está ocupada");
	case EInventoryFail::NeedBothHands: return TEXT("Hacen falta las dos manos libres");
	case EInventoryFail::HandsFull: return TEXT("Las dos manos están ocupadas");
	case EInventoryFail::SameItem: return TEXT("Es un único objeto en las dos manos");
	case EInventoryFail::TooBig: return TEXT("Demasiado grande");
	case EInventoryFail::WrongKind: return TEXT("No es de lo que se guarda ahí");
	case EInventoryFail::ContainerFull: return TEXT("No quedan huecos");
	case EInventoryFail::TooHeavy: return TEXT("Pesa demasiado");
	case EInventoryFail::NoRoom: return TEXT("No cabe");
	case EInventoryFail::NoBackpack: return TEXT("Sin mochila");
	case EInventoryFail::NoPouch: return TEXT("Sin bolsa estanca");
	case EInventoryFail::NoSledge: return TEXT("Sin angarillas");
	case EInventoryFail::SledgeAttached: return TEXT("Ya hay angarillas enganchadas");
	case EInventoryFail::NotEquippable: return TEXT("No se puede poner");
	case EInventoryFail::ContainerNotEmpty: return TEXT("Hay que vaciarlo antes");
	case EInventoryFail::OverCarryLimit: return TEXT("Demasiado peso encima");
	case EInventoryFail::NotALiquidContainer: return TEXT("No guarda líquidos");
	case EInventoryFail::DuplicateId: return TEXT("Id de instancia repetido");
	case EInventoryFail::CorruptState: return TEXT("Estado incoherente");
	default: return TEXT("Desconocido");
	}
}

// --------------------------------------------------------------------------- FInventoryItem

bool FInventoryItem::operator==(const FInventoryItem& Other) const
{
	return InstanceId == Other.InstanceId
		&& DefinitionId == Other.DefinitionId
		&& WeightKg == Other.WeightKg
		&& VolumeLiters == Other.VolumeLiters
		&& Size == Other.Size
		&& Tags == Other.Tags
		&& LiquidLiters == Other.LiquidLiters
		&& LiquidCapacityLiters == Other.LiquidCapacityLiters;
}

// --------------------------------------------------------------------------- FInventoryContainerSpec

bool FInventoryContainerSpec::operator==(const FInventoryContainerSpec& Other) const
{
	return MaxSlots == Other.MaxSlots
		&& MaxVolumeLiters == Other.MaxVolumeLiters
		&& MaxWeightKg == Other.MaxWeightKg
		&& MaxSize == Other.MaxSize
		&& AcceptedTags == Other.AcceptedTags
		&& bWaterproof == Other.bWaterproof;
}

FInventoryContainerSpec FInventoryContainerSpec::Pockets()
{
	FInventoryContainerSpec Spec;
	Spec.MaxSlots = FInventoryModel::PocketSlots;
	Spec.MaxSize = EInventorySize::Pequeno;
	return Spec;
}

FInventoryContainerSpec FInventoryContainerSpec::Belt(int32 Hooks)
{
	FInventoryContainerSpec Spec;
	Spec.MaxSlots = Hooks;
	Spec.MaxSize = EInventorySize::Mediano;
	// «herramienta» y «recipiente/contenedor» ya los usaba UCarryComponent;
	// «instrumento» (brújula, catalejo) y «cantimplora» son de este paquete.
	Spec.AcceptedTags = { FName(TEXT("herramienta")), FName(TEXT("recipiente")), FName(TEXT("contenedor")),
		FName(TEXT("instrumento")), FName(TEXT("cantimplora")) };
	return Spec;
}

FInventoryContainerSpec FInventoryContainerSpec::Pouch()
{
	FInventoryContainerSpec Spec;
	Spec.MaxSlots = FInventoryModel::PouchSlots;
	Spec.MaxSize = EInventorySize::Pequeno;
	Spec.bWaterproof = true;
	return Spec;
}

FInventoryContainerSpec FInventoryContainerSpec::Backpack(float VolumeLiters, float WeightKg)
{
	FInventoryContainerSpec Spec;
	Spec.MaxVolumeLiters = VolumeLiters;
	Spec.MaxWeightKg = WeightKg;
	Spec.MaxSize = EInventorySize::Mediano;
	return Spec;
}

FInventoryContainerSpec FInventoryContainerSpec::Sledge()
{
	FInventoryContainerSpec Spec;
	Spec.MaxSlots = InventoryModelDetail::SledgeSlots;
	Spec.MaxWeightKg = InventoryModelDetail::SledgeMaxKg;
	Spec.MaxSize = EInventorySize::DosManos;
	// Para lo que no cabe en la mochila: troncos, piedra y material de obra
	// (el cuello de botella de 112 kg de troncos, balance/2026-09-26-progresion.md).
	Spec.AcceptedTags = { FName(TEXT("madera")), FName(TEXT("piedra")), FName(TEXT("bambu")),
		FName(TEXT("arcilla")), FName(TEXT("metal")), FName(TEXT("carga")) };
	return Spec;
}

FInventoryContainerSpec FInventoryContainerSpec::Basket()
{
	FInventoryContainerSpec Spec;
	Spec.MaxSlots = 6;
	Spec.MaxVolumeLiters = 12.0f;
	Spec.MaxSize = EInventorySize::Mediano;
	return Spec;
}

FInventoryContainerSpec FInventoryContainerSpec::Shelf()
{
	FInventoryContainerSpec Spec;
	Spec.MaxSlots = 8;
	Spec.MaxSize = EInventorySize::Grande;
	return Spec;
}

FInventoryContainerSpec FInventoryContainerSpec::Chest()
{
	FInventoryContainerSpec Spec;
	Spec.MaxSlots = 12;
	Spec.MaxVolumeLiters = 80.0f;
	Spec.MaxSize = EInventorySize::Grande;
	return Spec;
}

// --------------------------------------------------------------------------- FInventoryContainer

float FInventoryContainer::GetUsedWeightKg() const
{
	float Total = 0.0f;
	for (const FInventoryEntry& Entry : Entries)
	{
		Total += Entry.Item.GetTotalWeightKg();
	}
	return Total;
}

float FInventoryContainer::GetUsedVolumeLiters() const
{
	float Total = 0.0f;
	for (const FInventoryEntry& Entry : Entries)
	{
		Total += Entry.Item.VolumeLiters;
	}
	return Total;
}

EInventoryFail FInventoryContainer::CanAccept(const FInventoryItem& Item) const
{
	using namespace InventoryModelDetail;

	if (!Item.IsValid())
	{
		return EInventoryFail::InvalidItem;
	}
	if (FindIndexById(Item.InstanceId) != INDEX_NONE)
	{
		return EInventoryFail::DuplicateId;
	}
	if (static_cast<uint8>(Item.Size) > static_cast<uint8>(Spec.MaxSize))
	{
		return EInventoryFail::TooBig;
	}
	if (Spec.AcceptedTags.Num() > 0 && !Spec.AcceptedTags.ContainsByPredicate([&Item](const FName& T) { return Item.HasTag(T); }))
	{
		return EInventoryFail::WrongKind;
	}
	if (Spec.MaxSlots > 0 && Entries.Num() >= Spec.MaxSlots)
	{
		return EInventoryFail::ContainerFull;
	}
	if (Spec.MaxVolumeLiters > 0.0f && GetUsedVolumeLiters() + Item.VolumeLiters > Spec.MaxVolumeLiters + CapacityTolerance)
	{
		return EInventoryFail::NoRoom;
	}
	if (Spec.MaxWeightKg > 0.0f && GetUsedWeightKg() + Item.GetTotalWeightKg() > Spec.MaxWeightKg + CapacityTolerance)
	{
		return EInventoryFail::TooHeavy;
	}
	return EInventoryFail::None;
}

bool FInventoryContainer::Add(const FInventoryItem& Item, EInventoryFail& OutFail)
{
	OutFail = CanAccept(Item);
	if (OutFail != EInventoryFail::None)
	{
		return false;
	}
	FInventoryEntry Entry;
	Entry.Item = Item;
	Entry.SlotIndex = FirstFreeSlotIndex();
	Entries.Add(Entry);
	return true;
}

bool FInventoryContainer::RemoveById(int64 InstanceId, FInventoryItem& OutItem)
{
	const int32 Index = FindIndexById(InstanceId);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	OutItem = Entries[Index].Item;
	Entries.RemoveAt(Index);
	return true;
}

int32 FInventoryContainer::FindIndexById(int64 InstanceId) const
{
	if (InstanceId == 0)
	{
		return INDEX_NONE;
	}
	return Entries.IndexOfByPredicate([InstanceId](const FInventoryEntry& E) { return E.Item.InstanceId == InstanceId; });
}

const FInventoryItem* FInventoryContainer::FindById(int64 InstanceId) const
{
	const int32 Index = FindIndexById(InstanceId);
	return Index == INDEX_NONE ? nullptr : &Entries[Index].Item;
}

int32 FInventoryContainer::GetSlotIndexOf(int64 InstanceId) const
{
	const int32 Index = FindIndexById(InstanceId);
	return Index == INDEX_NONE ? INDEX_NONE : Entries[Index].SlotIndex;
}

const FInventoryItem* FInventoryContainer::FindBySlotIndex(int32 SlotIndex) const
{
	const FInventoryEntry* Entry = Entries.FindByPredicate([SlotIndex](const FInventoryEntry& E) { return E.SlotIndex == SlotIndex; });
	return Entry ? &Entry->Item : nullptr;
}

int32 FInventoryContainer::FirstFreeSlotIndex() const
{
	// El menor índice que no usa nadie: al sacar algo, su hueco se reaprovecha
	// sin mover lo demás (lo guardado no «salta» de balda).
	int32 Candidate = 0;
	while (Entries.ContainsByPredicate([Candidate](const FInventoryEntry& E) { return E.SlotIndex == Candidate; }))
	{
		++Candidate;
	}
	return Candidate;
}

bool FInventoryContainer::HasItemWithTag(FName Tag) const
{
	return Entries.ContainsByPredicate([Tag](const FInventoryEntry& E) { return E.Item.HasTag(Tag); });
}

// --------------------------------------------------------------------------- FInventoryState

bool FInventoryState::operator==(const FInventoryState& Other) const
{
	return HandLeft == Other.HandLeft
		&& HandRight == Other.HandRight
		&& bHandsHoldTwoHanded == Other.bHandsHoldTwoHanded
		&& Pockets == Other.Pockets
		&& Belt == Other.Belt
		&& Pouch == Other.Pouch
		&& Backpack == Other.Backpack
		&& Sledge == Other.Sledge
		&& bHasBackpack == Other.bHasBackpack
		&& BackpackItem == Other.BackpackItem
		&& BackpackComfortBonusKg == Other.BackpackComfortBonusKg
		&& bBackpackWaterproofPocket == Other.bBackpackWaterproofPocket
		&& BeltItem == Other.BeltItem
		&& bHasSledge == Other.bHasSledge
		&& SledgeItem == Other.SledgeItem
		&& NextInstanceId == Other.NextInstanceId;
}

// --------------------------------------------------------------------------- FInventoryModel

FInventoryModel::FInventoryModel()
{
	RebuildSpecs(State);
}

int64 FInventoryModel::AllocateInstanceId()
{
	return State.NextInstanceId++;
}

void FInventoryModel::RebuildSpecs(FInventoryState& InOut)
{
	InOut.Pockets.Spec = FInventoryContainerSpec::Pockets();
	InOut.Pouch.Spec = FInventoryContainerSpec::Pouch();
	InOut.Sledge.Spec = FInventoryContainerSpec::Sledge();

	int32 Hooks = BaseBeltHooks;
	FInventoryEquipmentSpec BeltSpec;
	if (InOut.BeltItem.IsValid() && FindEquipmentSpec(InOut.BeltItem, BeltSpec) && BeltSpec.Kind == EInventoryEquipment::Belt)
	{
		Hooks = BeltSpec.BeltHooks;
	}
	InOut.Belt.Spec = FInventoryContainerSpec::Belt(Hooks);

	FInventoryEquipmentSpec BackpackSpec;
	if (InOut.bHasBackpack && InOut.BackpackItem.IsValid() && FindEquipmentSpec(InOut.BackpackItem, BackpackSpec)
		&& BackpackSpec.Kind == EInventoryEquipment::Backpack)
	{
		InOut.Backpack.Spec = FInventoryContainerSpec::Backpack(BackpackSpec.BackpackVolumeLiters, BackpackSpec.BackpackWeightKg);
		InOut.BackpackComfortBonusKg = BackpackSpec.ComfortBonusKg;
		InOut.bBackpackWaterproofPocket = BackpackSpec.bWaterproofPocket;
	}
	// Una mochila sin objeto (SetCustomBackpack) conserva la capacidad guardada.
}

bool FInventoryModel::StateProvidesPouch(const FInventoryState& InState)
{
	if (InState.bHasBackpack && InState.bBackpackWaterproofPocket)
	{
		return true;
	}
	return InState.Belt.Entries.ContainsByPredicate([](const FInventoryEntry& E) { return InventoryModelDetail::ProvidesPouch(E.Item); });
}

bool FInventoryModel::HasPouch() const
{
	return StateProvidesPouch(State);
}

// ----------------------------------------------------------------- manos

const FInventoryItem* FInventoryModel::GetHandItem(EInventorySlot Hand) const
{
	if (Hand == EInventorySlot::HandLeft)
	{
		return State.HandLeft.IsValid() ? &State.HandLeft : nullptr;
	}
	if (Hand == EInventorySlot::HandRight)
	{
		return State.HandRight.IsValid() ? &State.HandRight : nullptr;
	}
	return nullptr;
}

bool FInventoryModel::PickUp(const FInventoryItem& Item, EInventoryFail& OutFail)
{
	if (!Item.IsValid())
	{
		OutFail = EInventoryFail::InvalidItem;
		return false;
	}
	if (FindItem(Item.InstanceId) != EInventorySlot::None)
	{
		OutFail = EInventoryFail::DuplicateId;
		return false;
	}
	EInventorySlot Target = EInventorySlot::HandLeft;
	if (Item.IsTwoHanded())
	{
		if (State.HandLeft.IsValid() || State.HandRight.IsValid())
		{
			OutFail = EInventoryFail::NeedBothHands;
			return false;
		}
	}
	else if (State.HandLeft.IsValid())
	{
		if (State.HandRight.IsValid())
		{
			OutFail = EInventoryFail::HandsFull;
			return false;
		}
		Target = EInventorySlot::HandRight;
	}
	return PlaceInHand(Item, Target, OutFail);
}

bool FInventoryModel::PlaceInHand(const FInventoryItem& Item, EInventorySlot Hand, EInventoryFail& OutFail)
{
	if (!InventoryModelDetail::IsHand(Hand))
	{
		OutFail = EInventoryFail::InvalidItem;
		return false;
	}
	if (Item.IsValid() && FindItem(Item.InstanceId) != EInventorySlot::None)
	{
		OutFail = EInventoryFail::DuplicateId;
		return false;
	}
	OutFail = CanPlace(Item, Hand, /*bAlreadyOnBody=*/false);
	if (OutFail != EInventoryFail::None)
	{
		return false;
	}
	PlaceUnchecked(Item, Hand);
	State.NextInstanceId = FMath::Max(State.NextInstanceId, Item.InstanceId + 1);
	return true;
}

bool FInventoryModel::RemoveFromHand(EInventorySlot Hand, FInventoryItem& OutItem, EInventoryFail& OutFail)
{
	const FInventoryItem* Held = GetHandItem(Hand);
	if (!Held)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	OutFail = EInventoryFail::None;
	return RemoveUnchecked(Held->InstanceId, OutItem);
}

TArray<FInventoryItem> FInventoryModel::ClearHands()
{
	TArray<FInventoryItem> Removed;
	if (State.HandLeft.IsValid())
	{
		Removed.Add(State.HandLeft);
	}
	if (State.HandRight.IsValid() && !State.bHandsHoldTwoHanded)
	{
		Removed.Add(State.HandRight);
	}
	State.HandLeft = FInventoryItem();
	State.HandRight = FInventoryItem();
	State.bHandsHoldTwoHanded = false;
	return Removed;
}

bool FInventoryModel::SwapHands()
{
	if (State.bHandsHoldTwoHanded)
	{
		return false;
	}
	Swap(State.HandLeft, State.HandRight);
	return true;
}

bool FInventoryModel::CanCombineHands(EInventoryFail& OutFail) const
{
	if (State.bHandsHoldTwoHanded)
	{
		OutFail = EInventoryFail::SameItem;
		return false;
	}
	if (!State.HandLeft.IsValid() || !State.HandRight.IsValid())
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	if (State.HandLeft.InstanceId == State.HandRight.InstanceId)
	{
		OutFail = EInventoryFail::SameItem;
		return false;
	}
	OutFail = EInventoryFail::None;
	return true;
}

// ------------------------------------------------------------- búsqueda

EInventorySlot FInventoryModel::FindItem(int64 InstanceId) const
{
	if (InstanceId == 0)
	{
		return EInventorySlot::None;
	}
	if (State.HandLeft.InstanceId == InstanceId)
	{
		return EInventorySlot::HandLeft;
	}
	if (State.HandRight.InstanceId == InstanceId)
	{
		return EInventorySlot::HandRight;
	}
	const EInventorySlot Slots[] = { EInventorySlot::Pockets, EInventorySlot::Belt, EInventorySlot::Pouch, EInventorySlot::Backpack, EInventorySlot::Sledge };
	for (EInventorySlot Slot : Slots)
	{
		if (GetContainer(Slot)->FindIndexById(InstanceId) != INDEX_NONE)
		{
			return Slot;
		}
	}
	return EInventorySlot::None;
}

const FInventoryItem* FInventoryModel::FindItemById(int64 InstanceId) const
{
	const EInventorySlot Slot = FindItem(InstanceId);
	if (InventoryModelDetail::IsHand(Slot))
	{
		return GetHandItem(Slot);
	}
	if (const FInventoryContainer* Container = GetContainer(Slot))
	{
		return Container->FindById(InstanceId);
	}
	return nullptr;
}

FInventoryItem* FInventoryModel::FindMutableItemById(int64 InstanceId)
{
	return const_cast<FInventoryItem*>(FindItemById(InstanceId));
}

const FInventoryContainer* FInventoryModel::GetContainer(EInventorySlot Slot) const
{
	switch (Slot)
	{
	case EInventorySlot::Pockets: return &State.Pockets;
	case EInventorySlot::Belt: return &State.Belt;
	case EInventorySlot::Pouch: return &State.Pouch;
	case EInventorySlot::Backpack: return &State.Backpack;
	case EInventorySlot::Sledge: return &State.Sledge;
	default: return nullptr;
	}
}

FInventoryContainer* FInventoryModel::GetMutableContainer(EInventorySlot Slot)
{
	return const_cast<FInventoryContainer*>(GetContainer(Slot));
}

// ------------------------------------------------------------- colocación

EInventoryFail FInventoryModel::CanPlace(const FInventoryItem& Item, EInventorySlot To, bool bAlreadyOnBody) const
{
	using namespace InventoryModelDetail;

	if (!Item.IsValid())
	{
		return EInventoryFail::InvalidItem;
	}

	if (IsHand(To))
	{
		if (Item.IsTwoHanded())
		{
			if (State.HandLeft.IsValid() || State.HandRight.IsValid())
			{
				return EInventoryFail::NeedBothHands;
			}
		}
		else if (GetHandItem(To))
		{
			return EInventoryFail::HandOccupied;
		}
	}
	else
	{
		const FInventoryContainer* Container = GetContainer(To);
		if (!Container)
		{
			return EInventoryFail::InvalidItem;
		}
		if (To == EInventorySlot::Backpack && !State.bHasBackpack)
		{
			return EInventoryFail::NoBackpack;
		}
		if (To == EInventorySlot::Pouch)
		{
			if (!HasPouch())
			{
				return EInventoryFail::NoPouch;
			}
			if (ProvidesPouch(Item))
			{
				// La bolsa no se guarda dentro de sí misma.
				return EInventoryFail::WrongKind;
			}
		}
		if (To == EInventorySlot::Sledge && !State.bHasSledge)
		{
			return EInventoryFail::NoSledge;
		}
		const EInventoryFail Fail = Container->CanAccept(Item);
		if (Fail != EInventoryFail::None)
		{
			return Fail;
		}
	}

	if (IsBodySlot(To) && !bAlreadyOnBody
		&& GetBodyWeightKg() + Item.GetTotalWeightKg() > GetComfortableCapacityKg() * MaxLoadRatio + CapacityTolerance)
	{
		return EInventoryFail::OverCarryLimit;
	}
	return EInventoryFail::None;
}

void FInventoryModel::PlaceUnchecked(const FInventoryItem& Item, EInventorySlot To)
{
	if (InventoryModelDetail::IsHand(To))
	{
		if (Item.IsTwoHanded())
		{
			State.HandLeft = Item;
			State.HandRight = Item;
			State.bHandsHoldTwoHanded = true;
		}
		else if (To == EInventorySlot::HandLeft)
		{
			State.HandLeft = Item;
		}
		else
		{
			State.HandRight = Item;
		}
		return;
	}
	EInventoryFail Ignored = EInventoryFail::None;
	FInventoryContainer* Container = GetMutableContainer(To);
	check(Container);
	const bool bAdded = Container->Add(Item, Ignored);
	check(bAdded);
}

bool FInventoryModel::RemoveUnchecked(int64 InstanceId, FInventoryItem& OutItem)
{
	const EInventorySlot From = FindItem(InstanceId);
	if (From == EInventorySlot::None)
	{
		return false;
	}
	if (InventoryModelDetail::IsHand(From))
	{
		if (State.bHandsHoldTwoHanded)
		{
			OutItem = State.HandLeft;
			State.HandLeft = FInventoryItem();
			State.HandRight = FInventoryItem();
			State.bHandsHoldTwoHanded = false;
			return true;
		}
		FInventoryItem& HandRef = (From == EInventorySlot::HandLeft) ? State.HandLeft : State.HandRight;
		OutItem = HandRef;
		HandRef = FInventoryItem();
		return true;
	}
	return GetMutableContainer(From)->RemoveById(InstanceId, OutItem);
}

bool FInventoryModel::WouldOrphanPouch(int64 InstanceId, EInventorySlot From, EInventorySlot To) const
{
	if (From != EInventorySlot::Belt || To == EInventorySlot::Belt || State.Pouch.IsEmpty())
	{
		return false;
	}
	const FInventoryItem* Item = State.Belt.FindById(InstanceId);
	if (!Item || !InventoryModelDetail::ProvidesPouch(*Item))
	{
		return false;
	}
	if (State.bHasBackpack && State.bBackpackWaterproofPocket)
	{
		return false;
	}
	// ¿Hay otra bolsa colgada que la sustituya?
	for (const FInventoryEntry& Entry : State.Belt.Entries)
	{
		if (Entry.Item.InstanceId != InstanceId && InventoryModelDetail::ProvidesPouch(Entry.Item))
		{
			return false;
		}
	}
	return true;
}

// ------------------------------------------------------------- traslados

EInventoryFail FInventoryModel::CanMove(int64 InstanceId, EInventorySlot To) const
{
	using namespace InventoryModelDetail;

	const EInventorySlot From = FindItem(InstanceId);
	if (From == EInventorySlot::None)
	{
		return EInventoryFail::NotFound;
	}
	const FInventoryItem* Item = FindItemById(InstanceId);
	check(Item);
	if (From == To || (IsHand(From) && IsHand(To) && Item->IsTwoHanded()))
	{
		return EInventoryFail::AlreadyThere;
	}
	if (WouldOrphanPouch(InstanceId, From, To))
	{
		return EInventoryFail::ContainerNotEmpty;
	}
	if (IsHand(From) && IsHand(To))
	{
		// De una mano a la otra: solo importa que la de destino esté libre.
		return GetHandItem(To) ? EInventoryFail::HandOccupied : EInventoryFail::None;
	}
	return CanPlace(*Item, To, /*bAlreadyOnBody=*/IsBodySlot(From));
}

bool FInventoryModel::Move(int64 InstanceId, EInventorySlot To, EInventoryFail& OutFail)
{
	OutFail = CanMove(InstanceId, To);
	if (OutFail != EInventoryFail::None)
	{
		return false;
	}
	FInventoryItem Item;
	const bool bRemoved = RemoveUnchecked(InstanceId, Item);
	check(bRemoved);
	PlaceUnchecked(Item, To);
	return true;
}

bool FInventoryModel::StoreInWorld(int64 InstanceId, FInventoryContainer& World, EInventoryFail& OutFail)
{
	const EInventorySlot From = FindItem(InstanceId);
	if (From == EInventorySlot::None)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	if (WouldOrphanPouch(InstanceId, From, EInventorySlot::None))
	{
		OutFail = EInventoryFail::ContainerNotEmpty;
		return false;
	}
	OutFail = World.CanAccept(*FindItemById(InstanceId));
	if (OutFail != EInventoryFail::None)
	{
		return false;
	}
	FInventoryItem Item;
	RemoveUnchecked(InstanceId, Item);
	const bool bAdded = World.Add(Item, OutFail);
	check(bAdded);
	return true;
}

bool FInventoryModel::TakeFromWorld(FInventoryContainer& World, int64 InstanceId, EInventorySlot To, EInventoryFail& OutFail)
{
	const FInventoryItem* Found = World.FindById(InstanceId);
	if (!Found)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	if (FindItem(InstanceId) != EInventorySlot::None)
	{
		OutFail = EInventoryFail::DuplicateId;
		return false;
	}
	OutFail = CanPlace(*Found, To, /*bAlreadyOnBody=*/false);
	if (OutFail != EInventoryFail::None)
	{
		return false;
	}
	FInventoryItem Item;
	World.RemoveById(InstanceId, Item);
	PlaceUnchecked(Item, To);
	State.NextInstanceId = FMath::Max(State.NextInstanceId, Item.InstanceId + 1);
	return true;
}

EInventorySlot FInventoryModel::SuggestStowSlot(const FInventoryItem& Item) const
{
	using namespace InventoryModelDetail;

	TArray<EInventorySlot> Candidates;
	if (NeedsDryStorage(Item))
	{
		Candidates.Add(EInventorySlot::Pouch);
	}
	if (BelongsOnBelt(Item))
	{
		Candidates.Add(EInventorySlot::Belt);
	}
	if (IsHaulingMaterial(Item) && State.bHasSledge && Item.Size != EInventorySize::Pequeno)
	{
		// Troncos y maderas grandes: mejor a las angarillas que a la espalda.
		Candidates.Add(EInventorySlot::Sledge);
	}
	Candidates.Add(EInventorySlot::Pockets);
	Candidates.Add(EInventorySlot::Backpack);
	Candidates.Add(EInventorySlot::Belt);
	Candidates.Add(EInventorySlot::Sledge);

	const bool bOnBody = FindItem(Item.InstanceId) != EInventorySlot::None && FindItem(Item.InstanceId) != EInventorySlot::Sledge;
	for (EInventorySlot Slot : Candidates)
	{
		if (FindItem(Item.InstanceId) == Slot)
		{
			continue;
		}
		if (CanPlace(Item, Slot, bOnBody) == EInventoryFail::None)
		{
			return Slot;
		}
	}
	return EInventorySlot::None;
}

bool FInventoryModel::AutoStowFromHand(EInventorySlot Hand, EInventorySlot& OutWhere, EInventoryFail& OutFail)
{
	OutWhere = EInventorySlot::None;
	const FInventoryItem* Held = GetHandItem(Hand);
	if (!Held)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	const EInventorySlot Target = SuggestStowSlot(*Held);
	if (Target == EInventorySlot::None)
	{
		OutFail = Held->IsTwoHanded() ? EInventoryFail::TooBig : EInventoryFail::NoRoom;
		return false;
	}
	if (!Move(Held->InstanceId, Target, OutFail))
	{
		return false;
	}
	OutWhere = Target;
	return true;
}

// ----------------------------------------------------------------- equipo

bool FInventoryModel::FindEquipmentSpec(const FInventoryItem& Item, FInventoryEquipmentSpec& OutSpec)
{
	using namespace InventoryModelDetail;

	OutSpec = FInventoryEquipmentSpec();
	const FName Id = Item.DefinitionId;
	if (Id == FName(TEXT("mochila_cuero_bambu")))
	{
		OutSpec.Kind = EInventoryEquipment::Backpack;
		OutSpec.BackpackVolumeLiters = LeatherBackpackLiters;
		OutSpec.BackpackWeightKg = LeatherBackpackKg;
		OutSpec.ComfortBonusKg = FramedBackpackComfortKg;
		OutSpec.bWaterproofPocket = true;
		return true;
	}
	if (Id == FName(TEXT("mochila_fibra")))
	{
		OutSpec.Kind = EInventoryEquipment::Backpack;
		OutSpec.BackpackVolumeLiters = FibreBackpackLiters;
		OutSpec.BackpackWeightKg = FibreBackpackKg;
		OutSpec.ComfortBonusKg = BackpackComfortKg;
		OutSpec.bWaterproofPocket = false;
		return true;
	}
	if (Id == FName(TEXT("mochila")) || Item.HasTag(Tag(TEXT("mochila"))))
	{
		// La del Albatros (y cualquier mochila sin fila propia).
		OutSpec.Kind = EInventoryEquipment::Backpack;
		OutSpec.BackpackVolumeLiters = AlbatrosBackpackLiters;
		OutSpec.BackpackWeightKg = AlbatrosBackpackKg;
		OutSpec.ComfortBonusKg = BackpackComfortKg;
		OutSpec.bWaterproofPocket = true;
		return true;
	}
	if (Id == FName(TEXT("cinturon_cuero")) || Item.HasTag(Tag(TEXT("cinturon"))))
	{
		OutSpec.Kind = EInventoryEquipment::Belt;
		OutSpec.BeltHooks = LeatherBeltHooks;
		return true;
	}
	if (Id == FName(TEXT("angarillas")) || Item.HasTag(Tag(TEXT("angarillas"))))
	{
		OutSpec.Kind = EInventoryEquipment::Sledge;
		return true;
	}
	return false;
}

bool FInventoryModel::EquipFromHand(EInventorySlot Hand, EInventoryFail& OutFail)
{
	const FInventoryItem* HeldPtr = GetHandItem(Hand);
	if (!HeldPtr)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	const FInventoryItem Held = *HeldPtr;
	FInventoryEquipmentSpec Spec;
	if (!FindEquipmentSpec(Held, Spec))
	{
		OutFail = EInventoryFail::NotEquippable;
		return false;
	}

	switch (Spec.Kind)
	{
	case EInventoryEquipment::Backpack:
	{
		// El contenido pasa a la mochila nueva: tiene que caber entero.
		FInventoryContainer NewBackpack;
		if (!InventoryModelDetail::RefillInto(State.Backpack, FInventoryContainerSpec::Backpack(Spec.BackpackVolumeLiters, Spec.BackpackWeightKg), NewBackpack, OutFail))
		{
			return false;
		}
		const bool bOldPocket = State.bHasBackpack && State.bBackpackWaterproofPocket;
		if (bOldPocket && !Spec.bWaterproofPocket && !State.Pouch.IsEmpty())
		{
			FInventoryState Probe = State;
			Probe.bBackpackWaterproofPocket = false;
			if (!StateProvidesPouch(Probe))
			{
				OutFail = EInventoryFail::ContainerNotEmpty;
				return false;
			}
		}
		const FInventoryItem OldItem = State.bHasBackpack ? State.BackpackItem : FInventoryItem();
		FInventoryItem Removed;
		RemoveUnchecked(Held.InstanceId, Removed);
		State.bHasBackpack = true;
		State.BackpackItem = Held;
		State.Backpack = NewBackpack;
		RebuildSpecs(State);
		if (OldItem.IsValid())
		{
			// La vieja queda en la mano que sostenía la nueva (la mochila no es DosManos).
			PlaceUnchecked(OldItem, Hand);
		}
		OutFail = EInventoryFail::None;
		return true;
	}
	case EInventoryEquipment::Belt:
	{
		if (State.Belt.Num() > Spec.BeltHooks)
		{
			OutFail = EInventoryFail::ContainerNotEmpty;
			return false;
		}
		const FInventoryItem OldItem = State.BeltItem;
		FInventoryItem Removed;
		RemoveUnchecked(Held.InstanceId, Removed);
		State.BeltItem = Held;
		RebuildSpecs(State);
		InventoryModelDetail::CompactSlots(State.Belt);
		if (OldItem.IsValid())
		{
			PlaceUnchecked(OldItem, Hand);
		}
		OutFail = EInventoryFail::None;
		return true;
	}
	case EInventoryEquipment::Sledge:
	{
		if (State.bHasSledge)
		{
			OutFail = EInventoryFail::SledgeAttached;
			return false;
		}
		FInventoryItem Removed;
		RemoveUnchecked(Held.InstanceId, Removed);
		State.bHasSledge = true;
		State.SledgeItem = Held;
		State.Sledge.Entries.Reset();
		OutFail = EInventoryFail::None;
		return true;
	}
	default:
		OutFail = EInventoryFail::NotEquippable;
		return false;
	}
}

bool FInventoryModel::UnequipBackpack(EInventorySlot Hand, EInventoryFail& OutFail)
{
	if (!State.bHasBackpack)
	{
		OutFail = EInventoryFail::NoBackpack;
		return false;
	}
	if (!State.Backpack.IsEmpty())
	{
		OutFail = EInventoryFail::ContainerNotEmpty;
		return false;
	}
	if (State.bBackpackWaterproofPocket && !State.Pouch.IsEmpty())
	{
		FInventoryState Probe = State;
		Probe.bHasBackpack = false;
		if (!StateProvidesPouch(Probe))
		{
			OutFail = EInventoryFail::ContainerNotEmpty;
			return false;
		}
	}
	if (State.BackpackItem.IsValid())
	{
		// Ya pesaba encima: al pasar a la mano no cambia el peso del cuerpo.
		OutFail = CanPlace(State.BackpackItem, Hand, /*bAlreadyOnBody=*/true);
		if (OutFail != EInventoryFail::None)
		{
			return false;
		}
		PlaceUnchecked(State.BackpackItem, Hand);
	}
	State.bHasBackpack = false;
	State.BackpackItem = FInventoryItem();
	State.BackpackComfortBonusKg = 0.0f;
	State.bBackpackWaterproofPocket = false;
	OutFail = EInventoryFail::None;
	return true;
}

bool FInventoryModel::UnequipBelt(EInventorySlot Hand, EInventoryFail& OutFail)
{
	if (!State.BeltItem.IsValid())
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	if (State.Belt.Num() > BaseBeltHooks)
	{
		OutFail = EInventoryFail::ContainerNotEmpty;
		return false;
	}
	OutFail = CanPlace(State.BeltItem, Hand, /*bAlreadyOnBody=*/true);
	if (OutFail != EInventoryFail::None)
	{
		return false;
	}
	PlaceUnchecked(State.BeltItem, Hand);
	State.BeltItem = FInventoryItem();
	RebuildSpecs(State);
	InventoryModelDetail::CompactSlots(State.Belt);
	return true;
}

bool FInventoryModel::SetCustomBackpack(bool bEquipped, float VolumeLiters, float WeightKg, EInventoryFail& OutFail)
{
	if (State.BackpackItem.IsValid())
	{
		OutFail = EInventoryFail::NotEquippable;
		return false;
	}
	if (!bEquipped)
	{
		if (!State.Backpack.IsEmpty())
		{
			OutFail = EInventoryFail::ContainerNotEmpty;
			return false;
		}
		State.bHasBackpack = false;
		State.BackpackComfortBonusKg = 0.0f;
		State.bBackpackWaterproofPocket = false;
		OutFail = EInventoryFail::None;
		return true;
	}
	FInventoryContainer Refilled;
	if (!InventoryModelDetail::RefillInto(State.Backpack, FInventoryContainerSpec::Backpack(VolumeLiters, WeightKg), Refilled, OutFail))
	{
		return false;
	}
	State.Backpack = Refilled;
	State.bHasBackpack = true;
	State.BackpackComfortBonusKg = 0.0f;
	State.bBackpackWaterproofPocket = false;
	return true;
}

bool FInventoryModel::AttachSledge(const FInventoryItem& SledgeItem, const FInventoryContainer& Load, EInventoryFail& OutFail)
{
	if (State.bHasSledge)
	{
		OutFail = EInventoryFail::SledgeAttached;
		return false;
	}
	FInventoryEquipmentSpec Spec;
	if (!SledgeItem.IsValid() || !FindEquipmentSpec(SledgeItem, Spec) || Spec.Kind != EInventoryEquipment::Sledge)
	{
		OutFail = EInventoryFail::NotEquippable;
		return false;
	}
	if (FindItem(SledgeItem.InstanceId) != EInventorySlot::None)
	{
		OutFail = EInventoryFail::DuplicateId;
		return false;
	}
	FInventoryContainer NewLoad;
	if (!InventoryModelDetail::RefillInto(Load, FInventoryContainerSpec::Sledge(), NewLoad, OutFail))
	{
		return false;
	}
	int64 MaxId = SledgeItem.InstanceId;
	for (const FInventoryEntry& Entry : NewLoad.Entries)
	{
		if (FindItem(Entry.Item.InstanceId) != EInventorySlot::None || Entry.Item.InstanceId == SledgeItem.InstanceId)
		{
			OutFail = EInventoryFail::DuplicateId;
			return false;
		}
		MaxId = FMath::Max(MaxId, Entry.Item.InstanceId);
	}
	NewLoad.Id = FName();
	State.bHasSledge = true;
	State.SledgeItem = SledgeItem;
	State.Sledge = NewLoad;
	State.NextInstanceId = FMath::Max(State.NextInstanceId, MaxId + 1);
	OutFail = EInventoryFail::None;
	return true;
}

bool FInventoryModel::DetachSledge(FInventorySledgeDrop& OutDrop)
{
	if (!State.bHasSledge)
	{
		return false;
	}
	OutDrop.SledgeItem = State.SledgeItem;
	OutDrop.Load = State.Sledge;
	State.bHasSledge = false;
	State.SledgeItem = FInventoryItem();
	State.Sledge.Entries.Reset();
	return true;
}

bool FInventoryModel::ApplyEnterWater(FInventorySledgeDrop& OutDrop)
{
	return DetachSledge(OutDrop);
}

// ------------------------------------------------------------------- peso

float FInventoryModel::GetBodyWeightKg() const
{
	float Total = 0.0f;
	InventoryModelDetail::ForEachCarried(State, /*bIncludeSledge=*/false, [&Total](const FInventoryItem& Item, EInventorySlot) { Total += Item.GetTotalWeightKg(); });
	InventoryModelDetail::ForEachEquipped(State, /*bIncludeSledge=*/false, [&Total](const FInventoryItem& Item) { Total += Item.GetTotalWeightKg(); });
	return Total;
}

float FInventoryModel::GetSledgeWeightKg() const
{
	if (!State.bHasSledge)
	{
		return 0.0f;
	}
	return State.SledgeItem.GetTotalWeightKg() + State.Sledge.GetUsedWeightKg();
}

float FInventoryModel::GetComfortableCapacityKg() const
{
	return BaseComfortableKg + (State.bHasBackpack ? State.BackpackComfortBonusKg : 0.0f);
}

float FInventoryModel::GetCarriedWeightRatio() const
{
	return (GetBodyWeightKg() + GetSledgeWeightKg() * SledgeDragFactor) / GetComfortableCapacityKg();
}

float FInventoryModel::GetSwimLoadRatio() const
{
	return GetBodyWeightKg() / GetComfortableCapacityKg();
}

float FInventoryModel::GetNoiseLevel() const
{
	using namespace InventoryModelDetail;

	float Noise = NoiseFromLoad * FMath::Clamp(GetCarriedWeightRatio() / MaxLoadRatio, 0.0f, 1.0f);
	// Las herramientas colgadas chocan entre sí al andar.
	Noise += NoisePerExtraBeltItem * FMath::Max(0, State.Belt.Num() - 1);
	if (State.bHasSledge)
	{
		Noise += NoiseFromSledge;
	}
	if (State.bHandsHoldTwoHanded)
	{
		Noise += NoiseFromTwoHanded;
	}
	return FMath::Clamp(Noise, 0.0f, 1.0f);
}

float FInventoryModel::GetMoveSpeedMultiplier() const
{
	using namespace InventoryModelDetail;

	float Multiplier = 1.0f;
	const float BodyRatio = GetSwimLoadRatio();
	if (BodyRatio > 1.0f)
	{
		Multiplier -= OverloadSpeedPenalty * (BodyRatio - 1.0f);
	}
	if (State.bHasSledge)
	{
		const float Fill = FMath::Clamp(GetSledgeWeightKg() / SledgeMaxKg, 0.0f, 1.0f);
		Multiplier *= SledgeBaseSpeed - SledgeFullPenalty * Fill;
	}
	return FMath::Clamp(Multiplier, MinSpeedMultiplier, 1.0f);
}

// ---------------------------------------------------------------- consultas

bool FInventoryModel::HasItemWithTag(FName Tag, bool bIncludeSledge) const
{
	bool bFound = false;
	InventoryModelDetail::ForEachCarried(State, bIncludeSledge, [&bFound, Tag](const FInventoryItem& Item, EInventorySlot) { bFound = bFound || Item.HasTag(Tag); });
	InventoryModelDetail::ForEachEquipped(State, bIncludeSledge, [&bFound, Tag](const FInventoryItem& Item) { bFound = bFound || Item.HasTag(Tag); });
	return bFound;
}

bool FInventoryModel::HasItemWithTagAtHand(FName Tag) const
{
	return State.HandLeft.HasTag(Tag) || State.HandRight.HasTag(Tag)
		|| State.Pockets.HasItemWithTag(Tag) || State.Belt.HasItemWithTag(Tag);
}

bool FInventoryModel::HasCompassAtHand() const
{
	return HasItemWithTagAtHand(FName(TEXT("brujula")));
}

bool FInventoryModel::IsStoredDry(int64 InstanceId) const
{
	const FInventoryContainer* Container = GetContainer(FindItem(InstanceId));
	return Container && Container->Spec.bWaterproof;
}

bool FInventoryModel::AreTaggedItemsDry(FName Tag) const
{
	bool bAllDry = true;
	InventoryModelDetail::ForEachCarried(State, /*bIncludeSledge=*/true, [this, &bAllDry, Tag](const FInventoryItem& Item, EInventorySlot Slot)
	{
		if (Item.HasTag(Tag))
		{
			const FInventoryContainer* Container = GetContainer(Slot);
			bAllDry = bAllDry && Container && Container->Spec.bWaterproof;
		}
	});
	return bAllDry;
}

float FInventoryModel::GetCarriedWaterLiters() const
{
	float Total = 0.0f;
	InventoryModelDetail::ForEachCarried(State, /*bIncludeSledge=*/true, [&Total](const FInventoryItem& Item, EInventorySlot) { Total += Item.LiquidLiters; });
	return Total;
}

float FInventoryModel::FillLiquid(int64 InstanceId, float Liters, EInventoryFail& OutFail)
{
	const EInventorySlot Slot = FindItem(InstanceId);
	FInventoryItem* Item = FindMutableItemById(InstanceId);
	if (!Item)
	{
		OutFail = EInventoryFail::NotFound;
		return 0.0f;
	}
	if (Item->LiquidCapacityLiters <= 0.0f)
	{
		OutFail = EInventoryFail::NotALiquidContainer;
		return 0.0f;
	}
	float Added = FMath::Clamp(Liters, 0.0f, Item->LiquidCapacityLiters - Item->LiquidLiters);
	if (const FInventoryContainer* Container = GetContainer(Slot))
	{
		if (Container->Spec.MaxWeightKg > 0.0f)
		{
			// El agua pesa: dentro de la mochila no se llena por encima de su límite.
			Added = FMath::Clamp(Added, 0.0f, Container->Spec.MaxWeightKg - Container->GetUsedWeightKg());
		}
	}
	Item->LiquidLiters += Added;
	if (State.bHandsHoldTwoHanded && State.HandLeft.InstanceId == InstanceId)
	{
		State.HandRight = State.HandLeft;
	}
	OutFail = Added > 0.0f ? EInventoryFail::None : EInventoryFail::NoRoom;
	return Added;
}

float FInventoryModel::DrinkFrom(int64 InstanceId, float Liters)
{
	FInventoryItem* Item = FindMutableItemById(InstanceId);
	if (!Item)
	{
		return 0.0f;
	}
	const float Drunk = FMath::Clamp(Liters, 0.0f, Item->LiquidLiters);
	Item->LiquidLiters -= Drunk;
	if (State.bHandsHoldTwoHanded && State.HandLeft.InstanceId == InstanceId)
	{
		State.HandRight = State.HandLeft;
	}
	return Drunk;
}

// ---------------------------------------------------------------- guardado

bool FInventoryModel::ValidateState(const FInventoryState& InState, EInventoryFail& OutFail)
{
	FInventoryState S = InState;
	RebuildSpecs(S);
	OutFail = EInventoryFail::CorruptState;

	// Manos.
	if (S.bHandsHoldTwoHanded)
	{
		if (!S.HandLeft.IsValid() || !S.HandLeft.IsTwoHanded() || S.HandLeft != S.HandRight)
		{
			return false;
		}
	}
	else if ((S.HandLeft.IsValid() && S.HandLeft.IsTwoHanded()) || (S.HandRight.IsValid() && S.HandRight.IsTwoHanded()))
	{
		return false;
	}

	// Equipo.
	FInventoryEquipmentSpec Spec;
	if (S.BackpackItem.IsValid() && (!S.bHasBackpack || !FindEquipmentSpec(S.BackpackItem, Spec) || Spec.Kind != EInventoryEquipment::Backpack))
	{
		return false;
	}
	if (S.BeltItem.IsValid() && (!FindEquipmentSpec(S.BeltItem, Spec) || Spec.Kind != EInventoryEquipment::Belt))
	{
		return false;
	}
	if (S.bHasSledge && (!S.SledgeItem.IsValid() || !FindEquipmentSpec(S.SledgeItem, Spec) || Spec.Kind != EInventoryEquipment::Sledge))
	{
		return false;
	}
	if ((!S.bHasBackpack && !S.Backpack.IsEmpty()) || (!S.bHasSledge && !S.Sledge.IsEmpty())
		|| (!StateProvidesPouch(S) && !S.Pouch.IsEmpty()))
	{
		return false;
	}

	// Contenido: cada contenedor tiene que caber en su capacidad y sus huecos no se repiten.
	const FInventoryContainer* Containers[] = { &S.Pockets, &S.Belt, &S.Pouch, &S.Backpack, &S.Sledge };
	for (const FInventoryContainer* Container : Containers)
	{
		FInventoryContainer Refilled;
		EInventoryFail Fail = EInventoryFail::None;
		if (!InventoryModelDetail::RefillInto(*Container, Container->Spec, Refilled, Fail))
		{
			OutFail = Fail == EInventoryFail::DuplicateId ? EInventoryFail::DuplicateId : EInventoryFail::CorruptState;
			return false;
		}
		TArray<int32> SlotsSeen;
		for (const FInventoryEntry& Entry : Container->Entries)
		{
			if (Entry.SlotIndex < 0 || SlotsSeen.Contains(Entry.SlotIndex)
				|| (Container->Spec.MaxSlots > 0 && Entry.SlotIndex >= Container->Spec.MaxSlots))
			{
				return false;
			}
			SlotsSeen.Add(Entry.SlotIndex);
		}
	}

	// Ids únicos y por debajo del siguiente que se va a repartir.
	TArray<int64> Ids;
	bool bDuplicate = false;
	auto Visit = [&Ids, &bDuplicate](const FInventoryItem& Item)
	{
		if (Ids.Contains(Item.InstanceId))
		{
			bDuplicate = true;
		}
		Ids.Add(Item.InstanceId);
	};
	InventoryModelDetail::ForEachCarried(S, /*bIncludeSledge=*/true, [&Visit](const FInventoryItem& Item, EInventorySlot) { Visit(Item); });
	InventoryModelDetail::ForEachEquipped(S, /*bIncludeSledge=*/true, Visit);
	if (bDuplicate)
	{
		OutFail = EInventoryFail::DuplicateId;
		return false;
	}
	for (int64 Id : Ids)
	{
		if (Id <= 0 || Id >= S.NextInstanceId)
		{
			return false;
		}
	}

	OutFail = EInventoryFail::None;
	return true;
}

bool FInventoryModel::LoadState(const FInventoryState& InState, EInventoryFail& OutFail)
{
	if (!ValidateState(InState, OutFail))
	{
		return false;
	}
	State = InState;
	RebuildSpecs(State);
	return true;
}
