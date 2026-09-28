#include "Misc/AutomationTest.h"

#include <limits>

#include "WorldGen/TerrainChunkChecksumModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FTerrainChunkChecksumModelSpec, "Explored.TerrainChunkChecksum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	using FSample = FTerrainDeltaCodecModel::FSample;
END_DEFINE_SPEC(FTerrainChunkChecksumModelSpec)

void FTerrainChunkChecksumModelSpec::Define()
{
	Describe("Compute", [this]()
	{
		It("un chunk sin muestras da la base de FNV-1a", [this]()
		{
			TestEqual(TEXT("Base de FNV-1a de 32 bits"), FTerrainChunkChecksumModel::Compute({}), (uint32)0x811C9DC5u);
		});

		It("es determinista: no depende del orden de entrada ni de índices repetidos", [this]()
		{
			TArray<FSample> InOrder = { FSample{ 5, 10 }, FSample{ 1, -3 }, FSample{ 9, 7 } };
			TArray<FSample> Shuffled = { FSample{ 9, 7 }, FSample{ 5, 10 }, FSample{ 1, -3 } };
			TArray<FSample> WithDuplicate = { FSample{ 1, 999 }, FSample{ 5, 10 }, FSample{ 1, -3 }, FSample{ 9, 7 } };

			const uint32 A = FTerrainChunkChecksumModel::Compute(InOrder);
			const uint32 B = FTerrainChunkChecksumModel::Compute(Shuffled);
			const uint32 C = FTerrainChunkChecksumModel::Compute(WithDuplicate);

			TestEqual(TEXT("Mismo checksum sin importar el orden"), A, B);
			TestEqual(TEXT("El duplicado se resuelve igual que Canonicalize (gana el último, -3)"), A, C);
		});

		It("un cambio de un solo byte en una muestra cambia el checksum (sin colisión en estos casos concretos)", [this]()
		{
			TArray<FSample> Base = { FSample{ 5, 10 }, FSample{ 100, -200 }, FSample{ 32000, 1 } };
			const uint32 BaseChecksum = FTerrainChunkChecksumModel::Compute(Base);

			TArray<FSample> DeltaChanged = Base;
			DeltaChanged[0].DeltaMm += 1; // cambia el byte bajo de un delta
			TestNotEqual(TEXT("Cambiar un delta en 1 cambia el checksum"), BaseChecksum, FTerrainChunkChecksumModel::Compute(DeltaChanged));

			TArray<FSample> IndexChanged = Base;
			IndexChanged[1].LocalIndex += 1; // cambia el byte bajo de un índice
			TestNotEqual(TEXT("Cambiar un índice en 1 cambia el checksum"), BaseChecksum, FTerrainChunkChecksumModel::Compute(IndexChanged));

			TArray<FSample> HighByteChanged = Base;
			HighByteChanged[2].DeltaMm += 256; // cambia el byte alto en vez del bajo
			TestNotEqual(TEXT("Cambiar el byte alto también cambia el checksum"), BaseChecksum, FTerrainChunkChecksumModel::Compute(HighByteChanged));
		});
	});

	Describe("FTracker", [this]()
	{
		It("manda la comprobación la primera vez que ve un chunk, y luego solo cada 30 s", [this]()
		{
			FTerrainChunkChecksumModel::FTracker Tracker;
			const FIntVector ChunkA(1, 2, 3);
			TMap<FIntVector, uint32> Checksums = { { ChunkA, 111u } };

			TArray<FIntVector> Due;
			Tracker.DueChunks(Checksums, 0.0, Due);
			TestEqual(TEXT("Primera vez: toca"), Due.Num(), 1);

			Tracker.DueChunks(Checksums, 10.0, Due);
			TestEqual(TEXT("A los 10 s todavía no toca"), Due.Num(), 0);

			Tracker.DueChunks(Checksums, 29.9, Due);
			TestEqual(TEXT("Justo antes de los 30 s tampoco"), Due.Num(), 0);

			Tracker.DueChunks(Checksums, 30.0, Due);
			TestEqual(TEXT("A los 30 s exactos vuelve a tocar"), Due.Num(), 1);
		});

		It("cada chunk lleva su propio calendario de 30 s, independiente del resto", [this]()
		{
			FTerrainChunkChecksumModel::FTracker Tracker;
			const FIntVector ChunkA(0, 0, 0);
			const FIntVector ChunkB(1, 0, 0);

			TArray<FIntVector> Due;
			Tracker.DueChunks({ { ChunkA, 1u } }, 0.0, Due); // A visto en el segundo 0
			Tracker.DueChunks({ { ChunkB, 2u } }, 20.0, Due); // B aparece en el segundo 20

			Tracker.DueChunks({ { ChunkA, 1u }, { ChunkB, 2u } }, 25.0, Due);
			TestEqual(TEXT("A los 25 s ninguno de los dos toca todavía"), Due.Num(), 0);

			Tracker.DueChunks({ { ChunkA, 1u }, { ChunkB, 2u } }, 30.0, Due);
			TestEqual(TEXT("A los 30 s solo toca A (empezó en el 0)"), Due.Num(), 1);
			if (Due.Num() == 1)
			{
				TestTrue(TEXT("Es A"), Due[0] == ChunkA);
			}

			Tracker.DueChunks({ { ChunkA, 1u }, { ChunkB, 2u } }, 50.0, Due);
			TestEqual(TEXT("A los 50 s toca B (empezó en el 20)"), Due.Num(), 1);
			if (Due.Num() == 1)
			{
				TestTrue(TEXT("Es B"), Due[0] == ChunkB);
			}
		});

		It("ConfirmAndCheck acepta cuando coincide y detecta la desincronización cuando no", [this]()
		{
			FTerrainChunkChecksumModel::FTracker Tracker;
			const FIntVector ChunkA(4, 4, 4);
			TArray<FIntVector> Due;
			Tracker.DueChunks({ { ChunkA, 777u } }, 0.0, Due);

			TestTrue(TEXT("Coincide"), Tracker.ConfirmAndCheck(ChunkA, 777u));
			TestFalse(TEXT("No coincide: desincronización"), Tracker.ConfirmAndCheck(ChunkA, 778u));

			const FIntVector NeverSent(9, 9, 9);
			TestFalse(TEXT("Chunk nunca mandado: se trata como desincronizado"), Tracker.ConfirmAndCheck(NeverSent, 0u));
		});

		It("ForgetChunk olvida el historial: vuelve a tocar de inmediato la próxima vez", [this]()
		{
			FTerrainChunkChecksumModel::FTracker Tracker;
			const FIntVector ChunkA(0, 0, 0);
			TArray<FIntVector> Due;
			Tracker.DueChunks({ { ChunkA, 1u } }, 0.0, Due);
			Tracker.DueChunks({ { ChunkA, 1u } }, 5.0, Due);
			TestEqual(TEXT("A los 5 s no toca"), Due.Num(), 0);

			Tracker.ForgetChunk(ChunkA);
			Tracker.DueChunks({ { ChunkA, 1u } }, 6.0, Due);
			TestEqual(TEXT("Tras olvidarlo, vuelve a tocar aunque no hayan pasado 30 s"), Due.Num(), 1);
		});

		It("si el reloj retrocede, el chunk vuelve a tocar en vez de quedarse mudo; con un reloj no finito no se manda nada", [this]()
		{
			FTerrainChunkChecksumModel::FTracker Tracker;
			const FIntVector Chunk(1, 1, 1);
			TArray<FIntVector> Due;
			Tracker.DueChunks({ { Chunk, 5u } }, 1000.0, Due);
			TestEqual(TEXT("Primera vez"), Due.Num(), 1);
			Tracker.DueChunks({ { Chunk, 5u } }, 3.0, Due);
			TestEqual(TEXT("El reloj vuelve a 3 s: toca otra vez"), Due.Num(), 1);
			Tracker.DueChunks({ { Chunk, 5u } }, 20.0, Due);
			TestEqual(TEXT("Y luego sigue su calendario desde ahí"), Due.Num(), 0);

			FTerrainChunkChecksumModel::FTracker Broken;
			Broken.DueChunks({ { Chunk, 5u } }, std::numeric_limits<double>::quiet_NaN(), Due);
			TestEqual(TEXT("NaN: nada"), Due.Num(), 0);
			TestEqual(TEXT("Ni se apunta"), Broken.NumTracked(), 0);
		});

		It("con 40 chunks, cada ciclo de 30 s los manda todos una vez, en orden estable", [this]()
		{
			FTerrainChunkChecksumModel::FTracker Tracker;
			TMap<FIntVector, uint32> Current;
			for (int32 I = 39; I >= 0; --I)
			{
				Current.Add(FIntVector(I % 7 - 3, I / 7, -I % 2), static_cast<uint32>(I));
			}
			TArray<FIntVector> Due;
			int32 Sent = 0;
			for (int32 Tick = 0; Tick <= 30 * 90; ++Tick)
			{
				Tracker.DueChunks(Current, Tick / 30.0, Due);
				for (int32 K = 1; K < Due.Num(); ++K)
				{
					const FIntVector& A = Due[K - 1];
					const FIntVector& B = Due[K];
					const bool bOrdered = A.X < B.X || (A.X == B.X && (A.Y < B.Y || (A.Y == B.Y && A.Z < B.Z)));
					if (!bOrdered)
					{
						AddError(TEXT("Salida sin ordenar"));
						return;
					}
				}
				Sent += Due.Num();
			}
			// t = 0, 30, 60 y 90 s: cuatro rondas de 40.
			TestEqual(TEXT("160 comprobaciones en 90 s"), Sent, 160);
			// 4 B por comprobación: 40 × 4 / 30 ≈ 5,3 B/s, lo que dice biblia 08 §2.2.
			TestTrue(TEXT("≈ 5 B/s"), 40.0 * 4.0 / FTerrainChunkChecksumModel::VerificationIntervalSeconds < 6.0);
		});
	});

	Describe("Formato estable", [this]()
	{
		It("coincide con un FNV-1a de referencia calculado fuera del motor", [this]()
		{
			// Calculado en Python con struct.pack('<ii', índice, delta) por muestra.
			TestEqual(TEXT("{0: 1}"), FTerrainChunkChecksumModel::Compute({ FSample{ 0, 1 } }), (uint32)0x4BB53254u);
			TestEqual(TEXT("{5: -250, 32767: 32767}"), FTerrainChunkChecksumModel::Compute({ FSample{ 32767, 32767 }, FSample{ 5, -250 } }), (uint32)0x499DE8F1u);
		});

		It("un delta 0 cuenta igual que una muestra sin tocar (así lo guarda FTerrainEditModel)", [this]()
		{
			const TArray<FSample> Plain = { FSample{ 3, 40 }, FSample{ 8, -2 } };
			const TArray<FSample> WithZeros = { FSample{ 0, 0 }, FSample{ 3, 40 }, FSample{ 5, 0 }, FSample{ 8, -2 } };
			const TArray<FSample> Overwritten = { FSample{ 3, 40 }, FSample{ 9, 70 }, FSample{ 8, -2 }, FSample{ 9, 0 } };
			const uint32 Expected = FTerrainChunkChecksumModel::Compute(Plain);
			TestEqual(TEXT("Con ceros explícitos"), FTerrainChunkChecksumModel::Compute(WithZeros), Expected);
			TestEqual(TEXT("Con una muestra devuelta a 0"), FTerrainChunkChecksumModel::Compute(Overwritten), Expected);
			TestEqual(TEXT("Solo ceros = chunk vacío"), FTerrainChunkChecksumModel::Compute({ FSample{ 4, 0 } }), (uint32)0x811C9DC5u);

			FTerrainDeltaCodecModel::FChunkState State;
			State.Add(8, -2);
			State.Add(3, 40);
			TestEqual(TEXT("ComputeState da lo mismo que Compute"), FTerrainChunkChecksumModel::ComputeState(State), Expected);
		});

		It("cambiar un solo byte de cualquier delta de un chunk grande siempre cambia la comprobación", [this]()
		{
			TArray<FSample> Samples;
			for (int32 I = 0; I < 3000; I += 3)
			{
				Samples.Add(FSample{ I, (I * 7919) % 6001 - 3000 });
			}
			const uint32 Base = FTerrainChunkChecksumModel::Compute(Samples);
			int32 Collisions = 0;
			for (int32 K = 0; K < Samples.Num(); ++K)
			{
				for (int32 Byte = 0; Byte < 2; ++Byte)
				{
					TArray<FSample> Changed = Samples;
					Changed[K].DeltaMm = static_cast<int16>(static_cast<uint16>(Changed[K].DeltaMm) ^ (0x40u << (8 * Byte)));
					if (Changed[K].DeltaMm == 0)
					{
						continue; // pasaría a «sin tocar»: es otro caso, cubierto arriba
					}
					if (FTerrainChunkChecksumModel::Compute(Changed) == Base)
					{
						++Collisions;
					}
				}
			}
			TestEqual(TEXT("Sin colisiones"), Collisions, 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
