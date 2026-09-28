#include "Misc/AutomationTest.h"

#include <limits>

#include "Debug/NetBudgetModel.h"
#include "WorldGen/TerrainDeltaQueueModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FTerrainDeltaQueueModelSpec, "Explored.TerrainDeltaQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	using FSample = FTerrainDeltaCodecModel::FSample;
	using FQueue = FTerrainDeltaQueueModel;

	/** Parche de N muestras contiguas con el mismo delta, para forzar un coste conocido en bytes. */
	TArray<FSample> MakeSamples(int32 FirstIndex, int32 Count, int32 DeltaMm = 1)
	{
		TArray<FSample> Samples;
		Samples.Reserve(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			Samples.Add(FSample{ FirstIndex + I, DeltaMm });
		}
		return Samples;
	}

	/** Saca paquetes hasta que la cola se niegue, y los aplica al mundo del cliente. */
	int32 DrainInto(FQueue& Queue, TMap<FIntVector, FTerrainDeltaCodecModel::FChunkState>& ClientWorld, int32& OutBytes)
	{
		int32 Packets = 0;
		OutBytes = 0;
		FQueue::FOutgoingPacket Packet;
		while (Queue.TryPopPacket(Packet))
		{
			++Packets;
			OutBytes += Packet.Bytes.Num();
			TestTrue(TEXT("todo paquete que sale se decodifica y se aplica"), FTerrainDeltaCodecModel::DecodeAndApply(Packet.Bytes, ClientWorld));
		}
		return Packets;
	}
END_DEFINE_SPEC(FTerrainDeltaQueueModelSpec)

void FTerrainDeltaQueueModelSpec::Define()
{
	Describe("Fusión y orden de la cola", [this]()
	{
		It("fusiona una reedición del mismo chunk sin duplicar la entrada ni cambiar su posición", [this]()
		{
			FQueue Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 3), 100.0); // lejos, primero
			Queue.Enqueue(FIntVector(1, 0, 0), MakeSamples(0, 3), 100.0); // lejos, segundo
			// Se reedita el primer chunk con muestras nuevas y disjuntas: se fusiona, no se duplica.
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(10, 2), 90.0);

			TestEqual(TEXT("Sigue habiendo solo dos entradas"), Queue.Num(), 2);

			Queue.FillBudget();
			FQueue::FOutgoingPacket First, Second;
			TestTrue(TEXT("Sale el primero en llegar"), Queue.TryPopPacket(First));
			TestTrue(TEXT("Es el chunk (0,0,0)"), First.Chunk == FIntVector(0, 0, 0));
			TestTrue(TEXT("Cabe en un paquete"), First.bLastOfChunk);
			FTerrainDeltaCodecModel::FPacket Decoded;
			TestTrue(TEXT("Se decodifica"), FTerrainDeltaCodecModel::Decode(First.Bytes, Decoded));
			TestEqual(TEXT("Trae las 3 muestras originales más las 2 nuevas"), Decoded.Samples.Num(), 5);

			TestTrue(TEXT("Sale el segundo"), Queue.TryPopPacket(Second));
			TestTrue(TEXT("Es el chunk (1,0,0)"), Second.Chunk == FIntVector(1, 0, 0));
			TestTrue(TEXT("Cola vacía"), Queue.IsEmpty());
		});

		It("prioriza los chunks a menos de 30 m aunque hayan llegado después", [this]()
		{
			FQueue Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 3), 50.0); // lejos, llega primero
			Queue.Enqueue(FIntVector(1, 0, 0), MakeSamples(0, 3), 10.0); // cerca, llega después

			Queue.FillBudget();
			FQueue::FOutgoingPacket Out;
			TestTrue(TEXT("Saca algo"), Queue.TryPopPacket(Out));
			TestTrue(TEXT("Sale primero el cercano, aunque llegó después"), Out.Chunk == FIntVector(1, 0, 0));
		});

		It("dentro del mismo grupo de prioridad respeta el orden de llegada (FIFO), no la distancia exacta", [this]()
		{
			FQueue Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 3), 20.0); // cerca, llega primero
			Queue.Enqueue(FIntVector(1, 0, 0), MakeSamples(0, 3), 5.0);  // aún más cerca, llega después

			Queue.FillBudget();
			FQueue::FOutgoingPacket Out;
			TestTrue(TEXT("Saca algo"), Queue.TryPopPacket(Out));
			TestTrue(TEXT("Sale el que llegó primero, aunque el otro está más cerca todavía"), Out.Chunk == FIntVector(0, 0, 0));
		});

		It("no manda lo que está a más de 120 m hasta que el receptor se acerca, y no lo pierde", [this]()
		{
			FQueue Queue;
			Queue.Enqueue(FIntVector(5, 0, 0), MakeSamples(0, 4), 300.0);
			Queue.FillBudget();

			FQueue::FOutgoingPacket Out;
			TestFalse(TEXT("A 300 m no sale"), Queue.TryPopPacket(Out));
			TestTrue(TEXT("Sigue en cola"), Queue.Contains(FIntVector(5, 0, 0)));
			TestTrue(TEXT("Justo en el borde (120 m) ya es relevante"), Queue.UpdateDistance(FIntVector(5, 0, 0), 120.0));
			TestTrue(TEXT("Ahora sale"), Queue.TryPopPacket(Out));
			TestFalse(TEXT("UpdateDistance de un chunk que no está"), Queue.UpdateDistance(FIntVector(9, 9, 9), 1.0));
		});

		It("una distancia no finita no es relevante y una negativa cuenta como 0", [this]()
		{
			FQueue Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 2), std::numeric_limits<double>::quiet_NaN());
			Queue.Enqueue(FIntVector(1, 0, 0), MakeSamples(0, 2), 50.0);
			Queue.Enqueue(FIntVector(2, 0, 0), MakeSamples(0, 2), -7.0);
			Queue.Enqueue(FIntVector(3, 0, 0), MakeSamples(0, 2), -std::numeric_limits<double>::infinity());
			Queue.Enqueue(FIntVector(4, 0, 0), MakeSamples(0, 2), std::numeric_limits<double>::infinity());
			Queue.FillBudget();

			FQueue::FOutgoingPacket A, B, C;
			TestTrue(TEXT("Sale uno"), Queue.TryPopPacket(A));
			TestTrue(TEXT("El de distancia negativa, como cercano"), A.Chunk == FIntVector(2, 0, 0));
			TestTrue(TEXT("Sale otro"), Queue.TryPopPacket(B));
			TestTrue(TEXT("El de 50 m"), B.Chunk == FIntVector(1, 0, 0));
			TestFalse(TEXT("Los de NaN y ±infinito se quedan esperando una distancia buena"), Queue.TryPopPacket(C));
			TestEqual(TEXT("Quedan tres"), Queue.Num(), 3);
		});

		It("descarta y cuenta las muestras y chunks que no caben en el cable; un parche sin nada válido no crea entrada", [this]()
		{
			FQueue Queue;
			TArray<FSample> Mixed = { FSample{ -1, 5 }, FSample{ 3, 40000 }, FSample{ 4, 7 }, FSample{ 32768, 1 } };
			TestEqual(TEXT("Tres rechazadas"), Queue.Enqueue(FIntVector(0, 0, 0), Mixed, 0.0), 3);
			TestEqual(TEXT("Queda la válida"), Queue.PendingSamples(FIntVector(0, 0, 0))->Num(), 1);

			TestEqual(TEXT("Chunk fuera de int16: todas rechazadas"), Queue.Enqueue(FIntVector(40000, 0, 0), MakeSamples(0, 5), 0.0), 5);
			TestEqual(TEXT("Parche vacío: nada rechazado"), Queue.Enqueue(FIntVector(1, 0, 0), TArray<FSample>(), 0.0), 0);
			TestEqual(TEXT("Solo una entrada en cola"), Queue.Num(), 1);
		});

		It("una reedición de un chunk a medio enviar reenvía el valor nuevo y el cliente acaba igual que el servidor", [this]()
		{
			FQueue Queue;
			TMap<int32, int32> Server;
			const FIntVector Chunk(3, -2, 1);
			// 600 muestras: tres paquetes.
			TArray<FSample> First = MakeSamples(0, 600, 11);
			for (const FSample& S : First) { Server.Add(S.LocalIndex, S.DeltaMm); }
			Queue.Enqueue(Chunk, First, 5.0);

			TMap<FIntVector, FTerrainDeltaCodecModel::FChunkState> Client;
			Queue.Accrue(0.1); // 800 B: solo cabe el primer paquete
			int32 Bytes = 0;
			TestEqual(TEXT("Sale un paquete"), DrainInto(Queue, Client, Bytes), 1);
			TestTrue(TEXT("El chunk sigue en cola"), Queue.Contains(Chunk));

			// Se reedita: muestras ya enviadas (0..9), pendientes (500..509) y se devuelven a 0 algunas (20..24).
			TArray<FSample> Second = MakeSamples(0, 10, -3);
			Second.Append(MakeSamples(500, 10, 99));
			Second.Append(MakeSamples(20, 5, 0));
			for (const FSample& S : Second)
			{
				if (S.DeltaMm == 0) { Server.Remove(S.LocalIndex); } else { Server.Add(S.LocalIndex, S.DeltaMm); }
			}
			Queue.Enqueue(Chunk, Second, 5.0);

			for (int32 Tick = 0; Tick < 100 && !Queue.IsEmpty(); ++Tick)
			{
				Queue.Accrue(0.1);
				DrainInto(Queue, Client, Bytes);
			}
			TestTrue(TEXT("La cola se vacía"), Queue.IsEmpty());
			const FTerrainDeltaCodecModel::FChunkState* ClientChunk = Client.Find(Chunk);
			TestTrue(TEXT("El cliente tiene el chunk"), ClientChunk != nullptr);
			if (ClientChunk)
			{
				TestEqual(TEXT("Mismo número de muestras"), ClientChunk->Num(), Server.Num());
				TestTrue(TEXT("Estado idéntico"), FTerrainDeltaCodecModel::SamplesOf(*ClientChunk) == FTerrainDeltaCodecModel::SamplesOf(Server));
			}
		});
	});

	Describe("Presupuesto de bytes por segundo", [this]()
	{
		It("sin presupuesto acumulado, no saca nada de la cola", [this]()
		{
			FQueue Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 10), 0.0);
			FQueue::FOutgoingPacket Out;
			TestFalse(TEXT("Presupuesto en cero: no sale nada"), Queue.TryPopPacket(Out));
			TestEqual(TEXT("La cola no cambia"), Queue.Num(), 1);
			TestEqual(TEXT("Las muestras siguen ahí"), Queue.PendingSamples(FIntVector(0, 0, 0))->Num(), 10);
		});

		It("el crédito de ráfaga no pasa de 40 KB aunque pase mucho tiempo, y un tick roto no regala nada", [this]()
		{
			FQueue Queue;
			Queue.Accrue(1000.0);
			TestEqual(TEXT("Tope del cubo sostenido"), Queue.SustainedBudgetBytes(), FQueue::BurstCreditBytes);
			TestEqual(TEXT("40 000 B"), FQueue::BurstCreditBytes, 40000.0);
			TestEqual(TEXT("Disponible de golpe: solo un segundo de pico"), Queue.AvailableBudgetBytes(), FQueue::BurstBytesPerSecond);

			FQueue Broken;
			Broken.Accrue(std::numeric_limits<double>::quiet_NaN());
			Broken.Accrue(-5.0);
			Broken.Accrue(std::numeric_limits<double>::infinity());
			TestEqual(TEXT("NaN, negativo e infinito no dan presupuesto"), Broken.AvailableBudgetBytes(), 0.0);
		});

		It("cada paquete cuesta exactamente sus bytes y ninguno pasa de 512", [this]()
		{
			FQueue Queue;
			Queue.Enqueue(FIntVector(0, 0, 0), MakeSamples(0, 1000), 0.0);
			Queue.FillBudget();
			double Before = Queue.SustainedBudgetBytes();
			FQueue::FOutgoingPacket Out;
			int32 Count = 0;
			while (Queue.TryPopPacket(Out))
			{
				++Count;
				TestTrue(TEXT("≤ 512 B"), Out.Bytes.Num() <= FTerrainDeltaCodecModel::MaxPacketBytes);
				TestEqual(TEXT("Descuenta exactamente el paquete"), Queue.SustainedBudgetBytes(), Before - Out.Bytes.Num(), 1e-9);
				Before = Queue.SustainedBudgetBytes();
			}
			TestTrue(TEXT("Hacen falta varios paquetes"), Count >= 4);
			TestTrue(TEXT("Y la cola se vacía"), Queue.IsEmpty());
		});

		It("un chunk completo (32 768 muestras) no se atasca aunque pese más que todo el crédito", [this]()
		{
			FQueue Queue;
			TArray<FSample> Full;
			for (int32 I = 0; I <= FTerrainDeltaCodecModel::MaxLocalIndex; ++I)
			{
				Full.Add(FSample{ I, (I * 37) % 2001 - 1000 });
			}
			Queue.Enqueue(FIntVector(-1, 2, -3), Full, 0.0);

			TMap<FIntVector, FTerrainDeltaCodecModel::FChunkState> Client;
			int32 Bytes = 0;
			int32 Ticks = 0;
			for (; Ticks < 30 * 60 && !Queue.IsEmpty(); ++Ticks)
			{
				Queue.Accrue(1.0 / 30.0);
				DrainInto(Queue, Client, Bytes);
			}
			TestTrue(TEXT("Sale entero"), Queue.IsEmpty());
			// ≈ 66 KB a 8 KB/s desde cubo vacío: entre 8 y 9 s.
			TestTrue(TEXT("Y tarda lo que dice el presupuesto"), Ticks > 30 * 7 && Ticks < 30 * 10);
			const FTerrainDeltaCodecModel::FChunkState* State = Client.Find(FIntVector(-1, 2, -3));
			TestTrue(TEXT("Llega"), State != nullptr);
			if (State)
			{
				TArray<FSample> Expected;
				for (const FSample& S : Full) { if (S.DeltaMm != 0) { Expected.Add(S); } }
				TestTrue(TEXT("Llega exactamente igual"), FTerrainDeltaCodecModel::SamplesOf(*State) == Expected);
			}
		});

		It("ráfaga: con crédito lleno va a 16 KB/s durante 5 s y luego baja a 8 KB/s, sin pasar nunca de 16 KB en un segundo", [this]()
		{
			FQueue Queue;
			// 120 chunks de 2 000 muestras: ≈ 490 KB, mucho más de lo que cabe en 20 s.
			for (int32 C = 0; C < 120; ++C)
			{
				Queue.Enqueue(FIntVector(C, 0, 0), MakeSamples(0, 2000, C + 1), 10.0);
			}
			Queue.FillBudget();

			constexpr int32 Hz = 30;
			constexpr int32 Seconds = 20;
			TArray<int32> BytesAtTick;
			FNetBudgetModel Budget;
			const FName Client(TEXT("Cliente"));
			FQueue::FOutgoingPacket Out;
			for (int32 Tick = 0; Tick < Hz * Seconds; ++Tick)
			{
				if (Tick > 0)
				{
					Queue.Accrue(1.0 / Hz);
				}
				int32 Sent = 0;
				while (Queue.TryPopPacket(Out))
				{
					Sent += Out.Bytes.Num();
					Budget.RecordBytes(Client, FNetBudgetModel::TerrainChannel(), Out.Bytes.Num(), (Tick + 0.5) / Hz);
				}
				BytesAtTick.Add(Sent);
			}

			// Ventana deslizante de 1 s = 30 ticks: nunca más de 16 000 B.
			int32 WorstWindow = 0;
			for (int32 Start = 0; Start + Hz <= BytesAtTick.Num(); ++Start)
			{
				int32 Sum = 0;
				for (int32 K = 0; K < Hz; ++K) { Sum += BytesAtTick[Start + K]; }
				WorstWindow = FMath::Max(WorstWindow, Sum);
			}
			TestTrue(TEXT("Ningún segundo pasa de 16 KB"), WorstWindow <= 16000);

			auto SecondBytes = [&](int32 S)
			{
				int32 Sum = 0;
				for (int32 K = 0; K < Hz; ++K) { Sum += BytesAtTick[S * Hz + K]; }
				return Sum;
			};
			for (int32 S = 0; S < 4; ++S)
			{
				TestTrue(*FString::Printf(TEXT("Segundo %d de ráfaga cerca de 16 KB"), S), SecondBytes(S) > 15000);
			}
			for (int32 S = 7; S < Seconds; ++S)
			{
				TestTrue(*FString::Printf(TEXT("Segundo %d ya a ritmo sostenido"), S), SecondBytes(S) > 7000 && SecondBytes(S) <= 8000 + FTerrainDeltaCodecModel::MaxPacketBytes);
			}
			int32 Total = 0;
			for (const int32 B : BytesAtTick) { Total += B; }
			TestTrue(TEXT("En total no pasa de crédito + 8 KB/s"), Total <= FQueue::BurstCreditBytes + FQueue::SustainedBytesPerSecond * Seconds);

			// El validador de Explored.NetBudget está de acuerdo con la cola.
			Budget.CloseAllOpenSeconds();
			TArray<FNetBudgetModel::FSecondSummary> Rows;
			Budget.DrainClosedSeconds(Rows);
			TArray<FString> Violations;
			TestTrue(TEXT("La serie de la cola pasa el validador en pico"), FNetBudgetModel::ValidateSeries(Rows, FNetBudgetModel::EScenario::Peak, Violations));
			for (const FString& V : Violations) { AddError(V); }
		});

		It("es determinista: las mismas llamadas dan los mismos bytes en el mismo orden", [this]()
		{
			auto Run = [this]()
			{
				FQueue Queue;
				TArray<uint8> Stream;
				for (int32 Step = 0; Step < 200; ++Step)
				{
					const int32 C = (Step * 7) % 13;
					Queue.Enqueue(FIntVector(C, -C, C % 3), MakeSamples((Step * 131) % 30000, 1 + (Step * 17) % 300, Step - 100), static_cast<double>((Step * 11) % 150));
					Queue.Accrue(0.05);
					FQueue::FOutgoingPacket Out;
					while (Queue.TryPopPacket(Out)) { Stream.Append(Out.Bytes); }
				}
				return Stream;
			};
			const TArray<uint8> A = Run();
			const TArray<uint8> B = Run();
			TestTrue(TEXT("Mandó algo"), A.Num() > 0);
			TestTrue(TEXT("Idéntico byte a byte"), A == B);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
