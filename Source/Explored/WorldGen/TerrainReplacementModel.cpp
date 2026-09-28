#include "WorldGen/TerrainReplacementModel.h"

#include "WorldGen/TerrainNetSyncModel.h"
#include "WorldGen/TerrainRemeshModel.h"

FTerrainReplacementModel::FTerrainReplacementModel(int32 InEditChunksPerRender)
	: EditChunksPerRender(FMath::Max(1, InEditChunksPerRender))
{
}

FIntVector FTerrainReplacementModel::RenderChunkOf(const FIntVector& EditChunk) const
{
	return FTerrainRemeshModel::RenderChunkOf(EditChunk, EditChunksPerRender);
}

bool FTerrainReplacementModel::Request(const FIntVector& RenderChunk)
{
	FEntry& Entry = Entries.FindOrAdd(RenderChunk);
	if (Entry.State != ETerrainReplacementState::Baked)
	{
		return false;
	}
	Entry.State = ETerrainReplacementState::Surveying;
	return true;
}

void FTerrainReplacementModel::SetRequired(const FIntVector& RenderChunk, const TArray<FIntVector>& EditChunks,
	TArray<FIntVector>& OutCompleted)
{
	FEntry* Entry = Entries.Find(RenderChunk);
	if (!Entry || Entry->State != ETerrainReplacementState::Surveying)
	{
		return;
	}
	Entry->Missing.Reset();
	for (const FIntVector& Chunk : EditChunks)
	{
		if (RenderChunkOf(Chunk) == RenderChunk && !Meshed.Contains(Chunk))
		{
			Entry->Missing.Add(Chunk);
		}
	}
	if (Entry->Missing.Num() == 0)
	{
		Entry->State = ETerrainReplacementState::Replaced;
		OutCompleted.Add(RenderChunk);
		return;
	}
	Entry->State = ETerrainReplacementState::Building;
}

void FTerrainReplacementModel::OnEditChunkMeshed(const FIntVector& EditChunk, TArray<FIntVector>& OutCompleted)
{
	Meshed.Add(EditChunk);
	const FIntVector RenderChunk = RenderChunkOf(EditChunk);
	FEntry* Entry = Entries.Find(RenderChunk);
	if (!Entry || Entry->State != ETerrainReplacementState::Building)
	{
		return;
	}
	Entry->Missing.Remove(EditChunk);
	if (Entry->Missing.Num() == 0)
	{
		Entry->State = ETerrainReplacementState::Replaced;
		OutCompleted.Add(RenderChunk);
	}
}

ETerrainReplacementState FTerrainReplacementModel::GetState(const FIntVector& RenderChunk) const
{
	const FEntry* Entry = Entries.Find(RenderChunk);
	return Entry ? Entry->State : ETerrainReplacementState::Baked;
}

int32 FTerrainReplacementModel::NumMissing(const FIntVector& RenderChunk) const
{
	const FEntry* Entry = Entries.Find(RenderChunk);
	return Entry && Entry->State == ETerrainReplacementState::Building ? Entry->Missing.Num() : 0;
}

TArray<FIntVector> FTerrainReplacementModel::ActiveRenderChunks() const
{
	TArray<FIntVector> Out;
	for (const auto& Pair : Entries)
	{
		if (Pair.Value.State != ETerrainReplacementState::Baked)
		{
			Out.Add(Pair.Key);
		}
	}
	FTerrainNetSyncModel::SortUnique(Out);
	return Out;
}

void FTerrainReplacementModel::Reset()
{
	Entries.Reset();
	Meshed.Reset();
}
