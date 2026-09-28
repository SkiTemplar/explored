#include "Carry/InventoryNetModel.h"

namespace InventoryNetModelDetail
{
	constexpr uint8 KnownFlags = InventoryNetFlags::TwoHanded | InventoryNetFlags::LiquidInCount | InventoryNetFlags::ExtraMask;

	void SortById(TArray<FInventoryNetEntry>& Entries)
	{
		Entries.Sort([](const FInventoryNetEntry& A, const FInventoryNetEntry& B) { return A.InstanceId < B.InstanceId; });
	}

	void PutU16(TArray<uint8>& Out, uint16 V)
	{
		Out.Add(static_cast<uint8>(V & 0xFF));
		Out.Add(static_cast<uint8>(V >> 8));
	}

	void PutU32(TArray<uint8>& Out, uint32 V)
	{
		for (int32 I = 0; I < 4; ++I)
		{
			Out.Add(static_cast<uint8>((V >> (8 * I)) & 0xFF));
		}
	}

	uint16 GetU16(const uint8* Data) { return static_cast<uint16>(Data[0] | (Data[1] << 8)); }

	uint32 GetU32(const uint8* Data)
	{
		return static_cast<uint32>(Data[0]) | (static_cast<uint32>(Data[1]) << 8)
			| (static_cast<uint32>(Data[2]) << 16) | (static_cast<uint32>(Data[3]) << 24);
	}
}

// --------------------------------------------------------------------------- tipos

bool FInventoryNetEntry::operator==(const FInventoryNetEntry& Other) const
{
	return Slot == Other.Slot && SlotIndex == Other.SlotIndex && DefinitionId == Other.DefinitionId
		&& InstanceId == Other.InstanceId && Quality01x255 == Other.Quality01x255
		&& Durability01x255 == Other.Durability01x255 && Count == Other.Count && Flags == Other.Flags;
}

bool FInventoryNetHands::operator==(const FInventoryNetHands& Other) const
{
	return LeftDefinitionId == Other.LeftDefinitionId && LeftQuality01x255 == Other.LeftQuality01x255
		&& RightDefinitionId == Other.RightDefinitionId && RightQuality01x255 == Other.RightQuality01x255;
}

int32 FInventoryNetDelta::GetPayloadBytes() const
{
	return Changed.Num() * FInventoryNetModel::EntryBytes + Removed.Num() * FInventoryNetModel::RemovedIdBytes;
}

// --------------------------------------------------------------------------- cuantización

uint8 FInventoryNetModel::QualityToByte(uint8 Quality)
{
	const int32 Q = FMath::Clamp(static_cast<int32>(Quality), 1, 5);
	return static_cast<uint8>(FMath::RoundToInt(static_cast<float>(Q - 1) * 255.0f / 4.0f));
}

uint8 FInventoryNetModel::ByteToQuality(uint8 Byte)
{
	return static_cast<uint8>(FMath::RoundToInt(static_cast<float>(Byte) * 4.0f / 255.0f) + 1);
}

uint8 FInventoryNetModel::Durability01ToByte(float Durability01)
{
	// NaN o negativo: roto (0). Así un valor corrupto nunca parece una herramienta nueva.
	if (!(Durability01 > 0.0f))
	{
		return 0;
	}
	return static_cast<uint8>(FMath::RoundToInt(FMath::Min(Durability01, 1.0f) * 255.0f));
}

float FInventoryNetModel::ByteToDurability01(uint8 Byte)
{
	return static_cast<float>(Byte) / 255.0f;
}

uint8 FInventoryNetModel::LitersToCentiliters(float Liters)
{
	if (!(Liters > 0.0f))
	{
		return 0;
	}
	return static_cast<uint8>(FMath::Min(FMath::RoundToInt(Liters * 100.0f), 255));
}

// --------------------------------------------------------------------------- estado

void FInventoryNetModel::BuildSnapshot(const FInventoryState& State, const FContentIdTableModel& Table,
	const TMap<int64, FInventoryNetExtras>& Extras, TArray<FInventoryNetEntry>& OutEntries, TArray<FInventoryNetSkip>& OutSkipped)
{
	OutEntries.Reset();
	OutSkipped.Reset();

	auto Add = [&](const FInventoryItem& Item, EInventoryNetSlot Slot, int32 SlotIndex, uint8 Flags)
	{
		const uint16 NetDefinition = Table.ToNetId(EContentKind::Item, Item.DefinitionId);
		if (NetDefinition == FContentIdTableModel::InvalidNetId || Item.InstanceId <= 0
			|| Item.InstanceId > static_cast<int64>(MAX_uint32) || SlotIndex < 0 || SlotIndex > 255)
		{
			OutSkipped.Add({ Item.InstanceId, Item.DefinitionId });
			return;
		}
		FInventoryNetEntry Entry;
		Entry.Slot = static_cast<uint8>(Slot);
		Entry.SlotIndex = static_cast<uint8>(SlotIndex);
		Entry.DefinitionId = NetDefinition;
		Entry.InstanceId = static_cast<uint32>(Item.InstanceId);
		Entry.Quality01x255 = QualityToByte(Item.Quality);
		Entry.Flags = Flags;
		if (const FInventoryNetExtras* Extra = Extras.Find(Item.InstanceId))
		{
			Entry.Durability01x255 = Durability01ToByte(Extra->Durability01);
			Entry.Flags |= Extra->Flags & InventoryNetFlags::ExtraMask;
		}
		if (Item.LiquidCapacityLiters > 0.0f)
		{
			Entry.Count = LitersToCentiliters(Item.LiquidLiters);
			Entry.Flags |= InventoryNetFlags::LiquidInCount;
		}
		else
		{
			Entry.Count = static_cast<uint8>(FMath::Clamp(Item.Count, 1, 255));
		}
		OutEntries.Add(Entry);
	};

	if (State.HandLeft.IsValid())
	{
		Add(State.HandLeft, EInventoryNetSlot::HandLeft, 0, State.bHandsHoldTwoHanded ? InventoryNetFlags::TwoHanded : 0);
	}
	if (State.HandRight.IsValid() && !State.bHandsHoldTwoHanded)
	{
		Add(State.HandRight, EInventoryNetSlot::HandRight, 0, 0);
	}
	const TPair<const FInventoryContainer*, EInventoryNetSlot> Containers[] = {
		{ &State.Pockets, EInventoryNetSlot::Pockets }, { &State.Belt, EInventoryNetSlot::Belt },
		{ &State.Pouch, EInventoryNetSlot::Pouch }, { &State.Backpack, EInventoryNetSlot::Backpack },
		{ &State.Sledge, EInventoryNetSlot::Sledge } };
	for (const TPair<const FInventoryContainer*, EInventoryNetSlot>& Container : Containers)
	{
		for (const FInventoryEntry& Entry : Container.Key->Entries)
		{
			Add(Entry.Item, Container.Value, Entry.SlotIndex, 0);
		}
	}
	if (State.bHasBackpack && State.BackpackItem.IsValid())
	{
		Add(State.BackpackItem, EInventoryNetSlot::EquippedBackpack, 0, 0);
	}
	if (State.BeltItem.IsValid())
	{
		Add(State.BeltItem, EInventoryNetSlot::EquippedBelt, 0, 0);
	}
	if (State.bHasSledge && State.SledgeItem.IsValid())
	{
		Add(State.SledgeItem, EInventoryNetSlot::EquippedSledge, 0, 0);
	}
	InventoryNetModelDetail::SortById(OutEntries);
}

FInventoryNetHands FInventoryNetModel::BuildHands(const FInventoryState& State, const FContentIdTableModel& Table)
{
	FInventoryNetHands Hands;
	if (State.HandLeft.IsValid())
	{
		Hands.LeftDefinitionId = Table.ToNetId(EContentKind::Item, State.HandLeft.DefinitionId);
		Hands.LeftQuality01x255 = QualityToByte(State.HandLeft.Quality);
	}
	// Un DosManos se ve en las dos manos: la malla lo sabe por la definición repetida.
	if (State.HandRight.IsValid())
	{
		Hands.RightDefinitionId = Table.ToNetId(EContentKind::Item, State.HandRight.DefinitionId);
		Hands.RightQuality01x255 = QualityToByte(State.HandRight.Quality);
	}
	return Hands;
}

FInventoryNetDelta FInventoryNetModel::Diff(const TArray<FInventoryNetEntry>& Old, const TArray<FInventoryNetEntry>& New)
{
	FInventoryNetDelta Delta;
	TMap<uint32, const FInventoryNetEntry*> OldById;
	for (const FInventoryNetEntry& Entry : Old)
	{
		OldById.Add(Entry.InstanceId, &Entry);
	}
	TSet<uint32> NewIds;
	for (const FInventoryNetEntry& Entry : New)
	{
		NewIds.Add(Entry.InstanceId);
		const FInventoryNetEntry* const* Before = OldById.Find(Entry.InstanceId);
		if (!Before || **Before != Entry)
		{
			Delta.Changed.Add(Entry);
		}
	}
	for (const FInventoryNetEntry& Entry : Old)
	{
		if (!NewIds.Contains(Entry.InstanceId))
		{
			Delta.Removed.Add(Entry.InstanceId);
		}
	}
	InventoryNetModelDetail::SortById(Delta.Changed);
	Delta.Removed.Sort();
	return Delta;
}

bool FInventoryNetModel::ApplyDelta(TArray<FInventoryNetEntry>& InOut, const FInventoryNetDelta& Delta)
{
	TSet<uint32> Present;
	for (const FInventoryNetEntry& Entry : InOut)
	{
		Present.Add(Entry.InstanceId);
	}
	TSet<uint32> Touched;
	for (const FInventoryNetEntry& Entry : Delta.Changed)
	{
		if (Entry.InstanceId == 0 || Touched.Contains(Entry.InstanceId))
		{
			return false;
		}
		Touched.Add(Entry.InstanceId);
	}
	for (uint32 Id : Delta.Removed)
	{
		if (!Present.Contains(Id) || Touched.Contains(Id))
		{
			return false;
		}
		Touched.Add(Id);
	}

	InOut.RemoveAll([&Delta](const FInventoryNetEntry& E) { return Delta.Removed.Contains(E.InstanceId); });
	for (const FInventoryNetEntry& Entry : Delta.Changed)
	{
		FInventoryNetEntry* Existing = InOut.FindByPredicate([&Entry](const FInventoryNetEntry& E) { return E.InstanceId == Entry.InstanceId; });
		if (Existing)
		{
			*Existing = Entry;
		}
		else
		{
			InOut.Add(Entry);
		}
	}
	InventoryNetModelDetail::SortById(InOut);
	return true;
}

// --------------------------------------------------------------------------- bytes

void FInventoryNetModel::EncodeEntry(const FInventoryNetEntry& Entry, TArray<uint8>& Out)
{
	using namespace InventoryNetModelDetail;
	Out.Add(Entry.Slot);
	Out.Add(Entry.SlotIndex);
	PutU16(Out, Entry.DefinitionId);
	PutU32(Out, Entry.InstanceId);
	Out.Add(Entry.Quality01x255);
	Out.Add(Entry.Durability01x255);
	Out.Add(Entry.Count);
	Out.Add(Entry.Flags);
}

bool FInventoryNetModel::DecodeEntry(const uint8* Data, int32 Num, FInventoryNetEntry& Out)
{
	using namespace InventoryNetModelDetail;
	if (!Data || Num < EntryBytes)
	{
		return false;
	}
	FInventoryNetEntry Entry;
	Entry.Slot = Data[0];
	Entry.SlotIndex = Data[1];
	Entry.DefinitionId = GetU16(Data + 2);
	Entry.InstanceId = GetU32(Data + 4);
	Entry.Quality01x255 = Data[8];
	Entry.Durability01x255 = Data[9];
	Entry.Count = Data[10];
	Entry.Flags = Data[11];
	const bool bLiquid = (Entry.Flags & InventoryNetFlags::LiquidInCount) != 0;
	if (Entry.Slot == 0 || Entry.Slot >= static_cast<uint8>(EInventoryNetSlot::Count)
		|| Entry.DefinitionId == FContentIdTableModel::InvalidNetId || Entry.InstanceId == 0
		|| (Entry.Flags & ~KnownFlags) != 0
		|| (!bLiquid && (Entry.Count == 0 || Entry.Count > FInventoryModel::MaxStackSize))
		|| ((Entry.Flags & InventoryNetFlags::TwoHanded) != 0 && Entry.Slot != static_cast<uint8>(EInventoryNetSlot::HandLeft)))
	{
		return false;
	}
	Out = Entry;
	return true;
}

void FInventoryNetModel::EncodeHands(const FInventoryNetHands& Hands, TArray<uint8>& Out)
{
	using namespace InventoryNetModelDetail;
	PutU16(Out, Hands.LeftDefinitionId);
	Out.Add(Hands.LeftQuality01x255);
	PutU16(Out, Hands.RightDefinitionId);
	Out.Add(Hands.RightQuality01x255);
}

bool FInventoryNetModel::DecodeHands(const uint8* Data, int32 Num, FInventoryNetHands& Out)
{
	using namespace InventoryNetModelDetail;
	if (!Data || Num != HandsBytes)
	{
		return false;
	}
	Out.LeftDefinitionId = GetU16(Data);
	Out.LeftQuality01x255 = Data[2];
	Out.RightDefinitionId = GetU16(Data + 3);
	Out.RightQuality01x255 = Data[5];
	return true;
}

TArray<uint8> FInventoryNetModel::EncodeDelta(const FInventoryNetDelta& Delta)
{
	using namespace InventoryNetModelDetail;
	check(Delta.Changed.Num() <= MAX_uint16 && Delta.Removed.Num() <= MAX_uint16);
	TArray<uint8> Out;
	Out.Reserve(DeltaHeaderBytes + Delta.GetPayloadBytes());
	PutU16(Out, static_cast<uint16>(Delta.Changed.Num()));
	PutU16(Out, static_cast<uint16>(Delta.Removed.Num()));
	for (const FInventoryNetEntry& Entry : Delta.Changed)
	{
		EncodeEntry(Entry, Out);
	}
	for (uint32 Id : Delta.Removed)
	{
		PutU32(Out, Id);
	}
	return Out;
}

bool FInventoryNetModel::DecodeDelta(const TArray<uint8>& Bytes, FInventoryNetDelta& Out)
{
	using namespace InventoryNetModelDetail;
	if (Bytes.Num() < DeltaHeaderBytes)
	{
		return false;
	}
	const int32 NumChanged = GetU16(Bytes.GetData());
	const int32 NumRemoved = GetU16(Bytes.GetData() + 2);
	if (Bytes.Num() != DeltaHeaderBytes + NumChanged * EntryBytes + NumRemoved * RemovedIdBytes)
	{
		return false;
	}
	FInventoryNetDelta Delta;
	int32 Offset = DeltaHeaderBytes;
	for (int32 I = 0; I < NumChanged; ++I, Offset += EntryBytes)
	{
		FInventoryNetEntry Entry;
		if (!DecodeEntry(Bytes.GetData() + Offset, EntryBytes, Entry))
		{
			return false;
		}
		Delta.Changed.Add(Entry);
	}
	for (int32 I = 0; I < NumRemoved; ++I, Offset += RemovedIdBytes)
	{
		const uint32 Id = GetU32(Bytes.GetData() + Offset);
		if (Id == 0)
		{
			return false;
		}
		Delta.Removed.Add(Id);
	}
	Out = MoveTemp(Delta);
	return true;
}

// --------------------------------------------------------------------------- coalescencia

bool FInventoryNetCoalescer::Tick(float DeltaSeconds, const TArray<FInventoryNetEntry>& Current, FInventoryNetDelta& OutDelta)
{
	OutDelta = FInventoryNetDelta();
	if (DeltaSeconds > 0.0f)
	{
		Accumulated += DeltaSeconds;
	}
	if (Accumulated < IntervalSeconds)
	{
		return false;
	}
	// Un fotograma largo no dispara varios envíos seguidos.
	Accumulated = FMath::Min(Accumulated - IntervalSeconds, IntervalSeconds * 0.5f);
	OutDelta = FInventoryNetModel::Diff(LastSent, Current);
	if (OutDelta.IsEmpty())
	{
		return false;
	}
	LastSent = Current;
	InventoryNetModelDetail::SortById(LastSent);
	return true;
}

FInventoryNetDelta FInventoryNetCoalescer::MakeFullSnapshot(const TArray<FInventoryNetEntry>& Current)
{
	FInventoryNetDelta Delta;
	Delta.Changed = Current;
	InventoryNetModelDetail::SortById(Delta.Changed);
	LastSent = Delta.Changed;
	Accumulated = 0.0f;
	return Delta;
}
