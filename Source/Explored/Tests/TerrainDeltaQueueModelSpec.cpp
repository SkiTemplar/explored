#include "Misc/AutomationTest.h"

#include "WorldGen/TerrainDeltaQueueModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FTerrainDeltaQueueModelSpec, "Explored.TerrainDeltaQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	using FSample = FTerrainDeltaCodecModel::FSample;

	/** Parche de N muestras contiguas de un byte de delta cada una, para forzar un coste conocido en bytes. */
	TArray<FSample> MakeSamples(int32 FirstIndex, int32 Count)
	{
		TArray<FSample> Samples;
		Samples.Reserve(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			Samples.Add(FSample{ FirstIndex + I, 1 });
		}
		return Samples;
	}
END_DEFINE_SPEC(FTerrainDeltaQueueModelSpec)

void FTerrainDeltaQueueModelSpec::Define()
{
	Describe("Fusión y orden de la cola", [this]()
	{
		It("fusiona una reedición del mismo chunk sin duplicar la entrada ni cambiar su posición", [this]()
		{
			FTerrainDeltaQueueModel Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 3), 100.0); // lejos, primero
			Queue.Enqueue(FIntVector(1, 0, 0), MakeSamples(0, 3), 100.0); // lejos, segundo
			// Se reedita el primer chunk con muestras nuevas y disjuntas: se fusiona, no se duplica.
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(10, 2), 90.0);

			TestEqual(TEXT("Sigue habiendo solo dos entradas"), Queue.Num(), 2);

			Queue.FillBudget();
			FTerrainDeltaQueueModel::FEntry First, Second;
			TestTrue(TEXT("Sale el primero en llegar"), Queue.TryPopWithinBudget(First));
			TestTrue(TEXT("Es el chunk (0,0,0)"), First.Chunk == FIntVector(0, 0, 0));
			TestEqual(TEXT("Trae las 3 muestras originales más las 2 nuevas"), First.Samples.Num(), 5);

			TestTrue(TEXT("Sale el segundo"), Queue.TryPopWithinBudget(Second));
			TestTrue(TEXT("Es el chunk (1,0,0)"), Second.Chunk == FIntVector(1, 0, 0));
		});

		It("prioriza los chunks a menos de 30 m aunque hayan llegado después", [this]()
		{
			FTerrainDeltaQueueModel Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 3), 50.0); // lejos, llega primero
			Queue.Enqueue(FIntVector(1, 0, 0), MakeSamples(0, 3), 10.0); // cerca, llega después

			Queue.FillBudget();
			FTerrainDeltaQueueModel::FEntry Out;
			TestTrue(TEXT("Saca algo"), Queue.TryPopWithinBudget(Out));
			TestTrue(TEXT("Sale primero el cercano, aunque llegó después"), Out.Chunk == FIntVector(1, 0, 0));
		});

		It("dentro del mismo grupo de prioridad respeta el orden de llegada (FIFO), no la distancia exacta", [this]()
		{
			FTerrainDeltaQueueModel Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 3), 20.0); // cerca, llega primero
			Queue.Enqueue(FIntVector(1, 0, 0), MakeSamples(0, 3), 5.0);  // aún más cerca, llega después

			Queue.FillBudget();
			FTerrainDeltaQueueModel::FEntry Out;
			TestTrue(TEXT("Saca algo"), Queue.TryPopWithinBudget(Out));
			TestTrue(TEXT("Sale el que llegó primero, aunque el otro está más cerca todavía"), Out.Chunk == FIntVector(0, 0, 0));
		});
	});

	Describe("Presupuesto de bytes por segundo", [this]()
	{
		It("sin presupuesto acumulado, no saca nada de la cola", [this]()
		{
			FTerrainDeltaQueueModel Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 10), 0.0);
			FTerrainDeltaQueueModel::FEntry Out;
			TestFalse(TEXT("Presupuesto en cero: no sale nada"), Queue.TryPopWithinBudget(Out));
			TestEqual(TEXT("La cola no cambia"), Queue.Num(), 1);
		});

		It("el cubo de fichas no pasa de la capacidad de ráfaga aunque se acumule mucho tiempo", [this]()
		{
			FTerrainDeltaQueueModel Queue;
			Queue.Accrue(1000.0); // una hora de inactividad, en teoría
			TestEqual(TEXT("Tope de ráfaga"), Queue.AvailableBudgetBytes(), FTerrainDeltaQueueModel::BurstCapacityBytes);
		});

		It("gasta exactamente el coste codificado de la entrada al sacarla", [this]()
		{
			FTerrainDeltaQueueModel Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 10), 0.0);
			Queue.FillBudget();
			const double Before = Queue.AvailableBudgetBytes();

			FTerrainDeltaQueueModel::FEntry Out;
			TestTrue(TEXT("Sale"), Queue.TryPopWithinBudget(Out));

			FTerrainDeltaCodecModel::FChunkPatch Patch;
			Patch.Chunk = Out.Chunk;
			Patch.Samples = Out.Samples;
			const int32 ExpectedCost = FTerrainDeltaCodecModel::EncodedByteCount(Patch);

			TestEqual(TEXT("El presupuesto baja exactamente el coste del paquete"), Queue.AvailableBudgetBytes(), Before - ExpectedCost, 1e-6);
		});

		It("bajo una ráfaga de ediciones grandes, nunca gasta más bytes de los que ha acumulado", [this]()
		{
			FTerrainDeltaQueueModel Queue;
			// 30 chunks de 2000 muestras contiguas cada uno: a MaxRunCount por tramo, cada
			// chunk pesa ~4 KB codificados, muy por encima de BurstCapacityBytes (80 KB) en total.
			for (int32 C = 0; C < 30; ++C)
			{
				Queue.Enqueue(FIntVector(C, 0, 0), MakeSamples(0, 2000), 1000.0);
			}

			// 10 s de acumulación desde el cubo vacío: como mucho BurstCapacityBytes en total
			// (el cubo no puede tener más que su capacidad, la acumulación está limitada por Accrue).
			Queue.Accrue(10.0);
			const double BudgetAfterAccrue = Queue.AvailableBudgetBytes();
			TestTrue(TEXT("El presupuesto acumulado no pasa de la capacidad de ráfaga"),
				BudgetAfterAccrue <= FTerrainDeltaQueueModel::BurstCapacityBytes + 1e-6);

			double TotalSpent = 0.0;
			FTerrainDeltaQueueModel::FEntry Out;
			while (Queue.TryPopWithinBudget(Out))
			{
				FTerrainDeltaCodecModel::FChunkPatch Patch;
				Patch.Chunk = Out.Chunk;
				Patch.Samples = Out.Samples;
				TotalSpent += FTerrainDeltaCodecModel::EncodedByteCount(Patch);
			}

			TestTrue(TEXT("Lo gastado no pasa de lo acumulado"), TotalSpent <= BudgetAfterAccrue + 1e-6);
			TestTrue(TEXT("La ráfaga no vació una cola de 50 chunks grandes de golpe"), Queue.Num() > 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
