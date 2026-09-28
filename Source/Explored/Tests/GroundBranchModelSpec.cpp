#include "Misc/AutomationTest.h"

#include "WorldGen/FellingModel.h"
#include "WorldGen/GroundBranchModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FGroundBranchModelSpec, "Explored.GroundBranch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TArray<FGroundBranchSource> Sources;
	static constexpr int64 Day = FGroundBranchModel::MinutesPerDay;
	static constexpr int64 Cycle = FGroundBranchModel::CycleMinutes;
	void EmptyCell(FGroundBranchCell& Cell)
	{
		while (Cell.Present.Num() > 0)
		{
			FGroundBranchModel::Pick(Cell, Cell.Present[0].Serial);
		}
	}
	bool SameCell(const FGroundBranchCell& A, const FGroundBranchCell& B)
	{
		if (A.Present.Num() != B.Present.Num() || A.NextSerial != B.NextSerial || A.LastUpdateMinute != B.LastUpdateMinute)
		{
			return false;
		}
		for (int32 i = 0; i < A.Present.Num(); ++i)
		{
			if (A.Present[i].Serial != B.Present[i].Serial || A.Present[i].Position != B.Present[i].Position
				|| A.Present[i].SourceIndex != B.Present[i].SourceIndex || A.Present[i].ItemId != B.Present[i].ItemId)
			{
				return false;
			}
		}
		return true;
	}
END_DEFINE_SPEC(FGroundBranchModelSpec)

void FGroundBranchModelSpec::Define()
{
	BeforeEach([this]()
	{
		const TArray<FFellingProfile> Profiles = FFellingModel::DefaultProfiles();
		Sources.Reset();
		// Un gigante al borde negativo de la celda y una palmera: los dos sueltan rama_seca.
		Sources.Add(FFellingModel::MakeBranchSource(*FFellingModel::FindProfile(Profiles, FName(TEXT("JungleGiant"))), FVector2D(-10.0, -3190.0)));
		Sources.Add(FFellingModel::MakeBranchSource(*FFellingModel::FindProfile(Profiles, FName(TEXT("Palm"))), FVector2D(-1600.0, -1600.0)));
	});

	It("empieza con la tirada de un ciclo por árbol (2–4), dentro de su copa y con su objeto", [this]()
	{
		const FGroundBranchCell Cell = FGroundBranchModel::Initialize(1234u, Sources, 0);
		for (int32 i = 0; i < Sources.Num(); ++i)
		{
			const int32 N = FGroundBranchModel::CountUnder(Cell, i);
			TestTrue(*FString::Printf(TEXT("árbol %d: %d en [2, 4]"), i, N), 2 <= N && N <= 4);
		}
		for (const FGroundBranch& B : Cell.Present)
		{
			if (!TestTrue(TEXT("con árbol"), Sources.IsValidIndex(B.SourceIndex))) { continue; }
			const FGroundBranchSource& S = Sources[B.SourceIndex];
			TestTrue(TEXT("bajo la copa"), FVector2D::Distance(B.Position, S.Position) <= S.CrownRadiusMeters * 100.0 + 1.0e-6);
			TestTrue(TEXT("rama_seca"), B.ItemId == FName(TEXT("rama_seca")));
		}
	});

	It("cada ciclo de 6 h añade de 2 a 4 bajo cada árbol con sitio, justo al empezar el ciclo", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(1u, Sources, 0);
		EmptyCell(Cell);
		TestEqual(TEXT("un minuto antes del ciclo, nada"), FGroundBranchModel::Advance(Cell, Sources, Cycle - 1), 0);
		const int32 Spawned = FGroundBranchModel::Advance(Cell, Sources, Cycle);
		TestTrue(*FString::Printf(TEXT("al empezar el ciclo: %d en [4, 8]"), Spawned), 4 <= Spawned && Spawned <= 8);
		for (int32 i = 0; i < Sources.Num(); ++i)
		{
			const int32 N = FGroundBranchModel::CountUnder(Cell, i);
			TestTrue(TEXT("2–4 por árbol"), 2 <= N && N <= 4);
		}
		TestEqual(TEXT("el resto del ciclo, nada"), FGroundBranchModel::Advance(Cell, Sources, 2 * Cycle - 1), 0);
	});

	It("tope de ramas: nunca más de 6 bajo un árbol, por mucho tiempo que pase o en pasos que sean", [this]()
	{
		for (uint32 Seed = 1; Seed <= 40; ++Seed)
		{
			FGroundBranchCell Cell = FGroundBranchModel::Initialize(Seed, Sources, 0);
			for (int64 T = Cycle; T <= 10 * Day; T += Cycle)
			{
				FGroundBranchModel::Advance(Cell, Sources, T);
				for (int32 i = 0; i < Sources.Num(); ++i)
				{
					if (!TestTrue(TEXT("≤ 6"), FGroundBranchModel::CountUnder(Cell, i) <= 6)) { return; }
				}
			}
			for (int32 i = 0; i < Sources.Num(); ++i)
			{
				TestEqual(TEXT("lleno tras 10 días"), FGroundBranchModel::CountUnder(Cell, i), 6);
			}
			FGroundBranchModel::Advance(Cell, Sources, 400 * Day);
			TestEqual(TEXT("tope total"), Cell.Present.Num(), FGroundBranchModel::TotalCapacity(Sources));
		}
	});

	It("tope de ramas: un árbol lleno no acumula y al recoger una solo vuelve una en el ciclo siguiente", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(1u, Sources, 0);
		FGroundBranchModel::Advance(Cell, Sources, 100 * Day);
		TestEqual(TEXT("lleno"), Cell.Present.Num(), 12);
		FGroundBranch Picked;
		TestTrue(TEXT("recoge"), FGroundBranchModel::Pick(Cell, Cell.Present[0].Serial, &Picked));
		TestEqual(TEXT("dentro del mismo ciclo no vuelve"), FGroundBranchModel::Advance(Cell, Sources, 100 * Day + Cycle - 1), 0);
		TestEqual(TEXT("en el siguiente vuelve una, no una ráfaga"), FGroundBranchModel::Advance(Cell, Sources, 100 * Day + Cycle), 1);
		TestEqual(TEXT("bajo el mismo árbol"), FGroundBranchModel::CountUnder(Cell, Picked.SourceIndex), 6);
	});

	It("un árbol con tope o ritmo 0 (arrancado, arbusto) no suelta nada y no frena a los demás", [this]()
	{
		TArray<FGroundBranchSource> Mixed = Sources;
		Mixed[0].Capacity = 0;
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(4u, Mixed, 0);
		TestEqual(TEXT("sin tope, nada al empezar"), FGroundBranchModel::CountUnder(Cell, 0), 0);
		FGroundBranchModel::Advance(Cell, Mixed, 5 * Day);
		TestEqual(TEXT("ni después"), FGroundBranchModel::CountUnder(Cell, 0), 0);
		TestEqual(TEXT("el otro se llena"), FGroundBranchModel::CountUnder(Cell, 1), 6);
		Mixed[1].CycleMax = 0;
		EmptyCell(Cell);
		TestEqual(TEXT("ritmo 0, nada"), FGroundBranchModel::Advance(Cell, Mixed, 50 * Day), 0);
	});

	It("da exactamente lo mismo en un paso de 3 días que en 4320 pasos de un minuto", [this]()
	{
		FGroundBranchCell Big = FGroundBranchModel::Initialize(77u, Sources, 0);
		EmptyCell(Big);
		FGroundBranchCell Small = Big;
		FGroundBranchModel::Advance(Big, Sources, 3 * Day);
		for (int64 T = 1; T <= 3 * Day; ++T)
		{
			FGroundBranchModel::Advance(Small, Sources, T);
		}
		TestTrue(TEXT("mismas ramas, mismas posiciones"), SameCell(Big, Small));
	});

	It("con recogidas entre medias, pasos grandes y pequeños siguen coincidiendo", [this]()
	{
		FGroundBranchCell A = FGroundBranchModel::Initialize(5u, Sources, 0);
		FGroundBranchCell B = A;
		for (int64 T = 0; T < 4 * Day; T += Cycle)
		{
			// A avanza de golpe al final del ciclo; B, de hora en hora. Los dos recogen la primera rama.
			FGroundBranchModel::Advance(A, Sources, T + Cycle);
			for (int64 H = T + 60; H <= T + Cycle; H += 60) { FGroundBranchModel::Advance(B, Sources, H); }
			if (A.Present.Num() > 0) { FGroundBranchModel::Pick(A, A.Present[0].Serial); }
			if (B.Present.Num() > 0) { FGroundBranchModel::Pick(B, B.Present[0].Serial); }
			if (!TestTrue(TEXT("iguales"), SameCell(A, B))) { return; }
		}
	});

	It("funciona con minutos negativos: los ciclos van por suelo", [this]()
	{
		TestEqual(TEXT("-1 → ciclo -1"), FGroundBranchModel::CycleOf(-1), (int64)-1);
		TestEqual(TEXT("-360 → ciclo -1"), FGroundBranchModel::CycleOf(-Cycle), (int64)-1);
		TestEqual(TEXT("-361 → ciclo -2"), FGroundBranchModel::CycleOf(-Cycle - 1), (int64)-2);
		TestEqual(TEXT("0 → ciclo 0"), FGroundBranchModel::CycleOf(0), (int64)0);
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(2u, Sources, -Cycle - 10);
		EmptyCell(Cell);
		TestTrue(TEXT("cruzar -360 y 0 son dos ciclos"), FGroundBranchModel::Advance(Cell, Sources, 0) >= 8);
	});

	It("sin árboles no aparece nada y las del suelo se quedan", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(9u, Sources, 0);
		const int32 Before = Cell.Present.Num();
		FGroundBranchModel::Pick(Cell, Cell.Present[0].Serial);
		const TArray<FGroundBranchSource> None;
		TestEqual(TEXT("nada nuevo"), FGroundBranchModel::Advance(Cell, None, 50 * Day), 0);
		TestEqual(TEXT("las del suelo siguen"), Cell.Present.Num(), Before - 1);
		TestEqual(TEXT("colocar sin fuentes no inventa árbol"), FGroundBranchModel::PlaceBranch(9u, 0u, 0, None).SourceIndex, (int32)INDEX_NONE);
	});

	It("un reloj que retrocede no hace nada ni rebobina LastUpdateMinute", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(3u, Sources, 10 * Day);
		EmptyCell(Cell);
		TestEqual(TEXT("nada"), FGroundBranchModel::Advance(Cell, Sources, 5 * Day), 0);
		TestEqual(TEXT("no rebobina"), Cell.LastUpdateMinute, 10 * Day);
	});

	It("un paso gigantesco (reloj manipulado) llena y no se cuelga", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(3u, Sources, 0);
		EmptyCell(Cell);
		FGroundBranchModel::Advance(Cell, Sources, TNumericLimits<int64>::Max() / 2);
		TestEqual(TEXT("llena"), Cell.Present.Num(), FGroundBranchModel::TotalCapacity(Sources));
	});

	It("Pick quita una sola vez y los números de serie nunca se repiten", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(8u, Sources, 0);
		const uint32 Serial = Cell.Present[1].Serial;
		TestTrue(TEXT("primera vez"), FGroundBranchModel::Pick(Cell, Serial));
		TestFalse(TEXT("segunda vez"), FGroundBranchModel::Pick(Cell, Serial));
		FGroundBranchModel::Advance(Cell, Sources, 10 * Day);
		TSet<uint32> Seen;
		for (const FGroundBranch& B : Cell.Present)
		{
			TestFalse(TEXT("serie única"), Seen.Contains(B.Serial));
			Seen.Add(B.Serial);
		}
		TestFalse(TEXT("la recogida no vuelve con el mismo número"), Seen.Contains(Serial));
	});

	It("la misma semilla y el mismo número dan la misma rama; otra celda, otra posición", [this]()
	{
		const FGroundBranch A = FGroundBranchModel::PlaceBranch(100u, 4u, 0, Sources);
		const FGroundBranch B = FGroundBranchModel::PlaceBranch(100u, 4u, 0, Sources);
		const FGroundBranch C = FGroundBranchModel::PlaceBranch(101u, 4u, 0, Sources);
		TestTrue(TEXT("determinista"), A.Position == B.Position && A.SourceIndex == B.SourceIndex);
		TestFalse(TEXT("otra semilla"), A.Position == C.Position);
	});

	It("la tirada por ciclo cubre 2, 3 y 4 sin salirse", [this]()
	{
		int32 Seen[5] = { 0, 0, 0, 0, 0 };
		for (int64 C = -500; C < 500; ++C)
		{
			const int32 N = FGroundBranchModel::RollCycle(42u, C, 0, Sources[0]);
			if (!TestTrue(TEXT("en [2, 4]"), 2 <= N && N <= 4)) { return; }
			++Seen[N];
		}
		TestTrue(TEXT("salen los tres valores"), Seen[2] > 0 && Seen[3] > 0 && Seen[4] > 0);
	});

	Describe("el guardado", [this]()
	{
		It("va y vuelve igual, y seguir avanzando da lo mismo que sin guardar", [this]()
		{
			FGroundBranchCell Cell = FGroundBranchModel::Initialize(21u, Sources, 0);
			FGroundBranchModel::Advance(Cell, Sources, Day + 17);
			FGroundBranchModel::Pick(Cell, Cell.Present[2].Serial);
			FGroundBranchCell Loaded;
			TestTrue(TEXT("carga"), FGroundBranchModel::FromValue(FGroundBranchModel::ToValue(Cell), 21u, Sources, Loaded));
			TestTrue(TEXT("igual"), SameCell(Cell, Loaded));
			FGroundBranchModel::Advance(Cell, Sources, 3 * Day);
			FGroundBranchModel::Advance(Loaded, Sources, 3 * Day);
			TestTrue(TEXT("igual después"), SameCell(Cell, Loaded));
		});

		It("rechaza series repetidas, series ≥ next, árboles que no existen y tipos rotos, sin tocar la salida", [this]()
		{
			const FGroundBranchCell Cell = FGroundBranchModel::Initialize(21u, Sources, 0);
			const FSaveValue Good = FGroundBranchModel::ToValue(Cell);
			FGroundBranchCell Out = Cell;
			auto Broken = [&](TFunctionRef<void(FSaveValue&)> Break)
			{
				FSaveValue V = Good;
				Break(V);
				return !FGroundBranchModel::FromValue(V, 21u, Sources, Out) && SameCell(Out, Cell);
			};
			TestTrue(TEXT("serie repetida"), Broken([](FSaveValue& V) { FSaveValue* P = V.Find(TEXT("present")); P->Add(P->At(0)); }));
			TestTrue(TEXT("serie ≥ next"), Broken([](FSaveValue& V) { V.Set(TEXT("next"), FSaveValue::MakeInt(0)); }));
			TestTrue(TEXT("árbol inexistente"), Broken([](FSaveValue& V)
			{
				FSaveValue Row = FSaveValue::MakeArray();
				Row.Add(FSaveValue::MakeInt(0));
				Row.Add(FSaveValue::MakeInt(7));
				FSaveValue Present = FSaveValue::MakeArray();
				Present.Add(Row);
				V.Set(TEXT("present"), Present);
			}));
			TestTrue(TEXT("next negativo"), Broken([](FSaveValue& V) { V.Set(TEXT("next"), FSaveValue::MakeInt(-1)); }));
			TestTrue(TEXT("sin «last»"), Broken([](FSaveValue& V) { V.Remove(TEXT("last")); }));
			TestTrue(TEXT("no es un objeto"), Broken([](FSaveValue& V) { V = FSaveValue::MakeString(TEXT("x")); }));
		});
	});
}

#endif
