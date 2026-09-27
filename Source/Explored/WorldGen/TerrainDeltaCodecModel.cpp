#include "WorldGen/TerrainDeltaCodecModel.h"

namespace TerrainDeltaCodecDetail
{
	bool SampleLess(const FTerrainDeltaCodecModel::FSample& A, const FTerrainDeltaCodecModel::FSample& B)
	{
		return A.LocalIndex < B.LocalIndex;
	}

	void WriteUint16LE(TArray<uint8>& Out, uint16 Value)
	{
		Out.Add(static_cast<uint8>(Value & 0xFF));
		Out.Add(static_cast<uint8>((Value >> 8) & 0xFF));
	}

	void WriteInt16LE(TArray<uint8>& Out, int16 Value)
	{
		WriteUint16LE(Out, static_cast<uint16>(Value));
	}

	bool ReadUint16LE(const TArray<uint8>& Bytes, int32& Cursor, uint16& Out)
	{
		if (Cursor < 0 || Cursor + 2 > Bytes.Num())
		{
			return false;
		}
		Out = static_cast<uint16>(static_cast<uint16>(Bytes[Cursor]) | (static_cast<uint16>(Bytes[Cursor + 1]) << 8));
		Cursor += 2;
		return true;
	}

	bool ReadInt16LE(const TArray<uint8>& Bytes, int32& Cursor, int16& Out)
	{
		uint16 Raw = 0;
		if (!ReadUint16LE(Bytes, Cursor, Raw))
		{
			return false;
		}
		Out = static_cast<int16>(Raw);
		return true;
	}

	bool ReadUint8(const TArray<uint8>& Bytes, int32& Cursor, uint8& Out)
	{
		if (Cursor < 0 || Cursor + 1 > Bytes.Num())
		{
			return false;
		}
		Out = Bytes[Cursor];
		++Cursor;
		return true;
	}

	/** Tramo de muestras contiguas (LocalIndex consecutivo) antes de volcarlo al cable. */
	struct FRun
	{
		int32 FirstSample = 0;
		TArray<int32> Deltas;
	};

	/** Agrupa muestras ya canónicas (ordenadas, únicas) en tramos contiguos de como mucho MaxRunCount. */
	void BuildRuns(const TArray<FTerrainDeltaCodecModel::FSample>& Sorted, TArray<FRun>& OutRuns)
	{
		OutRuns.Reset();
		int32 I = 0;
		while (I < Sorted.Num())
		{
			FRun Run;
			Run.FirstSample = Sorted[I].LocalIndex;
			Run.Deltas.Add(Sorted[I].DeltaMm);
			int32 Prev = Sorted[I].LocalIndex;
			++I;
			while (I < Sorted.Num()
				&& Sorted[I].LocalIndex == Prev + 1
				&& Run.Deltas.Num() < FTerrainDeltaCodecModel::MaxRunCount)
			{
				Run.Deltas.Add(Sorted[I].DeltaMm);
				Prev = Sorted[I].LocalIndex;
				++I;
			}
			OutRuns.Add(MoveTemp(Run));
		}
	}
}

void FTerrainDeltaCodecModel::Canonicalize(const TArray<FSample>& Samples, TArray<FSample>& OutSorted)
{
	// TMap conserva "el último gana" al reinsertar la misma clave (ver TMap::Add), que es
	// justo la semántica de fusión que pide la biblia: la edición más reciente manda.
	TMap<int32, int32> ByIndex;
	for (const FSample& S : Samples)
	{
		ByIndex.Add(S.LocalIndex, S.DeltaMm);
	}

	OutSorted.Reset();
	OutSorted.Reserve(ByIndex.Num());
	for (const auto& Pair : ByIndex)
	{
		OutSorted.Add(FSample{ Pair.Key, Pair.Value });
	}
	OutSorted.Sort(&TerrainDeltaCodecDetail::SampleLess);
}

TArray<TArray<uint8>> FTerrainDeltaCodecModel::Encode(const FChunkPatch& Patch)
{
	TArray<TArray<uint8>> Packets;

	// Un paquete que produce este códec siempre es válido: se descarta lo que no cabría
	// en el formato de cable en vez de generar basura silenciosa.
	TArray<FSample> Filtered;
	Filtered.Reserve(Patch.Samples.Num());
	for (const FSample& S : Patch.Samples)
	{
		if (S.LocalIndex >= 0 && S.LocalIndex <= MaxLocalIndex && S.DeltaMm >= MinDeltaMm && S.DeltaMm <= MaxDeltaMm)
		{
			Filtered.Add(S);
		}
	}

	TArray<FSample> Sorted;
	Canonicalize(Filtered, Sorted);
	if (Sorted.IsEmpty())
	{
		return Packets;
	}

	TArray<TerrainDeltaCodecDetail::FRun> Runs;
	TerrainDeltaCodecDetail::BuildRuns(Sorted, Runs);

	TArray<uint8> CurrentBody;
	int32 CurrentRunCount = 0;

	auto FlushPacket = [&]()
	{
		if (CurrentRunCount == 0)
		{
			return;
		}
		TArray<uint8> Packet;
		Packet.Reserve(HeaderBytes + CurrentBody.Num());
		Packet.Add(CurrentVersion);
		TerrainDeltaCodecDetail::WriteInt16LE(Packet, static_cast<int16>(Patch.Chunk.X));
		TerrainDeltaCodecDetail::WriteInt16LE(Packet, static_cast<int16>(Patch.Chunk.Y));
		TerrainDeltaCodecDetail::WriteInt16LE(Packet, static_cast<int16>(Patch.Chunk.Z));
		TerrainDeltaCodecDetail::WriteUint16LE(Packet, static_cast<uint16>(CurrentRunCount));
		Packet.Append(CurrentBody);
		Packets.Add(MoveTemp(Packet));
		CurrentBody.Reset();
		CurrentRunCount = 0;
	};

	for (const TerrainDeltaCodecDetail::FRun& Run : Runs)
	{
		const int32 RunBytes = RunOverheadBytes + Run.Deltas.Num() * 2;
		// MaxRunCount garantiza que un tramo, él solo, siempre cabe en un paquete vacío:
		// este flush solo dispara cuando el paquete en curso ya tiene contenido.
		if (CurrentRunCount > 0 && HeaderBytes + CurrentBody.Num() + RunBytes > MaxPacketBytes)
		{
			FlushPacket();
		}
		TerrainDeltaCodecDetail::WriteUint16LE(CurrentBody, static_cast<uint16>(Run.FirstSample));
		CurrentBody.Add(static_cast<uint8>(Run.Deltas.Num()));
		for (const int32 Delta : Run.Deltas)
		{
			TerrainDeltaCodecDetail::WriteInt16LE(CurrentBody, static_cast<int16>(Delta));
		}
		++CurrentRunCount;
	}
	FlushPacket();

	return Packets;
}

bool FTerrainDeltaCodecModel::Decode(const TArray<uint8>& Bytes, FPacket& Out)
{
	using namespace TerrainDeltaCodecDetail;

	if (Bytes.Num() > MaxPacketBytes || Bytes.Num() < HeaderBytes)
	{
		return false;
	}

	int32 Cursor = 0;
	uint8 Version = 0;
	if (!ReadUint8(Bytes, Cursor, Version) || Version != CurrentVersion)
	{
		return false;
	}

	int16 ChunkX = 0, ChunkY = 0, ChunkZ = 0;
	uint16 NumRuns = 0;
	if (!ReadInt16LE(Bytes, Cursor, ChunkX) || !ReadInt16LE(Bytes, Cursor, ChunkY)
		|| !ReadInt16LE(Bytes, Cursor, ChunkZ) || !ReadUint16LE(Bytes, Cursor, NumRuns))
	{
		return false;
	}

	TArray<FSample> Samples;
	int32 LastLocalIndexUsed = -1;
	// NumRuns es un uint16: se recorre con un contador más ancho para que no pueda
	// envolver a 0 y convertir un paquete corrupto en un bucle infinito.
	for (int32 R = 0; R < static_cast<int32>(NumRuns); ++R)
	{
		uint16 FirstSample = 0;
		uint8 Count = 0;
		if (!ReadUint16LE(Bytes, Cursor, FirstSample) || !ReadUint8(Bytes, Cursor, Count))
		{
			return false;
		}
		if (Count == 0)
		{
			// El formato dice "Count (1..255)": un tramo vacío es un paquete manipulado.
			return false;
		}
		const int32 LastSample = static_cast<int32>(FirstSample) + static_cast<int32>(Count) - 1;
		if (LastSample > MaxLocalIndex)
		{
			return false;
		}
		if (static_cast<int32>(FirstSample) <= LastLocalIndexUsed)
		{
			// Tramos desordenados o solapados: no los produce este códec, es manipulación.
			return false;
		}
		for (int32 I = 0; I < static_cast<int32>(Count); ++I)
		{
			int16 Delta = 0;
			if (!ReadInt16LE(Bytes, Cursor, Delta))
			{
				return false;
			}
			Samples.Add(FSample{ static_cast<int32>(FirstSample) + I, static_cast<int32>(Delta) });
		}
		LastLocalIndexUsed = LastSample;
	}

	if (Cursor != Bytes.Num())
	{
		// Sobran bytes al final: o el paquete está manipulado, o truncado dejando basura.
		return false;
	}

	Out.Chunk = FIntVector(static_cast<int32>(ChunkX), static_cast<int32>(ChunkY), static_cast<int32>(ChunkZ));
	Out.Samples = MoveTemp(Samples);
	return true;
}

int32 FTerrainDeltaCodecModel::EncodedByteCount(const FChunkPatch& Patch)
{
	int32 Total = 0;
	for (const TArray<uint8>& Packet : Encode(Patch))
	{
		Total += Packet.Num();
	}
	return Total;
}
