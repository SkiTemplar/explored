#include "WorldGen/TerrainDeltaQueueModel.h"

double FTerrainDeltaQueueModel::SanitizeDistance(double DistanceToReceiverM)
{
	// IsFinite y no IsNaN: con matemáticas rápidas IsNaN puede desaparecer, y −∞ pasaría como 0 (prioritario).
	if (!FMath::IsFinite(DistanceToReceiverM))
	{
		// Sin una distancia fiable no se puede decir que sea relevante: se espera a la siguiente.
		return TNumericLimits<double>::Max();
	}
	return FMath::Max(DistanceToReceiverM, 0.0);
}

int32 FTerrainDeltaQueueModel::Enqueue(const FIntVector& Chunk, const TArray<FTerrainDeltaCodecModel::FSample>& Samples, double DistanceToReceiverM)
{
	if (!FTerrainDeltaCodecModel::IsChunkEncodable(Chunk))
	{
		return Samples.Num();
	}

	int32 Rejected = 0;
	TArray<FTerrainDeltaCodecModel::FSample> Valid;
	Valid.Reserve(Samples.Num());
	for (const FTerrainDeltaCodecModel::FSample& S : Samples)
	{
		if (FTerrainDeltaCodecModel::IsSampleEncodable(S))
		{
			Valid.Add(S);
		}
		else
		{
			++Rejected;
		}
	}
	if (Valid.IsEmpty())
	{
		return Rejected;
	}

	const double Distance = SanitizeDistance(DistanceToReceiverM);
	for (FEntry& Entry : Entries)
	{
		if (Entry.Chunk == Chunk)
		{
			// Las muestras nuevas van detrás: Canonicalize hace que la edición más
			// reciente gane cuando dos parches tocan el mismo índice.
			TArray<FTerrainDeltaCodecModel::FSample> Merged = MoveTemp(Entry.Samples);
			Merged.Append(Valid);
			FTerrainDeltaCodecModel::Canonicalize(Merged, Entry.Samples);
			Entry.DistanceToReceiverM = Distance;
			return Rejected;
		}
	}

	FEntry NewEntry;
	NewEntry.Chunk = Chunk;
	FTerrainDeltaCodecModel::Canonicalize(Valid, NewEntry.Samples);
	NewEntry.DistanceToReceiverM = Distance;
	NewEntry.SequenceNumber = NextSequenceNumber++;
	Entries.Add(MoveTemp(NewEntry));
	return Rejected;
}

bool FTerrainDeltaQueueModel::UpdateDistance(const FIntVector& Chunk, double DistanceToReceiverM)
{
	for (FEntry& Entry : Entries)
	{
		if (Entry.Chunk == Chunk)
		{
			Entry.DistanceToReceiverM = SanitizeDistance(DistanceToReceiverM);
			return true;
		}
	}
	return false;
}

bool FTerrainDeltaQueueModel::Contains(const FIntVector& Chunk) const
{
	return PendingSamples(Chunk) != nullptr;
}

const TArray<FTerrainDeltaCodecModel::FSample>* FTerrainDeltaQueueModel::PendingSamples(const FIntVector& Chunk) const
{
	for (const FEntry& Entry : Entries)
	{
		if (Entry.Chunk == Chunk)
		{
			return &Entry.Samples;
		}
	}
	return nullptr;
}

void FTerrainDeltaQueueModel::Reset()
{
	Entries.Reset();
	SustainedBytes = 0.0;
	ClockSeconds = 0.0;
	RecentSends.Reset();
	NextSequenceNumber = 0;
}

void FTerrainDeltaQueueModel::Accrue(double DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0)
	{
		return;
	}
	SustainedBytes = FMath::Min(SustainedBytes + SustainedBytesPerSecond * DeltaSeconds, BurstCreditBytes);
	ClockSeconds += DeltaSeconds;
	// Un envío en t cuenta en la ventana (Reloj − 1 s, Reloj]: sale cuando ha pasado un segundo entero.
	int32 Expired = 0;
	while (Expired < RecentSends.Num() && RecentSends[Expired].AtSeconds <= ClockSeconds - 1.0)
	{
		++Expired;
	}
	RecentSends.RemoveAt(0, Expired);
}

double FTerrainDeltaQueueModel::BytesInPeakWindow() const
{
	double Sum = 0.0;
	for (const FSent& Sent : RecentSends)
	{
		Sum += static_cast<double>(Sent.Bytes);
	}
	return Sum;
}

double FTerrainDeltaQueueModel::AvailableBudgetBytes() const
{
	return FMath::Max(0.0, FMath::Min(SustainedBytes, BurstBytesPerSecond - BytesInPeakWindow()));
}

int32 FTerrainDeltaQueueModel::IndexOfHighestPriority() const
{
	int32 Best = INDEX_NONE;
	bool bBestIsNear = false;
	for (int32 I = 0; I < Entries.Num(); ++I)
	{
		const FEntry& E = Entries[I];
		if (E.DistanceToReceiverM > RelevanceDistanceM)
		{
			continue;
		}
		const bool bNear = E.DistanceToReceiverM < PriorityDistanceM;
		if (Best == INDEX_NONE
			|| (bNear && !bBestIsNear)
			|| (bNear == bBestIsNear && E.SequenceNumber < Entries[Best].SequenceNumber))
		{
			Best = I;
			bBestIsNear = bNear;
		}
	}
	return Best;
}

bool FTerrainDeltaQueueModel::TryPopPacket(FOutgoingPacket& Out)
{
	const int32 Index = IndexOfHighestPriority();
	if (Index == INDEX_NONE)
	{
		return false;
	}

	FEntry& Entry = Entries[Index];
	int32 Consumed = 0;
	TArray<uint8> Bytes = FTerrainDeltaCodecModel::EncodeFirstPacket(Entry.Chunk, Entry.Samples, Consumed);
	if (Consumed <= 0)
	{
		// No debería pasar (Enqueue solo guarda muestras válidas y canónicas); si pasa, la
		// entrada es irrecuperable y se quita para no bloquear la cola para siempre.
		Entries.RemoveAt(Index);
		return false;
	}

	const double Cost = static_cast<double>(Bytes.Num());
	if (Cost > AvailableBudgetBytes())
	{
		return false;
	}

	SustainedBytes -= Cost;
	RecentSends.Add(FSent{ ClockSeconds, Bytes.Num() });
	Out.Chunk = Entry.Chunk;
	Out.Bytes = MoveTemp(Bytes);
	Entry.Samples.RemoveAt(0, Consumed);
	Out.bLastOfChunk = Entry.Samples.IsEmpty();
	if (Out.bLastOfChunk)
	{
		Entries.RemoveAt(Index);
	}
	return true;
}
