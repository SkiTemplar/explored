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

	/**
	 * Etiquetas que nunca apilan (biblia 03 §1.3): equipo que se lleva puesto,
	 * contenedores (cada uno guarda cosas), piezas legendarias únicas y registros
	 * internos. Tools/DataCheck lee esta lista: mantener el formato TEXT("...").
	 */
	const TCHAR* const NonStackableTagNames[] = {
		TEXT("mochila"), TEXT("cinturon"), TEXT("angarillas"), TEXT("contenedor"), TEXT("legendario"), TEXT("interno"),
	};

	/** Unidades de tamaño Unit (kg o litros) que caben en Free, con la misma holgura que CanAccept. */
	int32 UnitsIn(float Free, float Unit)
	{
		if (Unit <= 0.0f)
		{
			return TNumericLimits<int32>::Max();
		}
		if (Free + CapacityTolerance < Unit)
		{
			return 0;
		}
		return FMath::Max(0, FMath::FloorToInt((Free + CapacityTolerance) / Unit));
	}

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
	case EInventoryFail::NotStackable: return TEXT("No se apilan juntos");
	case EInventoryFail::StackFull: return TEXT("La pila está llena");
	case EInventoryFail::InvalidCount: return TEXT("Cantidad no válida");
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
		&& LiquidCapacityLiters == Other.LiquidCapacityLiters
		&& Count == Other.Count
		&& MaxStack == Other.MaxStack
		&& Quality == Other.Quality;
}

bool FInventoryItem::CanStackWith(const FInventoryItem& Other) const
{
	constexpr float UnitTolerance = 1.0e-4f;
	return IsValid() && Other.IsValid()
		&& IsStackable() && Other.IsStackable()
		&& MaxStack == Other.MaxStack
		&& DefinitionId == Other.DefinitionId
		&& Quality == Other.Quality
		&& Size == Other.Size
		&& !IsTwoHanded()
		&& LiquidCapacityLiters <= 0.0f && Other.LiquidCapacityLiters <= 0.0f
		&& LiquidLiters <= 0.0f && Other.LiquidLiters <= 0.0f
		&& FMath::IsNearlyEqual(WeightKg, Other.WeightKg, UnitTolerance)
		&& FMath::IsNearlyEqual(VolumeLiters, Other.VolumeLiters, UnitTolerance)
		&& Tags == Other.Tags;
}

FInventoryItem FInventoryItem::WithCount(int32 NewCount) const
{
	FInventoryItem Copy = *this;
	Copy.Count = NewCount;
	return Copy;
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
		Total += Entry.Item.GetTotalVolumeLiters();
	}
	return Total;
}

EInventoryFail FInventoryContainer::CanAccept(const FInventoryItem& Item) const
{
	using namespace InventoryModelDetail;

	if (!Item.IsValid() || Item.Count < 1 || Item.Count > FMath::Max(Item.MaxStack, 1))
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
	if (Spec.MaxVolumeLiters > 0.0f && GetUsedVolumeLiters() + Item.GetTotalVolumeLiters() > Spec.MaxVolumeLiters + CapacityTolerance)
	{
		return EInventoryFail::NoRoom;
	}
	if (Spec.MaxWeightKg > 0.0f && GetUsedWeightKg() + Item.GetTotalWeightKg() > Spec.MaxWeightKg + CapacityTolerance)
	{
		return EInventoryFail::TooHeavy;
	}
	return EInventoryFail::None;
}

int32 FInventoryContainer::UnitsThatFitInStack(const FInventoryItem& Item, int64 IntoInstanceId) const
{
	using namespace InventoryModelDetail;

	const FInventoryItem* Into = FindById(IntoInstanceId);
	if (!Into || Into->InstanceId == Item.InstanceId || !Into->CanStackWith(Item))
	{
		return 0;
	}
	int32 Units = Into->GetStackRoom();
	if (Spec.MaxVolumeLiters > 0.0f)
	{
		Units = FMath::Min(Units, UnitsIn(Spec.MaxVolumeLiters - GetUsedVolumeLiters(), Item.VolumeLiters));
	}
	if (Spec.MaxWeightKg > 0.0f)
	{
		Units = FMath::Min(Units, UnitsIn(Spec.MaxWeightKg - GetUsedWeightKg(), Item.WeightKg));
	}
	return FMath::Max(Units, 0);
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

const TArray<FName>& FInventoryModel::GetNonStackableTags()
{
	static const TArray<FName> Tags = []()
	{
		TArray<FName> Out;
		for (const TCHAR* Name : InventoryModelDetail::NonStackableTagNames)
		{
			Out.Add(FName(Name));
		}
		return Out;
	}();
	return Tags;
}

int32 FInventoryModel::ComputeMaxStack(float MaxDurability, float LiquidCapacityLiters, EInventorySize Size, const TArray<FName>& Tags, bool bComposite)
{
	if (MaxDurability > 0.0f || LiquidCapacityLiters > 0.0f || Size == EInventorySize::DosManos || bComposite)
	{
		return 1;
	}
	for (const FName& Tag : GetNonStackableTags())
	{
		if (Tags.Contains(Tag))
		{
			return 1;
		}
	}
	return MaxStackSize;
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

bool FInventoryModel::PickUpMerging(const FInventoryItem& Item, int64& OutMergedInto, EInventoryFail& OutFail)
{
	OutMergedInto = 0;
	if (Item.IsValid() && Item.IsStackable() && FindItem(Item.InstanceId) == EInventorySlot::None)
	{
		for (EInventorySlot Hand : { EInventorySlot::HandLeft, EInventorySlot::HandRight })
		{
			const FInventoryItem* Held = GetHandItem(Hand);
			if (Held && UnitsThatFitOnPlayerStack(Item, Held->InstanceId, /*bAlreadyOnBody=*/false) >= Item.Count)
			{
				OutMergedInto = Held->InstanceId;
				CommitItem(Held->WithCount(Held->Count + Item.Count));
				State.NextInstanceId = FMath::Max(State.NextInstanceId, Item.InstanceId + 1);
				OutFail = EInventoryFail::None;
				return true;
			}
		}
	}
	return PickUp(Item, OutFail);
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

// ------------------------------------------------------------- pilas

void FInventoryModel::CommitItem(const FInventoryItem& Updated)
{
	FInventoryItem* Item = FindMutableItemById(Updated.InstanceId);
	check(Item);
	*Item = Updated;
	if (State.bHandsHoldTwoHanded && State.HandLeft.InstanceId == Updated.InstanceId)
	{
		State.HandRight = State.HandLeft;
	}
}

int32 FInventoryModel::UnitsThatFitOnPlayerStack(const FInventoryItem& Item, int64 IntoId, bool bAlreadyOnBody) const
{
	using namespace InventoryModelDetail;

	const EInventorySlot IntoSlot = FindItem(IntoId);
	const FInventoryItem* Into = FindItemById(IntoId);
	if (!Into || Into->InstanceId == Item.InstanceId || !Into->CanStackWith(Item))
	{
		return 0;
	}
	int32 Units = Into->GetStackRoom();
	const FInventoryContainer* Container = GetContainer(IntoSlot);
	// Dentro del mismo contenedor el volumen y el peso no cambian: solo manda el tope de la pila.
	if (Container && Container->FindIndexById(Item.InstanceId) == INDEX_NONE)
	{
		Units = FMath::Min(Units, Container->UnitsThatFitInStack(Item, IntoId));
	}
	if (IsBodySlot(IntoSlot) && !bAlreadyOnBody)
	{
		const float Free = GetComfortableCapacityKg() * MaxLoadRatio - GetBodyWeightKg();
		Units = FMath::Min(Units, UnitsIn(Free, Item.WeightKg));
	}
	return FMath::Max(Units, 0);
}

bool FInventoryModel::SplitStack(int64 InstanceId, int32 Count, EInventorySlot To, int64& OutNewId, EInventoryFail& OutFail)
{
	using namespace InventoryModelDetail;

	OutNewId = 0;
	const EInventorySlot From = FindItem(InstanceId);
	const FInventoryItem* Found = FindItemById(InstanceId);
	if (!Found)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	if (Count < 1 || Count >= Found->Count)
	{
		OutFail = EInventoryFail::InvalidCount;
		return false;
	}
	if (!IsHand(To) && !GetContainer(To))
	{
		OutFail = EInventoryFail::InvalidItem;
		return false;
	}

	// Se prueba sobre una copia: la pila que queda y la nueva comparten contenedor a veces.
	FInventoryModel Probe = *this;
	Probe.CommitItem(Found->WithCount(Found->Count - Count));
	FInventoryItem Part = Found->WithCount(Count);
	Part.InstanceId = Probe.State.NextInstanceId;
	OutFail = Probe.CanPlace(Part, To, /*bAlreadyOnBody=*/IsBodySlot(From));
	if (OutFail != EInventoryFail::None)
	{
		return false;
	}
	Probe.PlaceUnchecked(Part, To);
	++Probe.State.NextInstanceId;
	*this = MoveTemp(Probe);
	OutNewId = Part.InstanceId;
	return true;
}

bool FInventoryModel::MergeStacks(int64 FromId, int64 IntoId, int32& OutMoved, EInventoryFail& OutFail)
{
	using namespace InventoryModelDetail;

	OutMoved = 0;
	const FInventoryItem* From = FindItemById(FromId);
	const FInventoryItem* Into = FindItemById(IntoId);
	if (!From || !Into)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	if (FromId == IntoId)
	{
		OutFail = EInventoryFail::AlreadyThere;
		return false;
	}
	if (!Into->CanStackWith(*From))
	{
		OutFail = EInventoryFail::NotStackable;
		return false;
	}
	if (Into->GetStackRoom() == 0)
	{
		OutFail = EInventoryFail::StackFull;
		return false;
	}
	const EInventorySlot FromSlot = FindItem(FromId);
	const EInventorySlot IntoSlot = FindItem(IntoId);
	if (WouldOrphanPouch(FromId, FromSlot, IntoSlot))
	{
		OutFail = EInventoryFail::ContainerNotEmpty;
		return false;
	}
	const int32 Units = FMath::Min(From->Count, UnitsThatFitOnPlayerStack(*From, IntoId, /*bAlreadyOnBody=*/IsBodySlot(FromSlot)));
	if (Units <= 0)
	{
		// El motivo es el de meter una sola unidad más donde está la pila de destino.
		const FInventoryContainer* Container = GetContainer(IntoSlot);
		const EInventoryFail ContainerFail = Container ? Container->CanAccept(From->WithCount(1)) : EInventoryFail::None;
		OutFail = (ContainerFail == EInventoryFail::NoRoom || ContainerFail == EInventoryFail::TooHeavy) ? ContainerFail : EInventoryFail::OverCarryLimit;
		return false;
	}

	const FInventoryItem FromCopy = *From;
	CommitItem(Into->WithCount(Into->Count + Units));
	if (Units == FromCopy.Count)
	{
		FInventoryItem Removed;
		RemoveUnchecked(FromId, Removed);
	}
	else
	{
		CommitItem(FromCopy.WithCount(FromCopy.Count - Units));
	}
	OutMoved = Units;
	OutFail = EInventoryFail::None;
	return true;
}

bool FInventoryModel::StowMerging(int64 InstanceId, EInventorySlot To, FInventoryStowResult& OutResult, EInventoryFail& OutFail)
{
	using namespace InventoryModelDetail;

	OutResult = FInventoryStowResult();
	const EInventorySlot From = FindItem(InstanceId);
	const FInventoryItem* Found = FindItemById(InstanceId);
	if (!Found)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	const FInventoryContainer* Target = GetContainer(To);
	if (!Found->IsStackable() || !Target)
	{
		// Lo que no apila (o va a una mano) se mueve entero.
		const int32 Units = Found->Count;
		if (!Move(InstanceId, To, OutFail))
		{
			return false;
		}
		OutResult.MovedUnits = Units;
		OutResult.bMovedWhole = true;
		return true;
	}
	if (From == To)
	{
		OutFail = EInventoryFail::AlreadyThere;
		return false;
	}
	if (WouldOrphanPouch(InstanceId, From, To))
	{
		OutFail = EInventoryFail::ContainerNotEmpty;
		return false;
	}
	// Mismas condiciones que un traslado (mochila puesta, bolsa, angarillas, tamaño y
	// etiquetas); la falta de hueco, volumen o peso aún puede salvarse fundiendo.
	const EInventoryFail Gate = CanPlace(Found->WithCount(1), To, /*bAlreadyOnBody=*/true);
	if (Gate != EInventoryFail::None && Gate != EInventoryFail::ContainerFull
		&& Gate != EInventoryFail::NoRoom && Gate != EInventoryFail::TooHeavy)
	{
		OutFail = Gate;
		return false;
	}

	const bool bAlreadyOnBody = IsBodySlot(From);
	FInventoryModel Probe = *this;
	int32 Remaining = Found->Count;
	OutFail = EInventoryFail::None;

	// 1) Completar las pilas que ya hay, por orden de hueco visible.
	TArray<FInventoryEntry> Sorted = Target->Entries;
	Sorted.StableSort([](const FInventoryEntry& A, const FInventoryEntry& B) { return A.SlotIndex < B.SlotIndex; });
	for (const FInventoryEntry& Entry : Sorted)
	{
		if (Remaining == 0)
		{
			break;
		}
		const FInventoryItem Source = *Probe.FindItemById(InstanceId);
		const int32 Units = FMath::Min(Remaining, Probe.UnitsThatFitOnPlayerStack(Source, Entry.Item.InstanceId, bAlreadyOnBody));
		if (Units <= 0)
		{
			continue;
		}
		const FInventoryItem Into = *Probe.FindItemById(Entry.Item.InstanceId);
		Probe.CommitItem(Into.WithCount(Into.Count + Units));
		Remaining -= Units;
		if (Remaining > 0)
		{
			Probe.CommitItem(Source.WithCount(Remaining));
		}
		OutResult.ToppedUp.Add(TPair<int64, int32>(Entry.Item.InstanceId, Units));
		OutResult.MovedUnits += Units;
	}

	if (Remaining == 0)
	{
		FInventoryItem Removed;
		Probe.RemoveUnchecked(InstanceId, Removed);
		OutResult.bSourceRemoved = true;
	}
	else
	{
		// 2) Lo que sobra, a un hueco nuevo: entero (conserva el id) o, si no cabe, la parte que quepa.
		const FInventoryItem Rest = *Probe.FindItemById(InstanceId);
		OutFail = Probe.CanPlace(Rest, To, bAlreadyOnBody);
		if (OutFail == EInventoryFail::None)
		{
			FInventoryItem Removed;
			Probe.RemoveUnchecked(InstanceId, Removed);
			Probe.PlaceUnchecked(Rest, To);
			OutResult.bMovedWhole = true;
			OutResult.MovedUnits += Rest.Count;
			Remaining = 0;
		}
		else if (OutFail != EInventoryFail::ContainerFull)
		{
			for (int32 Part = Rest.Count - 1; Part >= 1; --Part)
			{
				FInventoryItem Piece = Rest.WithCount(Part);
				Piece.InstanceId = Probe.State.NextInstanceId;
				FInventoryModel Try = Probe;
				Try.CommitItem(Rest.WithCount(Rest.Count - Part));
				if (Try.CanPlace(Piece, To, bAlreadyOnBody) == EInventoryFail::None)
				{
					Try.PlaceUnchecked(Piece, To);
					++Try.State.NextInstanceId;
					Probe = MoveTemp(Try);
					OutResult.NewStackId = Piece.InstanceId;
					OutResult.MovedUnits += Part;
					Remaining -= Part;
					break;
				}
			}
		}
	}

	if (OutResult.MovedUnits == 0)
	{
		if (OutFail == EInventoryFail::None)
		{
			OutFail = EInventoryFail::NoRoom;
		}
		OutResult = FInventoryStowResult();
		return false;
	}
	OutResult.LeftAtSource = Remaining;
	*this = MoveTemp(Probe);
	OutFail = EInventoryFail::None;
	return true;
}

bool FInventoryModel::RemoveUnits(int64 InstanceId, int32 Count, EInventoryFail& OutFail)
{
	const FInventoryItem* Found = FindItemById(InstanceId);
	if (!Found)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	if (Count < 1 || Count > Found->Count)
	{
		OutFail = EInventoryFail::InvalidCount;
		return false;
	}
	if (Count == Found->Count)
	{
		FInventoryItem Removed;
		return ConsumeItem(InstanceId, Removed, OutFail);
	}
	CommitItem(Found->WithCount(Found->Count - Count));
	OutFail = EInventoryFail::None;
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

bool FInventoryModel::AutoStowMergingFromHand(EInventorySlot Hand, EInventorySlot& OutWhere, FInventoryStowResult& OutResult, EInventoryFail& OutFail)
{
	OutWhere = EInventorySlot::None;
	OutResult = FInventoryStowResult();
	const FInventoryItem* HeldPtr = GetHandItem(Hand);
	if (!HeldPtr)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	const FInventoryItem Held = *HeldPtr;

	// Primero donde ya hay una pila igual con sitio: así no se gasta un hueco nuevo.
	EInventorySlot Target = EInventorySlot::None;
	if (Held.IsStackable())
	{
		const EInventorySlot Slots[] = { EInventorySlot::Pockets, EInventorySlot::Pouch, EInventorySlot::Belt, EInventorySlot::Backpack, EInventorySlot::Sledge };
		for (EInventorySlot Slot : Slots)
		{
			const EInventoryFail Gate = CanPlace(Held.WithCount(1), Slot, /*bAlreadyOnBody=*/true);
			if (Gate != EInventoryFail::None && Gate != EInventoryFail::ContainerFull
				&& Gate != EInventoryFail::NoRoom && Gate != EInventoryFail::TooHeavy)
			{
				continue;
			}
			const bool bHasRoomyStack = GetContainer(Slot)->Entries.ContainsByPredicate([this, &Held](const FInventoryEntry& E)
			{
				return UnitsThatFitOnPlayerStack(Held, E.Item.InstanceId, /*bAlreadyOnBody=*/true) > 0;
			});
			if (bHasRoomyStack)
			{
				Target = Slot;
				break;
			}
		}
	}
	if (Target == EInventorySlot::None)
	{
		Target = SuggestStowSlot(Held);
	}
	if (Target == EInventorySlot::None)
	{
		OutFail = Held.IsTwoHanded() ? EInventoryFail::TooBig : EInventoryFail::NoRoom;
		return false;
	}
	if (!StowMerging(Held.InstanceId, Target, OutResult, OutFail))
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

bool FInventoryModel::ConsumeItem(int64 InstanceId, FInventoryItem& OutItem, EInventoryFail& OutFail)
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
	if (!RemoveUnchecked(InstanceId, OutItem))
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	OutFail = EInventoryFail::None;
	return true;
}

bool FInventoryModel::ShrinkItem(const FInventoryItem& Updated, EInventoryFail& OutFail)
{
	FInventoryItem* Item = FindMutableItemById(Updated.InstanceId);
	if (!Item)
	{
		OutFail = EInventoryFail::NotFound;
		return false;
	}
	if (!Updated.IsValid() || Updated.DefinitionId != Item->DefinitionId || Updated.Size != Item->Size
		|| Updated.Count < 1 || Updated.Count > FMath::Max(Updated.MaxStack, 1))
	{
		OutFail = EInventoryFail::InvalidItem;
		return false;
	}
	if (Updated.GetTotalWeightKg() > Item->GetTotalWeightKg() + UE_KINDA_SMALL_NUMBER ||
		Updated.VolumeLiters > Item->VolumeLiters + UE_KINDA_SMALL_NUMBER)
	{
		OutFail = EInventoryFail::TooHeavy;
		return false;
	}
	*Item = Updated;
	if (State.bHandsHoldTwoHanded && State.HandLeft.InstanceId == Updated.InstanceId)
	{
		State.HandRight = State.HandLeft;
	}
	OutFail = EInventoryFail::None;
	return true;
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
	bool bBadStack = false;
	auto Visit = [&Ids, &bDuplicate, &bBadStack](const FInventoryItem& Item)
	{
		if (Ids.Contains(Item.InstanceId))
		{
			bDuplicate = true;
		}
		Ids.Add(Item.InstanceId);
		// Pilas (biblia 03 §1.3): 1..MaxStack unidades y nada con líquido ni DosManos por encima de 1.
		const bool bStackRange = Item.MaxStack >= 1 && Item.MaxStack <= MaxStackSize && Item.Count >= 1 && Item.Count <= Item.MaxStack;
		const bool bStackKind = Item.MaxStack == 1 || (!Item.IsTwoHanded() && Item.LiquidCapacityLiters <= 0.0f && Item.LiquidLiters <= 0.0f);
		bBadStack = bBadStack || !bStackRange || !bStackKind;
	};
	InventoryModelDetail::ForEachCarried(S, /*bIncludeSledge=*/true, [&Visit](const FInventoryItem& Item, EInventorySlot) { Visit(Item); });
	InventoryModelDetail::ForEachEquipped(S, /*bIncludeSledge=*/true, [&Visit, &bBadStack](const FInventoryItem& Item)
	{
		Visit(Item);
		// El equipo puesto es una pieza: nunca una pila.
		bBadStack = bBadStack || Item.Count != 1;
	});
	if (bDuplicate)
	{
		OutFail = EInventoryFail::DuplicateId;
		return false;
	}
	if (bBadStack)
	{
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

bool FInventoryModel::SanitizeUnknownDefinitions(FInventoryState& InOut, TFunctionRef<bool(FName)> IsKnown, FInventoryLoadReport& OutReport)
{
	using namespace InventoryModelDetail;

	OutReport = FInventoryLoadReport();
	auto IsUnknown = [&IsKnown](const FInventoryItem& Item) { return Item.IsValid() && !IsKnown(Item.DefinitionId); };
	auto Orphan = [&OutReport](FInventoryContainer& Container)
	{
		Container.Entries.StableSort([](const FInventoryEntry& A, const FInventoryEntry& B) { return A.SlotIndex < B.SlotIndex; });
		for (const FInventoryEntry& Entry : Container.Entries)
		{
			OutReport.Orphaned.Add(Entry.Item);
		}
		Container.Entries.Reset();
	};

	// 1) Manos (un DosManos desconocido libera las dos).
	if (IsUnknown(InOut.HandLeft))
	{
		OutReport.Unknown.Add(InOut.HandLeft);
		if (InOut.bHandsHoldTwoHanded)
		{
			InOut.HandRight = FInventoryItem();
			InOut.bHandsHoldTwoHanded = false;
		}
		InOut.HandLeft = FInventoryItem();
	}
	if (!InOut.bHandsHoldTwoHanded && IsUnknown(InOut.HandRight))
	{
		OutReport.Unknown.Add(InOut.HandRight);
		InOut.HandRight = FInventoryItem();
	}

	// 2) Contenido: lo desconocido sale; el resto conserva su hueco visible.
	for (FInventoryContainer* Container : { &InOut.Pockets, &InOut.Belt, &InOut.Pouch, &InOut.Backpack, &InOut.Sledge })
	{
		for (int32 Index = Container->Entries.Num() - 1; Index >= 0; --Index)
		{
			if (IsUnknown(Container->Entries[Index].Item))
			{
				OutReport.Unknown.Insert(Container->Entries[Index].Item, 0);
				Container->Entries.RemoveAt(Index);
			}
		}
	}

	// 3) Equipo desconocido: se quita y lo que llevaba cae al suelo.
	if (IsUnknown(InOut.BackpackItem))
	{
		OutReport.Unknown.Add(InOut.BackpackItem);
		InOut.BackpackItem = FInventoryItem();
		InOut.bHasBackpack = false;
		InOut.BackpackComfortBonusKg = 0.0f;
		InOut.bBackpackWaterproofPocket = false;
		Orphan(InOut.Backpack);
	}
	if (IsUnknown(InOut.BeltItem))
	{
		OutReport.Unknown.Add(InOut.BeltItem);
		InOut.BeltItem = FInventoryItem();
	}
	if (IsUnknown(InOut.SledgeItem))
	{
		OutReport.Unknown.Add(InOut.SledgeItem);
		InOut.SledgeItem = FInventoryItem();
		InOut.bHasSledge = false;
		Orphan(InOut.Sledge);
	}
	RebuildSpecs(InOut);

	// El cinturón puede haber vuelto a sus enganches básicos: lo que sobra, al suelo.
	InOut.Belt.Entries.StableSort([](const FInventoryEntry& A, const FInventoryEntry& B) { return A.SlotIndex < B.SlotIndex; });
	while (InOut.Belt.Num() > InOut.Belt.Spec.MaxSlots)
	{
		OutReport.Orphaned.Add(InOut.Belt.Entries.Last().Item);
		InOut.Belt.Entries.Pop();
	}
	if (InOut.Belt.Entries.ContainsByPredicate([&InOut](const FInventoryEntry& E) { return E.SlotIndex >= InOut.Belt.Spec.MaxSlots; }))
	{
		CompactSlots(InOut.Belt);
	}
	// Sin mochila impermeable ni bolsa colgada no hay bolsa estanca.
	if (!StateProvidesPouch(InOut) && !InOut.Pouch.IsEmpty())
	{
		Orphan(InOut.Pouch);
	}
	return !OutReport.IsClean();
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
