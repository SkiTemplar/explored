#include "Misc/AutomationTest.h"

#include <limits>

#include "WorldGen/TerrainChunkChecksumModel.h"
#include "WorldGen/TerrainDeltaCodecModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FTerrainDeltaCodecModelSpec, "Explored.TerrainDeltaCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	using FSample = FTerrainDeltaCodecModel::FSample;
	using FChunkPatch = FTerrainDeltaCodecModel::FChunkPatch;
	using FPacket = FTerrainDeltaCodecModel::FPacket;

	/** Toma todos los paquetes de un parche y los decodifica de vuelta a una única lista de muestras, en orden. */
	bool DecodeAll(const TArray<TArray<uint8>>& Packets, const FIntVector& ExpectedChunk, TArray<FSample>& OutSamples)
	{
		OutSamples.Reset();
		for (const TArray<uint8>& PacketBytes : Packets)
		{
			FPacket Decoded;
			if (!FTerrainDeltaCodecModel::Decode(PacketBytes, Decoded) || Decoded.Chunk != ExpectedChunk)
			{
				return false;
			}
			OutSamples.Append(Decoded.Samples);
		}
		return true;
	}

	/** xorshift32: pseudoaleatorio determinista para los tests de fuzz (mismo resultado en el host y en el editor). */
	uint32 NextRandom(uint32& State)
	{
		State ^= State << 13;
		State ^= State >> 17;
		State ^= State << 5;
		return State;
	}

	/** Estado de un chunk a partir de una lista de muestras (0 borra, como FTerrainEditModel). */
	FTerrainDeltaCodecModel::FChunkState StateOf(const TArray<FSample>& Samples)
	{
		FTerrainDeltaCodecModel::FChunkState State;
		for (const FSample& S : Samples)
		{
			if (S.DeltaMm == 0) { State.Remove(S.LocalIndex); } else { State.Add(S.LocalIndex, S.DeltaMm); }
		}
		return State;
	}

	/** Parche disperso y variado: tramos cortos y largos, huecos, ceros y extremos. */
	FChunkPatch MakeScatteredPatch(uint32 Seed, int32 NumSamples)
	{
		FChunkPatch Patch;
		Patch.Chunk = FIntVector(static_cast<int32>(Seed % 200) - 100, -7, 3);
		uint32 State = Seed | 1u;
		int32 Index = static_cast<int32>(NextRandom(State) % 1000);
		for (int32 I = 0; I < NumSamples && Index <= FTerrainDeltaCodecModel::MaxLocalIndex; ++I)
		{
			const uint32 R = NextRandom(State);
			const int32 Delta = (R % 17 == 0) ? 0 : (R % 23 == 0 ? -32768 : (R % 29 == 0 ? 32767 : static_cast<int32>(R % 4001) - 2000));
			Patch.Samples.Add(FSample{ Index, Delta });
			Index += (R % 5 == 0) ? 1 + static_cast<int32>((R >> 8) % 40) : 1;
		}
		return Patch;
	}
END_DEFINE_SPEC(FTerrainDeltaCodecModelSpec)

void FTerrainDeltaCodecModelSpec::Define()
{
	Describe("Encode/Decode", [this]()
	{
		It("hace ida y vuelta exacta de un parche pequeño, incluidos los extremos del rango", [this]()
		{
			FChunkPatch Patch;
			Patch.Chunk = FIntVector(-3, 7, 0);
			Patch.Samples = {
				FSample{ 0, -32768 },
				FSample{ 100, 25 },
				FSample{ FTerrainDeltaCodecModel::MaxLocalIndex, 32767 },
			};

			const TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch);
			TestEqual(TEXT("Un parche pequeño cabe en un paquete"), Packets.Num(), 1);
			TestTrue(TEXT("El único paquete no pasa del tope"), Packets[0].Num() <= FTerrainDeltaCodecModel::MaxPacketBytes);

			TArray<FSample> RoundTripped;
			TestTrue(TEXT("Decodifica sin error"), DecodeAll(Packets, Patch.Chunk, RoundTripped));

			TArray<FSample> Expected;
			FTerrainDeltaCodecModel::Canonicalize(Patch.Samples, Expected);
			TestTrue(TEXT("Las muestras vuelven exactamente igual, en orden"), RoundTripped == Expected);
		});

		It("reparte una edición grande en varios paquetes acotados, sin perder ni reordenar muestras", [this]()
		{
			FChunkPatch Patch;
			Patch.Chunk = FIntVector(12, -4, 2);
			// 1000 muestras contiguas: más de 4 veces MaxRunCount, así que fuerza varios tramos y paquetes.
			for (int32 I = 0; I < 1000; ++I)
			{
				Patch.Samples.Add(FSample{ I, (I % 7) - 3 });
			}

			const TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch);
			TestTrue(TEXT("Una edición grande no cabe en un solo paquete"), Packets.Num() > 1);
			for (const TArray<uint8>& PacketBytes : Packets)
			{
				TestTrue(TEXT("Cada paquete respeta el tope de 512 B"), PacketBytes.Num() <= FTerrainDeltaCodecModel::MaxPacketBytes);
			}

			TArray<FSample> RoundTripped;
			TestTrue(TEXT("Decodifica todos los paquetes sin error"), DecodeAll(Packets, Patch.Chunk, RoundTripped));
			TArray<FSample> Expected;
			FTerrainDeltaCodecModel::Canonicalize(Patch.Samples, Expected);
			TestTrue(TEXT("La concatenación de los paquetes reconstruye el parche completo, en orden"), RoundTripped == Expected);
		});

		It("un parche vacío no produce ningún paquete", [this]()
		{
			FChunkPatch Patch;
			Patch.Chunk = FIntVector(1, 1, 1);
			TestEqual(TEXT("Cero paquetes"), FTerrainDeltaCodecModel::Encode(Patch).Num(), 0);
		});

		It("descarta sin reventar las muestras fuera de rango, y conserva las válidas", [this]()
		{
			FChunkPatch Patch;
			Patch.Chunk = FIntVector::ZeroValue;
			Patch.Samples = {
				FSample{ -1, 5 },                                                  // índice negativo: inválido
				FSample{ FTerrainDeltaCodecModel::MaxLocalIndex + 1, 5 },          // índice fuera de rango
				FSample{ 10, FTerrainDeltaCodecModel::MaxDeltaMm + 1 },            // delta fuera de rango
				FSample{ 20, 42 },                                                 // válida
			};

			const TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch);
			TArray<FSample> RoundTripped;
			TestTrue(TEXT("Decodifica"), DecodeAll(Packets, Patch.Chunk, RoundTripped));
			TestEqual(TEXT("Solo sobrevive la muestra válida"), RoundTripped.Num(), 1);
			if (RoundTripped.Num() == 1)
			{
				TestTrue(TEXT("Es la muestra 20:42"), RoundTripped[0] == FSample{ 20, 42 });
			}
		});
	});

	Describe("Canonicalize (fusión antes de enviar)", [this]()
	{
		It("es idempotente: fusionar un parche consigo mismo no cambia el resultado", [this]()
		{
			TArray<FSample> A = { FSample{ 1, 10 }, FSample{ 5, -20 } };
			TArray<FSample> Once;
			FTerrainDeltaCodecModel::Canonicalize(A, Once);

			TArray<FSample> Twice = A;
			Twice.Append(A);
			TArray<FSample> CanonTwice;
			FTerrainDeltaCodecModel::Canonicalize(Twice, CanonTwice);

			TestTrue(TEXT("Merge(A, A) == Merge(A)"), Once == CanonTwice);
		});

		It("es conmutativa cuando dos parches no comparten ningún índice", [this]()
		{
			TArray<FSample> A = { FSample{ 1, 10 }, FSample{ 5, -20 } };
			TArray<FSample> B = { FSample{ 2, 30 }, FSample{ 7, 40 } };

			TArray<FSample> AB = A; AB.Append(B);
			TArray<FSample> BA = B; BA.Append(A);

			TArray<FSample> CanonAB, CanonBA;
			FTerrainDeltaCodecModel::Canonicalize(AB, CanonAB);
			FTerrainDeltaCodecModel::Canonicalize(BA, CanonBA);

			TestTrue(TEXT("Merge(A, B) == Merge(B, A) cuando son disjuntos"), CanonAB == CanonBA);
			TestEqual(TEXT("Las cuatro muestras sobreviven"), CanonAB.Num(), 4);
		});

		It("cuando dos ediciones tocan la misma muestra, gana la aplicada después (regla explícita, no accidental)", [this]()
		{
			TArray<FSample> Old = { FSample{ 3, 10 } };
			TArray<FSample> New = { FSample{ 3, 99 } };

			TArray<FSample> OldThenNew = Old; OldThenNew.Append(New);
			TArray<FSample> Canon;
			FTerrainDeltaCodecModel::Canonicalize(OldThenNew, Canon);
			TestEqual(TEXT("Una sola muestra en el índice 3"), Canon.Num(), 1);
			if (Canon.Num() == 1)
			{
				TestEqual(TEXT("Gana el delta más reciente (99)"), Canon[0].DeltaMm, 99);
			}
		});
	});

	Describe("Decode ante datos corruptos o truncados", [this]()
	{
		It("rechaza un buffer más corto que la cabecera, o vacío, sin tocar Out", [this]()
		{
			FPacket Out;
			Out.Chunk = FIntVector(9, 9, 9); // centinela
			TArray<uint8> Empty;
			TestFalse(TEXT("Vacío"), FTerrainDeltaCodecModel::Decode(Empty, Out));
			TArray<uint8> TooShort = { 1, 0, 0 };
			TestFalse(TEXT("Más corto que la cabecera"), FTerrainDeltaCodecModel::Decode(TooShort, Out));
			TestTrue(TEXT("Out no se ha tocado"), Out.Chunk == FIntVector(9, 9, 9));
		});

		It("rechaza un buffer más largo que MaxPacketBytes", [this]()
		{
			TArray<uint8> TooLong;
			TooLong.SetNumZeroed(FTerrainDeltaCodecModel::MaxPacketBytes + 1);
			FPacket Out;
			TestFalse(TEXT("Demasiado largo"), FTerrainDeltaCodecModel::Decode(TooLong, Out));
		});

		It("rechaza una versión que no reconoce", [this]()
		{
			FChunkPatch Patch;
			Patch.Chunk = FIntVector::ZeroValue;
			Patch.Samples = { FSample{ 0, 1 } };
			TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch);
			TestEqual(TEXT("Un paquete"), Packets.Num(), 1);
			TArray<uint8> Corrupt = Packets[0];
			Corrupt[0] = FTerrainDeltaCodecModel::CurrentVersion + 1;
			FPacket Out;
			TestFalse(TEXT("Versión desconocida rechazada"), FTerrainDeltaCodecModel::Decode(Corrupt, Out));
		});

		It("rechaza un paquete truncado a mitad de un tramo, y uno con bytes sobrantes al final", [this]()
		{
			FChunkPatch Patch;
			Patch.Chunk = FIntVector::ZeroValue;
			Patch.Samples = { FSample{ 0, 1 }, FSample{ 1, 2 }, FSample{ 2, 3 } };
			TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch);
			TestEqual(TEXT("Un paquete"), Packets.Num(), 1);

			TArray<uint8> Truncated = Packets[0];
			Truncated.SetNum(Truncated.Num() - 1);
			FPacket Out;
			TestFalse(TEXT("Truncado a mitad de tramo"), FTerrainDeltaCodecModel::Decode(Truncated, Out));

			TArray<uint8> WithGarbage = Packets[0];
			WithGarbage.Add(0xFF);
			TestFalse(TEXT("Bytes sobrantes al final"), FTerrainDeltaCodecModel::Decode(WithGarbage, Out));
		});

		It("rechaza un tramo con cuenta 0 y un tramo que se sale de MaxLocalIndex", [this]()
		{
			// Cabecera: versión 1, chunk (0,0,0), 1 tramo. Tramo: FirstSample=0, Count=0 (inválido, sin deltas).
			TArray<uint8> ZeroCount = { 1, 0,0, 0,0, 0,0, 1,0, 0,0, 0 };
			FPacket Out;
			TestFalse(TEXT("Tramo de cuenta 0"), FTerrainDeltaCodecModel::Decode(ZeroCount, Out));

			// Tramo: FirstSample=32700 (0x7FBC LE = BC,7F), Count=100 -> último índice 32799 > MaxLocalIndex.
			TArray<uint8> Header = { 1, 0,0, 0,0, 0,0, 1,0 };
			TArray<uint8> OutOfRange = Header;
			OutOfRange.Add(0xBC); OutOfRange.Add(0x7F); // FirstSample = 32700
			OutOfRange.Add(100);                        // Count
			for (int32 I = 0; I < 100; ++I) { OutOfRange.Add(0); OutOfRange.Add(0); }
			TestFalse(TEXT("Tramo fuera de MaxLocalIndex"), FTerrainDeltaCodecModel::Decode(OutOfRange, Out));
		});

		It("rechaza tramos solapados y tramos que no vienen en orden creciente", [this]()
		{
			TArray<uint8> Header = { 1, 0,0, 0,0, 0,0, 2,0 }; // 2 tramos

			// Tramo A: FirstSample=10, Count=5 (10..14). Tramo B: FirstSample=12, Count=3 (solapa con A).
			TArray<uint8> Overlapping = Header;
			Overlapping.Add(10); Overlapping.Add(0); Overlapping.Add(5);
			for (int32 I = 0; I < 5; ++I) { Overlapping.Add(0); Overlapping.Add(0); }
			Overlapping.Add(12); Overlapping.Add(0); Overlapping.Add(3);
			for (int32 I = 0; I < 3; ++I) { Overlapping.Add(0); Overlapping.Add(0); }
			FPacket Out;
			TestFalse(TEXT("Tramos solapados"), FTerrainDeltaCodecModel::Decode(Overlapping, Out));

			// Tramo A: FirstSample=20, Count=2 (20..21). Tramo B: FirstSample=10 (retrocede: desordenado).
			TArray<uint8> OutOfOrder = Header;
			OutOfOrder.Add(20); OutOfOrder.Add(0); OutOfOrder.Add(2);
			OutOfOrder.Add(0); OutOfOrder.Add(0); OutOfOrder.Add(0); OutOfOrder.Add(0);
			OutOfOrder.Add(10); OutOfOrder.Add(0); OutOfOrder.Add(2);
			OutOfOrder.Add(0); OutOfOrder.Add(0); OutOfOrder.Add(0); OutOfOrder.Add(0);
			TestFalse(TEXT("Tramos desordenados"), FTerrainDeltaCodecModel::Decode(OutOfOrder, Out));
		});
	});

	Describe("Límites de paquete", [this]()
	{
		It("un tramo de 250 muestras llena un paquete de 512 B exactos y el de 251 ya necesita dos", [this]()
		{
			FChunkPatch Patch;
			for (int32 I = 0; I < FTerrainDeltaCodecModel::MaxRunCount; ++I)
			{
				Patch.Samples.Add(FSample{ 1000 + I, I - 125 });
			}
			TestEqual(TEXT("MaxRunCount es 250"), FTerrainDeltaCodecModel::MaxRunCount, 250);
			TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch);
			TestEqual(TEXT("Un paquete"), Packets.Num(), 1);
			TestEqual(TEXT("De 512 B justos"), Packets[0].Num(), FTerrainDeltaCodecModel::MaxPacketBytes);
			TestEqual(TEXT("Coincide con PacketBytesForRun"), Packets[0].Num(), FTerrainDeltaCodecModel::PacketBytesForRun(250));

			Patch.Samples.Add(FSample{ 1000 + 250, 7 });
			Packets = FTerrainDeltaCodecModel::Encode(Patch);
			TestEqual(TEXT("Con una más, dos paquetes"), Packets.Num(), 2);
			TestEqual(TEXT("El segundo lleva solo la muestra que sobra"), Packets[1].Num(), FTerrainDeltaCodecModel::PacketBytesForRun(1));
			TArray<FSample> Back;
			TestTrue(TEXT("Y vuelve entero"), DecodeAll(Packets, Patch.Chunk, Back) && Back.Num() == 251);
		});

		It("el peor caso (muestras sueltas, un tramo por muestra) respeta el tope y aprovecha el paquete", [this]()
		{
			FChunkPatch Patch;
			for (int32 I = 0; I < 1000; ++I)
			{
				Patch.Samples.Add(FSample{ I * 2, 1 });
			}
			const TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch);
			// Cabecera 9 + 100 tramos de 5 B = 509 B: 100 muestras por paquete.
			TestEqual(TEXT("Diez paquetes"), Packets.Num(), 10);
			for (const TArray<uint8>& P : Packets)
			{
				TestEqual(TEXT("509 B cada uno"), P.Num(), 509);
			}
			TArray<FSample> Back;
			TestTrue(TEXT("Ida y vuelta"), DecodeAll(Packets, Patch.Chunk, Back) && Back.Num() == 1000);
		});

		It("un tramo que no cabe entero se parte y aprovecha el hueco del paquete en curso", [this]()
		{
			FChunkPatch Patch;
			for (int32 I = 0; I < 200; ++I) { Patch.Samples.Add(FSample{ I, 3 }); }
			for (int32 I = 0; I < 200; ++I) { Patch.Samples.Add(FSample{ 5000 + I, -3 }); }
			const TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch);
			TestEqual(TEXT("400 muestras en dos paquetes, no en tres"), Packets.Num(), 2);
			// 9 + (3 + 400) + (3 + 96) = 511: solo sobra un byte, menos que una muestra.
			TestEqual(TEXT("El primero va lleno"), Packets[0].Num(), 511);
			TArray<FSample> Back;
			TArray<FSample> Expected;
			FTerrainDeltaCodecModel::Canonicalize(Patch.Samples, Expected);
			TestTrue(TEXT("Ida y vuelta"), DecodeAll(Packets, Patch.Chunk, Back) && Back == Expected);
		});

		It("ida y vuelta exacta en 200 parches pseudoaleatorios, con ceros, extremos y huecos", [this]()
		{
			for (uint32 Seed = 1; Seed <= 200; ++Seed)
			{
				const FChunkPatch Patch = MakeScatteredPatch(Seed * 2654435761u, 50 + static_cast<int32>(Seed * 37 % 3000));
				int32 Rejected = -1;
				const TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch, &Rejected);
				TestEqual(TEXT("Nada rechazado"), Rejected, 0);
				int32 Bytes = 0;
				for (const TArray<uint8>& P : Packets)
				{
					TestTrue(TEXT("≤ 512 B"), P.Num() <= FTerrainDeltaCodecModel::MaxPacketBytes);
					Bytes += P.Num();
				}
				TestEqual(TEXT("EncodedByteCount cuadra"), FTerrainDeltaCodecModel::EncodedByteCount(Patch), Bytes);
				TArray<FSample> Back;
				TArray<FSample> Expected;
				FTerrainDeltaCodecModel::Canonicalize(Patch.Samples, Expected);
				if (!TestTrue(TEXT("Decodifica"), DecodeAll(Packets, Patch.Chunk, Back)) || !TestTrue(TEXT("Exactamente lo que entró"), Back == Expected))
				{
					AddError(FString::Printf(TEXT("Semilla %u"), Seed));
					return;
				}
			}
		});

		It("EncodeFirstPacket saca el mismo primer paquete que Encode y rechaza entradas no canónicas", [this]()
		{
			const FChunkPatch Patch = MakeScatteredPatch(99, 900);
			TArray<FSample> Canon;
			FTerrainDeltaCodecModel::Canonicalize(Patch.Samples, Canon);
			int32 Consumed = 0;
			const TArray<uint8> First = FTerrainDeltaCodecModel::EncodeFirstPacket(Patch.Chunk, Canon, Consumed);
			TestTrue(TEXT("Igual al primero de Encode"), First == FTerrainDeltaCodecModel::Encode(Patch)[0]);
			FPacket Decoded;
			TestTrue(TEXT("Consumed = muestras del paquete"), FTerrainDeltaCodecModel::Decode(First, Decoded) && Decoded.Samples.Num() == Consumed);

			TArray<FSample> Unsorted = { FSample{ 5, 1 }, FSample{ 2, 1 } };
			TestEqual(TEXT("Desordenada: nada"), FTerrainDeltaCodecModel::EncodeFirstPacket(FIntVector::ZeroValue, Unsorted, Consumed).Num(), 0);
			TestEqual(TEXT("Y consume 0"), Consumed, 0);
			TArray<FSample> Repeated = { FSample{ 2, 1 }, FSample{ 2, 3 } };
			TestEqual(TEXT("Repetida: nada"), FTerrainDeltaCodecModel::EncodeFirstPacket(FIntVector::ZeroValue, Repeated, Consumed).Num(), 0);
		});

		It("un chunk fuera de int16 no se trunca: se rechaza entero y se cuenta", [this]()
		{
			FChunkPatch Patch;
			Patch.Chunk = FIntVector(32768, 0, 0); // truncado a int16 sería el chunk -32768
			Patch.Samples = { FSample{ 0, 5 }, FSample{ 1, 6 } };
			int32 Rejected = 0;
			TestEqual(TEXT("Cero paquetes"), FTerrainDeltaCodecModel::Encode(Patch, &Rejected).Num(), 0);
			TestEqual(TEXT("Las dos muestras rechazadas"), Rejected, 2);
			TestTrue(TEXT("-32768 sí cabe"), FTerrainDeltaCodecModel::IsChunkEncodable(FIntVector(-32768, 32767, 0)));
			TestFalse(TEXT("-32769 no"), FTerrainDeltaCodecModel::IsChunkEncodable(FIntVector(0, 0, -32769)));
		});
	});

	Describe("Cuantización", [this]()
	{
		It("redondea al milímetro más cercano, los medios lejos de cero, y satura sin comportamiento indefinido", [this]()
		{
			int32 Mm = 0;
			TestTrue(TEXT("0,25 m"), FTerrainDeltaCodecModel::QuantizeDeltaMm(0.25, Mm) && Mm == 250);
			TestTrue(TEXT("0,0004 m → 0"), FTerrainDeltaCodecModel::QuantizeDeltaMm(0.0004, Mm) && Mm == 0);
			TestTrue(TEXT("0,0005 m → 1"), FTerrainDeltaCodecModel::QuantizeDeltaMm(0.0005, Mm) && Mm == 1);
			TestTrue(TEXT("-0,0005 m → -1"), FTerrainDeltaCodecModel::QuantizeDeltaMm(-0.0005, Mm) && Mm == -1);
			TestTrue(TEXT("-0,5 m (banda mínima)"), FTerrainDeltaCodecModel::QuantizeDeltaMm(-0.5, Mm) && Mm == -500);
			TestTrue(TEXT("32,767 m cabe"), FTerrainDeltaCodecModel::QuantizeDeltaMm(32.767, Mm) && Mm == 32767);
			TestTrue(TEXT("-32,768 m cabe"), FTerrainDeltaCodecModel::QuantizeDeltaMm(-32.768, Mm) && Mm == -32768);
			TestFalse(TEXT("40 m no cabe"), FTerrainDeltaCodecModel::QuantizeDeltaMm(40.0, Mm));
			TestEqual(TEXT("…y satura arriba"), Mm, 32767);
			TestFalse(TEXT("-1e30 m no cabe"), FTerrainDeltaCodecModel::QuantizeDeltaMm(-1e30, Mm));
			TestEqual(TEXT("…y satura abajo"), Mm, -32768);
			TestFalse(TEXT("NaN"), FTerrainDeltaCodecModel::QuantizeDeltaMm(std::numeric_limits<double>::quiet_NaN(), Mm));
			TestEqual(TEXT("…da 0"), Mm, 0);
			TestFalse(TEXT("Infinito"), FTerrainDeltaCodecModel::QuantizeDeltaMm(std::numeric_limits<double>::infinity(), Mm));
		});

		It("cuantizar lo decodificado no cambia nada: el valor en mm es un punto fijo", [this]()
		{
			for (int32 V = -32768; V <= 32767; V += 97)
			{
				int32 Mm = 0;
				if (!FTerrainDeltaCodecModel::QuantizeDeltaMm(static_cast<double>(V) / 1000.0, Mm) || Mm != V)
				{
					AddError(FString::Printf(TEXT("%d mm no vuelve igual (%d)"), V, Mm));
					return;
				}
			}
		});
	});

	Describe("Fusión al aplicar en el cliente", [this]()
	{
		It("aplicar el mismo paquete dos veces es igual que una (idempotente)", [this]()
		{
			const FChunkPatch Patch = MakeScatteredPatch(7, 700);
			const TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch);
			TMap<FIntVector, FTerrainDeltaCodecModel::FChunkState> Once, Twice;
			for (const TArray<uint8>& P : Packets)
			{
				FTerrainDeltaCodecModel::DecodeAndApply(P, Once);
				FTerrainDeltaCodecModel::DecodeAndApply(P, Twice);
				FTerrainDeltaCodecModel::DecodeAndApply(P, Twice);
			}
			// Y reenviar el lote entero otra vez tampoco cambia nada.
			for (const TArray<uint8>& P : Packets) { FTerrainDeltaCodecModel::DecodeAndApply(P, Twice); }
			const FTerrainDeltaCodecModel::FChunkState Expected = StateOf(Patch.Samples);
			TestTrue(TEXT("Una vez = estado esperado"), FTerrainDeltaCodecModel::SamplesOf(Once[Patch.Chunk]) == FTerrainDeltaCodecModel::SamplesOf(Expected));
			TestTrue(TEXT("Dos veces = una vez"), FTerrainDeltaCodecModel::SamplesOf(Twice[Patch.Chunk]) == FTerrainDeltaCodecModel::SamplesOf(Once[Patch.Chunk]));
		});

		It("los paquetes de una edición grande y de dos ediciones disjuntas conmutan en cualquier orden", [this]()
		{
			FChunkPatch A = MakeScatteredPatch(11, 800);
			FChunkPatch B;
			B.Chunk = A.Chunk;
			for (int32 I = 20000; I < 20600; I += 2) { B.Samples.Add(FSample{ I, I % 300 - 150 }); }

			TArray<TArray<uint8>> All = FTerrainDeltaCodecModel::Encode(A);
			All.Append(FTerrainDeltaCodecModel::Encode(B));
			TestTrue(TEXT("Varios paquetes"), All.Num() >= 4);

			TMap<FIntVector, FTerrainDeltaCodecModel::FChunkState> Forward, Backward, Shuffled;
			for (int32 I = 0; I < All.Num(); ++I) { FTerrainDeltaCodecModel::DecodeAndApply(All[I], Forward); }
			for (int32 I = All.Num() - 1; I >= 0; --I) { FTerrainDeltaCodecModel::DecodeAndApply(All[I], Backward); }
			for (int32 I = 0; I < All.Num(); ++I) { FTerrainDeltaCodecModel::DecodeAndApply(All[(I * 5 + 3) % All.Num()], Shuffled); }
			const TArray<FSample> F = FTerrainDeltaCodecModel::SamplesOf(Forward[A.Chunk]);
			TestTrue(TEXT("Al revés, igual"), F == FTerrainDeltaCodecModel::SamplesOf(Backward[A.Chunk]));
			TestTrue(TEXT("Barajado, igual"), F == FTerrainDeltaCodecModel::SamplesOf(Shuffled[A.Chunk]));
			TestEqual(TEXT("Mismo checksum"), FTerrainChunkChecksumModel::ComputeState(Forward[A.Chunk]), FTerrainChunkChecksumModel::ComputeState(Backward[A.Chunk]));
		});

		It("un delta 0 devuelve la muestra a la base y un chunk que se queda vacío desaparece", [this]()
		{
			FChunkPatch Set;
			Set.Chunk = FIntVector(1, 2, 3);
			Set.Samples = { FSample{ 4, 10 }, FSample{ 5, 20 } };
			FChunkPatch Clear = Set;
			Clear.Samples = { FSample{ 4, 0 }, FSample{ 5, 0 } };

			TMap<FIntVector, FTerrainDeltaCodecModel::FChunkState> World;
			TestTrue(TEXT("Aplica"), FTerrainDeltaCodecModel::DecodeAndApply(FTerrainDeltaCodecModel::Encode(Set)[0], World));
			TestEqual(TEXT("Dos muestras"), World[Set.Chunk].Num(), 2);
			TestTrue(TEXT("Aplica los ceros"), FTerrainDeltaCodecModel::DecodeAndApply(FTerrainDeltaCodecModel::Encode(Clear)[0], World));
			TestFalse(TEXT("El chunk ya no está"), World.Contains(Set.Chunk));
		});
	});

	Describe("Paquetes corruptos o truncados", [this]()
	{
		It("cualquier byte cambiado se rechaza o se detecta con la comprobación del chunk", [this]()
		{
			// Lo que protege biblia 08 §2.2 es el estado del chunk, no el paquete: servidor y
			// cliente parten del mismo estado previo (denso, para que un delta 0 desplazado a
			// otro índice también se note) y se compara la comprobación tras aplicar.
			const FChunkPatch Patch = MakeScatteredPatch(1234, 400);
			const TArray<TArray<uint8>> Packets = FTerrainDeltaCodecModel::Encode(Patch);
			const TArray<uint8>& Original = Packets[0];

			TMap<FIntVector, FTerrainDeltaCodecModel::FChunkState> Prior;
			for (int32 I = 0; I <= FTerrainDeltaCodecModel::MaxLocalIndex; I += 1)
			{
				Prior.FindOrAdd(Patch.Chunk).Add(I, 31111);
			}
			TMap<FIntVector, FTerrainDeltaCodecModel::FChunkState> Server = Prior;
			TestTrue(TEXT("El original se aplica"), FTerrainDeltaCodecModel::DecodeAndApply(Original, Server));
			const uint32 ServerSum = FTerrainChunkChecksumModel::ComputeState(Server[Patch.Chunk]);

			const uint8 Masks[] = { 0x01, 0x02, 0x10, 0x80, 0xFF, 0x55 };
			int32 Undetected = 0;
			int32 Rejected = 0;
			for (int32 Pos = 0; Pos < Original.Num(); ++Pos)
			{
				for (const uint8 Mask : Masks)
				{
					TArray<uint8> Corrupt = Original;
					Corrupt[Pos] ^= Mask;
					TMap<FIntVector, FTerrainDeltaCodecModel::FChunkState> Client = Prior;
					if (!FTerrainDeltaCodecModel::DecodeAndApply(Corrupt, Client))
					{
						++Rejected;
						continue;
					}
					const FTerrainDeltaCodecModel::FChunkState* State = Client.Find(Patch.Chunk);
					const bool bSameChunkSame = State && Client.Num() == 1 && FTerrainChunkChecksumModel::ComputeState(*State) == ServerSum;
					if (bSameChunkSame)
					{
						++Undetected;
						AddError(FString::Printf(TEXT("Byte %d con máscara 0x%02X pasa sin detectar"), Pos, Mask));
					}
				}
			}
			TestEqual(TEXT("Ningún cambio de un byte pasa sin detectar"), Undetected, 0);
			TestTrue(TEXT("Y muchos se rechazan ya al decodificar"), Rejected > 0);
		});

		It("todas las longitudes truncadas, y bytes de más, se rechazan sin tocar el estado", [this]()
		{
			const FChunkPatch Patch = MakeScatteredPatch(4321, 300);
			const TArray<uint8> Original = FTerrainDeltaCodecModel::Encode(Patch)[0];

			TMap<FIntVector, FTerrainDeltaCodecModel::FChunkState> World;
			World.Add(FIntVector(9, 9, 9)).Add(1, 42);
			const TArray<FSample> Before = FTerrainDeltaCodecModel::SamplesOf(World[FIntVector(9, 9, 9)]);
			for (int32 Len = 0; Len < Original.Num(); ++Len)
			{
				TArray<uint8> Truncated(Original.GetData(), Len);
				if (FTerrainDeltaCodecModel::DecodeAndApply(Truncated, World))
				{
					AddError(FString::Printf(TEXT("Truncado a %d B se aceptó"), Len));
					return;
				}
			}
			TArray<uint8> Longer = Original;
			Longer.Add(0);
			TestFalse(TEXT("Un byte de más"), FTerrainDeltaCodecModel::DecodeAndApply(Longer, World));
			TestEqual(TEXT("El mundo no cambia"), World.Num(), 1);
			TestTrue(TEXT("Ni su contenido"), FTerrainDeltaCodecModel::SamplesOf(World[FIntVector(9, 9, 9)]) == Before);
		});

		It("20 000 buffers basura (aleatorios o con cabecera buena) no revientan ni dejan muestras fuera de rango", [this]()
		{
			uint32 State = 0xC0FFEEu;
			int32 Accepted = 0;
			for (int32 Case = 0; Case < 20000; ++Case)
			{
				const int32 Len = static_cast<int32>(NextRandom(State) % 600);
				TArray<uint8> Bytes;
				Bytes.SetNumUninitialized(Len);
				for (int32 I = 0; I < Len; ++I) { Bytes[I] = static_cast<uint8>(NextRandom(State)); }
				if (Len >= FTerrainDeltaCodecModel::HeaderBytes && (Case % 2 == 0))
				{
					// Cabecera plausible: versión buena y pocos tramos, para llegar al cuerpo.
					Bytes[0] = FTerrainDeltaCodecModel::CurrentVersion;
					Bytes[7] = static_cast<uint8>(1 + NextRandom(State) % 4);
					Bytes[8] = 0;
				}
				FPacket Out;
				if (FTerrainDeltaCodecModel::Decode(Bytes, Out))
				{
					++Accepted;
					int32 Prev = -1;
					for (const FSample& S : Out.Samples)
					{
						if (S.LocalIndex <= Prev || !FTerrainDeltaCodecModel::IsSampleEncodable(S))
						{
							AddError(FString::Printf(TEXT("Caso %d: muestra inválida aceptada"), Case));
							return;
						}
						Prev = S.LocalIndex;
					}
					// Lo aceptado se vuelve a codificar y da lo mismo.
					FChunkPatch Again;
					Again.Chunk = Out.Chunk;
					Again.Samples = Out.Samples;
					TArray<FSample> Back;
					if (!DecodeAll(FTerrainDeltaCodecModel::Encode(Again), Out.Chunk, Back) || Back != Out.Samples)
					{
						AddError(FString::Printf(TEXT("Caso %d: lo aceptado no hace ida y vuelta"), Case));
						return;
					}
				}
			}
			AddInfo(FString::Printf(TEXT("%d buffers aceptados"), Accepted));
			TestTrue(TEXT("Termina"), true);
		});

		It("rechaza un paquete sin tramos, que este códec nunca produce", [this]()
		{
			const TArray<uint8> Empty = { FTerrainDeltaCodecModel::CurrentVersion, 0, 0, 0, 0, 0, 0, 0, 0 };
			FPacket Out;
			TestFalse(TEXT("Cero tramos"), FTerrainDeltaCodecModel::Decode(Empty, Out));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
