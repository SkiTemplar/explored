#include "Misc/AutomationTest.h"

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
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
