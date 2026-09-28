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

	/**
	 * Escribe un paquete con los tramos contiguos de `Sorted[First..]` que quepan en
	 * MaxPacketBytes y devuelve cuántas muestras ha metido. `Sorted` es canónica y todas
	 * sus muestras son codificables. Un tramo nunca se parte entre paquetes salvo al llegar
	 * a MaxRunCount, que por construcción cabe él solo en un paquete vacío.
	 */
	int32 WritePacket(const FIntVector& Chunk, const TArray<FTerrainDeltaCodecModel::FSample>& Sorted, int32 First, TArray<uint8>& OutPacket)
	{
		using FCodec = FTerrainDeltaCodecModel;
		OutPacket.Reset();
		OutPacket.Reserve(FCodec::MaxPacketBytes);
		OutPacket.Add(FCodec::CurrentVersion);
		WriteInt16LE(OutPacket, static_cast<int16>(Chunk.X));
		WriteInt16LE(OutPacket, static_cast<int16>(Chunk.Y));
		WriteInt16LE(OutPacket, static_cast<int16>(Chunk.Z));
		const int32 NumRunsOffset = OutPacket.Num();
		WriteUint16LE(OutPacket, 0);

		int32 NumRuns = 0;
		int32 I = First;
		while (I < Sorted.Num())
		{
			// Longitud del tramo contiguo que empieza en I, acotada por el formato.
			int32 Count = 1;
			while (I + Count < Sorted.Num()
				&& Count < FCodec::MaxRunCount
				&& Sorted[I + Count].LocalIndex == Sorted[I].LocalIndex + Count)
			{
				++Count;
			}
			const int32 Room = FCodec::MaxPacketBytes - OutPacket.Num() - FCodec::RunOverheadBytes;
			if (Room < 2)
			{
				break;
			}
			// Lo que no cabe se queda para el paquete siguiente: el tramo se parte ahí.
			Count = FMath::Min(Count, Room / 2);

			WriteUint16LE(OutPacket, static_cast<uint16>(Sorted[I].LocalIndex));
			OutPacket.Add(static_cast<uint8>(Count));
			for (int32 K = 0; K < Count; ++K)
			{
				WriteInt16LE(OutPacket, static_cast<int16>(Sorted[I + K].DeltaMm));
			}
			I += Count;
			++NumRuns;
		}

		OutPacket[NumRunsOffset] = static_cast<uint8>(NumRuns & 0xFF);
		OutPacket[NumRunsOffset + 1] = static_cast<uint8>((NumRuns >> 8) & 0xFF);
		return I - First;
	}
}

bool FTerrainDeltaCodecModel::IsChunkEncodable(const FIntVector& Chunk)
{
	auto Fits = [](int32 V) { return V >= -32768 && V <= 32767; };
	return Fits(Chunk.X) && Fits(Chunk.Y) && Fits(Chunk.Z);
}

bool FTerrainDeltaCodecModel::IsSampleEncodable(const FSample& Sample)
{
	return Sample.LocalIndex >= 0 && Sample.LocalIndex <= MaxLocalIndex
		&& Sample.DeltaMm >= MinDeltaMm && Sample.DeltaMm <= MaxDeltaMm;
}

bool FTerrainDeltaCodecModel::QuantizeDeltaMm(double DeltaMeters, int32& OutMm)
{
	if (!FMath::IsFinite(DeltaMeters))
	{
		OutMm = 0;
		return false;
	}
	// Se compara en double antes de convertir: 1e30 m pasado a int32 sería comportamiento indefinido.
	const double Mm = DeltaMeters * 1000.0;
	const double Rounded = Mm < 0.0 ? -FMath::FloorToDouble(-Mm + 0.5) : FMath::FloorToDouble(Mm + 0.5);
	if (Rounded > static_cast<double>(MaxDeltaMm))
	{
		OutMm = MaxDeltaMm;
		return false;
	}
	if (Rounded < static_cast<double>(MinDeltaMm))
	{
		OutMm = MinDeltaMm;
		return false;
	}
	OutMm = static_cast<int32>(Rounded);
	return true;
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

TArray<TArray<uint8>> FTerrainDeltaCodecModel::Encode(const FChunkPatch& Patch, int32* OutRejected)
{
	TArray<TArray<uint8>> Packets;
	if (OutRejected)
	{
		*OutRejected = 0;
	}
	if (!IsChunkEncodable(Patch.Chunk))
	{
		if (OutRejected)
		{
			*OutRejected = Patch.Samples.Num();
		}
		return Packets;
	}

	// Un paquete que produce este códec siempre es válido: se descarta lo que no cabría
	// en el formato de cable en vez de generar basura silenciosa.
	TArray<FSample> Filtered;
	Filtered.Reserve(Patch.Samples.Num());
	for (const FSample& S : Patch.Samples)
	{
		if (IsSampleEncodable(S))
		{
			Filtered.Add(S);
		}
		else if (OutRejected)
		{
			++*OutRejected;
		}
	}

	TArray<FSample> Sorted;
	Canonicalize(Filtered, Sorted);

	int32 Next = 0;
	while (Next < Sorted.Num())
	{
		TArray<uint8> Packet;
		Next += TerrainDeltaCodecDetail::WritePacket(Patch.Chunk, Sorted, Next, Packet);
		Packets.Add(MoveTemp(Packet));
	}
	return Packets;
}

TArray<uint8> FTerrainDeltaCodecModel::EncodeFirstPacket(const FIntVector& Chunk, const TArray<FSample>& CanonicalSamples, int32& OutConsumed)
{
	OutConsumed = 0;
	TArray<uint8> Packet;
	if (CanonicalSamples.IsEmpty() || !IsChunkEncodable(Chunk))
	{
		return Packet;
	}
	for (int32 I = 0; I < CanonicalSamples.Num(); ++I)
	{
		if (!IsSampleEncodable(CanonicalSamples[I]) || (I > 0 && CanonicalSamples[I].LocalIndex <= CanonicalSamples[I - 1].LocalIndex))
		{
			return Packet;
		}
	}
	OutConsumed = TerrainDeltaCodecDetail::WritePacket(Chunk, CanonicalSamples, 0, Packet);
	return Packet;
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

	if (NumRuns == 0)
	{
		// Este códec nunca manda un paquete sin tramos.
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

void FTerrainDeltaCodecModel::ApplyPacket(const FPacket& Packet, FChunkState& InOutState)
{
	for (const FSample& S : Packet.Samples)
	{
		if (S.DeltaMm == 0)
		{
			InOutState.Remove(S.LocalIndex);
		}
		else
		{
			InOutState.Add(S.LocalIndex, S.DeltaMm);
		}
	}
}

bool FTerrainDeltaCodecModel::DecodeAndApply(const TArray<uint8>& Bytes, TMap<FIntVector, FChunkState>& InOutWorld)
{
	FPacket Packet;
	if (!Decode(Bytes, Packet))
	{
		return false;
	}
	FChunkState& State = InOutWorld.FindOrAdd(Packet.Chunk);
	ApplyPacket(Packet, State);
	if (State.Num() == 0)
	{
		InOutWorld.Remove(Packet.Chunk);
	}
	return true;
}

TArray<FTerrainDeltaCodecModel::FSample> FTerrainDeltaCodecModel::SamplesOf(const FChunkState& State)
{
	TArray<FSample> Samples;
	Samples.Reserve(State.Num());
	for (const auto& Pair : State)
	{
		Samples.Add(FSample{ Pair.Key, Pair.Value });
	}
	Samples.Sort(&TerrainDeltaCodecDetail::SampleLess);
	return Samples;
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
