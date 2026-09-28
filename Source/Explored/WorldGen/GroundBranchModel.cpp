#include "WorldGen/GroundBranchModel.h"

#include "Core/ExploredRandom.h"

namespace
{
	bool CanGrow(const FGroundBranchSource& Source)
	{
		return Source.Capacity > 0 && Source.CycleMax > 0;
	}

	/** Ciclo como dos int32 para el hash: los relojes de más de 2^31 ciclos no se repiten. */
	uint32 CycleSeed(uint32 CellSeed, int64 Cycle)
	{
		return ExploredHash::Hash2D(CellSeed, (int32)(uint32)(uint64)Cycle, (int32)(uint32)((uint64)Cycle >> 32));
	}
}

int32 FGroundBranchModel::TotalCapacity(const TArray<FGroundBranchSource>& Sources)
{
	int32 Total = 0;
	for (const FGroundBranchSource& Source : Sources)
	{
		Total += FMath::Max(0, Source.Capacity);
	}
	return Total;
}

int32 FGroundBranchModel::CountUnder(const FGroundBranchCell& Cell, int32 SourceIndex)
{
	int32 Count = 0;
	for (const FGroundBranch& Branch : Cell.Present)
	{
		Count += Branch.SourceIndex == SourceIndex ? 1 : 0;
	}
	return Count;
}

int64 FGroundBranchModel::CycleOf(int64 Minute)
{
	const int64 Q = Minute / CycleMinutes;
	return (Minute % CycleMinutes != 0 && Minute < 0) ? Q - 1 : Q;
}

int32 FGroundBranchModel::RollCycle(uint32 CellSeed, int64 Cycle, int32 SourceIndex, const FGroundBranchSource& Source)
{
	const int32 Lo = FMath::Max(0, Source.CycleMin);
	const int32 Hi = FMath::Max(Lo, Source.CycleMax);
	if (Hi <= Lo)
	{
		return Lo;
	}
	const uint32 H = ExploredHash::Hash2D(CycleSeed(CellSeed, Cycle), SourceIndex, 3);
	return Lo + (int32)(H % (uint32)(Hi - Lo + 1));
}

FGroundBranchCell FGroundBranchModel::Initialize(uint32 CellSeed, const TArray<FGroundBranchSource>& Sources, int64 NowMinute)
{
	FGroundBranchCell Cell;
	Cell.CellSeed = CellSeed;
	Cell.LastUpdateMinute = NowMinute;
	const int64 Cycle = CycleOf(NowMinute);
	for (int32 i = 0; i < Sources.Num(); ++i)
	{
		if (!CanGrow(Sources[i]))
		{
			continue;
		}
		const int32 N = FMath::Min(RollCycle(CellSeed, Cycle, i, Sources[i]), Sources[i].Capacity);
		for (int32 n = 0; n < N; ++n)
		{
			Cell.Present.Add(PlaceBranch(CellSeed, Cell.NextSerial++, i, Sources));
		}
	}
	return Cell;
}

int32 FGroundBranchModel::Advance(FGroundBranchCell& Cell, const TArray<FGroundBranchSource>& Sources, int64 NowMinute)
{
	if (NowMinute <= Cell.LastUpdateMinute)
	{
		return 0;
	}
	// Comienzos de ciclo en (Last, Now]: del ciclo siguiente al de Last hasta el de Now.
	int64 FirstCycle = CycleOf(Cell.LastUpdateMinute) + 1;
	const int64 LastCycle = CycleOf(NowMinute);
	Cell.LastUpdateMinute = NowMinute;
	if (LastCycle < FirstCycle)
	{
		return 0;
	}
	if (LastCycle - FirstCycle >= MaxCyclesPerAdvance)
	{
		FirstCycle = LastCycle - MaxCyclesPerAdvance + 1;
	}

	TArray<int32> Under;
	Under.SetNumZeroed(Sources.Num());
	for (const FGroundBranch& Branch : Cell.Present)
	{
		if (Under.IsValidIndex(Branch.SourceIndex))
		{
			++Under[Branch.SourceIndex];
		}
	}

	int32 Spawned = 0;
	for (int64 Cycle = FirstCycle; Cycle <= LastCycle; ++Cycle)
	{
		bool bAnyRoom = false;
		for (int32 i = 0; i < Sources.Num(); ++i)
		{
			const FGroundBranchSource& Source = Sources[i];
			const int32 Free = Source.Capacity - Under[i];
			if (!CanGrow(Source) || Free <= 0)
			{
				continue;
			}
			const int32 N = FMath::Min(RollCycle(Cell.CellSeed, Cycle, i, Source), Free);
			for (int32 n = 0; n < N; ++n)
			{
				Cell.Present.Add(PlaceBranch(Cell.CellSeed, Cell.NextSerial++, i, Sources));
			}
			Under[i] += N;
			Spawned += N;
			bAnyRoom |= Under[i] < Source.Capacity;
		}
		if (!bAnyRoom)
		{
			// Todo lleno (o nada que crezca): los ciclos que quedan no añaden nada.
			break;
		}
	}
	return Spawned;
}

bool FGroundBranchModel::Pick(FGroundBranchCell& Cell, uint32 Serial, FGroundBranch* OutBranch)
{
	for (int32 i = 0; i < Cell.Present.Num(); ++i)
	{
		if (Cell.Present[i].Serial == Serial)
		{
			if (OutBranch)
			{
				*OutBranch = Cell.Present[i];
			}
			// RemoveAt conserva el orden: el estado guardado no depende del orden de recogida.
			Cell.Present.RemoveAt(i);
			return true;
		}
	}
	return false;
}

FGroundBranch FGroundBranchModel::PlaceBranch(uint32 CellSeed, uint32 Serial, int32 SourceIndex, const TArray<FGroundBranchSource>& Sources)
{
	FGroundBranch Branch;
	Branch.Serial = Serial;
	if (!Sources.IsValidIndex(SourceIndex))
	{
		return Branch;
	}
	const FGroundBranchSource& Source = Sources[SourceIndex];
	Branch.SourceIndex = SourceIndex;
	Branch.ItemId = Source.ItemId;

	// Punto uniforme en el disco de la copa.
	const double U = (double)ExploredHash::ToUnitFloat(ExploredHash::Hash2D(CellSeed, (int32)Serial, 1));
	const double V = (double)ExploredHash::ToUnitFloat(ExploredHash::Hash2D(CellSeed, (int32)Serial, 2));
	const double Radius = FMath::Max(0.0f, Source.CrownRadiusMeters) * 100.0 * FMath::Sqrt(U);
	const double Angle = V * 2.0 * UE_DOUBLE_PI;
	Branch.Position = Source.Position + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
	return Branch;
}

FSaveValue FGroundBranchModel::ToValue(const FGroundBranchCell& Cell)
{
	FSaveValue Out = FSaveValue::MakeObject();
	Out.Set(TEXT("last"), FSaveValue::MakeInt(Cell.LastUpdateMinute));
	Out.Set(TEXT("next"), FSaveValue::MakeInt((int64)Cell.NextSerial));
	FSaveValue Present = FSaveValue::MakeArray();
	for (const FGroundBranch& Branch : Cell.Present)
	{
		FSaveValue Entry = FSaveValue::MakeArray();
		Entry.Add(FSaveValue::MakeInt((int64)Branch.Serial));
		Entry.Add(FSaveValue::MakeInt(Branch.SourceIndex));
		Present.Add(MoveTemp(Entry));
	}
	Out.Set(TEXT("present"), MoveTemp(Present));
	return Out;
}

bool FGroundBranchModel::FromValue(const FSaveValue& Value, uint32 CellSeed, const TArray<FGroundBranchSource>& Sources, FGroundBranchCell& Out)
{
	const FSaveValue* Last = Value.IsObject() ? Value.Find(TEXT("last")) : nullptr;
	const FSaveValue* Next = Value.IsObject() ? Value.Find(TEXT("next")) : nullptr;
	const FSaveValue* Present = Value.IsObject() ? Value.Find(TEXT("present")) : nullptr;
	int64 LastMinute = 0;
	int64 NextSerial = 0;
	if (!Last || !Next || !Present || !Present->IsArray()
		|| !Last->TryGetInt(LastMinute) || !Next->TryGetInt(NextSerial)
		|| NextSerial < 0 || NextSerial > (int64)MAX_uint32)
	{
		return false;
	}

	FGroundBranchCell Cell;
	Cell.CellSeed = CellSeed;
	Cell.LastUpdateMinute = LastMinute;
	Cell.NextSerial = (uint32)NextSerial;
	TSet<uint32> Seen;
	for (int32 i = 0; i < Present->Num(); ++i)
	{
		const FSaveValue& Entry = Present->At(i);
		int64 Serial = 0;
		int64 SourceIndex = 0;
		if (!Entry.IsArray() || Entry.Num() != 2
			|| !Entry.At(0).TryGetInt(Serial) || !Entry.At(1).TryGetInt(SourceIndex)
			|| Serial < 0 || Serial >= NextSerial
			|| SourceIndex < 0 || SourceIndex >= Sources.Num()
			|| Seen.Contains((uint32)Serial))
		{
			return false;
		}
		Seen.Add((uint32)Serial);
		Cell.Present.Add(PlaceBranch(CellSeed, (uint32)Serial, (int32)SourceIndex, Sources));
	}
	Out = MoveTemp(Cell);
	return true;
}
