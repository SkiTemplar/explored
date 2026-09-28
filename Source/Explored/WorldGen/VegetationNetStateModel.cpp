#include "WorldGen/VegetationNetStateModel.h"

#include "Core/ExploredRandom.h"
#include "Core/NetQuantize.h"

bool FVegetationNetKey::operator<(const FVegetationNetKey& Other) const
{
	if (CellX != Other.CellX)
	{
		return CellX < Other.CellX;
	}
	if (CellY != Other.CellY)
	{
		return CellY < Other.CellY;
	}
	if (Species != Other.Species)
	{
		return Species < Other.Species;
	}
	return Index < Other.Index;
}

uint32 GetTypeHash(const FVegetationNetKey& Key)
{
	const uint32 Cell = static_cast<uint32>(static_cast<uint16>(Key.CellX)) | (static_cast<uint32>(static_cast<uint16>(Key.CellY)) << 16);
	const uint32 Local = static_cast<uint32>(Key.Index) | (static_cast<uint32>(Key.Species) << 16);
	return ExploredHash::Hash32(Cell ^ ExploredHash::Hash32(Local));
}

namespace VegetationNetDetail
{
	FVegetationNetState PlainStump()
	{
		FVegetationNetState S;
		S.Stage = EVegetationNetStage::Stump;
		return S;
	}

	bool CellLess(const FIntPoint& A, const FIntPoint& B)
	{
		return A.X != B.X ? A.X < B.X : A.Y < B.Y;
	}
}

bool FVegetationNetStateModel::MakeKey(const FIntPoint& Cell, int32 Species, int32 Index, FVegetationNetKey& OutKey)
{
	if (Cell.X < MIN_int16 || Cell.X > MAX_int16 || Cell.Y < MIN_int16 || Cell.Y > MAX_int16
		|| Species < 0 || Species >= MaxSpeciesPerCell || Index < 0 || Index > MAX_uint16)
	{
		return false;
	}
	OutKey.CellX = static_cast<int16>(Cell.X);
	OutKey.CellY = static_cast<int16>(Cell.Y);
	OutKey.Species = static_cast<uint8>(Species);
	OutKey.Index = static_cast<uint16>(Index);
	return true;
}

void FVegetationNetStateModel::BuildSpeciesTable(const TArray<FName>& ComponentNames, TArray<FName>& OutTable)
{
	TArray<FString> Lower;
	for (const FName& Name : ComponentNames)
	{
		if (Name.IsNone())
		{
			continue;
		}
		const FString Key = Name.ToString().ToLower();
		if (!Lower.Contains(Key))
		{
			Lower.Add(Key);
		}
	}
	Lower.Sort();
	OutTable.Reset();
	for (const FString& Key : Lower)
	{
		OutTable.Add(FName(*Key));
	}
}

int32 FVegetationNetStateModel::SpeciesIndex(const TArray<FName>& Table, FName Component)
{
	if (Component.IsNone())
	{
		return INDEX_NONE;
	}
	const int32 Index = Table.IndexOfByKey(Component);
	return Index < MaxSpeciesPerCell ? Index : INDEX_NONE;
}

uint16 FVegetationNetStateModel::QuantizeRegrowDays(float RegrowAtDays)
{
	if (!FMath::IsFinite(RegrowAtDays) || RegrowAtDays < 0.0f)
	{
		return 0;
	}
	// Hacia arriba y nunca 0 (que significa «sin rebrote»): el día 0 no existe en una partida.
	const double Quarters = FMath::CeilToDouble(static_cast<double>(RegrowAtDays) * 4.0 - 1e-9);
	return static_cast<uint16>(ExploredNet::RoundClamped(Quarters, 1, MAX_uint16));
}

float FVegetationNetStateModel::RegrowDays(uint16 QuarterDays)
{
	return QuarterDays == 0 ? -1.0f : static_cast<float>(QuarterDays) * 0.25f;
}

FVegetationNetState FVegetationNetStateModel::Canonical(const FVegetationNetState& State)
{
	FVegetationNetState Out;
	Out.Stage = static_cast<uint8>(State.Stage) < static_cast<uint8>(EVegetationNetStage::Count) ? State.Stage : EVegetationNetStage::Intact;
	Out.Hits = Out.Stage == EVegetationNetStage::Felling ? static_cast<uint8>(FMath::Min<int32>(State.Hits, MaxHits)) : 0;
	const bool bRegrows = Out.Stage == EVegetationNetStage::Stump || Out.Stage == EVegetationNetStage::Sprout;
	Out.RegrowAtQuarterDays = bRegrows ? State.RegrowAtQuarterDays : 0;
	return Out;
}

FVegetationNetState FVegetationNetStateModel::MakeState(EVegetationNetStage Stage, int32 Hits, float RegrowAtDays)
{
	FVegetationNetState S;
	S.Stage = Stage;
	S.Hits = static_cast<uint8>(FMath::Clamp(Hits, 0, MaxHits));
	S.RegrowAtQuarterDays = QuantizeRegrowDays(RegrowAtDays);
	return Canonical(S);
}

void FVegetationNetStateModel::AppendEntry(const FVegetationNetKey& Key, const FVegetationNetState& State, TArray<uint8>& Out)
{
	const FVegetationNetState C = Canonical(State);
	ExploredNet::FByteWriter W(Out);
	W.I16(Key.CellX);
	W.I16(Key.CellY);
	W.U8(Key.Species);
	W.U16(Key.Index);
	W.U8(static_cast<uint8>(static_cast<uint8>(C.Stage) | (C.Hits << 2)));
	W.U16(C.RegrowAtQuarterDays);
}

bool FVegetationNetStateModel::DecodeEntry(const TArray<uint8>& In, FVegetationNetKey& OutKey, FVegetationNetState& OutState)
{
	if (In.Num() != EntryBytes)
	{
		return false;
	}
	ExploredNet::FByteReader R(In);
	FVegetationNetKey Key;
	Key.CellX = R.I16();
	Key.CellY = R.I16();
	Key.Species = R.U8();
	Key.Index = R.U16();
	const uint8 Packed = R.U8();
	FVegetationNetState State;
	State.Stage = static_cast<EVegetationNetStage>(Packed & 0x03);
	State.Hits = (Packed >> 2) & 0x0F;
	State.RegrowAtQuarterDays = R.U16();
	if (!R.IsDone() || Key.Species >= MaxSpeciesPerCell || (Packed & 0xC0) != 0 || Canonical(State) != State)
	{
		return false;
	}
	OutKey = Key;
	OutState = State;
	return true;
}

bool FVegetationNetStateModel::ShouldSendToClient(const FVegetationNetState& State, double DistanceCm)
{
	if (State.Stage != EVegetationNetStage::Felling)
	{
		return true;
	}
	return FMath::IsFinite(DistanceCm) && DistanceCm < FellingProgressRadiusCm;
}

int32 FVegetationNetStateModel::FDelta::EstimateBytes() const
{
	int32 Bytes = Changed.Num() * BytesPerChange + Removed.Num() * FastArrayOverheadBytes;
	for (const TPair<FIntPoint, FString>& Cell : CellSnapshots)
	{
		// 4 B de celda (2 × int16) + 2 B de longitud + el texto ASCII.
		Bytes += 6 + Cell.Value.Len();
	}
	return Bytes;
}

bool FVegetationNetStateModel::IsPlainStump(const FVegetationNetState& State)
{
	return State == VegetationNetDetail::PlainStump();
}

bool FVegetationNetStateModel::IsInSnapshot(const FVegetationNetKey& Key) const
{
	const FSaveIndexSet* Set = Snapshots.Find(Key.Cell());
	return Set && Set->Contains(SnapshotIndex(Key));
}

FVegetationNetState FVegetationNetStateModel::View(const FVegetationNetKey& Key) const
{
	if (const FEntry* Entry = Entries.Find(Key))
	{
		return Entry->State;
	}
	return IsInSnapshot(Key) ? VegetationNetDetail::PlainStump() : FVegetationNetState();
}

void FVegetationNetStateModel::RemoveEntry(const FVegetationNetKey& Key)
{
	if (Entries.Remove(Key) > 0)
	{
		DirtyKeys.Add(Key);
	}
}

void FVegetationNetStateModel::Set(const FVegetationNetKey& Key, const FVegetationNetState& InState)
{
	const FVegetationNetState State = Canonical(InState);
	if (Key.Species >= MaxSpeciesPerCell)
	{
		return;
	}
	if (IsInSnapshot(Key) && !IsPlainStump(State))
	{
		// Vuelve a tener vida propia (brote, rebrote, arrancado de nuevo): sale del snapshot.
		FSaveIndexSet& Set = Snapshots.FindChecked(Key.Cell());
		Set.Remove(SnapshotIndex(Key));
		if (Set.IsEmpty())
		{
			Snapshots.Remove(Key.Cell());
		}
		DirtyCells.Add(Key.Cell());
	}
	const FVegetationNetState Default = IsInSnapshot(Key) ? VegetationNetDetail::PlainStump() : FVegetationNetState();
	if (State == Default)
	{
		RemoveEntry(Key);
		return;
	}
	FEntry* Existing = Entries.Find(Key);
	if (Existing && Existing->State == State)
	{
		return;
	}
	FEntry& Entry = Existing ? *Existing : Entries.Add(Key, FEntry());
	Entry.State = State;
	Entry.Seq = NextSeq++;
	DirtyKeys.Add(Key);
}

void FVegetationNetStateModel::MoveToSnapshot(const FVegetationNetKey& Key)
{
	Snapshots.FindOrAdd(Key.Cell()).Add(SnapshotIndex(Key));
	DirtyCells.Add(Key.Cell());
	RemoveEntry(Key);
}

int32 FVegetationNetStateModel::Compact()
{
	if (Entries.Num() <= MaxEntries)
	{
		return 0;
	}
	struct FCandidate
	{
		FVegetationNetKey Key;
		uint64 Seq;
		bool bPlain;
	};
	TArray<FCandidate> Candidates;
	for (const TPair<FVegetationNetKey, FEntry>& Pair : Entries)
	{
		if (Pair.Value.State.Stage == EVegetationNetStage::Stump)
		{
			Candidates.Add({Pair.Key, Pair.Value.Seq, IsPlainStump(Pair.Value.State)});
		}
	}
	// Primero los tocones sin rebrote, luego los que rebrotan; dentro de cada grupo, el más antiguo.
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.bPlain != B.bPlain)
		{
			return A.bPlain;
		}
		return A.Seq < B.Seq;
	});
	int32 Moved = 0;
	for (const FCandidate& Candidate : Candidates)
	{
		if (Entries.Num() <= CompactTarget)
		{
			break;
		}
		MoveToSnapshot(Candidate.Key);
		++Moved;
	}
	return Moved;
}

void FVegetationNetStateModel::ConsumeDelta(FDelta& OutDelta)
{
	OutDelta = FDelta();
	TArray<FVegetationNetKey> Keys = DirtyKeys.Array();
	Keys.Sort();
	for (const FVegetationNetKey& Key : Keys)
	{
		if (const FEntry* Entry = Entries.Find(Key))
		{
			OutDelta.Changed.Add(TPair<FVegetationNetKey, FVegetationNetState>(Key, Entry->State));
		}
		else
		{
			OutDelta.Removed.Add(Key);
		}
	}
	TArray<FIntPoint> Cells = DirtyCells.Array();
	Cells.Sort(&VegetationNetDetail::CellLess);
	for (const FIntPoint& Cell : Cells)
	{
		OutDelta.CellSnapshots.Add(TPair<FIntPoint, FString>(Cell, CellSnapshot(Cell)));
	}
	DirtyKeys.Reset();
	DirtyCells.Reset();
}

FString FVegetationNetStateModel::CellSnapshot(const FIntPoint& Cell) const
{
	const FSaveIndexSet* Set = Snapshots.Find(Cell);
	return Set ? Set->Encode() : FString();
}

bool FVegetationNetStateModel::ApplyDelta(const FDelta& Delta)
{
	bool bAllOk = true;
	for (const TPair<FIntPoint, FString>& Cell : Delta.CellSnapshots)
	{
		if (Cell.Value.IsEmpty())
		{
			Snapshots.Remove(Cell.Key);
			continue;
		}
		FSaveIndexSet Set;
		if (!Set.Decode(Cell.Value))
		{
			bAllOk = false;
			continue;
		}
		Snapshots.Add(Cell.Key, Set);
	}
	for (const FVegetationNetKey& Key : Delta.Removed)
	{
		Entries.Remove(Key);
	}
	for (const TPair<FVegetationNetKey, FVegetationNetState>& Change : Delta.Changed)
	{
		if (Change.Key.Species >= MaxSpeciesPerCell || Canonical(Change.Value) != Change.Value)
		{
			bAllOk = false;
			continue;
		}
		FEntry& Entry = Entries.FindOrAdd(Change.Key);
		Entry.State = Change.Value;
		Entry.Seq = NextSeq++;
	}
	return bAllOk;
}
