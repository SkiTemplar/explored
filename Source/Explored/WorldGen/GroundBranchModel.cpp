#include "WorldGen/GroundBranchModel.h"

#include "Core/ExploredRandom.h"

namespace
{
	/** Sin paso más largo que ~19 siglos de juego: acota Σ milésimas · minutos lejos del desbordamiento de int64. */
	constexpr int64 MaxStepMinutes = 1000000000;

	int64 TotalRateMilli(const TArray<FGroundBranchSource>& Sources)
	{
		int64 Rate = 0;
		for (const FGroundBranchSource& Source : Sources)
		{
			if (Source.Capacity > 0 && Source.PerDayMilli > 0)
			{
				Rate += Source.PerDayMilli;
			}
		}
		return Rate;
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

FGroundBranchCell FGroundBranchModel::Initialize(uint32 CellSeed, const TArray<FGroundBranchSource>& Sources, int64 NowMinute)
{
	FGroundBranchCell Cell;
	Cell.CellSeed = CellSeed;
	Cell.LastUpdateMinute = NowMinute;
	const int32 Capacity = TotalCapacity(Sources);
	for (int32 i = 0; i < Capacity; ++i)
	{
		Cell.Present.Add(PlaceBranch(CellSeed, Cell.NextSerial++, Sources));
	}
	return Cell;
}

int32 FGroundBranchModel::Advance(FGroundBranchCell& Cell, const TArray<FGroundBranchSource>& Sources, int64 NowMinute)
{
	if (NowMinute <= Cell.LastUpdateMinute)
	{
		return 0;
	}
	// NowMinute > LastUpdateMinute: la diferencia sin signo es exacta aunque la resta con signo
	// desbordara (LastUpdateMinute = INT64_MIN en una partida manipulada).
	const int64 Step = (int64)FMath::Min<uint64>((uint64)NowMinute - (uint64)Cell.LastUpdateMinute, (uint64)MaxStepMinutes);
	Cell.LastUpdateMinute = NowMinute;

	const int32 Capacity = TotalCapacity(Sources);
	const int64 Rate = TotalRateMilli(Sources);
	if (Rate <= 0 || Cell.Present.Num() >= Capacity)
	{
		// Llena (o sin árboles): nada se acumula, así que recoger no provoca una ráfaga.
		Cell.Accumulator = 0;
		return 0;
	}

	// Lo que sobre al llenarse se descarta igual; acotarlo antes evita desbordar con pasos enormes.
	const int64 Missing = Capacity - Cell.Present.Num();
	const int64 Cap = Missing * MilliPerBranch;
	// Acumulador cargado acotado antes de sumar, y la ganancia saturada: ninguna suma desborda.
	const int64 Gain = Rate > Cap / Step ? Cap : Step * Rate;
	Cell.Accumulator = FMath::Min(FMath::Min(Cell.Accumulator, Cap) + Gain, Cap);

	int32 Spawned = 0;
	// Sin series libres (solo con una partida manipulada: harían falta 4e9 ramas) no aparece
	// ninguna más: NextSerial++ daría la vuelta a 0 y repetiría una serie que Pick confundiría.
	while (Cell.Accumulator >= MilliPerBranch && Cell.Present.Num() < Capacity && Cell.NextSerial < MAX_uint32)
	{
		Cell.Present.Add(PlaceBranch(Cell.CellSeed, Cell.NextSerial++, Sources));
		Cell.Accumulator -= MilliPerBranch;
		++Spawned;
	}
	if (Cell.Present.Num() >= Capacity)
	{
		Cell.Accumulator = 0;
	}
	return Spawned;
}

bool FGroundBranchModel::Pick(FGroundBranchCell& Cell, const TArray<FGroundBranchSource>& Sources, int64 NowMinute, uint32 Serial,
	FGroundBranch* OutBranch, int32* OutSpawned)
{
	// Con la celda llena, este avance pone el acumulador a cero y el reloj en NowMinute: el
	// hueco que deja la recogida solo empieza a rellenarse desde ahora.
	const int32 Spawned = Advance(Cell, Sources, NowMinute);
	if (OutSpawned)
	{
		*OutSpawned = Spawned;
	}
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

FGroundBranch FGroundBranchModel::PlaceBranch(uint32 CellSeed, uint32 Serial, const TArray<FGroundBranchSource>& Sources)
{
	FGroundBranch Branch;
	Branch.Serial = Serial;

	const int32 Capacity = TotalCapacity(Sources);
	if (Capacity <= 0)
	{
		return Branch;
	}

	// Ejemplar ponderado por capacidad: los árboles grandes sueltan más ramas.
	const int32 Pick = (int32)(ExploredHash::Hash2D(CellSeed, (int32)Serial, 0) % (uint32)Capacity);
	int32 Running = 0;
	for (int32 i = 0; i < Sources.Num(); ++i)
	{
		Running += FMath::Max(0, Sources[i].Capacity);
		if (Pick < Running)
		{
			Branch.SourceIndex = i;
			break;
		}
	}
	const FGroundBranchSource& Source = Sources[Branch.SourceIndex];
	Branch.ItemId = Source.ItemId;

	// Punto uniforme en el disco de la copa.
	const double U = (double)ExploredHash::ToUnitFloat(ExploredHash::Hash2D(CellSeed, (int32)Serial, 1));
	const double V = (double)ExploredHash::ToUnitFloat(ExploredHash::Hash2D(CellSeed, (int32)Serial, 2));
	const double Radius = FMath::Max(0.0f, Source.CrownRadiusMeters) * 100.0 * FMath::Sqrt(U);
	const double Angle = V * 2.0 * UE_DOUBLE_PI;
	Branch.Position = Source.Position + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
	return Branch;
}

bool FGroundBranchModel::NeedsSave(const FGroundBranchCell& Cell, const TArray<FGroundBranchSource>& Sources)
{
	const int32 Capacity = TotalCapacity(Sources);
	if (Cell.Accumulator != 0 || Cell.Present.Num() != Capacity || Cell.NextSerial != (uint32)Capacity)
	{
		return true;
	}
	for (int32 i = 0; i < Cell.Present.Num(); ++i)
	{
		const FGroundBranch& Branch = Cell.Present[i];
		const FGroundBranch Fresh = PlaceBranch(Cell.CellSeed, (uint32)i, Sources);
		// PlaceBranch es determinista: con las mismas fuentes la posición es la misma bit a bit.
		if (Branch.Serial != (uint32)i || Branch.Position != Fresh.Position || Branch.ItemId != Fresh.ItemId
			|| Branch.SourceIndex != Fresh.SourceIndex)
		{
			return true;
		}
	}
	return false;
}

FSaveValue FGroundBranchModel::SaveCell(const FGroundBranchCell& Cell)
{
	FSaveValue Root = FSaveValue::MakeObject();
	Root.Set(TEXT("version"), FSaveValue::MakeInt(SaveVersion));
	Root.Set(TEXT("lastMinute"), FSaveValue::MakeInt(Cell.LastUpdateMinute));
	Root.Set(TEXT("accumulator"), FSaveValue::MakeInt(Cell.Accumulator));
	Root.Set(TEXT("nextSerial"), FSaveValue::MakeInt((int64)Cell.NextSerial));
	FSaveValue& List = Root.Set(TEXT("branches"), FSaveValue::MakeArray());
	for (const FGroundBranch& Branch : Cell.Present)
	{
		FSaveValue Row = FSaveValue::MakeArray();
		Row.Add(FSaveValue::MakeInt((int64)Branch.Serial));
		Row.Add(FSaveValue::MakeDouble(Branch.Position.X));
		Row.Add(FSaveValue::MakeDouble(Branch.Position.Y));
		Row.Add(FSaveValue::MakeString(Branch.ItemId.IsNone() ? FString() : Branch.ItemId.ToString()));
		Row.Add(FSaveValue::MakeInt(Branch.SourceIndex));
		List.Add(MoveTemp(Row));
	}
	return Root;
}

bool FGroundBranchModel::LoadCell(const FSaveValue& Value, uint32 CellSeed, FGroundBranchCell& OutCell)
{
	const FSaveValue* Version = Value.Find(TEXT("version"));
	const FSaveValue* Last = Value.Find(TEXT("lastMinute"));
	const FSaveValue* Acc = Value.Find(TEXT("accumulator"));
	const FSaveValue* Next = Value.Find(TEXT("nextSerial"));
	const FSaveValue* List = Value.Find(TEXT("branches"));
	int64 V = 0, LastMinute = 0, Accumulator = 0, NextSerial = 0;
	if (!Version || !Version->TryGetInt(V) || V != SaveVersion
		|| !Last || !Last->TryGetInt(LastMinute) || LastMinute < -MaxAbsMinute || LastMinute > MaxAbsMinute
		|| !Acc || !Acc->TryGetInt(Accumulator) || Accumulator < 0 || Accumulator > (int64)MaxSavedBranches * MilliPerBranch
		|| !Next || !Next->TryGetInt(NextSerial) || NextSerial < 0 || NextSerial > (int64)MAX_uint32
		|| !List || !List->IsArray() || List->Num() > MaxSavedBranches)
	{
		return false;
	}
	FGroundBranchCell Cell;
	Cell.CellSeed = CellSeed;
	Cell.LastUpdateMinute = LastMinute;
	Cell.Accumulator = Accumulator;
	Cell.NextSerial = (uint32)NextSerial;
	Cell.Present.Reserve(List->Num());
	int64 PreviousSerial = -1;
	for (int32 i = 0; i < List->Num(); ++i)
	{
		const FSaveValue& Row = List->At(i);
		int64 Serial = 0, SourceIndex = 0;
		double X = 0.0, Y = 0.0;
		FString Item;
		if (!Row.IsArray() || Row.Num() != 5
			|| !Row.At(0).TryGetInt(Serial) || !Row.At(1).TryGetDouble(X) || !Row.At(2).TryGetDouble(Y)
			|| !Row.At(3).TryGetString(Item) || !Row.At(4).TryGetInt(SourceIndex))
		{
			return false;
		}
		// Series crecientes (así las deja Advance y así las conserva Pick) y por debajo de NextSerial:
		// si no, la próxima rama repetiría una serie y Pick recogería la que no es.
		if (Serial <= PreviousSerial || Serial >= NextSerial
			|| !FMath::IsFinite(X) || !FMath::IsFinite(Y) || X < -MaxAbsPositionCm || X > MaxAbsPositionCm
			|| Y < -MaxAbsPositionCm || Y > MaxAbsPositionCm
			|| SourceIndex < INDEX_NONE || SourceIndex >= MaxSavedBranches)
		{
			return false;
		}
		PreviousSerial = Serial;
		FGroundBranch& Branch = Cell.Present.AddDefaulted_GetRef();
		Branch.Serial = (uint32)Serial;
		Branch.Position = FVector2D(X, Y);
		Branch.ItemId = Item.IsEmpty() ? FName() : FName(*Item);
		Branch.SourceIndex = (int32)SourceIndex;
	}
	OutCell = MoveTemp(Cell);
	return true;
}
