#include "WorldGen/VegetationStateModel.h"

namespace
{
	/** Bits libres del primer byte del estado: deben ir a cero (un paquete con ellos puestos está manipulado). */
	constexpr uint8 StateSpareMask = 0xC0;

	int32 CompareClock(const FVegetationClockEntry& E, const FIntPoint& Cell, FName Component, int32 Index)
	{
		if (E.Cell.Y != Cell.Y)
		{
			return E.Cell.Y < Cell.Y ? -1 : 1;
		}
		if (E.Cell.X != Cell.X)
		{
			return E.Cell.X < Cell.X ? -1 : 1;
		}
		if (E.Component != Component)
		{
			const int32 ByName = FSaveValue::CompareKeys(E.Component.ToString(), Component.ToString());
			if (ByName != 0)
			{
				return ByName;
			}
		}
		if (E.Index != Index)
		{
			return E.Index < Index ? -1 : 1;
		}
		return 0;
	}
}

// ---------------------------------------------------------------------------
// FVegetationClock
// ---------------------------------------------------------------------------

int32 FVegetationClock::LowerBound(const FIntPoint& Cell, FName Component, int32 Index) const
{
	int32 Lo = 0;
	int32 Hi = Entries.Num();
	while (Lo < Hi)
	{
		const int32 Mid = Lo + (Hi - Lo) / 2;
		if (CompareClock(Entries[Mid], Cell, Component, Index) < 0)
		{
			Lo = Mid + 1;
		}
		else
		{
			Hi = Mid;
		}
	}
	return Lo;
}

bool FVegetationClock::Set(const FIntPoint& Cell, FName Component, int32 Index, int64 FelledAtMinute)
{
	if (Index < 0 || Component.IsNone())
	{
		return false;
	}
	const int32 At = LowerBound(Cell, Component, Index);
	if (At < Entries.Num() && CompareClock(Entries[At], Cell, Component, Index) == 0)
	{
		Entries[At].FelledAtMinute = FelledAtMinute;
		return true;
	}
	FVegetationClockEntry Entry;
	Entry.Cell = Cell;
	Entry.Component = Component;
	Entry.Index = Index;
	Entry.FelledAtMinute = FelledAtMinute;
	Entries.Insert(Entry, At);
	return true;
}

bool FVegetationClock::Remove(const FIntPoint& Cell, FName Component, int32 Index)
{
	const int32 At = LowerBound(Cell, Component, Index);
	if (At < Entries.Num() && CompareClock(Entries[At], Cell, Component, Index) == 0)
	{
		Entries.RemoveAt(At);
		return true;
	}
	return false;
}

const int64* FVegetationClock::Find(const FIntPoint& Cell, FName Component, int32 Index) const
{
	const int32 At = LowerBound(Cell, Component, Index);
	if (At < Entries.Num() && CompareClock(Entries[At], Cell, Component, Index) == 0)
	{
		return &Entries[At].FelledAtMinute;
	}
	return nullptr;
}

FSaveValue FVegetationClock::ToValue() const
{
	FSaveValue Out = FSaveValue::MakeArray();
	for (const FVegetationClockEntry& E : Entries)
	{
		FSaveValue Row = FSaveValue::MakeArray();
		Row.Add(FSaveValue::MakeInt(E.Cell.X));
		Row.Add(FSaveValue::MakeInt(E.Cell.Y));
		Row.Add(FSaveValue::MakeString(E.Component.ToString()));
		Row.Add(FSaveValue::MakeInt(E.Index));
		Row.Add(FSaveValue::MakeInt(E.FelledAtMinute));
		Out.Add(MoveTemp(Row));
	}
	return Out;
}

int32 FVegetationClock::FromValue(const FSaveValue& Value)
{
	Reset();
	if (!Value.IsArray())
	{
		return -1;
	}
	int32 Skipped = 0;
	for (int32 i = 0; i < Value.Num(); ++i)
	{
		const FSaveValue& Row = Value.At(i);
		int64 X = 0;
		int64 Y = 0;
		int64 Index = 0;
		int64 Minute = 0;
		FString Component;
		if (!Row.IsArray() || Row.Num() != 5
			|| !Row.At(0).TryGetInt(X) || !Row.At(1).TryGetInt(Y)
			|| !Row.At(2).TryGetString(Component) || Component.IsEmpty()
			|| !Row.At(3).TryGetInt(Index) || !Row.At(4).TryGetInt(Minute)
			|| X < MIN_int32 || X > MAX_int32 || Y < MIN_int32 || Y > MAX_int32
			|| Index < 0 || Index > MAX_int32 || Minute < 0)
		{
			++Skipped;
			continue;
		}
		// Una entrada repetida (texto editado a mano) se queda con la tala más reciente: rebrota más tarde, nunca antes.
		const FIntPoint Cell((int32)X, (int32)Y);
		const FName Name(*Component);
		const int64* Existing = Find(Cell, Name, (int32)Index);
		Set(Cell, Name, (int32)Index, Existing ? FMath::Max(*Existing, Minute) : Minute);
	}
	return Skipped;
}

bool FVegetationClock::operator==(const FVegetationClock& Other) const
{
	if (Entries.Num() != Other.Entries.Num())
	{
		return false;
	}
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FVegetationClockEntry& A = Entries[i];
		const FVegetationClockEntry& B = Other.Entries[i];
		if (A.Cell != B.Cell || A.Component != B.Component || A.Index != B.Index || A.FelledAtMinute != B.FelledAtMinute)
		{
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// FVegetationStateModel: estado
// ---------------------------------------------------------------------------

FVegetationInstanceState FVegetationStateModel::Fell(int64 NowMinute, int64 SproutMinutes, int64 RegrowMinutes)
{
	FVegetationInstanceState State;
	State.FelledAtMinute = NowMinute;
	if (RegrowMinutes > 0)
	{
		State.RegrowAtMinute = NowMinute + RegrowMinutes;
		State.SproutAtMinute = SproutMinutes > 0 && SproutMinutes < RegrowMinutes ? NowMinute + SproutMinutes : -1;
	}
	return State;
}

EVegetationStage FVegetationStateModel::StageAt(const FVegetationInstanceState& State, int64 NowMinute)
{
	if (State.FelledAtMinute < 0)
	{
		return State.Hits > 0 ? EVegetationStage::Felling : EVegetationStage::Intact;
	}
	if (IsRegrown(State, NowMinute))
	{
		return EVegetationStage::Intact;
	}
	if (State.SproutAtMinute >= 0 && NowMinute >= State.SproutAtMinute)
	{
		return EVegetationStage::Sapling;
	}
	return EVegetationStage::Stump;
}

bool FVegetationStateModel::IsRegrown(const FVegetationInstanceState& State, int64 NowMinute)
{
	return State.FelledAtMinute >= 0 && State.RegrowAtMinute >= 0 && NowMinute >= State.RegrowAtMinute;
}

int64 FVegetationStateModel::ResolveFelledAt(const int64* SavedMinute, int64 NowMinute)
{
	if (!SavedMinute || *SavedMinute < 0 || *SavedMinute > NowMinute)
	{
		return NowMinute;
	}
	return *SavedMinute;
}

int64 FVegetationStateModel::MinuteFromDays(double TotalDays)
{
	if (!FMath::IsFinite(TotalDays) || TotalDays <= 0.0)
	{
		return 0;
	}
	// Tope de ~6 millones de años de juego: lejos del desbordamiento de int64 al sumar plazos.
	constexpr double MaxDays = 2.0e9;
	return (int64)FMath::FloorToDouble(FMath::Min(TotalDays, MaxDays) * (double)MinutesPerDay);
}

// ---------------------------------------------------------------------------
// FVegetationStateModel: red
// ---------------------------------------------------------------------------

TArray<FName> FVegetationStateModel::SpeciesSlots(const TArray<FName>& ComponentNames)
{
	TArray<FString> Names;
	for (const FName& Name : ComponentNames)
	{
		if (!Name.IsNone())
		{
			Names.AddUnique(Name.ToString());
		}
	}
	Names.Sort([](const FString& A, const FString& B) { return FSaveValue::CompareKeys(A, B) < 0; });
	TArray<FName> Slots;
	for (int32 i = 0; i < Names.Num() && i < MaxSpeciesSlots; ++i)
	{
		Slots.Add(FName(*Names[i]));
	}
	return Slots;
}

int32 FVegetationStateModel::FindSpeciesSlot(const TArray<FName>& Slots, FName Component)
{
	return Component.IsNone() ? INDEX_NONE : Slots.IndexOfByKey(Component);
}

bool FVegetationStateModel::MakeKey(const FIntPoint& Cell, int32 SpeciesSlot, int32 Index, FVegetationNetKey& Out)
{
	if (Cell.X < -32768 || Cell.X > 32767 || Cell.Y < -32768 || Cell.Y > 32767
		|| SpeciesSlot < 0 || SpeciesSlot >= MaxSpeciesSlots || Index < 0 || Index > 0xFFFF)
	{
		return false;
	}
	Out.CellX = (int16)Cell.X;
	Out.CellY = (int16)Cell.Y;
	Out.Species = (uint8)SpeciesSlot;
	Out.Index = (uint16)Index;
	return true;
}

FVegetationNetState FVegetationStateModel::ToNet(const FVegetationInstanceState& State, int64 NowMinute)
{
	FVegetationNetState Net;
	Net.Stage = StageAt(State, NowMinute);
	Net.Hits = (uint8)FMath::Clamp(State.FelledAtMinute < 0 ? State.Hits : 0, 0, MaxNetHits);
	if (State.FelledAtMinute >= 0 && State.RegrowAtMinute < 0)
	{
		Net.RegrowAtQuarterDays = NoRegrow;
	}
	else if (State.RegrowAtMinute >= 0)
	{
		// Hacia arriba: el cliente nunca ve rebrotar antes que el servidor.
		const int64 Quarters = (State.RegrowAtMinute + MinutesPerQuarterDay - 1) / MinutesPerQuarterDay;
		Net.RegrowAtQuarterDays = (uint16)FMath::Clamp(Quarters, (int64)0, (int64)NoRegrow - 1);
	}
	return Net;
}

int64 FVegetationStateModel::RegrowMinuteFromNet(const FVegetationNetState& State)
{
	return State.RegrowAtQuarterDays == NoRegrow ? -1 : (int64)State.RegrowAtQuarterDays * MinutesPerQuarterDay;
}

void FVegetationStateModel::EncodeKey(const FVegetationNetKey& Key, TArray<uint8>& Out)
{
	const uint16 X = (uint16)Key.CellX;
	const uint16 Y = (uint16)Key.CellY;
	Out.Add((uint8)(X & 0xFF));
	Out.Add((uint8)(X >> 8));
	Out.Add((uint8)(Y & 0xFF));
	Out.Add((uint8)(Y >> 8));
	Out.Add(Key.Species);
	Out.Add((uint8)(Key.Index & 0xFF));
	Out.Add((uint8)(Key.Index >> 8));
}

void FVegetationStateModel::EncodeState(const FVegetationNetState& State, TArray<uint8>& Out)
{
	const uint8 Packed = (uint8)(((uint8)State.Stage & 0x3) | ((FMath::Min<uint8>(State.Hits, (uint8)MaxNetHits) & 0xF) << 2));
	Out.Add(Packed);
	Out.Add((uint8)(State.RegrowAtQuarterDays & 0xFF));
	Out.Add((uint8)(State.RegrowAtQuarterDays >> 8));
}

bool FVegetationStateModel::DecodeKey(const TArray<uint8>& In, int32 Offset, FVegetationNetKey& Out)
{
	if (Offset < 0 || Offset > In.Num() - KeyBytes)
	{
		return false;
	}
	const uint8 Species = In[Offset + 4];
	if (Species >= MaxSpeciesSlots)
	{
		return false;
	}
	Out.CellX = (int16)(uint16)(In[Offset] | (In[Offset + 1] << 8));
	Out.CellY = (int16)(uint16)(In[Offset + 2] | (In[Offset + 3] << 8));
	Out.Species = Species;
	Out.Index = (uint16)(In[Offset + 5] | (In[Offset + 6] << 8));
	return true;
}

bool FVegetationStateModel::DecodeState(const TArray<uint8>& In, int32 Offset, FVegetationNetState& Out)
{
	if (Offset < 0 || Offset > In.Num() - StateBytes)
	{
		return false;
	}
	const uint8 Packed = In[Offset];
	const uint16 Regrow = (uint16)(In[Offset + 1] | (In[Offset + 2] << 8));
	const EVegetationStage Stage = (EVegetationStage)(Packed & 0x3);
	const uint8 Hits = (uint8)((Packed >> 2) & 0xF);
	// Bits libres puestos, golpes en una instancia talada o «no rebrota» en una en pie: paquete imposible.
	if ((Packed & StateSpareMask) != 0
		|| (Hits > 0 && Stage != EVegetationStage::Felling)
		|| (Regrow == NoRegrow && Stage != EVegetationStage::Stump))
	{
		return false;
	}
	Out.Stage = Stage;
	Out.Hits = Hits;
	Out.RegrowAtQuarterDays = Regrow;
	return true;
}

bool FVegetationStateModel::KeyLess(const FVegetationNetKey& A, const FVegetationNetKey& B)
{
	if (A.CellY != B.CellY)
	{
		return A.CellY < B.CellY;
	}
	if (A.CellX != B.CellX)
	{
		return A.CellX < B.CellX;
	}
	if (A.Species != B.Species)
	{
		return A.Species < B.Species;
	}
	return A.Index < B.Index;
}

int32 FVegetationStateModel::Compact(TArray<FVegetationNetEntry>& Entries, int32 Cap, TArray<FVegetationNetKey>* OutMoved)
{
	const int32 Limit = FMath::Max(0, Cap);
	if (Entries.Num() <= Limit)
	{
		return 0;
	}
	TArray<FVegetationNetKey> Candidates;
	for (const FVegetationNetEntry& E : Entries)
	{
		if (E.State.Stage == EVegetationStage::Stump && E.State.RegrowAtQuarterDays == NoRegrow)
		{
			Candidates.Add(E.Key);
		}
	}
	Candidates.Sort([](const FVegetationNetKey& A, const FVegetationNetKey& B) { return KeyLess(A, B); });
	const int32 ToMove = FMath::Min(Entries.Num() - Limit, Candidates.Num());
	if (ToMove <= 0)
	{
		return 0;
	}
	Candidates.SetNum(ToMove);
	// Las que quedan conservan su orden relativo en el array.
	Entries.RemoveAll([&Candidates](const FVegetationNetEntry& E)
	{
		if (E.State.Stage != EVegetationStage::Stump || E.State.RegrowAtQuarterDays != NoRegrow)
		{
			return false;
		}
		// Búsqueda binaria en los candidatos ordenados: O(n log n) con el array lleno.
		int32 Lo = 0;
		int32 Hi = Candidates.Num();
		while (Lo < Hi)
		{
			const int32 Mid = Lo + (Hi - Lo) / 2;
			if (KeyLess(Candidates[Mid], E.Key))
			{
				Lo = Mid + 1;
			}
			else
			{
				Hi = Mid;
			}
		}
		return Lo < Candidates.Num() && Candidates[Lo] == E.Key;
	});
	if (OutMoved)
	{
		OutMoved->Append(Candidates);
	}
	return ToMove;
}
