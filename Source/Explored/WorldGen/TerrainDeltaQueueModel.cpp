#include "WorldGen/TerrainDeltaQueueModel.h"

void FTerrainDeltaQueueModel::Enqueue(const FIntVector& Chunk, const TArray<FTerrainDeltaCodecModel::FSample>& Samples, double DistanceToReceiverM)
{
	for (FEntry& Entry : Entries)
	{
		if (Entry.Chunk == Chunk)
		{
			// Las muestras nuevas van detrás: Canonicalize hace que la edición más
			// reciente gane cuando dos parches tocan el mismo índice.
			TArray<FTerrainDeltaCodecModel::FSample> Merged = Entry.Samples;
			Merged.Append(Samples);
			FTerrainDeltaCodecModel::Canonicalize(Merged, Entry.Samples);
			Entry.DistanceToReceiverM = DistanceToReceiverM;
			return;
		}
	}

	FEntry NewEntry;
	NewEntry.Chunk = Chunk;
	FTerrainDeltaCodecModel::Canonicalize(Samples, NewEntry.Samples);
	NewEntry.DistanceToReceiverM = DistanceToReceiverM;
	NewEntry.SequenceNumber = NextSequenceNumber++;
	Entries.Add(MoveTemp(NewEntry));
}

bool FTerrainDeltaQueueModel::Contains(const FIntVector& Chunk) const
{
	for (const FEntry& Entry : Entries)
	{
		if (Entry.Chunk == Chunk)
		{
			return true;
		}
	}
	return false;
}

void FTerrainDeltaQueueModel::Reset()
{
	Entries.Reset();
	AvailableBytes = 0.0;
	NextSequenceNumber = 0;
}

void FTerrainDeltaQueueModel::Accrue(double DeltaSeconds)
{
	if (DeltaSeconds <= 0.0)
	{
		return;
	}
	AvailableBytes = FMath::Min(AvailableBytes + SustainedBytesPerSecond * DeltaSeconds, BurstCapacityBytes);
}

int32 FTerrainDeltaQueueModel::IndexOfHighestPriority() const
{
	int32 Best = INDEX_NONE;
	// Primera pasada: solo los chunks cerca del receptor, el más antiguo de ellos.
	for (int32 I = 0; I < Entries.Num(); ++I)
	{
		if (Entries[I].DistanceToReceiverM < PriorityDistanceM)
		{
			if (Best == INDEX_NONE || Entries[I].SequenceNumber < Entries[Best].SequenceNumber)
			{
				Best = I;
			}
		}
	}
	if (Best != INDEX_NONE)
	{
		return Best;
	}
	// Nadie cerca: FIFO puro sobre el resto.
	for (int32 I = 0; I < Entries.Num(); ++I)
	{
		if (Best == INDEX_NONE || Entries[I].SequenceNumber < Entries[Best].SequenceNumber)
		{
			Best = I;
		}
	}
	return Best;
}

bool FTerrainDeltaQueueModel::TryPopWithinBudget(FEntry& Out)
{
	if (Entries.IsEmpty())
	{
		return false;
	}

	const int32 Index = IndexOfHighestPriority();
	check(Index != INDEX_NONE);

	FTerrainDeltaCodecModel::FChunkPatch Patch;
	Patch.Chunk = Entries[Index].Chunk;
	Patch.Samples = Entries[Index].Samples;
	const int32 Cost = FTerrainDeltaCodecModel::EncodedByteCount(Patch);

	if (static_cast<double>(Cost) > AvailableBytes)
	{
		return false;
	}

	AvailableBytes -= static_cast<double>(Cost);
	Out = Entries[Index];
	Entries.RemoveAt(Index);
	return true;
}
