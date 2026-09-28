#include "WorldGen/TerrainChunkChecksumModel.h"

namespace TerrainChunkChecksumDetail
{
	constexpr uint32 FnvOffsetBasis32 = 0x811C9DC5u;
	constexpr uint32 FnvPrime32 = 0x01000193u;

	void FoldByte(uint32& Hash, uint8 Byte)
	{
		Hash ^= Byte;
		Hash *= FnvPrime32;
	}

	/** Pliega un entero de 32 bits en orden little-endian, byte a byte. */
	void FoldInt32LE(uint32& Hash, int32 Value)
	{
		const uint32 U = static_cast<uint32>(Value);
		FoldByte(Hash, static_cast<uint8>(U & 0xFF));
		FoldByte(Hash, static_cast<uint8>((U >> 8) & 0xFF));
		FoldByte(Hash, static_cast<uint8>((U >> 16) & 0xFF));
		FoldByte(Hash, static_cast<uint8>((U >> 24) & 0xFF));
	}
}

uint32 FTerrainChunkChecksumModel::Compute(const TArray<FTerrainDeltaCodecModel::FSample>& Samples)
{
	TArray<FTerrainDeltaCodecModel::FSample> Sorted;
	FTerrainDeltaCodecModel::Canonicalize(Samples, Sorted);

	uint32 Hash = TerrainChunkChecksumDetail::FnvOffsetBasis32;
	for (const FTerrainDeltaCodecModel::FSample& S : Sorted)
	{
		if (S.DeltaMm == 0)
		{
			continue;
		}
		TerrainChunkChecksumDetail::FoldInt32LE(Hash, S.LocalIndex);
		TerrainChunkChecksumDetail::FoldInt32LE(Hash, S.DeltaMm);
	}
	return Hash;
}

uint32 FTerrainChunkChecksumModel::ComputeState(const FTerrainDeltaCodecModel::FChunkState& State)
{
	return Compute(FTerrainDeltaCodecModel::SamplesOf(State));
}

void FTerrainChunkChecksumModel::FTracker::DueChunks(const TMap<FIntVector, uint32>& CurrentChecksums, double NowSeconds, TArray<FIntVector>& OutDue)
{
	OutDue.Reset();
	if (!FMath::IsFinite(NowSeconds))
	{
		return;
	}
	for (const auto& Pair : CurrentChecksums)
	{
		const FIntVector& Chunk = Pair.Key;
		const uint32 Checksum = Pair.Value;

		const FChunkRecord* Existing = Records.Find(Chunk);
		const bool bDue = !Existing || !Existing->bEverSent
			|| NowSeconds < Existing->LastSentAtSeconds
			|| (NowSeconds - Existing->LastSentAtSeconds) >= VerificationIntervalSeconds;
		if (!bDue)
		{
			continue;
		}

		OutDue.Add(Chunk);
		FChunkRecord& Updated = Records.FindOrAdd(Chunk);
		Updated.LastChecksum = Checksum;
		Updated.LastSentAtSeconds = NowSeconds;
		Updated.bEverSent = true;
	}
	OutDue.Sort([](const FIntVector& A, const FIntVector& B)
	{
		if (A.X != B.X) { return A.X < B.X; }
		if (A.Y != B.Y) { return A.Y < B.Y; }
		return A.Z < B.Z;
	});
}

bool FTerrainChunkChecksumModel::FTracker::ConfirmAndCheck(const FIntVector& Chunk, uint32 ClientChecksum) const
{
	const FChunkRecord* Record = Records.Find(Chunk);
	if (!Record || !Record->bEverSent)
	{
		return false;
	}
	return Record->LastChecksum == ClientChecksum;
}
