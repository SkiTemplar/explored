#include "Misc/AutomationTest.h"

#include "Save/SaveValue.h"
#include "WorldGen/FellingModel.h"
#include "WorldGen/GroundBranchModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FGroundBranchModelSpec, "Explored.GroundBranch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TArray<FGroundBranchSource> Sources;
	static constexpr int64 Day = FGroundBranchModel::MinutesPerDay;
	void EmptyCell(FGroundBranchCell& Cell)
	{
		while (Cell.Present.Num() > 0)
		{
			FGroundBranchModel::Pick(Cell, Cell.Present[0].Serial);
		}
	}
	bool SameCell(const FGroundBranchCell& A, const FGroundBranchCell& B)
	{
		if (A.Present.Num() != B.Present.Num() || A.Accumulator != B.Accumulator || A.NextSerial != B.NextSerial)
		{
			return false;
		}
		for (int32 i = 0; i < A.Present.Num(); ++i)
		{
			if (A.Present[i].Serial != B.Present[i].Serial || A.Present[i].Position != B.Present[i].Position)
			{
				return false;
			}
		}
		return true;
	}
	/** Guarda la celda, la pasa por texto compacto y la carga con la misma semilla. */
	bool RoundTrip(const FGroundBranchCell& In, FGroundBranchCell& Out)
	{
		FSaveValue Parsed;
		FString Error;
		return FSaveText::Parse(FSaveText::Write(FGroundBranchModel::SaveCell(In), ESaveTextStyle::Compact), Parsed, Error)
			&& FGroundBranchModel::LoadCell(Parsed, In.CellSeed, Out);
	}
	bool SameBranches(const FGroundBranchCell& A, const FGroundBranchCell& B)
	{
		if (!SameCell(A, B) || A.LastUpdateMinute != B.LastUpdateMinute)
		{
			return false;
		}
		for (int32 i = 0; i < A.Present.Num(); ++i)
		{
			if (A.Present[i].ItemId != B.Present[i].ItemId || A.Present[i].SourceIndex != B.Present[i].SourceIndex)
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
		// Un gigante al borde negativo de la celda y una palmera: dos objetos distintos en el suelo.
		Sources.Add(FFellingModel::MakeBranchSource(*FFellingModel::FindProfile(Profiles, FName(TEXT("JungleGiant"))), FVector2D(-10.0, -3190.0)));
		Sources.Add(FFellingModel::MakeBranchSource(*FFellingModel::FindProfile(Profiles, FName(TEXT("Palm"))), FVector2D(-1600.0, -1600.0)));
	});

	It("empieza llena con ramas dentro de la copa de su ejemplar", [this]()
	{
		const FGroundBranchCell Cell = FGroundBranchModel::Initialize(1234u, Sources, 0);
		TestEqual(TEXT("llena"), Cell.Present.Num(), FGroundBranchModel::TotalCapacity(Sources));
		for (const FGroundBranch& B : Cell.Present)
		{
			if (!TestTrue(TEXT("con ejemplar"), Sources.IsValidIndex(B.SourceIndex))) { continue; }
			const FGroundBranchSource& S = Sources[B.SourceIndex];
			TestTrue(TEXT("bajo la copa"), FVector2D::Distance(B.Position, S.Position) <= S.CrownRadiusMeters * 100.0 + 1.0e-6);
			TestTrue(TEXT("objeto del ejemplar"), B.ItemId == S.ItemId);
		}
	});

	It("llena no acumula: recoger tras mucho tiempo no suelta una ráfaga", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(1u, Sources, 0);
		TestEqual(TEXT("100 días llena, nada nuevo"), FGroundBranchModel::Advance(Cell, Sources, 100 * Day), 0);
		FGroundBranchModel::Pick(Cell, Cell.Present[0].Serial);
		TestEqual(TEXT("un minuto después, nada"), FGroundBranchModel::Advance(Cell, Sources, 100 * Day + 1), 0);
	});

	It("reaparecen al ritmo del perfil y se detienen en la capacidad", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(1u, Sources, 0);
		EmptyCell(Cell);
		// Ritmo total 1,5 + 0,5 = 2 ramas/día: en 1 día, 2 ramas.
		TestEqual(TEXT("1 día → 2"), FGroundBranchModel::Advance(Cell, Sources, Day), 2);
		TestEqual(TEXT("medio día más → 1"), FGroundBranchModel::Advance(Cell, Sources, Day + Day / 2), 1);
		FGroundBranchModel::Advance(Cell, Sources, 1000 * Day);
		TestEqual(TEXT("tope"), Cell.Present.Num(), FGroundBranchModel::TotalCapacity(Sources));
		TestEqual(TEXT("acumulador a cero al llenarse"), Cell.Accumulator, (int64)0);
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
		TestTrue(TEXT("mismas ramas, mismas posiciones, mismo acumulador"), SameCell(Big, Small));
	});

	It("un paso de 2 horas con la celda a medias equivale a 2 pasos de 1 hora (sin llenarse)", [this]()
	{
		FGroundBranchCell A = FGroundBranchModel::Initialize(5u, Sources, 0);
		EmptyCell(A);
		FGroundBranchCell B = A;
		FGroundBranchModel::Advance(A, Sources, 120);
		FGroundBranchModel::Advance(B, Sources, 60);
		FGroundBranchModel::Advance(B, Sources, 120);
		TestTrue(TEXT("iguales"), SameCell(A, B));
		TestTrue(TEXT("acumula parcial"), A.Accumulator > 0);
	});

	It("sin ejemplares en pie (todo talado) no aparece nada y las del suelo se quedan", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(9u, Sources, 0);
		const int32 Before = Cell.Present.Num();
		FGroundBranchModel::Pick(Cell, Cell.Present[0].Serial);
		const TArray<FGroundBranchSource> None;
		TestEqual(TEXT("nada nuevo"), FGroundBranchModel::Advance(Cell, None, 50 * Day), 0);
		TestEqual(TEXT("las del suelo siguen"), Cell.Present.Num(), Before - 1);
		TestEqual(TEXT("colocar sin fuentes no inventa ejemplar"), FGroundBranchModel::PlaceBranch(9u, 0u, None).SourceIndex, (int32)INDEX_NONE);
	});

	It("un reloj que retrocede no hace nada ni rebobina LastUpdateMinute", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(3u, Sources, 10 * Day);
		EmptyCell(Cell);
		TestEqual(TEXT("nada"), FGroundBranchModel::Advance(Cell, Sources, 5 * Day), 0);
		TestEqual(TEXT("no rebobina"), Cell.LastUpdateMinute, 10 * Day);
	});

	It("un paso gigantesco no desborda el acumulador", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(3u, Sources, 0);
		EmptyCell(Cell);
		FGroundBranchModel::Advance(Cell, Sources, TNumericLimits<int64>::Max() / 2);
		TestEqual(TEXT("llena"), Cell.Present.Num(), FGroundBranchModel::TotalCapacity(Sources));
		TestTrue(TEXT("acumulador sano"), Cell.Accumulator >= 0);
	});

	It("un reloj o un acumulador guardados extremos no desbordan", [this]()
	{
		FGroundBranchCell Cell = FGroundBranchModel::Initialize(3u, Sources, 0);
		EmptyCell(Cell);
		// Antes NowMinute - INT64_MIN desbordaba a negativo y la celda no volvía a llenarse.
		Cell.LastUpdateMinute = TNumericLimits<int64>::Min();
		FGroundBranchModel::Advance(Cell, Sources, 100);
		TestEqual(TEXT("llena"), Cell.Present.Num(), FGroundBranchModel::TotalCapacity(Sources));

		FGroundBranchCell Rich = FGroundBranchModel::Initialize(3u, Sources, 0);
		EmptyCell(Rich);
		Rich.Accumulator = TNumericLimits<int64>::Max();
		FGroundBranchModel::Advance(Rich, Sources, Day);
		TestEqual(TEXT("llena con acumulador enorme"), Rich.Present.Num(), FGroundBranchModel::TotalCapacity(Sources));
		TestTrue(TEXT("acumulador sano"), Rich.Accumulator >= 0);
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
		const FGroundBranch A = FGroundBranchModel::PlaceBranch(100u, 4u, Sources);
		const FGroundBranch B = FGroundBranchModel::PlaceBranch(100u, 4u, Sources);
		const FGroundBranch C = FGroundBranchModel::PlaceBranch(101u, 4u, Sources);
		TestTrue(TEXT("determinista"), A.Position == B.Position && A.SourceIndex == B.SourceIndex);
		TestFalse(TEXT("otra semilla"), A.Position == C.Position);
	});

	It("reparte las ramas según la capacidad de cada ejemplar", [this]()
	{
		int32 BySource[2] = { 0, 0 };
		for (uint32 Serial = 0; Serial < 6000; ++Serial)
		{
			const FGroundBranch B = FGroundBranchModel::PlaceBranch(55u, Serial, Sources);
			if (Sources.IsValidIndex(B.SourceIndex)) { ++BySource[B.SourceIndex]; }
		}
		// Gigante 4, palmera 2: dos tercios bajo el gigante (±5 %).
		const double Share = (double)BySource[0] / 6000.0;
		TestTrue(*FString::Printf(TEXT("proporción %.3f ≈ 0,667"), Share), FMath::Abs(Share - 2.0 / 3.0) < 0.05);
	});

	Describe("guardado", [this]()
	{
		It("una celda sin tocar no se guarda; al recoger, al acumular o al talar, sí", [this]()
		{
			FGroundBranchCell Cell = FGroundBranchModel::Initialize(77u, Sources, 0);
			TestFalse(TEXT("recién generada"), FGroundBranchModel::NeedsSave(Cell, Sources));
			FGroundBranchModel::Advance(Cell, Sources, 40 * Day);
			TestFalse(TEXT("llena tras mucho tiempo"), FGroundBranchModel::NeedsSave(Cell, Sources));

			// Talar la palmera cambia las fuentes: Initialize ya no la reproduciría.
			TArray<FGroundBranchSource> Felled = Sources;
			Felled.RemoveAt(1);
			TestTrue(TEXT("fuentes distintas"), FGroundBranchModel::NeedsSave(Cell, Felled));

			FGroundBranchModel::Pick(Cell, Cell.Present.Last().Serial);
			TestTrue(TEXT("con una recogida"), FGroundBranchModel::NeedsSave(Cell, Sources));
			FGroundBranchModel::Advance(Cell, Sources, 41 * Day);
			TestTrue(TEXT("rellenada con otra serie"), FGroundBranchModel::NeedsSave(Cell, Sources));

			// Sin árboles y sin ramas nunca: nada que guardar.
			const TArray<FGroundBranchSource> None;
			TestFalse(TEXT("celda sin árboles"), FGroundBranchModel::NeedsSave(FGroundBranchModel::Initialize(5u, None, 0), None));
		});

		It("guardar y cargar da la misma celda y el mismo futuro", [this]()
		{
			FGroundBranchCell Cell = FGroundBranchModel::Initialize(4242u, Sources, 0);
			FGroundBranchModel::Pick(Cell, 1);
			FGroundBranchModel::Pick(Cell, 4);
			FGroundBranchModel::Advance(Cell, Sources, 1 * Day + 37); // acumulado a medias
			FGroundBranchCell Loaded;
			if (!TestTrue(TEXT("carga"), RoundTrip(Cell, Loaded))) { return; }
			TestTrue(TEXT("misma celda, bit a bit"), SameBranches(Cell, Loaded));
			// Mismo futuro: las series nuevas no chocan con las cargadas y caen en el mismo sitio.
			FGroundBranchModel::Advance(Cell, Sources, 5 * Day);
			FGroundBranchModel::Advance(Loaded, Sources, 5 * Day);
			TestTrue(TEXT("mismo futuro"), SameBranches(Cell, Loaded));
			TestTrue(TEXT("se recoge por su serie"), FGroundBranchModel::Pick(Loaded, Loaded.Present[0].Serial));
		});

		It("sin series libres no aparece ninguna rama ni se repite una serie", [this]()
		{
			FGroundBranchCell Cell = FGroundBranchModel::Initialize(4242u, Sources, 0);
			EmptyCell(Cell);
			Cell.NextSerial = MAX_uint32 - 1;
			// Antes NextSerial++ pasaba de 4294967295 a 0 y la rama nueva repetía una serie.
			FGroundBranchModel::Advance(Cell, Sources, 60 * Day);
			TestEqual(TEXT("solo cabe una"), Cell.Present.Num(), 1);
			TestEqual(TEXT("con la última serie"), Cell.Present[0].Serial, MAX_uint32 - 1);
			TestEqual(TEXT("NextSerial no da la vuelta"), Cell.NextSerial, MAX_uint32);
			FGroundBranchCell Loaded;
			TestTrue(TEXT("y se vuelve a cargar"), RoundTrip(Cell, Loaded) && SameCell(Cell, Loaded));
		});

		It("las ramas caídas no se mueven al cargar aunque se haya talado su árbol", [this]()
		{
			FGroundBranchCell Cell = FGroundBranchModel::Initialize(9u, Sources, 0);
			// Se tala el gigante (fuente 0): las fuentes que se reconstruyen al cargar son otras.
			TArray<FGroundBranchSource> AfterFelling = Sources;
			AfterFelling.RemoveAt(0);
			FGroundBranchCell Loaded;
			if (!TestTrue(TEXT("carga"), RoundTrip(Cell, Loaded))) { return; }
			TestTrue(TEXT("mismas posiciones"), SameBranches(Cell, Loaded));
			// Con PlaceBranch y las fuentes nuevas alguna rama habría saltado de sitio.
			bool bWouldMove = false;
			for (const FGroundBranch& B : Cell.Present)
			{
				bWouldMove |= FGroundBranchModel::PlaceBranch(Cell.CellSeed, B.Serial, AfterFelling).Position != B.Position;
			}
			TestTrue(TEXT("recalcular las movería"), bWouldMove);
			// Y con más ramas que capacidad no aparece ninguna hasta recoger.
			TestEqual(TEXT("sin ramas nuevas"), FGroundBranchModel::Advance(Loaded, AfterFelling, 30 * Day), 0);
		});

		It("rechaza celdas imposibles sin tocar la salida", [this]()
		{
			const FGroundBranchCell Good = FGroundBranchModel::Initialize(3u, Sources, 0);
			const FSaveValue Base = FGroundBranchModel::SaveCell(Good);
			auto WithBranch = [&Base](int32 Row, int32 Column, FSaveValue Value)
			{
				FSaveValue V = Base;
				*V.Find(TEXT("branches"))->AtMutable(Row)->AtMutable(Column) = MoveTemp(Value);
				return V;
			};
			auto WithField = [&Base](const TCHAR* Key, FSaveValue Value)
			{
				FSaveValue V = Base;
				V.Set(Key, MoveTemp(Value));
				return V;
			};
			const TArray<FSaveValue> Bad = {
				WithField(TEXT("version"), FSaveValue::MakeInt(2)),
				WithField(TEXT("accumulator"), FSaveValue::MakeInt(-1)),
				WithField(TEXT("lastMinute"), FSaveValue::MakeInt(TNumericLimits<int64>::Min())),
				// NextSerial por debajo de una serie presente: la próxima rama la repetiría.
				WithField(TEXT("nextSerial"), FSaveValue::MakeInt(Good.Present.Last().Serial)),
				WithField(TEXT("nextSerial"), FSaveValue::MakeInt((int64)MAX_uint32 + 1)),
				// Serie repetida (la 1 pasa a ser 0) y posición no finita.
				WithBranch(1, 0, FSaveValue::MakeInt(0)),
				WithBranch(0, 1, FSaveValue::MakeString(TEXT("NaN"))),
				WithBranch(0, 2, FSaveValue::MakeDouble(2.0e9)),
				WithBranch(0, 4, FSaveValue::MakeInt(-2)),
				WithBranch(0, 3, FSaveValue::MakeInt(7)),
			};
			for (int32 i = 0; i < Bad.Num(); ++i)
			{
				FGroundBranchCell Out;
				Out.NextSerial = 12345u;
				TestFalse(*FString::Printf(TEXT("caso %d rechazado"), i), FGroundBranchModel::LoadCell(Bad[i], 3u, Out));
				TestTrue(*FString::Printf(TEXT("caso %d salida intacta"), i), Out.NextSerial == 12345u);
			}
			FGroundBranchCell Out;
			TestTrue(TEXT("la buena carga"), FGroundBranchModel::LoadCell(Base, 3u, Out));
		});
	});
}

#endif
