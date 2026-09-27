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
	const int64 Step = FMath::Min(NowMinute - Cell.LastUpdateMinute, MaxStepMinutes);
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
	Cell.Accumulator = FMath::Min(Cell.Accumulator + Step * Rate, Missing * MilliPerBranch);

	int32 Spawned = 0;
	while (Cell.Accumulator >= MilliPerBranch && Cell.Present.Num() < Capacity)
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
