#include "Misc/AutomationTest.h"

#include "Raiders/PirateThreatModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PirateThreatSpecDetail
{
	constexpr uint64 Seed = 20260926;

	FRaidConditions Base(bool bExposed = true, bool bHostile = false, int32 Players = 1)
	{
		FRaidConditions C;
		C.bBaseMarked = true;
		C.bResourcesExposed = bExposed;
		C.bNearestVillageHostile = bHostile;
		C.PlayerCount = Players;
		return C;
	}

	FPirateThreatState WithThreat(int32 Threat, int32 Day = 0)
	{
		FPirateThreatState S;
		S.Threat = Threat;
		S.CalmSinceDay = Day;
		return S;
	}

	/** Días de asalto en [From, To) con la Amenaza fija (se reinicia la calma cada día). */
	TArray<int32> RaidDays(FPirateThreatState State, int32 From, int32 To, const FRaidConditions& C, uint64 InSeed,
		TArray<FRaidDayResult>* OutResults = nullptr)
	{
		TArray<int32> Days;
		for (int32 Day = From; Day < To; ++Day)
		{
			State.CalmSinceDay = Day;
			const FRaidDayResult R = FPirateThreatModel::EvaluateDay(State, Day, C, InSeed);
			if (OutResults)
			{
				OutResults->Add(R);
			}
			if (R.Event == ERaidDayEvent::Raid)
			{
				Days.Add(Day);
			}
		}
		return Days;
	}
}

BEGIN_DEFINE_SPEC(FPirateThreatModelSpec, "Explored.Raiders.Threat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FPirateThreatModelSpec)

void FPirateThreatModelSpec::Define()
{
	using namespace PirateThreatSpecDetail;

	Describe("los piratas", [this]()
	{
		It("tienen la vida y el daño de la biblia", [this]()
		{
			TestEqual(TEXT("saqueador"), FPirateThreatModel::Stats(EPirateType::Raider).Health, 40.0f);
			TestEqual(TEXT("saqueador daño"), FPirateThreatModel::Stats(EPirateType::Raider).Damage, 9.0f);
			// Machete: 1,2 m, el mismo alcance que el del jugador (biblia 05 §3.1).
			TestEqual(TEXT("saqueador alcance"), FPirateThreatModel::Stats(EPirateType::Raider).RangeM, 1.2f);
			TestEqual(TEXT("capitán alcance"), FPirateThreatModel::Stats(EPirateType::Captain).RangeM, 1.2f);
			TestEqual(TEXT("arquero"), FPirateThreatModel::Stats(EPirateType::Archer).Health, 30.0f);
			TestEqual(TEXT("arquero alcance"), FPirateThreatModel::Stats(EPirateType::Archer).RangeM, 15.0f);
			TestEqual(TEXT("incendiario"), FPirateThreatModel::Stats(EPirateType::Firestarter).Health, 35.0f);
			TestEqual(TEXT("incendiario alcance"), FPirateThreatModel::Stats(EPirateType::Firestarter).RangeM, 12.0f);
			TestEqual(TEXT("capitán"), FPirateThreatModel::Stats(EPirateType::Captain).Health, 90.0f);
			TestEqual(TEXT("capitán daño"), FPirateThreatModel::Stats(EPirateType::Captain).Damage, 15.0f);
			TestTrue(TEXT("capitán aturde"), FPirateThreatModel::Stats(EPirateType::Captain).bStuns);
		});
	});

	Describe("la Amenaza", [this]()
	{
		It("empieza en 0 y sube lo de la biblia; defenderse no suma", [this]()
		{
			FPirateThreatState S;
			TestEqual(TEXT("empieza en 0"), S.Threat, 0);
			TestEqual(TEXT("+8"), FPirateThreatModel::Apply(S, EPirateThreatAction::LootOrBurnCamp, 1), 8);
			TestEqual(TEXT("+15"), FPirateThreatModel::Apply(S, EPirateThreatAction::DefeatShipCrew, 1), 15);
			TestEqual(TEXT("+4"), FPirateThreatModel::Apply(S, EPirateThreatAction::KillInProvokedRaid, 1), 4);
			TestEqual(TEXT("+0"), FPirateThreatModel::Apply(S, EPirateThreatAction::RepelUnprovokedRaid, 1), 0);
			TestEqual(TEXT("27"), S.Threat, 27);
		});

		It("recorta a 100", [this]()
		{
			FPirateThreatState S = WithThreat(95);
			TestEqual(TEXT("cambio real"), FPirateThreatModel::Apply(S, EPirateThreatAction::DefeatShipCrew, 0), 5);
			TestEqual(TEXT("100"), S.Threat, 100);
		});

		It("baja 5 por cada 10 días sin agresión, con saltos largos y sin bajar de 0", [this]()
		{
			FPirateThreatState S = WithThreat(30, 100);
			FPirateThreatModel::AdvanceCalm(S, 109);
			TestEqual(TEXT("9 días: nada"), S.Threat, 30);
			FPirateThreatModel::AdvanceCalm(S, 110);
			TestEqual(TEXT("10 días: 25"), S.Threat, 25);
			FPirateThreatModel::AdvanceCalm(S, 135);
			TestEqual(TEXT("35 días: 15"), S.Threat, 15);
			FPirateThreatModel::AdvanceCalm(S, 139);
			TestEqual(TEXT("39 días: 15"), S.Threat, 15);
			FPirateThreatModel::AdvanceCalm(S, 140);
			TestEqual(TEXT("40 días: 10"), S.Threat, 10);
			FPirateThreatModel::AdvanceCalm(S, TNumericLimits<int32>::Max());
			TestEqual(TEXT("suelo 0"), S.Threat, 0);
			FPirateThreatModel::AdvanceCalm(S, -50);
			TestEqual(TEXT("un día anterior no hace nada"), S.Threat, 0);
		});

		It("da lo mismo avanzar día a día que de un salto", [this]()
		{
			FPirateThreatState Step = WithThreat(80, 3);
			FPirateThreatState Jump = Step;
			for (int32 Day = 3; Day <= 97; ++Day)
			{
				FPirateThreatModel::AdvanceCalm(Step, Day);
			}
			FPirateThreatModel::AdvanceCalm(Jump, 97);
			TestTrue(TEXT("idéntico"), Step == Jump);
			TestEqual(TEXT("80 − 9×5"), Jump.Threat, 35);
		});

		It("una agresión reinicia la cuenta de calma; defenderse no", [this]()
		{
			FPirateThreatState S = WithThreat(40, 0);
			FPirateThreatModel::Apply(S, EPirateThreatAction::KillInProvokedRaid, 8);  // 44, calma desde 8
			FPirateThreatModel::AdvanceCalm(S, 17);
			TestEqual(TEXT("aún no"), S.Threat, 44);
			FPirateThreatModel::AdvanceCalm(S, 18);
			TestEqual(TEXT("a los 10 días de la agresión"), S.Threat, 39);

			FPirateThreatState D = WithThreat(40, 0);
			FPirateThreatModel::Apply(D, EPirateThreatAction::RepelUnprovokedRaid, 8);
			FPirateThreatModel::AdvanceCalm(D, 10);
			TestEqual(TEXT("defenderse no la reinicia"), D.Threat, 35);
		});
	});

	Describe("las categorías", [this]()
	{
		It("cortan en 25, 50 y 75", [this]()
		{
			TestEqual(TEXT("0"), FPirateThreatModel::CategoryOf(0), ERaidCategory::None);
			TestEqual(TEXT("24"), FPirateThreatModel::CategoryOf(24), ERaidCategory::None);
			TestEqual(TEXT("25"), FPirateThreatModel::CategoryOf(25), ERaidCategory::Low);
			TestEqual(TEXT("49"), FPirateThreatModel::CategoryOf(49), ERaidCategory::Low);
			TestEqual(TEXT("50"), FPirateThreatModel::CategoryOf(50), ERaidCategory::Medium);
			TestEqual(TEXT("74"), FPirateThreatModel::CategoryOf(74), ERaidCategory::Medium);
			TestEqual(TEXT("75"), FPirateThreatModel::CategoryOf(75), ERaidCategory::High);
			TestEqual(TEXT("100"), FPirateThreatModel::CategoryOf(100), ERaidCategory::High);
		});

		It("la aldea Hostil y los jugadores suben la categoría con tope 3, sin crear asaltos", [this]()
		{
			TestEqual(TEXT("1 + Hostil"), FPirateThreatModel::EffectiveCategory(ERaidCategory::Low, true, 1), ERaidCategory::Medium);
			TestEqual(TEXT("3 + Hostil"), FPirateThreatModel::EffectiveCategory(ERaidCategory::High, true, 1), ERaidCategory::High);
			TestEqual(TEXT("sin asalto + Hostil"), FPirateThreatModel::EffectiveCategory(ERaidCategory::None, true, 4), ERaidCategory::None);
			TestEqual(TEXT("2 jugadores +0"), FPirateThreatModel::EffectiveCategory(ERaidCategory::Low, false, 2), ERaidCategory::Low);
			TestEqual(TEXT("3 jugadores +1"), FPirateThreatModel::EffectiveCategory(ERaidCategory::Low, false, 3), ERaidCategory::Medium);
			TestEqual(TEXT("4 jugadores +1"), FPirateThreatModel::EffectiveCategory(ERaidCategory::Low, false, 4), ERaidCategory::Medium);
			TestEqual(TEXT("4 jugadores + Hostil"), FPirateThreatModel::EffectiveCategory(ERaidCategory::Low, true, 4), ERaidCategory::High);
			TestEqual(TEXT("99 jugadores se recorta a 4"), FPirateThreatModel::EffectiveCategory(ERaidCategory::Low, false, 99), ERaidCategory::Medium);
			TestEqual(TEXT("0 jugadores se recorta a 1"), FPirateThreatModel::EffectiveCategory(ERaidCategory::Low, false, 0), ERaidCategory::Low);
		});
	});

	Describe("los grupos", [this]()
	{
		It("son los de la biblia con un jugador", [this]()
		{
			const FRaidParty Low = FPirateThreatModel::MakeParty(ERaidCategory::Low, 1, Seed, 0);
			TestTrue(TEXT("1 Saqueador + 1 Arquero"), Low == FRaidParty{ 1, 1, 0, 0 });
			for (int32 Serial = 0; Serial < 200; ++Serial)
			{
				const FRaidParty Mid = FPirateThreatModel::MakeParty(ERaidCategory::Medium, 1, Seed, Serial);
				const FRaidParty High = FPirateThreatModel::MakeParty(ERaidCategory::High, 1, Seed, Serial);
				if (Mid.Archers != 1 || Mid.Firestarters != 1 || Mid.Captains != 0 || Mid.Raiders < 1 || Mid.Raiders > 2)
				{
					AddError(FString::Printf(TEXT("grupo medio %d fuera de la biblia"), Serial));
				}
				if (High.Captains != 1 || High.Total() < 5 || High.Total() > 6)
				{
					AddError(FString::Printf(TEXT("grupo alto %d fuera de la biblia (%d)"), Serial, High.Total()));
				}
			}
			TestEqual(TEXT("sin asalto, sin grupo"), FPirateThreatModel::MakeParty(ERaidCategory::None, 4, Seed, 0).Total(), 0);
		});

		It("escalan ×(1 + 0,4·(N−1)): 5 → 7/9/11, nunca con dos capitanes", [this]()
		{
			TestEqual(TEXT("1"), FPirateThreatModel::ScaledPartySize(5, 1), 5);
			TestEqual(TEXT("2"), FPirateThreatModel::ScaledPartySize(5, 2), 7);
			TestEqual(TEXT("3"), FPirateThreatModel::ScaledPartySize(5, 3), 9);
			TestEqual(TEXT("4"), FPirateThreatModel::ScaledPartySize(5, 4), 11);
			TestEqual(TEXT("recorte a 4"), FPirateThreatModel::ScaledPartySize(5, 50), 11);
			TestEqual(TEXT("base negativa"), FPirateThreatModel::ScaledPartySize(-3, 4), 0);
			for (int32 N = 1; N <= 4; ++N)
			{
				for (int32 Serial = 0; Serial < 50; ++Serial)
				{
					const FRaidParty One = FPirateThreatModel::MakeParty(ERaidCategory::High, 1, Seed, Serial);
					const FRaidParty P = FPirateThreatModel::MakeParty(ERaidCategory::High, N, Seed, Serial);
					if (P.Captains != 1 || P.Total() != FPirateThreatModel::ScaledPartySize(One.Total(), N))
					{
						AddError(FString::Printf(TEXT("N=%d serie %d: %d piratas, %d capitanes"), N, Serial, P.Total(), P.Captains));
					}
				}
			}
		});

		It("son deterministas por semilla y serie", [this]()
		{
			bool bAnyDiffers = false;
			for (int32 Serial = 0; Serial < 64; ++Serial)
			{
				const FRaidParty A = FPirateThreatModel::MakeParty(ERaidCategory::Medium, 3, Seed, Serial);
				const FRaidParty B = FPirateThreatModel::MakeParty(ERaidCategory::Medium, 3, Seed, Serial);
				TestTrue(TEXT("misma semilla, mismo grupo"), A == B);
				bAnyDiffers |= !(A == FPirateThreatModel::MakeParty(ERaidCategory::Medium, 3, Seed, 0));
			}
			TestTrue(TEXT("la serie cambia el grupo alguna vez"), bAnyDiffers);
		});
	});

	Describe("el programador", [this]()
	{
		It("sin base marcada o con Amenaza < 25 no hay asaltos", [this]()
		{
			FRaidConditions NoBase = Base();
			NoBase.bBaseMarked = false;
			TestEqual(TEXT("sin base"), RaidDays(WithThreat(100), 0, 200, NoBase, Seed).Num(), 0);
			TestEqual(TEXT("Amenaza 24"), RaidDays(WithThreat(24), 0, 200, Base(), Seed).Num(), 0);
			TestTrue(TEXT("Amenaza 25 sí"), RaidDays(WithThreat(25), 0, 200, Base(), Seed).Num() > 0);
		});

		It("respeta el intervalo de cada categoría", [this]()
		{
			struct FRow
			{
				int32 Threat;
				int32 Min;
				int32 Max;
			};
			const FRow Table[] = { { 25, 6, 9 }, { 49, 6, 9 }, { 50, 4, 6 }, { 74, 4, 6 }, { 75, 2, 4 }, { 100, 2, 4 } };
			for (const FRow& Row : Table)
			{
				const TArray<int32> Days = RaidDays(WithThreat(Row.Threat), 0, 400, Base(), Seed);
				if (!TestTrue(FString::Printf(TEXT("Amenaza %d: hay asaltos"), Row.Threat), Days.Num() > 10))
				{
					continue;
				}
				TSet<int32> Seen;
				for (int32 I = 1; I < Days.Num(); ++I)
				{
					const int32 Gap = Days[I] - Days[I - 1];
					Seen.Add(Gap);
					if (Gap < Row.Min || Gap > Row.Max)
					{
						AddError(FString::Printf(TEXT("Amenaza %d: %d días entre asaltos"), Row.Threat, Gap));
					}
				}
				TestEqual(FString::Printf(TEXT("Amenaza %d: salen todos los intervalos"), Row.Threat), Seen.Num(),
					Row.Max - Row.Min + 1);
				TestTrue(FString::Printf(TEXT("Amenaza %d: el primero dentro del intervalo"), Row.Threat),
					Days[0] >= Row.Min && Days[0] <= Row.Max);
			}
		});

		It("avisa con humo la víspera y con tambor de madrugada solo en categoría 3", [this]()
		{
			for (const int32 Threat : { 30, 60, 90 })
			{
				TArray<FRaidDayResult> Results;
				const TArray<int32> Days = RaidDays(WithThreat(Threat), 0, 120, Base(), Seed, &Results);
				for (const int32 Day : Days)
				{
					TestTrue(FString::Printf(TEXT("Amenaza %d: humo el día %d"), Threat, Day - 1),
						Results[Day - 1].bSmokeWarning && Results[Day - 1].Event == ERaidDayEvent::Warning);
					TestEqual(FString::Printf(TEXT("Amenaza %d: tambor el día %d"), Threat, Day), Results[Day].bDawnDrum, Threat >= 75);
				}
			}
		});

		It("sin recursos a la vista ni aldea Hostil aplaza al día siguiente sin volver a tirar", [this]()
		{
			FPirateThreatState S = WithThreat(60);
			const FRaidDayResult First = FPirateThreatModel::EvaluateDay(S, 0, Base(false), Seed);
			TestEqual(TEXT("programado"), First.Event, ERaidDayEvent::Scheduled);
			const int32 Due = S.NextRaidDay;
			for (int32 Day = 1; Day < Due; ++Day)
			{
				FPirateThreatModel::EvaluateDay(S, Day, Base(false), Seed);
			}
			for (int32 Day = Due; Day < Due + 5; ++Day)
			{
				S.CalmSinceDay = Day;
				const FRaidDayResult R = FPirateThreatModel::EvaluateDay(S, Day, Base(false), Seed);
				TestEqual(FString::Printf(TEXT("aplazado el día %d"), Day), R.Event, ERaidDayEvent::Postponed);
				TestEqual(TEXT("a mañana"), R.NextRaidDay, Day + 1);
			}
			TestEqual(TEXT("la serie no avanza"), S.RaidSerial, 0);
			const FRaidDayResult Hostile = FPirateThreatModel::EvaluateDay(S, Due + 5, Base(false, true), Seed);
			TestEqual(TEXT("con la aldea Hostil sí ataca"), Hostile.Event, ERaidDayEvent::Raid);
			TestEqual(TEXT("y un grado más fuerte"), Hostile.Category, ERaidCategory::High);
			TestEqual(TEXT("serie 1"), S.RaidSerial, 1);
		});

		It("borra el asalto programado si la Amenaza baja de 25 o desaparece la base", [this]()
		{
			FPirateThreatState S = WithThreat(26);
			FPirateThreatModel::EvaluateDay(S, 0, Base(), Seed);
			TestTrue(TEXT("programado"), S.NextRaidDay > 0);
			S.Threat = 24;
			const FRaidDayResult R = FPirateThreatModel::EvaluateDay(S, 1, Base(), Seed);
			TestEqual(TEXT("nada"), R.Event, ERaidDayEvent::Nothing);
			TestEqual(TEXT("borrado"), S.NextRaidDay, -1);

			FPirateThreatState T = WithThreat(80);
			FPirateThreatModel::EvaluateDay(T, 0, Base(), Seed);
			FRaidConditions NoBase = Base();
			NoBase.bBaseMarked = false;
			FPirateThreatModel::EvaluateDay(T, 1, NoBase, Seed);
			TestEqual(TEXT("sin base, borrado"), T.NextRaidDay, -1);
		});

		It("la bajada pasiva puede apagar los asaltos por sí sola", [this]()
		{
			FPirateThreatState S = WithThreat(30, 0);
			int32 Raids = 0;
			for (int32 Day = 0; Day <= 60; ++Day)
			{
				Raids += FPirateThreatModel::EvaluateDay(S, Day, Base(), Seed).Event == ERaidDayEvent::Raid ? 1 : 0;
			}
			TestEqual(TEXT("a los 60 días, 6 bajadas de 5"), S.Threat, 0);
			TestTrue(TEXT("pocos asaltos antes de calmarse"), Raids <= 3);
			TestEqual(TEXT("sin asalto programado"), S.NextRaidDay, -1);
		});

		It("no repite un día ni vuelve atrás", [this]()
		{
			FPirateThreatState S = WithThreat(90);
			FPirateThreatModel::EvaluateDay(S, 10, Base(), Seed);
			const FPirateThreatState After = S;
			TestEqual(TEXT("mismo día"), FPirateThreatModel::EvaluateDay(S, 10, Base(), Seed).Event, ERaidDayEvent::Nothing);
			TestEqual(TEXT("día anterior"), FPirateThreatModel::EvaluateDay(S, 3, Base(), Seed).Event, ERaidDayEvent::Nothing);
			TestTrue(TEXT("estado intacto"), S == After);
		});

		It("un salto de días sobre la fecha del asalto lo dispara el primer día evaluado", [this]()
		{
			FPirateThreatState S = WithThreat(90, 0);
			FPirateThreatModel::EvaluateDay(S, 0, Base(), Seed);
			const int32 Due = S.NextRaidDay;
			S.CalmSinceDay = Due + 3;
			const FRaidDayResult R = FPirateThreatModel::EvaluateDay(S, Due + 3, Base(), Seed);
			TestEqual(TEXT("asalto"), R.Event, ERaidDayEvent::Raid);
			TestTrue(TEXT("el siguiente, después"), S.NextRaidDay > Due + 3);
		});

		It("la frecuencia no escala con los jugadores", [this]()
		{
			const TArray<int32> Solo = RaidDays(WithThreat(60), 0, 300, Base(true, false, 1), Seed);
			const TArray<int32> Four = RaidDays(WithThreat(60), 0, 300, Base(true, false, 4), Seed);
			TestTrue(TEXT("mismos días"), Solo == Four);
		});

		It("es determinista por semilla y distinto con otra", [this]()
		{
			TArray<FRaidDayResult> A, B, C;
			const TArray<int32> DaysA = RaidDays(WithThreat(55), 0, 300, Base(true, false, 3), Seed, &A);
			const TArray<int32> DaysB = RaidDays(WithThreat(55), 0, 300, Base(true, false, 3), Seed, &B);
			const TArray<int32> DaysC = RaidDays(WithThreat(55), 0, 300, Base(true, false, 3), Seed + 1, &C);
			TestTrue(TEXT("mismos días"), DaysA == DaysB);
			bool bSameParties = true;
			for (int32 I = 0; I < A.Num(); ++I)
			{
				bSameParties &= A[I].Event == B[I].Event && A[I].Party == B[I].Party && A[I].NextRaidDay == B[I].NextRaidDay;
			}
			TestTrue(TEXT("mismos grupos"), bSameParties);
			TestFalse(TEXT("otra semilla, otro calendario"), DaysA == DaysC);
		});

		It("guardar y cargar a mitad del calendario da los mismos asaltos", [this]()
		{
			FPirateThreatState Straight = WithThreat(70);
			FPirateThreatState Split = Straight;
			TArray<int32> A, B;
			for (int32 Day = 0; Day < 150; ++Day)
			{
				if (FPirateThreatModel::EvaluateDay(Straight, Day, Base(), Seed).Event == ERaidDayEvent::Raid)
				{
					A.Add(Day);
				}
				if (Day == 37 || Day == 88)
				{
					FSaveArchive Ar;
					FPirateThreatModel::Save(Ar, Split);
					FSaveValue Parsed;
					FString Error;
					verify(FSaveText::Parse(FSaveText::Write(Ar.GetRoot()), Parsed, Error));
					FPirateThreatModel::Load(FSaveArchive(Parsed), Split);
				}
				if (FPirateThreatModel::EvaluateDay(Split, Day, Base(), Seed).Event == ERaidDayEvent::Raid)
				{
					B.Add(Day);
				}
			}
			TestTrue(TEXT("mismos días"), A == B);
			TestTrue(TEXT("mismo estado"), Straight == Split);
		});
	});

	Describe("el guardado", [this]()
	{
		It("una partida sin la sección empieza con Amenaza 0", [this]()
		{
			FPirateThreatState S = WithThreat(66);
			FPirateThreatModel::Load(FSaveArchive(), S);
			TestTrue(TEXT("por defecto"), S == FPirateThreatState());
		});

		It("recorta la Amenaza y descarta un asalto programado imposible", [this]()
		{
			FSaveArchive Ar;
			Ar.Write(TEXT("threat"), static_cast<int64>(5000));
			Ar.Write(TEXT("lastEvaluatedDay"), 40);
			Ar.Write(TEXT("nextRaidDay"), 2000000000);
			Ar.Write(TEXT("raidSerial"), -7);
			FPirateThreatState S;
			FPirateThreatModel::Load(Ar, S);
			TestEqual(TEXT("100"), S.Threat, 100);
			TestEqual(TEXT("sin asalto roto"), S.NextRaidDay, -1);
			TestEqual(TEXT("serie 0"), S.RaidSerial, 0);
			const FRaidDayResult R = FPirateThreatModel::EvaluateDay(S, 41, Base(), Seed);
			TestEqual(TEXT("se vuelve a programar"), R.Event, ERaidDayEvent::Scheduled);

			Ar.Write(TEXT("threat"), static_cast<int64>(-3));
			Ar.Write(TEXT("nextRaidDay"), 45);
			FPirateThreatModel::Load(Ar, S);
			TestEqual(TEXT("0"), S.Threat, 0);
			TestEqual(TEXT("un asalto a 5 días se conserva"), S.NextRaidDay, 45);

			Ar.SetValue(TEXT("threat"), FSaveValue::MakeString(TEXT("mucha")));
			FPirateThreatModel::Load(Ar, S);
			TestEqual(TEXT("texto → 0"), S.Threat, 0);
		});
	});
}

#endif
