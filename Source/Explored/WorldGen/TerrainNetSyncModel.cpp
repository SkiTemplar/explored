#include "WorldGen/TerrainNetSyncModel.h"

bool FTerrainNetSyncModel::ChunkLess(const FIntVector& A, const FIntVector& B)
{
	if (A.Z != B.Z)
	{
		return A.Z < B.Z;
	}
	if (A.Y != B.Y)
	{
		return A.Y < B.Y;
	}
	return A.X < B.X;
}

void FTerrainNetSyncModel::SortUnique(TArray<FIntVector>& InOut)
{
	InOut.Sort(&FTerrainNetSyncModel::ChunkLess);
	int32 Write = 0;
	for (int32 Read = 0; Read < InOut.Num(); ++Read)
	{
		if (Write == 0 || InOut[Write - 1] != InOut[Read])
		{
			InOut[Write++] = InOut[Read];
		}
	}
	InOut.SetNum(Write);
}

TArray<FTerrainDeltaCodecModel::FChunkPatch> FTerrainNetSyncModel::PatchesForSamples(const FTerrainEditModel& Model,
	const TArray<FIntVector>& ChangedSamples)
{
	TMap<FIntVector, TArray<FTerrainDeltaCodecModel::FSample>> ByChunk;
	for (const FIntVector& Global : ChangedSamples)
	{
		FTerrainDeltaCodecModel::FSample Sample;
		Sample.LocalIndex = Model.LocalIndexOf(Global);
		Sample.DeltaMm = Model.SampleDeltaMm(Global);
		ByChunk.FindOrAdd(Model.ChunkOfSample(Global)).Add(Sample);
	}
	TArray<FIntVector> Keys;
	ByChunk.GetKeys(Keys);
	SortUnique(Keys);

	TArray<FTerrainDeltaCodecModel::FChunkPatch> Patches;
	Patches.Reserve(Keys.Num());
	for (const FIntVector& Chunk : Keys)
	{
		FTerrainDeltaCodecModel::FChunkPatch& Patch = Patches.AddDefaulted_GetRef();
		Patch.Chunk = Chunk;
		FTerrainDeltaCodecModel::Canonicalize(ByChunk.FindChecked(Chunk), Patch.Samples);
	}
	return Patches;
}

FTerrainDeltaCodecModel::FChunkPatch FTerrainNetSyncModel::FullChunkPatch(const FTerrainEditModel& Model, const FIntVector& Chunk)
{
	FTerrainDeltaCodecModel::FChunkPatch Patch;
	Patch.Chunk = Chunk;
	if (const TMap<int32, int32>* Deltas = Model.FindChunkDeltas(Chunk))
	{
		Patch.Samples = FTerrainDeltaCodecModel::SamplesOf(*Deltas);
	}
	return Patch;
}

bool FTerrainNetSyncModel::ApplyPacket(FTerrainEditModel& Model, const FTerrainDeltaCodecModel::FPacket& Packet,
	TArray<FIntVector>& OutDirty, TArray<FIntVector>* OutChangedSamples)
{
	// Primero se valida todo: un paquete a medias no se aplica nunca.
	TArray<TPair<FIntVector, int32>> Writes;
	Writes.Reserve(Packet.Samples.Num());
	for (const FTerrainDeltaCodecModel::FSample& Sample : Packet.Samples)
	{
		FIntVector Global;
		if (!Model.LocalToGlobal(Packet.Chunk, Sample.LocalIndex, Global)
			|| FMath::Abs(Sample.DeltaMm) > FTerrainEditModel::MaxDeltaMm)
		{
			return false;
		}
		Writes.Emplace(Global, Sample.DeltaMm);
	}

	for (const TPair<FIntVector, int32>& Write : Writes)
	{
		if (Model.SampleDeltaMm(Write.Key) == Write.Value)
		{
			continue;
		}
		Model.SetSampleDeltaMm(Write.Key, Write.Value);
		Model.ChunksReadingSample(Write.Key, OutDirty);
		if (OutChangedSamples)
		{
			OutChangedSamples->Add(Write.Key);
		}
	}
	SortUnique(OutDirty);
	return true;
}

FBox FTerrainNetSyncModel::SamplesBounds(const FTerrainEditModel& Model, const TArray<FIntVector>& Samples)
{
	FBox Box(ForceInit);
	for (const FIntVector& Global : Samples)
	{
		Box += Model.SamplePosition(Global);
	}
	return Box;
}
