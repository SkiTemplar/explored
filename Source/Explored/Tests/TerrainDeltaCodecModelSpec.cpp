#include "Misc/AutomationTest.h"

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
}

#endif // WITH_DEV_AUTOMATION_TESTS
