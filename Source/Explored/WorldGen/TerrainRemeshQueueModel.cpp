#include "WorldGen/TerrainRemeshQueueModel.h"

#include "WorldGen/TerrainNetSyncModel.h"

void FTerrainRemeshQueueModel::MarkDirty(const FIntVector& Chunk)
{
	Dirty.Add(Chunk);
}

void FTerrainRemeshQueueModel::MarkDirty(const TArray<FIntVector>& InChunks)
{
	for (const FIntVector& Chunk : InChunks)
	{
		Dirty.Add(Chunk);
	}
}

void FTerrainRemeshQueueModel::SelectStarts(double Now, int32 MaxInFlight, int32 MaxStarts,
	TFunctionRef<double(const FIntVector&)> Priority, TArray<FIntVector>& Out)
{
	if (!FMath::IsFinite(Now) || MaxStarts <= 0 || InFlight.Num() >= MaxInFlight)
	{
		return;
	}
	struct FCandidate
	{
		FIntVector Chunk;
		double Priority = 0.0;
	};
	TArray<FCandidate> Candidates;
	for (const FIntVector& Chunk : Dirty)
	{
		if (InFlight.Contains(Chunk))
		{
			continue;
		}
		const double* Last = LastStart.Find(Chunk);
		if (Last && Now - *Last < MinSecondsBetweenStarts)
		{
			continue;
		}
		const double P = Priority(Chunk);
		Candidates.Add({Chunk, FMath::IsFinite(P) ? P : TNumericLimits<double>::Max()});
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
		{
			return A.Priority != B.Priority ? A.Priority < B.Priority : FTerrainNetSyncModel::ChunkLess(A.Chunk, B.Chunk);
		});

	const int32 Room = FMath::Min(MaxStarts, MaxInFlight - InFlight.Num());
	for (int32 I = 0; I < Candidates.Num() && I < Room; ++I)
	{
		const FIntVector& Chunk = Candidates[I].Chunk;
		Dirty.Remove(Chunk);
		InFlight.Add(Chunk);
		LastStart.Add(Chunk, Now);
		Out.Add(Chunk);
	}

	// Poda: un arranque antiguo ya no frena nada.
	TArray<FIntVector> Stale;
	for (const auto& Pair : LastStart)
	{
		if (Now - Pair.Value >= MinSecondsBetweenStarts && !InFlight.Contains(Pair.Key))
		{
			Stale.Add(Pair.Key);
		}
	}
	for (const FIntVector& Chunk : Stale)
	{
		LastStart.Remove(Chunk);
	}
}

void FTerrainRemeshQueueModel::MarkFinished(const FIntVector& Chunk)
{
	InFlight.Remove(Chunk);
}

void FTerrainRemeshQueueModel::Reset()
{
	Dirty.Reset();
	InFlight.Reset();
	LastStart.Reset();
}
