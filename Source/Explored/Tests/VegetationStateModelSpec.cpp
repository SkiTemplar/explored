#include "Misc/AutomationTest.h"

#include "Save/SaveArchive.h"
#include "Save/SaveValue.h"
#include "Save/SaveWorldDeltas.h"
#include "WorldGen/FellingModel.h"
#include "WorldGen/VegetationStateModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FVegetationStateModelSpec, "Explored.VegetationState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	static constexpr int64 Day = FVegetationStateModel::MinutesPerDay;
	TArray<FFellingProfile> Profiles;
	const FFellingProfile& Get(const TCHAR* Species)
	{
		const FFellingProfile* P = FFellingModel::FindProfile(Profiles, FName(Species));
		check(P);
		return *P;
	}
	FVegetationInstanceState FellWith(const FFellingProfile& P, int64 Now)
	{
		return FVegetationStateModel::Fell(Now, FFellingModel::SproutMinutes(P), FFellingModel::RegrowMinutes(P));
	}
	/** Guarda el mundo, lo pasa por el texto de la partida (lo que hace el disco) y lo vuelve a cargar. */
	static FSaveWorldDeltas ThroughDisk(const FSaveWorldDeltas& World)
	{
		FSaveArchive Ar;
		World.Save(Ar);
		FSaveValue Parsed;
		FString Error;
		FSaveText::Parse(FSaveText::Write(Ar.GetRoot(), ESaveTextStyle::Compact), Parsed, Error);
		FSaveWorldDeltas Loaded;
		Loaded.Load(FSaveArchive(Parsed));
		return Loaded;
	}
	static TArray<uint8> Bytes(std::initializer_list<uint8> List)
	{
		TArray<uint8> Out;
		for (const uint8 B : List) { Out.Add(B); }
		return Out;
	}
END_DEFINE_SPEC(FVegetationStateModelSpec)

void FVegetationStateModelSpec::Define()
{
	BeforeEach([this]()
	{
		Profiles = FFellingModel::DefaultProfiles();
	});

	Describe("la etapa", [this]()
	{
		It("va de intacta a talándose, tocón, brote e intacta otra vez a los días de la especie", [this]()
		{
			FVegetationInstanceState S;
			TestTrue(TEXT("intacta"), FVegetationStateModel::StageAt(S, 0) == EVegetationStage::Intact);
			S.Hits = 3;
			TestTrue(TEXT("talándose"), FVegetationStateModel::StageAt(S, 0) == EVegetationStage::Felling);
			const FFellingProfile& Giant = Get(TEXT("JungleGiant"));
			const int64 T0 = 7 * Day + 600;
			S = FellWith(Giant, T0);
			TestTrue(TEXT("tocón"), FVegetationStateModel::StageAt(S, T0) == EVegetationStage::Stump);
			TestTrue(TEXT("brote a los 8 días"), FVegetationStateModel::StageAt(S, T0 + 8 * Day) == EVegetationStage::Sapling);
			TestFalse(TEXT("un minuto antes de los 24 días, aún no"), FVegetationStateModel::IsRegrown(S, T0 + 24 * Day - 1));
			TestTrue(TEXT("a los 24 días, talable"), FVegetationStateModel::IsRegrown(S, T0 + 24 * Day));
			TestTrue(TEXT("etapa intacta"), FVegetationStateModel::StageAt(S, T0 + 24 * Day) == EVegetationStage::Intact);
		});

		It("rebrote que atraviesa un cambio de día: 4 días exactos del arbusto, no «4 medianoches»", [this]()
		{
			const FFellingProfile& Shrub = Get(TEXT("Shrub"));
			const int64 Felled = 10 * Day + 23 * 60 + 59; // día 10, 23:59
			const FVegetationInstanceState S = FellWith(Shrub, Felled);
			TestTrue(TEXT("a las 00:00 del día 11 sigue talado"), FVegetationStateModel::StageAt(S, 11 * Day) != EVegetationStage::Intact);
			TestFalse(TEXT("a las 00:00 del día 14 tampoco"), FVegetationStateModel::IsRegrown(S, 14 * Day));
			TestFalse(TEXT("ni a las 23:58 del día 14"), FVegetationStateModel::IsRegrown(S, 14 * Day + 23 * 60 + 58));
			TestTrue(TEXT("a las 23:59 del día 14, sí"), FVegetationStateModel::IsRegrown(S, 14 * Day + 23 * 60 + 59));
			// Con el reloj del motor en días con decimales: 14,9993 días = 23:59 del día 14 (por suelo).
			TestEqual(TEXT("días → minuto"), FVegetationStateModel::MinuteFromDays(14.0 + (23.0 * 60.0 + 59.0) / 1440.0 + 1.0e-9), 14 * Day + 23 * 60 + 59);
		});

		It("una especie sin rebrote (o un tocón arrancado) se queda en tocón para siempre", [this]()
		{
			const FVegetationInstanceState S = FVegetationStateModel::Fell(100, 0, 0);
			TestTrue(TEXT("tocón"), FVegetationStateModel::StageAt(S, 100 + 100000 * Day) == EVegetationStage::Stump);
			TestFalse(TEXT("nunca rebrota"), FVegetationStateModel::IsRegrown(S, TNumericLimits<int64>::Max()));
		});

		It("MinuteFromDays: NaN, infinito y negativos dan 0; valores enormes no desbordan", [this]()
		{
			TestEqual(TEXT("NaN"), FVegetationStateModel::MinuteFromDays(NAN), (int64)0);
			TestEqual(TEXT("-inf"), FVegetationStateModel::MinuteFromDays(-INFINITY), (int64)0);
			TestEqual(TEXT("+inf"), FVegetationStateModel::MinuteFromDays(INFINITY), (int64)0);
			TestEqual(TEXT("-3"), FVegetationStateModel::MinuteFromDays(-3.0), (int64)0);
			TestTrue(TEXT("1e300 acotado"), FVegetationStateModel::MinuteFromDays(1.0e300) > 0);
			TestEqual(TEXT("20,5 días"), FVegetationStateModel::MinuteFromDays(20.5), (int64)29520);
		});
	});

	Describe("el guardado", [this]()
	{
		It("rebrote que atraviesa un guardado y una carga: vuelve al mismo minuto que sin guardar", [this]()
		{
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			const FIntPoint Cell(-3, 12);
			const FName Component(TEXT("HISM_Palm_0"));
			const int64 FelledAt = 5 * Day + 10 * 60; // día 5, 10:00
			const FVegetationInstanceState Before = FellWith(Palm, FelledAt);

			FSaveWorldDeltas World;
			World.Seed = 42;
			World.Layer(Component).Add(Cell, 17);
			FVegetationClock Clock;
			Clock.Set(Cell, Component, 17, FelledAt);
			World.VegetationClock = Clock.ToValue();

			// Se guarda con el tocón, se carga el día 12 (ya brote: asoma a los 6 días) y el reloj sigue desde ahí.
			const FSaveWorldDeltas Loaded = ThroughDisk(World);
			TestTrue(TEXT("el mundo vuelve igual"), Loaded == World);
			TestTrue(TEXT("sigue en la capa"), Loaded.FindLayer(Component) && Loaded.FindLayer(Component)->Contains(Cell, 17));
			FVegetationClock LoadedClock;
			TestEqual(TEXT("sin entradas descartadas"), LoadedClock.FromValue(Loaded.VegetationClock), 0);
			TestTrue(TEXT("mismo reloj"), LoadedClock == Clock);
			const int64 LoadNow = 12 * Day + 8 * 60;
			const int64 Resolved = FVegetationStateModel::ResolveFelledAt(LoadedClock.Find(Cell, Component, 17), LoadNow);
			TestEqual(TEXT("misma hora de tala"), Resolved, FelledAt);
			const FVegetationInstanceState After = FellWith(Palm, Resolved);
			TestEqual(TEXT("mismo minuto de rebrote"), After.RegrowAtMinute, Before.RegrowAtMinute);
			TestTrue(TEXT("recién cargado es brote"), FVegetationStateModel::StageAt(After, LoadNow) == EVegetationStage::Sapling);
			TestFalse(TEXT("un minuto antes, no"), FVegetationStateModel::IsRegrown(After, FelledAt + 18 * Day - 1));
			TestTrue(TEXT("a los 18 días, sí"), FVegetationStateModel::IsRegrown(After, FelledAt + 18 * Day));
		});

		It("dos guardados y cargas seguidos no mueven la hora (idempotente)", [this]()
		{
			FSaveWorldDeltas World;
			FVegetationClock Clock;
			Clock.Set(FIntPoint(0, 0), FName(TEXT("B")), 3, 1000);
			Clock.Set(FIntPoint(0, 0), FName(TEXT("A")), 9, 2000);
			Clock.Set(FIntPoint(-1, 5), FName(TEXT("A")), 0, 3000);
			World.VegetationClock = Clock.ToValue();
			const FSaveWorldDeltas Once = ThroughDisk(World);
			const FSaveWorldDeltas Twice = ThroughDisk(Once);
			TestTrue(TEXT("igual"), Once == Twice);
			FVegetationClock A, B;
			A.FromValue(Once.VegetationClock);
			B.FromValue(Twice.VegetationClock);
			TestTrue(TEXT("mismo reloj"), A == B && A == Clock);
			if (TestEqual(TEXT("tres entradas"), A.Num(), 3))
			{
				TestTrue(TEXT("ordenado por (Y, X, componente, índice)"),
					A.GetEntries()[0].Cell == FIntPoint(0, 0) && A.GetEntries()[0].Component == FName(TEXT("A"))
					&& A.GetEntries()[1].Component == FName(TEXT("B")) && A.GetEntries()[2].Cell == FIntPoint(-1, 5));
			}
		});

		It("una partida sin reloj (anterior a esta versión) toma lo talado como talado al cargar: rebrota más tarde, nunca antes", [this]()
		{
			FSaveWorldDeltas World;
			World.Layer(FName(TEXT("HISM_Giant"))).Add(FIntPoint(1, 1), 4);
			const FSaveWorldDeltas Loaded = ThroughDisk(World);
			TestTrue(TEXT("sin sección"), Loaded.VegetationClock.IsNull());
			FVegetationClock Clock;
			TestEqual(TEXT("un nulo no es una lista"), Clock.FromValue(Loaded.VegetationClock), -1);
			const int64 Now = 30 * Day;
			TestEqual(TEXT("talado ahora"), FVegetationStateModel::ResolveFelledAt(Clock.Find(FIntPoint(1, 1), FName(TEXT("HISM_Giant")), 4), Now), Now);
		});

		It("una entrada rota se descarta sola y las demás se conservan", [this]()
		{
			FSaveValue Value = FSaveValue::MakeArray();
			auto Row = [](std::initializer_list<FSaveValue> Items)
			{
				FSaveValue R = FSaveValue::MakeArray();
				for (const FSaveValue& V : Items) { R.Add(V); }
				return R;
			};
			using V = FSaveValue;
			Value.Add(Row({ V::MakeInt(0), V::MakeInt(0), V::MakeString(TEXT("A")), V::MakeInt(1), V::MakeInt(500) }));	// buena
			Value.Add(Row({ V::MakeInt(0), V::MakeInt(0), V::MakeString(TEXT("A")), V::MakeInt(2), V::MakeInt(-5) }));	// minuto negativo
			Value.Add(Row({ V::MakeInt(0), V::MakeInt(0), V::MakeString(TEXT("")), V::MakeInt(3), V::MakeInt(5) }));		// sin componente
			Value.Add(Row({ V::MakeInt(0), V::MakeInt(0), V::MakeString(TEXT("A")), V::MakeInt(-1), V::MakeInt(5) }));	// índice negativo
			Value.Add(Row({ V::MakeInt(1LL << 40), V::MakeInt(0), V::MakeString(TEXT("A")), V::MakeInt(4), V::MakeInt(5) })); // celda fuera de int32
			Value.Add(Row({ V::MakeInt(0), V::MakeInt(0), V::MakeString(TEXT("A")), V::MakeInt(5) }));					// corta
			Value.Add(Row({ V::MakeString(TEXT("0")), V::MakeInt(0), V::MakeString(TEXT("A")), V::MakeInt(6), V::MakeInt(5) })); // tipo roto
			Value.Add(V::MakeInt(7));																					// ni siquiera lista
			Value.Add(Row({ V::MakeInt(0), V::MakeInt(0), V::MakeString(TEXT("A")), V::MakeInt(1), V::MakeInt(900) }));	// repetida, más reciente
			FVegetationClock Clock;
			TestEqual(TEXT("siete descartadas"), Clock.FromValue(Value), 7);
			TestEqual(TEXT("una buena"), Clock.Num(), 1);
			const int64* Minute = Clock.Find(FIntPoint(0, 0), FName(TEXT("A")), 1);
			if (TestNotNull(TEXT("la buena"), Minute))
			{
				TestEqual(TEXT("repetida: gana la tala más reciente"), *Minute, (int64)900);
			}
		});

		It("una hora de tala posterior a la carga (reloj manipulado) cuenta como talado ahora", [this]()
		{
			const int64 Future = 99 * Day;
			TestEqual(TEXT("futuro"), FVegetationStateModel::ResolveFelledAt(&Future, 10 * Day), 10 * Day);
			const int64 Negative = -1;
			TestEqual(TEXT("negativo"), FVegetationStateModel::ResolveFelledAt(&Negative, 10 * Day), 10 * Day);
			const int64 Past = 3 * Day;
			TestEqual(TEXT("pasado, se respeta"), FVegetationStateModel::ResolveFelledAt(&Past, 10 * Day), 3 * Day);
		});

		It("Set y Remove: el mismo árbol se sustituye y los índices negativos o sin componente no entran", [this]()
		{
			FVegetationClock Clock;
			TestTrue(TEXT("alta"), Clock.Set(FIntPoint(2, 2), FName(TEXT("A")), 1, 10));
			TestTrue(TEXT("sustituye"), Clock.Set(FIntPoint(2, 2), FName(TEXT("A")), 1, 20));
			TestEqual(TEXT("una"), Clock.Num(), 1);
			TestEqual(TEXT("la nueva hora"), *Clock.Find(FIntPoint(2, 2), FName(TEXT("A")), 1), (int64)20);
			TestFalse(TEXT("índice negativo"), Clock.Set(FIntPoint(2, 2), FName(TEXT("A")), -1, 10));
			TestFalse(TEXT("sin componente"), Clock.Set(FIntPoint(2, 2), NAME_None, 1, 10));
			TestTrue(TEXT("baja"), Clock.Remove(FIntPoint(2, 2), FName(TEXT("A")), 1));
			TestFalse(TEXT("no dos veces"), Clock.Remove(FIntPoint(2, 2), FName(TEXT("A")), 1));
			TestTrue(TEXT("vacío"), Clock.IsEmpty());
		});
	});

	Describe("la red (biblia 08 §2.3)", [this]()
	{
		It("la clave de 7 bytes va y vuelve sin pérdida, también en los extremos", [this]()
		{
			for (const FIntPoint& Cell : { FIntPoint(0, 0), FIntPoint(-1, -1), FIntPoint(-32768, 32767), FIntPoint(32767, -32768), FIntPoint(123, -456) })
			{
				for (const int32 Slot : { 0, 7, 15 })
				{
					for (const int32 Index : { 0, 1, 255, 256, 65535 })
					{
						FVegetationNetKey Key;
						if (!TestTrue(TEXT("cabe"), FVegetationStateModel::MakeKey(Cell, Slot, Index, Key))) { return; }
						TArray<uint8> Wire;
						FVegetationStateModel::EncodeKey(Key, Wire);
						if (!TestEqual(TEXT("7 bytes"), Wire.Num(), FVegetationStateModel::KeyBytes)) { return; }
						FVegetationNetKey Back;
						TestTrue(TEXT("decodifica"), FVegetationStateModel::DecodeKey(Wire, 0, Back));
						if (!TestTrue(*FString::Printf(TEXT("(%d, %d) %d %d"), Cell.X, Cell.Y, Slot, Index), Back == Key)) { return; }
					}
				}
			}
		});

		It("la clave rechaza lo que no cabe y los paquetes cortos o con hueco imposible", [this]()
		{
			FVegetationNetKey Key;
			TestFalse(TEXT("celda X"), FVegetationStateModel::MakeKey(FIntPoint(40000, 0), 0, 0, Key));
			TestFalse(TEXT("celda Y"), FVegetationStateModel::MakeKey(FIntPoint(0, -32769), 0, 0, Key));
			TestFalse(TEXT("hueco 16"), FVegetationStateModel::MakeKey(FIntPoint(0, 0), 16, 0, Key));
			TestFalse(TEXT("hueco -1"), FVegetationStateModel::MakeKey(FIntPoint(0, 0), -1, 0, Key));
			TestFalse(TEXT("índice 65536"), FVegetationStateModel::MakeKey(FIntPoint(0, 0), 0, 65536, Key));
			TestFalse(TEXT("índice negativo"), FVegetationStateModel::MakeKey(FIntPoint(0, 0), 0, -1, Key));
			FVegetationNetKey Out;
			Out.Index = 77;
			TestFalse(TEXT("corto"), FVegetationStateModel::DecodeKey(Bytes({ 1, 2, 3, 4, 5, 6 }), 0, Out));
			TestFalse(TEXT("desplazamiento que se sale"), FVegetationStateModel::DecodeKey(Bytes({ 1, 2, 3, 4, 5, 6, 7 }), 1, Out));
			TestFalse(TEXT("desplazamiento negativo"), FVegetationStateModel::DecodeKey(Bytes({ 1, 2, 3, 4, 5, 6, 7 }), -1, Out));
			TestFalse(TEXT("hueco 16 en el cable"), FVegetationStateModel::DecodeKey(Bytes({ 0, 0, 0, 0, 16, 0, 0 }), 0, Out));
			TestEqual(TEXT("no toca la salida"), (int32)Out.Index, 77);
		});

		It("el estado de 3 bytes va y vuelve y redondea el rebrote hacia arriba al cuarto de día", [this]()
		{
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			const int64 Felled = 2 * Day + 7; // 7 minutos pasada la medianoche: el cuarto de día cae después
			const FVegetationInstanceState S = FellWith(Palm, Felled);
			const FVegetationNetState Net = FVegetationStateModel::ToNet(S, Felled);
			TestTrue(TEXT("tocón"), Net.Stage == EVegetationStage::Stump);
			TestEqual(TEXT("sin golpes en el cable"), (int32)Net.Hits, 0);
			const int64 ClientRegrow = FVegetationStateModel::RegrowMinuteFromNet(Net);
			TestTrue(TEXT("el cliente nunca antes que el servidor"), ClientRegrow >= S.RegrowAtMinute);
			TestTrue(TEXT("como mucho un cuarto de día después"), ClientRegrow - S.RegrowAtMinute < FVegetationStateModel::MinutesPerQuarterDay);
			TArray<uint8> Wire;
			FVegetationStateModel::EncodeState(Net, Wire);
			TestEqual(TEXT("3 bytes"), Wire.Num(), FVegetationStateModel::StateBytes);
			FVegetationNetState Back;
			TestTrue(TEXT("decodifica"), FVegetationStateModel::DecodeState(Wire, 0, Back));
			TestTrue(TEXT("igual"), Back == Net);
		});

		It("los golpes viajan con tope 15 y solo mientras se tala", [this]()
		{
			FVegetationInstanceState S;
			S.Hits = 40;
			const FVegetationNetState Net = FVegetationStateModel::ToNet(S, 0);
			TestTrue(TEXT("talándose"), Net.Stage == EVegetationStage::Felling);
			TestEqual(TEXT("tope 15"), (int32)Net.Hits, 15);
			TArray<uint8> Wire;
			FVegetationStateModel::EncodeState(Net, Wire);
			FVegetationNetState Back;
			TestTrue(TEXT("vuelve"), FVegetationStateModel::DecodeState(Wire, 0, Back) && Back == Net);
		});

		It("un tocón sin rebrote viaja como NoRegrow; un rebrote más allá del tope se acota por debajo de NoRegrow", [this]()
		{
			const FVegetationNetState Uprooted = FVegetationStateModel::ToNet(FVegetationStateModel::Fell(100, 0, 0), 200);
			TestEqual(TEXT("NoRegrow"), Uprooted.RegrowAtQuarterDays, FVegetationStateModel::NoRegrow);
			TestEqual(TEXT("-1 al leerlo"), FVegetationStateModel::RegrowMinuteFromNet(Uprooted), (int64)-1);
			const FVegetationNetState Far = FVegetationStateModel::ToNet(FVegetationStateModel::Fell(20000 * Day, Day, 24 * Day), 20000 * Day);
			TestEqual(TEXT("acotado"), Far.RegrowAtQuarterDays, (uint16)(FVegetationStateModel::NoRegrow - 1));
		});

		It("el estado rechaza paquetes imposibles o cortos sin tocar la salida", [this]()
		{
			FVegetationNetState Out;
			Out.RegrowAtQuarterDays = 1234;
			TestFalse(TEXT("bits libres"), FVegetationStateModel::DecodeState(Bytes({ 0x80 | 0x2, 0, 0 }), 0, Out));
			TestFalse(TEXT("golpes en un tocón"), FVegetationStateModel::DecodeState(Bytes({ (3 << 2) | 0x2, 0, 0 }), 0, Out));
			TestFalse(TEXT("golpes en una intacta"), FVegetationStateModel::DecodeState(Bytes({ (1 << 2) | 0x0, 0, 0 }), 0, Out));
			TestFalse(TEXT("NoRegrow en un brote"), FVegetationStateModel::DecodeState(Bytes({ 0x3, 0xFF, 0xFF }), 0, Out));
			TestFalse(TEXT("corto"), FVegetationStateModel::DecodeState(Bytes({ 0x2, 0 }), 0, Out));
			TestEqual(TEXT("no toca la salida"), (int32)Out.RegrowAtQuarterDays, 1234);
			TestTrue(TEXT("tocón con NoRegrow sí"), FVegetationStateModel::DecodeState(Bytes({ 0x2, 0xFF, 0xFF }), 0, Out));
		});

		It("ningún byte de estado posible cuelga ni da un valor fuera de rango", [this]()
		{
			for (int32 B0 = 0; B0 < 256; ++B0)
			{
				for (const int32 R : { 0, 1, 0xFFFE, 0xFFFF })
				{
					FVegetationNetState Out;
					if (FVegetationStateModel::DecodeState(Bytes({ (uint8)B0, (uint8)(R & 0xFF), (uint8)(R >> 8) }), 0, Out))
					{
						TestTrue(TEXT("etapa válida"), (uint8)Out.Stage <= 3);
						TestTrue(TEXT("golpes ≤ 15"), Out.Hits <= 15);
						TArray<uint8> Again;
						FVegetationStateModel::EncodeState(Out, Again);
						if (!TestEqual(TEXT("reencoda igual"), (int32)Again[0], B0)) { return; }
					}
				}
			}
		});

		It("la tabla de huecos de especie es la misma en las dos puntas, sea cual sea el orden de entrada", [this]()
		{
			const TArray<FName> A = FVegetationStateModel::SpeciesSlots({ FName(TEXT("HISM_Palm")), FName(TEXT("HISM_Giant")), FName(TEXT("HISM_Wide")), FName(TEXT("HISM_Palm")) });
			const TArray<FName> B = FVegetationStateModel::SpeciesSlots({ FName(TEXT("HISM_Wide")), NAME_None, FName(TEXT("HISM_Giant")), FName(TEXT("HISM_Palm")) });
			TestTrue(TEXT("iguales"), A == B);
			TestEqual(TEXT("sin repetir"), A.Num(), 3);
			TestEqual(TEXT("Giant primero"), FVegetationStateModel::FindSpeciesSlot(A, FName(TEXT("HISM_Giant"))), 0);
			TestEqual(TEXT("no está"), FVegetationStateModel::FindSpeciesSlot(A, FName(TEXT("HISM_Rock"))), (int32)INDEX_NONE);
			TArray<FName> Many;
			for (int32 i = 0; i < 20; ++i) { Many.Add(FName(*FString::Printf(TEXT("C%02d"), 19 - i))); }
			TestEqual(TEXT("tope de 16 huecos"), FVegetationStateModel::SpeciesSlots(Many).Num(), FVegetationStateModel::MaxSpeciesSlots);
		});

		It("el tope de 4096 saca primero los tocones sin rebrote, en orden de clave, y nada más", [this]()
		{
			TArray<FVegetationNetEntry> Entries;
			for (int32 i = 0; i < 4100; ++i)
			{
				FVegetationNetEntry E;
				FVegetationStateModel::MakeKey(FIntPoint(i % 50, i / 50), 0, i, E.Key);
				E.State.Stage = EVegetationStage::Stump;
				E.State.RegrowAtQuarterDays = 100;
				Entries.Add(E);
			}
			// Seis tocones arrancados (sin rebrote), en orden inverso de clave dentro del array.
			for (const int32 i : { 4000, 3000, 2000, 1000, 50, 10 })
			{
				Entries[i].State.RegrowAtQuarterDays = FVegetationStateModel::NoRegrow;
			}
			TArray<FVegetationNetKey> Moved;
			TestEqual(TEXT("sobran 4"), FVegetationStateModel::Compact(Entries, FVegetationStateModel::MaxReplicated, &Moved), 4);
			TestEqual(TEXT("en el tope"), Entries.Num(), FVegetationStateModel::MaxReplicated);
			if (TestEqual(TEXT("4 al snapshot"), Moved.Num(), 4))
			{
				// Orden (Y, X): la 10 (fila 0) antes que la 50 (fila 1), luego 1000 y 2000.
				TestEqual(TEXT("1.ª"), (int32)Moved[0].Index, 10);
				TestEqual(TEXT("2.ª"), (int32)Moved[1].Index, 50);
				TestEqual(TEXT("3.ª"), (int32)Moved[2].Index, 1000);
				TestEqual(TEXT("4.ª"), (int32)Moved[3].Index, 2000);
			}
			int32 StillNoRegrow = 0;
			for (const FVegetationNetEntry& E : Entries) { StillNoRegrow += E.State.RegrowAtQuarterDays == FVegetationStateModel::NoRegrow ? 1 : 0; }
			TestEqual(TEXT("quedan los otros dos"), StillNoRegrow, 2);
			TestEqual(TEXT("ya en el tope: nada más"), FVegetationStateModel::Compact(Entries, FVegetationStateModel::MaxReplicated), 0);
		});

		It("sin tocones sin rebrote el tope no quita nada (el coste queda por encima, pero nada se pierde)", [this]()
		{
			TArray<FVegetationNetEntry> Entries;
			Entries.SetNum(10);
			for (int32 i = 0; i < 10; ++i) { Entries[i].Key.Index = (uint16)i; Entries[i].State.Stage = EVegetationStage::Sapling; }
			TestEqual(TEXT("nada"), FVegetationStateModel::Compact(Entries, 4), 0);
			TestEqual(TEXT("las diez"), Entries.Num(), 10);
			TestEqual(TEXT("tope negativo = 0, sigue sin quitar lo que rebrota"), FVegetationStateModel::Compact(Entries, -5), 0);
		});
	});
}

#endif
