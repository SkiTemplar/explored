#include "Misc/AutomationTest.h"

#include "Core/SystemLinks.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

/** Reglas del cooperativo de biblia 08 §5.1, §5.2, §5.6 y §5.7 (FCoopRulesSpec de 08 §7.4). */
BEGIN_DEFINE_SPEC(FCoopRulesSpec, "Explored.Links.Coop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FCoopRulesSpec)

namespace CoopRulesSpecDetail
{
	using namespace ExploredLinks;

	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Inf = std::numeric_limits<float>::infinity();

	TArray<FCoopSleeper> Sleepers(int32 InBed, int32 Up, int32 Downed = 0)
	{
		TArray<FCoopSleeper> Out;
		for (int32 i = 0; i < InBed; ++i)
		{
			Out.Add({true, false});
		}
		for (int32 i = 0; i < Up; ++i)
		{
			Out.Add({false, false});
		}
		for (int32 i = 0; i < Downed; ++i)
		{
			Out.Add({false, true});
		}
		return Out;
	}
}

void FCoopRulesSpec::Define()
{
	using namespace ExploredLinks;
	using namespace CoopRulesSpecDetail;

	Describe("Dormir en grupo (08 §5.1)", [this]()
	{
		It("sigue la tabla: solo con todos acostados se salta la noche", [this]()
		{
			struct FRow { int32 InBed; int32 Up; int32 Downed; EGroupSleepStatus Status; int32 StillUp; };
			const FRow Rows[] = {
				{0, 4, 0, EGroupSleepStatus::NobodyInBed, 4},
				{1, 3, 0, EGroupSleepStatus::WaitingForOthers, 3},
				{3, 1, 0, EGroupSleepStatus::WaitingForOthers, 1},
				{4, 0, 0, EGroupSleepStatus::AllInBed, 0},
				{3, 0, 1, EGroupSleepStatus::BlockedByDowned, 1},
				{0, 3, 1, EGroupSleepStatus::NobodyInBed, 4},
				{1, 0, 0, EGroupSleepStatus::AllInBed, 0},
				{0, 0, 0, EGroupSleepStatus::NobodyInBed, 0},
			};
			for (const FRow& Row : Rows)
			{
				const FGroupSleepDecision D = DecideGroupSleep(Sleepers(Row.InBed, Row.Up, Row.Downed));
				const FString What = FString::Printf(TEXT("%d en cama, %d en pie, %d derribados"), Row.InBed, Row.Up, Row.Downed);
				TestTrue(What + TEXT(": estado"), D.Status == Row.Status);
				TestEqual(What + TEXT(": en pie"), D.StillUp, Row.StillUp);
			}
		});

		It("un derribado con la bandera de cama puesta no cuenta como acostado", [this]()
		{
			TArray<FCoopSleeper> Players = Sleepers(2, 0);
			Players.Add({true, true});
			const FGroupSleepDecision D = DecideGroupSleep(Players);
			TestTrue(TEXT("Bloqueado"), D.Status == EGroupSleepStatus::BlockedByDowned);
			TestEqual(TEXT("Uno en pie"), D.StillUp, 1);
		});

		It("calcula las horas hasta el amanecer en (0, 24]", [this]()
		{
			TestEqual(TEXT("22:00 → 7,5 h"), HoursUntilDawn(22.0f), 7.5f, 1e-4f);
			TestEqual(TEXT("03:00 → 2,5 h"), HoursUntilDawn(3.0f), 2.5f, 1e-4f);
			TestEqual(TEXT("Justo al amanecer → 24 h"), HoursUntilDawn(GroupSleepDawnHour), 24.0f, 1e-4f);
			TestEqual(TEXT("NaN se trata como medianoche"), HoursUntilDawn(NaN), GroupSleepDawnHour, 1e-4f);
		});

		It("con todos acostados pone ×120 y para en las 8 h si el amanecer queda más lejos", [this]()
		{
			FGroupSleepSession Session;
			const TArray<FCoopSleeper> All = Sleepers(4, 0);
			FGroupSleepSession::FResult R = Session.Update(All, 20.0f, 0.0f);
			TestTrue(TEXT("Empieza"), R.Event == EGroupSleepEvent::Started);
			TestEqual(TEXT("×120"), R.TimeScale, GroupSleepTimeScale);
			TestEqual(TEXT("Objetivo: 8 h (el amanecer está a 9,5 h)"), Session.TargetHours, 8.0f, 1e-4f);

			float Hours = 20.0f;
			int32 Ticks = 0;
			do
			{
				// Un tick de 1/60 s real a ×120 con días de 40 min: 120/60/100 h de juego.
				const float Step = 0.02f;
				Hours = FMath::Fmod(Hours + Step, 24.0f);
				R = Session.Update(All, Hours, Step);
				++Ticks;
			} while (R.Event == EGroupSleepEvent::None && Ticks < 10000);
			TestTrue(TEXT("Termina completo"), R.Event == EGroupSleepEvent::Completed);
			TestEqual(TEXT("Vuelve a ×1"), R.TimeScale, 1.0f);
			TestEqual(TEXT("8 h dormidas"), R.HoursSlept, 8.0f, 1e-4f);
			TestEqual(TEXT("Recuperación completa"), R.Recovery01, 1.0f, 1e-4f);

			R = Session.Update(All, Hours, 0.02f);
			TestTrue(TEXT("Siguen acostados: no se encadena otro salto"), R.Event == EGroupSleepEvent::None && R.TimeScale == 1.0f);

			Session.Update(Sleepers(3, 1), Hours, 0.02f);
			R = Session.Update(All, Hours, 0.02f);
			TestTrue(TEXT("Levantarse y volver a acostarse abre otro salto"), R.Event == EGroupSleepEvent::Started);
		});

		It("para al amanecer si llega antes de las 8 h", [this]()
		{
			FGroupSleepSession Session;
			const TArray<FCoopSleeper> All = Sleepers(2, 0);
			Session.Update(All, 2.0f, 0.0f);
			TestEqual(TEXT("Objetivo 3,5 h"), Session.TargetHours, 3.5f, 1e-4f);
			const FGroupSleepSession::FResult R = Session.Update(All, 5.6f, 3.6f);
			TestTrue(TEXT("Completo"), R.Event == EGroupSleepEvent::Completed);
			TestEqual(TEXT("No cuenta más de lo que había hasta el amanecer"), R.HoursSlept, 3.5f, 1e-4f);
			TestEqual(TEXT("Recuperación proporcional al sueño de 8 h"), R.Recovery01, 3.5f / 8.0f, 1e-4f);
		});

		It("si alguien se levanta a mitad vuelve a ×1 de inmediato y conserva las horas ganadas", [this]()
		{
			FGroupSleepSession Session;
			Session.Update(Sleepers(4, 0), 22.0f, 0.0f);
			FGroupSleepSession::FResult R = Session.Update(Sleepers(4, 0), 1.0f, 3.0f);
			TestEqual(TEXT("Sigue a ×120"), R.TimeScale, GroupSleepTimeScale);
			R = Session.Update(Sleepers(3, 1), 1.1f, 0.1f);
			TestTrue(TEXT("Interrumpido"), R.Event == EGroupSleepEvent::Interrupted);
			TestEqual(TEXT("×1 en el mismo tick"), R.TimeScale, 1.0f);
			TestEqual(TEXT("Se conservan las 3 h (el tick del que se levanta no cuenta)"), R.HoursSlept, 3.0f, 1e-4f);
			TestEqual(TEXT("Recuperación 3/8"), R.Recovery01, 3.0f / 8.0f, 1e-4f);
			TestFalse(TEXT("Sesión cerrada"), Session.bActive);
		});

		It("un jugador que entra en caliente o uno que cae derribado interrumpe el salto", [this]()
		{
			FGroupSleepSession Session;
			Session.Update(Sleepers(2, 0), 22.0f, 0.0f);
			FGroupSleepSession::FResult R = Session.Update(Sleepers(2, 1), 22.5f, 0.5f);
			TestTrue(TEXT("Entra uno nuevo en pie"), R.Event == EGroupSleepEvent::Interrupted);

			Session = FGroupSleepSession();
			Session.Update(Sleepers(3, 0), 22.0f, 0.0f);
			R = Session.Update(Sleepers(2, 0, 1), 22.5f, 0.5f);
			TestTrue(TEXT("Uno cae derribado"), R.Event == EGroupSleepEvent::Interrupted);
			TestTrue(TEXT("Y ahora está bloqueado"), R.Decision.Status == EGroupSleepStatus::BlockedByDowned);
		});

		It("ignora deltas negativos, NaN o infinitos", [this]()
		{
			FGroupSleepSession Session;
			const TArray<FCoopSleeper> All = Sleepers(2, 0);
			Session.Update(All, 22.0f, 0.0f);
			Session.Update(All, 22.0f, -5.0f);
			Session.Update(All, 22.0f, NaN);
			const FGroupSleepSession::FResult R = Session.Update(All, 22.0f, Inf);
			TestTrue(TEXT("No completa por un delta corrupto"), R.Event == EGroupSleepEvent::None);
			TestEqual(TEXT("Sin horas"), Session.HoursSlept, 0.0f);
		});
	});

	Describe("Derribado y reanimación (08 §5.2)", [this]()
	{
		It("decide derribado, muerte o espectador según modo y jugadores", [this]()
		{
			FCoopDownState S;
			TestTrue(TEXT("En solitario se muere"), OnHealthZero(S, FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor), 1, 5) == ECoopHealthZero::Dead);
			TestFalse(TEXT("Sin derribado en solitario"), S.bDowned);
			TestTrue(TEXT("Náufrago en cooperativo: espectador"), OnHealthZero(S, FSurvivalModeSettings::FromMode(ESurvivalMode::Castaway), 3, 5) == ECoopHealthZero::Spectator);
			TestFalse(TEXT("Sin derribado en Náufrago"), S.bDowned);
			TestTrue(TEXT("Explorador en cooperativo: derribado"), OnHealthZero(S, FSurvivalModeSettings::FromMode(ESurvivalMode::Explorer), 2, 5) == ECoopHealthZero::Downed);
			TestEqual(TEXT("90 s"), S.SecondsLeft, DownedSeconds);

			FCoopDownState T;
			TestTrue(TEXT("Superviviente en cooperativo: derribado"), OnHealthZero(T, FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor), 4, 5) == ECoopHealthZero::Downed);
			FCoopDownState U;
			TestTrue(TEXT("Personalizado se trata como Superviviente"), OnHealthZero(U, FSurvivalModeSettings::MakeCustom(2.0f, true), 2, 5) == ECoopHealthZero::Downed);
		});

		It("seguir a cero de salud no reinicia la cuenta atrás", [this]()
		{
			const FSurvivalModeSettings Mode = FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor);
			FCoopDownState S;
			OnHealthZero(S, Mode, 2, 5);
			TArray<FCoopDownState> Group = {S, FCoopDownState()};
			TArray<int32> Died;
			TickGroupDowned(Group, 60.0f, Died);
			OnHealthZero(Group[0], Mode, 2, 5);
			TestEqual(TEXT("Quedan 30 s"), Group[0].SecondsLeft, 30.0f, 1e-4f);
		});

		It("derribado y el compañero se va: muere una sola vez", [this]()
		{
			const FSurvivalModeSettings Mode = FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor);
			FCoopDownState S;
			OnHealthZero(S, Mode, 2, 5);
			TestTrue(TEXT("solo: muere"), OnHealthZero(S, Mode, 1, 5) == ECoopHealthZero::Dead);
			TestFalse(TEXT("ya no está derribado"), S.bDowned);
			TArray<FCoopDownState> Group = {S};
			TArray<int32> Died;
			TickGroupDowned(Group, 120.0f, Died);
			TestEqual(TEXT("no muere otra vez tras reaparecer"), Died.Num(), 0);
		});

		It("reanima en 6 s, o en 3 s con medicina, y soltar vuelve a empezar", [this]()
		{
			FCoopDownState S;
			OnHealthZero(S, FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor), 2, 5);
			TestFalse(TEXT("5,9 s no basta"), AdvanceRevive(S, 5.9f, false));
			CancelRevive(S);
			TestFalse(TEXT("Tras soltar, 1 s no basta"), AdvanceRevive(S, 1.0f, false));
			TestFalse(TEXT("4,9 s no basta"), AdvanceRevive(S, 3.9f, false));
			TestTrue(TEXT("6 s sí"), AdvanceRevive(S, 1.2f, false));

			FCoopDownState M;
			OnHealthZero(M, FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor), 2, 5);
			TestFalse(TEXT("2,9 s con botiquín no basta"), AdvanceRevive(M, 2.9f, true));
			TestTrue(TEXT("3 s con botiquín sí"), AdvanceRevive(M, 0.1f, true));
			FCoopDownState N;
			OnHealthZero(N, FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor), 2, 5);
			TestFalse(TEXT("NaN no avanza"), AdvanceRevive(N, NaN, true));
			TestFalse(TEXT("Infinito no avanza"), AdvanceRevive(N, Inf, true));
			TestEqual(TEXT("Progreso intacto"), N.ReviveProgressSeconds, 0.0f);
		});

		It("reconoce las tres medicinas de la tabla", [this]()
		{
			TestTrue(TEXT("botiquin"), IsReviveMedicine(FName(TEXT("botiquin"))));
			TestTrue(TEXT("vendaje_tela"), IsReviveMedicine(FName(TEXT("vendaje_tela"))));
			TestTrue(TEXT("gel_aloe"), IsReviveMedicine(FName(TEXT("gel_aloe"))));
			TestFalse(TEXT("planta_medicinal_aloe no (hay que machacarla)"), IsReviveMedicine(FName(TEXT("planta_medicinal_aloe"))));
			TestFalse(TEXT("Nada"), IsReviveMedicine(NAME_None));
		});

		It("levanta al 25 % de salud, ánimo −6 y con las heridas abiertas", [this]()
		{
			FCoopDownState S;
			OnHealthZero(S, FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor), 2, 5);
			FSurvivalState Body;
			Body.Health = 0.0f;
			Body.Morale = 40.0f;
			FWound Wound;
			Body.Wounds.Add(Wound);
			FinishRevive(S, Body, 5);
			TestFalse(TEXT("En pie"), S.bDowned);
			TestEqual(TEXT("Salud 25"), Body.Health, 25.0f);
			TestEqual(TEXT("Ánimo −6"), Body.Morale, 34.0f);
			TestEqual(TEXT("Heridas sin curar"), Body.Wounds.Num(), 1);
			TestEqual(TEXT("Una reanimación gastada"), RevivesUsedOn(S, 5), 1);

			Body.Morale = 3.0f;
			OnHealthZero(S, FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor), 2, 5);
			FinishRevive(S, Body, 5);
			TestEqual(TEXT("El ánimo no baja de 0"), Body.Morale, 0.0f);

			const float Before = Body.Health;
			FinishRevive(S, Body, 5);
			TestEqual(TEXT("Reanimar a quien está en pie no hace nada"), Body.Health, Before);
			TestEqual(TEXT("Ni gasta reanimación"), RevivesUsedOn(S, 5), 2);
		});

		It("tras 2 reanimaciones en el día el derribado dura 30 s, y al día siguiente vuelve a 90", [this]()
		{
			const FSurvivalModeSettings Mode = FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor);
			FCoopDownState S;
			FSurvivalState Body;
			for (int32 i = 0; i < MaxRevivesPerDay; ++i)
			{
				OnHealthZero(S, Mode, 2, 7);
				TestEqual(TEXT("90 s mientras queden reanimaciones"), S.SecondsLeft, DownedSeconds);
				FinishRevive(S, Body, 7);
			}
			OnHealthZero(S, Mode, 2, 7);
			TestEqual(TEXT("Tercera del día: 30 s"), S.SecondsLeft, DownedSecondsAfterCap);
			FinishRevive(S, Body, 7);
			OnHealthZero(S, Mode, 2, 8);
			TestEqual(TEXT("Día nuevo: 90 s"), S.SecondsLeft, DownedSeconds);
		});

		It("muere quien agota sus 90 s, pero no los demás si queda alguien en pie", [this]()
		{
			const FSurvivalModeSettings Mode = FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor);
			TArray<FCoopDownState> Group;
			Group.SetNum(3);
			OnHealthZero(Group[0], Mode, 3, 5);
			TArray<int32> Died;
			TickGroupDowned(Group, 30.0f, Died);
			OnHealthZero(Group[2], Mode, 3, 5);
			TickGroupDowned(Group, 59.0f, Died);
			TestEqual(TEXT("Nadie a los 89 s"), Died.Num(), 0);
			TickGroupDowned(Group, 1.0f, Died);
			TestTrue(TEXT("Muere solo el primero"), Died.Num() == 1 && Died[0] == 0);
			TestTrue(TEXT("El otro sigue derribado"), Group[2].bDowned);
			TestEqual(TEXT("Con 30 s"), Group[2].SecondsLeft, 30.0f, 1e-3f);
		});

		It("con todos derribados, al agotarse el primero mueren todos con él", [this]()
		{
			const FSurvivalModeSettings Mode = FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor);
			TArray<FCoopDownState> Group;
			Group.SetNum(4);
			for (int32 i = 0; i < 4; ++i)
			{
				OnHealthZero(Group[i], Mode, 4, 5);
				TArray<int32> Ignored;
				TickGroupDowned(Group, 10.0f, Ignored);
			}
			TArray<int32> Died;
			TickGroupDowned(Group, 50.0f, Died);
			TestEqual(TEXT("Al primero se le acaban los 90 s (40 + 50) y nadie está en pie: mueren los cuatro"), Died.Num(), 4);
			for (const FCoopDownState& P : Group)
			{
				TestFalse(TEXT("Ninguno queda derribado"), P.bDowned);
			}
		});

		It("un delta NaN no mata a nadie", [this]()
		{
			TArray<FCoopDownState> Group;
			Group.SetNum(2);
			OnHealthZero(Group[0], FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor), 2, 5);
			TArray<int32> Died;
			TickGroupDowned(Group, NaN, Died);
			TickGroupDowned(Group, -Inf, Died);
			TestEqual(TEXT("Nadie muere"), Died.Num(), 0);
			TestEqual(TEXT("Cuenta intacta"), Group[0].SecondsLeft, DownedSeconds);
		});
	});

	Describe("Escalado por jugadores y reparto de logros (08 §5.6, §5.7)", [this]()
	{
		It("escala lo que el grupo agota con la tabla de 1 a 4 jugadores", [this]()
		{
			const float Scales[] = {1.0f, 1.25f, 1.5f, 1.75f};
			const int32 Veins[] = {10, 12, 15, 17};
			const int32 Raiders[] = {5, 7, 9, 11};
			const int32 Bonus[] = {0, 0, 1, 1};
			for (int32 N = 1; N <= 4; ++N)
			{
				const FString What = FString::Printf(TEXT("N=%d"), N);
				TestEqual(What + TEXT(" escala"), CoopAbundanceScale(N), Scales[N - 1]);
				TestEqual(What + TEXT(" veta de 10"), ScaleFiniteVein(10, N), Veins[N - 1]);
				TestEqual(What + TEXT(" asaltantes"), PirateRaidersForPlayers(N), Raiders[N - 1]);
				TestEqual(What + TEXT(" categoría extra"), PirateCategoryBonus(N), Bonus[N - 1]);
			}
		});

		It("acota el número de jugadores y no devuelve vetas negativas", [this]()
		{
			TestEqual(TEXT("0 jugadores = 1"), CoopAbundanceScale(0), 1.0f);
			TestEqual(TEXT("9 jugadores = 4"), CoopAbundanceScale(9), 1.75f);
			TestEqual(TEXT("Veta negativa"), ScaleFiniteVein(-3, 4), 0);
			TestEqual(TEXT("Veta de 1 con 2 jugadores: redondeo abajo"), ScaleFiniteVein(1, 2), 1);
			TestEqual(TEXT("Veta enorme: se acota sin desbordar"), ScaleFiniteVein(2000000000, 4), MAX_int32);
			TestEqual(TEXT("Asaltantes enormes: se acota sin desbordar"), PirateRaidersForPlayers(4, 2000000000), MAX_int32);
		});

		It("lee coopScope sin distinguir mayúsculas y rechaza lo demás", [this]()
		{
			ECoopScope Scope = ECoopScope::Actor;
			TestTrue(TEXT("world"), ParseCoopScope(TEXT("World"), Scope) && Scope == ECoopScope::World);
			TestTrue(TEXT("witness"), ParseCoopScope(TEXT("witness"), Scope) && Scope == ECoopScope::Witness);
			TestTrue(TEXT("actor"), ParseCoopScope(TEXT("ACTOR"), Scope) && Scope == ECoopScope::Actor);
			TestFalse(TEXT("vacío"), ParseCoopScope(TEXT(""), Scope));
			TestFalse(TEXT("otro"), ParseCoopScope(TEXT("group"), Scope));
		});

		It("reparte el desbloqueo: actor, todos o testigos a menos de 50 m", [this]()
		{
			const FVector Event(1000.0, 0.0, 0.0);
			TArray<FCoopPlayerSpot> Players = {
				{10, FVector(1000.0, 0.0, 0.0), true},
				{11, FVector(1000.0, 4999.0, 0.0), true},
				{12, FVector(1000.0, 5000.0, 0.0), true},
				{13, FVector(1000.0, 10.0, 0.0), false},
				{14, FVector(NaN, 0.0, 0.0), true},
			};
			TestTrue(TEXT("actor"), AchievementRecipients(ECoopScope::Actor, 10, Event, Players) == TArray<int32>({10}));
			TestTrue(TEXT("world: todos los conectados"), AchievementRecipients(ECoopScope::World, 10, Event, Players) == TArray<int32>({10, 11, 12, 14}));
			TestTrue(TEXT("witness: a menos de 50 m, nunca en NaN"), AchievementRecipients(ECoopScope::Witness, 10, Event, Players) == TArray<int32>({10, 11}));
			TestTrue(TEXT("witness: el actor lejos lo recibe igual"), AchievementRecipients(ECoopScope::Witness, 12, Event, Players) == TArray<int32>({10, 11, 12}));
			TestTrue(TEXT("actor desconectado: nadie"), AchievementRecipients(ECoopScope::Actor, 13, Event, Players) == TArray<int32>());
			TestTrue(TEXT("hecho en NaN: solo el actor"), AchievementRecipients(ECoopScope::Witness, 11, FVector(NaN, 0.0, 0.0), Players) == TArray<int32>({11}));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
