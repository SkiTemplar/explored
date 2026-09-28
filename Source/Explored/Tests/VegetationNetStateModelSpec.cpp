#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/VegetationNetStateModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

/** FVegetationNetStateSpec de biblia 08 §7.4: clave de 7 B sin pérdida y tope de 4096. */
BEGIN_DEFINE_SPEC(FVegetationNetStateModelSpec, "Explored.Net.Vegetation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FVegetationNetStateModelSpec)

namespace VegetationNetSpecDetail
{
	using FModel = FVegetationNetStateModel;

	FVegetationNetKey Key(int32 X, int32 Y, int32 Species, int32 Index)
	{
		FVegetationNetKey K;
		FModel::MakeKey(FIntPoint(X, Y), Species, Index, K);
		return K;
	}

	FVegetationNetState Stump(float RegrowDays = -1.0f)
	{
		return FModel::MakeState(EVegetationNetStage::Stump, 0, RegrowDays);
	}

	FVegetationNetState Felling(int32 Hits)
	{
		return FModel::MakeState(EVegetationNetStage::Felling, Hits, -1.0f);
	}

	FVegetationNetState RandomState(FExploredRandom& Rng)
	{
		const EVegetationNetStage Stage = static_cast<EVegetationNetStage>(Rng.RangeInt(0, 3));
		return FModel::MakeState(Stage, Rng.RangeInt(0, 15), Rng.Chance(0.5f) ? Rng.RangeFloat(4.0f, 400.0f) : -1.0f);
	}

	/** Una ronda de replicación: el servidor saca el delta y el cliente lo aplica. */
	void Replicate(FModel& Server, FModel& Client)
	{
		FModel::FDelta Delta;
		Server.ConsumeDelta(Delta);
		Client.ApplyDelta(Delta);
	}
}

void FVegetationNetStateModelSpec::Define()
{
	using namespace VegetationNetSpecDetail;

	Describe("Clave de 7 B y estado de 3 B", [this]()
	{
		It("una entrada mide 10 B y un cambio cuesta 14 B", [this]()
		{
			TArray<uint8> Bytes;
			FModel::AppendEntry(Key(-3, 7, 15, 65535), Felling(9), Bytes);
			TestEqual(TEXT("10 B"), Bytes.Num(), 10);
			TestEqual(TEXT("7 + 3"), FModel::KeyBytes + FModel::StateBytes, FModel::EntryBytes);
			TestEqual(TEXT("14 B por cambio"), FModel::BytesPerChange, 14);
		});

		It("va y vuelve sin pérdida en los extremos de celda, especie e índice", [this]()
		{
			const int32 Cells[] = {MIN_int16, -1, 0, 1, MAX_int16};
			for (int32 X : Cells)
			{
				for (int32 Y : Cells)
				{
					FVegetationNetKey K;
					TestTrue(TEXT("Clave válida"), FModel::MakeKey(FIntPoint(X, Y), 15, 65535, K));
					const FVegetationNetState S = Stump(123.4f);
					TArray<uint8> Bytes;
					FModel::AppendEntry(K, S, Bytes);
					FVegetationNetKey OutK;
					FVegetationNetState OutS;
					TestTrue(TEXT("Decodifica"), FModel::DecodeEntry(Bytes, OutK, OutS));
					TestTrue(FString::Printf(TEXT("Clave (%d, %d)"), X, Y), OutK == K && OutK.Cell() == FIntPoint(X, Y));
					TestTrue(TEXT("Estado"), OutS == S);
				}
			}
		});

		It("rechaza claves que no caben", [this]()
		{
			FVegetationNetKey K;
			TestFalse(TEXT("Celda X de 17 bits"), FModel::MakeKey(FIntPoint(40000, 0), 0, 0, K));
			TestFalse(TEXT("Celda Y negativa de más"), FModel::MakeKey(FIntPoint(0, -40000), 0, 0, K));
			TestFalse(TEXT("Especie 16"), FModel::MakeKey(FIntPoint(0, 0), 16, 0, K));
			TestFalse(TEXT("Especie negativa"), FModel::MakeKey(FIntPoint(0, 0), -1, 0, K));
			TestFalse(TEXT("Índice 65 536"), FModel::MakeKey(FIntPoint(0, 0), 0, 65536, K));
			TestFalse(TEXT("Índice negativo (INDEX_NONE)"), FModel::MakeKey(FIntPoint(0, 0), 0, INDEX_NONE, K));
		});

		It("rechaza especie 16, bits reservados, estados no canónicos y tamaños malos", [this]()
		{
			TArray<uint8> Good;
			FModel::AppendEntry(Key(1, 2, 3, 4), Felling(5), Good);
			FVegetationNetKey K;
			FVegetationNetState S;
			TArray<uint8> Bad = Good;
			Bad[4] = 16;
			TestFalse(TEXT("Especie 16"), FModel::DecodeEntry(Bad, K, S));
			Bad = Good;
			Bad[7] |= 0x80;
			TestFalse(TEXT("Bit reservado"), FModel::DecodeEntry(Bad, K, S));
			Bad = Good;
			Bad[7] = static_cast<uint8>(static_cast<uint8>(EVegetationNetStage::Stump) | (3 << 2));
			TestFalse(TEXT("Golpes en un tocón"), FModel::DecodeEntry(Bad, K, S));
			Bad = Good;
			Bad[8] = 1;
			TestFalse(TEXT("Rebrote en una tala en curso"), FModel::DecodeEntry(Bad, K, S));
			Bad = Good;
			Bad.Add(0);
			TestFalse(TEXT("11 bytes"), FModel::DecodeEntry(Bad, K, S));
			Bad.SetNum(9);
			TestFalse(TEXT("9 bytes"), FModel::DecodeEntry(Bad, K, S));
		});

		It("toda entrada que acepta vuelve a codificarse igual (50 000 entradas con bytes cambiados al azar)", [this]()
		{
			FExploredRandom Rng(0x7EA);
			int32 Accepted = 0;
			int32 Rejected = 0;
			for (int32 i = 0; i < 50000; ++i)
			{
				// Una entrada válida con 1 a 3 bytes cambiados: explora los bordes del formato
				// mucho mejor que 10 bytes al azar, que casi nunca son canónicos.
				TArray<uint8> Bytes;
				FModel::AppendEntry(Key(Rng.RangeInt(-500, 500), Rng.RangeInt(-500, 500), Rng.RangeInt(0, 15), Rng.RangeInt(0, 65535)), RandomState(Rng), Bytes);
				const int32 Flips = Rng.RangeInt(1, 3);
				for (int32 f = 0; f < Flips; ++f)
				{
					Bytes[Rng.RangeInt(0, FModel::EntryBytes - 1)] = static_cast<uint8>(Rng.NextUInt32() & 0xFF);
				}
				FVegetationNetKey K;
				FVegetationNetState S;
				if (!FModel::DecodeEntry(Bytes, K, S))
				{
					++Rejected;
					continue;
				}
				++Accepted;
				TArray<uint8> Again;
				FModel::AppendEntry(K, S, Again);
				if (Again != Bytes)
				{
					AddError(FString::Printf(TEXT("La entrada %d no es canónica"), i));
					return;
				}
			}
			TestTrue(TEXT("Acepta bastantes"), Accepted > 10000);
			TestTrue(TEXT("Y rechaza bastantes"), Rejected > 1000);
		});

		It("cuantiza el día de rebrote hacia arriba, con 0 para «sin rebrote»", [this]()
		{
			TestEqual(TEXT("Día 10 exacto"), static_cast<int32>(FModel::QuantizeRegrowDays(10.0f)), 40);
			TestEqual(TEXT("Día 10,01 → 10,25"), static_cast<int32>(FModel::QuantizeRegrowDays(10.01f)), 41);
			TestEqual(TEXT("Negativo: sin rebrote"), static_cast<int32>(FModel::QuantizeRegrowDays(-1.0f)), 0);
			TestEqual(TEXT("NaN: sin rebrote"), static_cast<int32>(FModel::QuantizeRegrowDays(std::numeric_limits<float>::quiet_NaN())), 0);
			TestEqual(TEXT("Infinito: sin rebrote"), static_cast<int32>(FModel::QuantizeRegrowDays(std::numeric_limits<float>::infinity())), 0);
			TestEqual(TEXT("Día 0 no es «sin rebrote»"), static_cast<int32>(FModel::QuantizeRegrowDays(0.0f)), 1);
			TestEqual(TEXT("Tope: 16 383,75 días"), static_cast<int32>(FModel::QuantizeRegrowDays(1e9f)), 65535);
			TestEqual(TEXT("Ida"), FModel::RegrowDays(41), 10.25f);
			TestEqual(TEXT("Sin rebrote"), FModel::RegrowDays(0), -1.0f);
			for (float Day = 4.0f; Day < 300.0f; Day += 0.37f)
			{
				if (FModel::RegrowDays(FModel::QuantizeRegrowDays(Day)) < Day)
				{
					AddError(FString::Printf(TEXT("El cliente vería rebrotar antes del día %f"), Day));
					break;
				}
			}
		});

		It("la tabla de especies no depende del orden ni de las mayúsculas", [this]()
		{
			TArray<FName> A = {FName(TEXT("HISM_Palm")), FName(TEXT("HISM_Bush")), FName(TEXT("hism_palm")), NAME_None, FName(TEXT("HISM_Fern"))};
			TArray<FName> B = {FName(TEXT("HISM_Fern")), FName(TEXT("HISM_PALM")), FName(TEXT("HISM_Bush"))};
			TArray<FName> TableA;
			TArray<FName> TableB;
			FModel::BuildSpeciesTable(A, TableA);
			FModel::BuildSpeciesTable(B, TableB);
			TestEqual(TEXT("Tres especies"), TableA.Num(), 3);
			TestTrue(TEXT("Misma tabla"), TableA == TableB);
			TestEqual(TEXT("Bush primero"), FModel::SpeciesIndex(TableA, FName(TEXT("HISM_Bush"))), 0);
			TestEqual(TEXT("Palm tercero"), FModel::SpeciesIndex(TableB, FName(TEXT("hism_palm"))), 2);
			TestEqual(TEXT("Desconocida"), FModel::SpeciesIndex(TableA, FName(TEXT("HISM_Oak"))), INDEX_NONE);

			TArray<FName> Many;
			for (int32 i = 0; i < 20; ++i)
			{
				Many.Add(FName(*FString::Printf(TEXT("HISM_%02d"), i)));
			}
			TArray<FName> Big;
			FModel::BuildSpeciesTable(Many, Big);
			TestEqual(TEXT("La 16 cabe"), FModel::SpeciesIndex(Big, FName(TEXT("HISM_15"))), 15);
			TestEqual(TEXT("La 17 no"), FModel::SpeciesIndex(Big, FName(TEXT("HISM_16"))), INDEX_NONE);
		});

		It("manda el progreso de tala solo a menos de 60 m y el resto a todos", [this]()
		{
			TestTrue(TEXT("Golpe a 59 m"), FModel::ShouldSendToClient(Felling(3), 5900.0));
			TestFalse(TEXT("Golpe a 60 m"), FModel::ShouldSendToClient(Felling(3), 6000.0));
			TestFalse(TEXT("Golpe a distancia NaN"), FModel::ShouldSendToClient(Felling(3), std::numeric_limits<double>::quiet_NaN()));
			TestTrue(TEXT("Tocón a 5 km"), FModel::ShouldSendToClient(Stump(), 500000.0));
		});
	});

	Describe("Array replicado", [this]()
	{
		It("solo viajan las entradas que cambian, y volver a intacta es una baja", [this]()
		{
			FModel Server;
			const FVegetationNetKey K = Key(0, 0, 1, 10);
			Server.Set(K, Felling(1));
			Server.Set(K, Felling(2));
			FModel::FDelta Delta;
			Server.ConsumeDelta(Delta);
			TestEqual(TEXT("Un cambio"), Delta.Changed.Num(), 1);
			TestEqual(TEXT("Con el último estado"), static_cast<int32>(Delta.Changed[0].Value.Hits), 2);
			TestEqual(TEXT("14 B"), Delta.EstimateBytes(), 14);

			Server.Set(K, Felling(2));
			Server.ConsumeDelta(Delta);
			TestTrue(TEXT("Repetir el mismo estado no manda nada"), Delta.IsEmpty());

			Server.Set(K, FVegetationNetState());
			Server.ConsumeDelta(Delta);
			TestEqual(TEXT("Una baja"), Delta.Removed.Num(), 1);
			TestEqual(TEXT("Sin entradas"), Server.Num(), 0);
		});

		It("talar un árbol de 12 golpes cuesta del orden de 140 B a quien está cerca", [this]()
		{
			FModel Server;
			const FVegetationNetKey K = Key(5, 5, 0, 3);
			int32 Bytes = 0;
			for (int32 Hit = 1; Hit <= 11; ++Hit)
			{
				Server.Set(K, Felling(Hit));
				FModel::FDelta Delta;
				Server.ConsumeDelta(Delta);
				Bytes += Delta.EstimateBytes();
			}
			Server.Set(K, Stump(40.0f));
			FModel::FDelta Delta;
			Server.ConsumeDelta(Delta);
			Bytes += Delta.EstimateBytes();
			TestEqual(TEXT("12 cambios × 14 B"), Bytes, 168);
		});

		It("al pasar de 4096 compacta primero los tocones sin rebrote, del más antiguo al más nuevo", [this]()
		{
			FModel Server;
			// 100 talas en curso, 200 brotes y 500 tocones que rebrotan, luego 4000 tocones sin rebrote.
			for (int32 i = 0; i < 100; ++i)
			{
				Server.Set(Key(1, 0, 0, i), Felling(3));
			}
			for (int32 i = 0; i < 200; ++i)
			{
				Server.Set(Key(2, 0, 0, i), FModel::MakeState(EVegetationNetStage::Sprout, 0, 90.0f));
			}
			for (int32 i = 0; i < 500; ++i)
			{
				Server.Set(Key(3, 0, 0, i), Stump(60.0f));
			}
			for (int32 i = 0; i < 4000; ++i)
			{
				Server.Set(Key(10 + i / 1000, 0, i % 16, i), Stump());
			}
			TestEqual(TEXT("4800 entradas"), Server.Num(), 4800);
			const int32 Moved = Server.Compact();
			TestEqual(TEXT("Baja a 3072"), Server.Num(), FModel::CompactTarget);
			TestEqual(TEXT("Movió 1728"), Moved, 1728);
			TestTrue(TEXT("El tocón más antiguo sale el primero"), Server.IsInSnapshot(Key(10, 0, 0, 0)));
			TestFalse(TEXT("El más nuevo se queda"), Server.IsInSnapshot(Key(13, 0, 3999 % 16, 3999)));
			TestTrue(TEXT("Se sigue viendo como tocón"), Server.View(Key(10, 0, 0, 0)) == Stump());
			TestFalse(TEXT("Los que rebrotan no salen si bastan los otros"), Server.IsInSnapshot(Key(3, 0, 0, 0)));
			TestEqual(TEXT("Por debajo del tope no hace nada"), Server.Compact(), 0);
		});

		It("si no bastan los tocones sin rebrote, saca los que rebrotan; talas y brotes nunca", [this]()
		{
			FModel Server;
			for (int32 i = 0; i < 2000; ++i)
			{
				Server.Set(Key(1, 0, 0, i), Felling(1 + i % 14));
			}
			for (int32 i = 0; i < 1500; ++i)
			{
				Server.Set(Key(2, 0, 0, i), FModel::MakeState(EVegetationNetStage::Sprout, 0, 50.0f));
			}
			for (int32 i = 0; i < 1000; ++i)
			{
				Server.Set(Key(3, 0, 0, i), Stump(80.0f));
			}
			Server.Compact();
			TestEqual(TEXT("Quedan talas y brotes: 3500"), Server.Num(), 3500);
			TestTrue(TEXT("Por debajo del tope duro"), Server.Num() <= FModel::MaxEntries);
			TestTrue(TEXT("Tala intacta"), Server.View(Key(1, 0, 0, 7)) == Felling(8));
			TestTrue(TEXT("El tocón que rebrotaba pasa al snapshot"), Server.IsInSnapshot(Key(3, 0, 0, 0)));

			// Al brotar, el servidor lo vuelve a meter con Set y sale del snapshot.
			Server.Set(Key(3, 0, 0, 0), FModel::MakeState(EVegetationNetStage::Sprout, 0, 120.0f));
			TestFalse(TEXT("Fuera del snapshot"), Server.IsInSnapshot(Key(3, 0, 0, 0)));
			TestTrue(TEXT("Brote"), Server.View(Key(3, 0, 0, 0)).Stage == EVegetationNetStage::Sprout);
		});

		It("el cliente ve lo mismo que el servidor tras cada ronda (20 000 cambios al azar)", [this]()
		{
			FModel Server;
			FModel Client;
			FExploredRandom Rng(0xC0FFEE);
			TArray<FVegetationNetKey> Touched;
			TMap<FVegetationNetKey, int32> TouchedIndex;
			int32 MaxSeen = 0;
			for (int32 Round = 0; Round < 200; ++Round)
			{
				for (int32 Op = 0; Op < 100; ++Op)
				{
					const FVegetationNetKey K = Key(Rng.RangeInt(-2, 2), Rng.RangeInt(-2, 2), Rng.RangeInt(0, 15), Rng.RangeInt(0, 400));
					// Sobre todo talas (tocones sin rebrote), para que el tope se alcance y compacte.
					const FVegetationNetState S = Rng.Chance(0.6f) ? Stump() : RandomState(Rng);
					Server.Set(K, S);
					if (!TouchedIndex.Contains(K))
					{
						TouchedIndex.Add(K, Touched.Add(K));
					}
				}
				Server.Compact();
				MaxSeen = FMath::Max(MaxSeen, Server.Num());
				Replicate(Server, Client);
				if ((Round % 20) == 19 || Round == 199)
				{
					for (const FVegetationNetKey& K : Touched)
					{
						if (Server.View(K) != Client.View(K))
						{
							AddError(FString::Printf(TEXT("Ronda %d: la instancia (%d, %d, %d, %d) no coincide"), Round, K.CellX, K.CellY, K.Species, K.Index));
							return;
						}
					}
				}
			}
			TestTrue(TEXT("Nunca por encima del tope tras compactar"), MaxSeen <= FModel::MaxEntries);
			TestTrue(TEXT("La prueba llegó a compactar"), Touched.Num() > FModel::MaxEntries);
		});

		It("el cliente ignora un snapshot corrupto o una entrada no canónica y aplica el resto", [this]()
		{
			FModel Client;
			FModel::FDelta Delta;
			Delta.CellSnapshots.Add(TPair<FIntPoint, FString>(FIntPoint(0, 0), TEXT("r:9-3")));
			Delta.CellSnapshots.Add(TPair<FIntPoint, FString>(FIntPoint(1, 0), TEXT("r:5")));
			FVegetationNetState Bad;
			Bad.Stage = EVegetationNetStage::Stump;
			Bad.Hits = 7;
			Delta.Changed.Add(TPair<FVegetationNetKey, FVegetationNetState>(Key(2, 0, 0, 0), Bad));
			Delta.Changed.Add(TPair<FVegetationNetKey, FVegetationNetState>(Key(2, 0, 0, 1), Felling(4)));
			TestFalse(TEXT("Avisa del fallo"), Client.ApplyDelta(Delta));
			TestTrue(TEXT("Celda buena aplicada"), Client.View(Key(1, 0, 0, 5)) == Stump());
			TestTrue(TEXT("Celda corrupta ignorada"), Client.View(Key(0, 0, 0, 4)) == FVegetationNetState());
			TestTrue(TEXT("Entrada no canónica ignorada"), Client.View(Key(2, 0, 0, 0)) == FVegetationNetState());
			TestTrue(TEXT("Entrada buena aplicada"), Client.View(Key(2, 0, 0, 1)) == Felling(4));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
