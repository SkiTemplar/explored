#include "WorldGen/VegetationClockModel.h"

#include "Save/SaveWorldDeltas.h"

namespace VegetationClockPriv
{
	/** Orden del componente por texto sin mayúsculas: FName compara sin distinguirlas, así que el orden tampoco puede. */
	int32 CompareComponents(FName A, FName B)
	{
		return A.ToString().ToLower().Compare(B.ToString().ToLower());
	}

	bool IsValidKey(const FVegetationStumpKey& Key)
	{
		const int32 MaxCell = FVegetationClockModel::MaxAbsCell;
		return Key.Cell.X >= -MaxCell && Key.Cell.X <= MaxCell && Key.Cell.Y >= -MaxCell && Key.Cell.Y <= MaxCell
			&& Key.Index >= 0 && Key.Index <= FSaveIndexSet::MaxIndex
			&& !Key.Component.IsNone() && Key.Component.ToString().Len() <= FVegetationClockModel::MaxComponentLength;
	}

	bool IsValidMinute(int64 Minute)
	{
		return Minute >= -FVegetationClockModel::MaxAbsMinute && Minute <= FVegetationClockModel::MaxAbsMinute;
	}
}

bool FVegetationClockModel::KeyLess(const FVegetationStumpKey& A, const FVegetationStumpKey& B)
{
	if (A.Cell.Y != B.Cell.Y)
	{
		return A.Cell.Y < B.Cell.Y;
	}
	if (A.Cell.X != B.Cell.X)
	{
		return A.Cell.X < B.Cell.X;
	}
	const int32 ByName = VegetationClockPriv::CompareComponents(A.Component, B.Component);
	if (ByName != 0)
	{
		return ByName < 0;
	}
	return A.Index < B.Index;
}

int32 FVegetationClockModel::LowerBound(const FVegetationStumpKey& Key) const
{
	int32 Lo = 0;
	int32 Hi = Entries.Num();
	while (Lo < Hi)
	{
		const int32 Mid = Lo + (Hi - Lo) / 2;
		if (KeyLess(Entries[Mid].Key, Key))
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

void FVegetationClockModel::RecordFelled(const FVegetationStumpKey& Key, int64 NowMinute)
{
	if (!VegetationClockPriv::IsValidKey(Key) || !VegetationClockPriv::IsValidMinute(NowMinute))
	{
		return;
	}
	FStumpState Stump;
	Stump.FelledAtMinute = NowMinute;
	const int32 At = LowerBound(Key);
	if (Entries.IsValidIndex(At) && !KeyLess(Key, Entries[At].Key))
	{
		Entries[At].Stump = Stump;
		return;
	}
	FVegetationStumpEntry Entry;
	Entry.Key = Key;
	Entry.Stump = Stump;
	Entries.Insert(Entry, At);
}

const FStumpState* FVegetationClockModel::Find(const FVegetationStumpKey& Key) const
{
	const int32 At = LowerBound(Key);
	return Entries.IsValidIndex(At) && !KeyLess(Key, Entries[At].Key) ? &Entries[At].Stump : nullptr;
}

FStumpState* FVegetationClockModel::FindMutable(const FVegetationStumpKey& Key)
{
	return const_cast<FStumpState*>(static_cast<const FVegetationClockModel*>(this)->Find(Key));
}

bool FVegetationClockModel::Remove(const FVegetationStumpKey& Key)
{
	const int32 At = LowerBound(Key);
	if (!Entries.IsValidIndex(At) || KeyLess(Key, Entries[At].Key))
	{
		return false;
	}
	Entries.RemoveAt(At);
	return true;
}

int32 FVegetationClockModel::PruneMature(TFunctionRef<const FFellingProfile*(FName Component)> ProfileOf, int64 NowMinute, TArray<FVegetationStumpKey>* OutMatured)
{
	int32 Removed = 0;
	int32 Write = 0;
	for (int32 Read = 0; Read < Entries.Num(); ++Read)
	{
		const FFellingProfile* Profile = ProfileOf(Entries[Read].Key.Component);
		if (Profile && FFellingModel::StageAt(*Profile, Entries[Read].Stump, NowMinute) == EStumpStage::Mature)
		{
			if (OutMatured)
			{
				OutMatured->Add(Entries[Read].Key);
			}
			++Removed;
			continue;
		}
		if (Write != Read)
		{
			Entries[Write] = Entries[Read];
		}
		++Write;
	}
	Entries.SetNum(Write);
	return Removed;
}

int32 FVegetationClockModel::Reconcile(FName Component, const FSaveScatterDeltas& Felled, int64 NowMinute)
{
	if (Component.IsNone() || !VegetationClockPriv::IsValidMinute(NowMinute))
	{
		return 0;
	}
	int32 Changed = 0;

	// Huérfanas de este componente (su índice ya no está talado) fuera; horas futuras acotadas.
	int32 Write = 0;
	for (int32 Read = 0; Read < Entries.Num(); ++Read)
	{
		FVegetationStumpEntry& Entry = Entries[Read];
		if (Entry.Key.Component == Component)
		{
			if (!Felled.Contains(Entry.Key.Cell, Entry.Key.Index))
			{
				++Changed;
				continue;
			}
			if (Entry.Stump.FelledAtMinute > NowMinute)
			{
				Entry.Stump.FelledAtMinute = NowMinute;
				++Changed;
			}
		}
		if (Write != Read)
		{
			Entries[Write] = Entry;
		}
		++Write;
	}
	Entries.SetNum(Write);

	// Talados sin hora: talados ahora. Se recogen primero para no insertar mientras se recorre.
	TArray<FVegetationStumpKey> Missing;
	Felled.ForEach([&](const FIntPoint& Cell, int32 Index)
	{
		FVegetationStumpKey Key;
		Key.Cell = Cell;
		Key.Component = Component;
		Key.Index = Index;
		if (VegetationClockPriv::IsValidKey(Key) && !Find(Key))
		{
			Missing.Add(Key);
		}
	});
	for (const FVegetationStumpKey& Key : Missing)
	{
		RecordFelled(Key, NowMinute);
	}
	return Changed + Missing.Num();
}

FSaveValue FVegetationClockModel::Save() const
{
	FSaveValue Root = FSaveValue::MakeObject();
	Root.Set(TEXT("version"), FSaveValue::MakeInt(SaveVersion));
	FSaveValue& List = Root.Set(TEXT("stumps"), FSaveValue::MakeArray());
	for (const FVegetationStumpEntry& Entry : Entries)
	{
		FSaveValue Row = FSaveValue::MakeArray();
		Row.Add(FSaveValue::MakeInt(Entry.Key.Cell.X));
		Row.Add(FSaveValue::MakeInt(Entry.Key.Cell.Y));
		Row.Add(FSaveValue::MakeString(Entry.Key.Component.ToString()));
		Row.Add(FSaveValue::MakeInt(Entry.Key.Index));
		Row.Add(FSaveValue::MakeInt(Entry.Stump.FelledAtMinute));
		Row.Add(FSaveValue::MakeInt(Entry.Stump.UprootWork));
		List.Add(MoveTemp(Row));
	}
	return Root;
}

bool FVegetationClockModel::Load(const FSaveValue& Value)
{
	Entries.Reset();
	const FSaveValue* Version = Value.Find(TEXT("version"));
	const FSaveValue* List = Value.Find(TEXT("stumps"));
	int64 V = 0;
	if (!Version || !Version->TryGetInt(V) || V != SaveVersion || !List || !List->IsArray() || List->Num() > MaxEntries)
	{
		return false;
	}
	TArray<FVegetationStumpEntry> Loaded;
	Loaded.Reserve(List->Num());
	for (int32 i = 0; i < List->Num(); ++i)
	{
		const FSaveValue& Row = List->At(i);
		int64 X = 0, Y = 0, Index = 0, FelledAt = 0, UprootWork = 0;
		FString Component;
		if (!Row.IsArray() || Row.Num() != 6
			|| !Row.At(0).TryGetInt(X) || !Row.At(1).TryGetInt(Y) || !Row.At(2).TryGetString(Component)
			|| !Row.At(3).TryGetInt(Index) || !Row.At(4).TryGetInt(FelledAt) || !Row.At(5).TryGetInt(UprootWork))
		{
			return false;
		}
		// Rangos antes de estrechar a int32; nada de Abs (Abs(INT64_MIN) desborda).
		if (X < -MaxAbsCell || X > MaxAbsCell || Y < -MaxAbsCell || Y > MaxAbsCell
			|| Index < 0 || Index > FSaveIndexSet::MaxIndex
			|| !VegetationClockPriv::IsValidMinute(FelledAt)
			|| UprootWork < 0 || UprootWork > FFellingModel::WorkToFell
			|| Component.IsEmpty() || Component.Len() > MaxComponentLength)
		{
			return false;
		}
		FVegetationStumpEntry Entry;
		Entry.Key.Cell = FIntPoint((int32)X, (int32)Y);
		Entry.Key.Component = FName(*Component);
		Entry.Key.Index = (int32)Index;
		if (!VegetationClockPriv::IsValidKey(Entry.Key))
		{
			return false;
		}
		Entry.Stump.FelledAtMinute = FelledAt;
		Entry.Stump.UprootWork = (int32)UprootWork;
		Entry.Stump.bUprooted = UprootWork >= FFellingModel::WorkToFell;
		Loaded.Add(Entry);
	}
	// El escritor siempre ordena; un fichero reordenado a mano se acepta, uno con repetidas no.
	Loaded.Sort([](const FVegetationStumpEntry& A, const FVegetationStumpEntry& B) { return KeyLess(A.Key, B.Key); });
	for (int32 i = 1; i < Loaded.Num(); ++i)
	{
		if (!KeyLess(Loaded[i - 1].Key, Loaded[i].Key))
		{
			return false;
		}
	}
	Entries = MoveTemp(Loaded);
	return true;
}
