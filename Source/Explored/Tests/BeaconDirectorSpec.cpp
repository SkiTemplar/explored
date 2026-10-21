#include "Misc/AutomationTest.h"

#include "Narrative/BeaconDirectorModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FBeaconDirectorSpec, "Explored.BeaconDirector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	/** Horas absolutas del día (1-based) y la hora. */
	static double At(int32 Day, double Hour) { return (Day - 1) * 24.0 + Hour; }

	static FBeaconFacts FactsAt(double AbsHours)
	{
		FBeaconFacts Facts;
		Facts.AbsHours = AbsHours;
		return Facts;
	}

	static FBeaconRunState InStage(EBeaconStage Stage)
	{
		return FBeaconDirectorModel::StateForStage(Stage);
	}

END_DEFINE_SPEC(FBeaconDirectorSpec)

void FBeaconDirectorSpec::Define()
{
	Describe(TEXT("etapas"), [this]()
	{
		It(TEXT("el prólogo acaba al beber un coco o a las 08:30 del día 1"), [this]()
		{
			FBeaconFacts Facts = FactsAt(At(1, 7.5));
			TestEqual(TEXT("Sin coco sigue"), FBeaconDirectorModel::Step(FBeaconRunState(), Facts).State.Progress.Stage, EBeaconStage::Prologo);
			Facts.bDrankCoconut = true;
			const FBeaconStepResult Drank = FBeaconDirectorModel::Step(FBeaconRunState(), Facts);
			TestEqual(TEXT("Con coco, Orilla"), Drank.State.Progress.Stage, EBeaconStage::Orilla);
			TestTrue(TEXT("Avisa del cambio"), Drank.bStageChanged);
			TestEqual(TEXT("A las 08:30, Orilla"), FBeaconDirectorModel::Step(FBeaconRunState(), FactsAt(At(1, 8.5))).State.Progress.Stage, EBeaconStage::Orilla);
		});

		It(TEXT("de la orilla al faro al llegar a 25 m de la torre o al leer la página 1"), [this]()
		{
			FBeaconFacts Facts = FactsAt(At(1, 10.0));
			Facts.MetersToTower = 30.0f;
			TestEqual(TEXT("A 30 m no"), FBeaconDirectorModel::Step(InStage(EBeaconStage::Orilla), Facts).State.Progress.Stage, EBeaconStage::Orilla);
			Facts.MetersToTower = 24.0f;
			TestEqual(TEXT("A 24 m sí"), FBeaconDirectorModel::Step(InStage(EBeaconStage::Orilla), Facts).State.Progress.Stage, EBeaconStage::Faro);
			const FBeaconStepResult Read = FBeaconDirectorModel::ReadPage(InStage(EBeaconStage::Orilla), 1);
			TestTrue(TEXT("Página leída"), Read.bOk && FNotebookModel::IsRead(Read.State.Notebook, 1));
			TestEqual(TEXT("La página 1 lleva al faro"), Read.State.Progress.Stage, EBeaconStage::Faro);
			const FBeaconStepResult Other = FBeaconDirectorModel::ReadPage(InStage(EBeaconStage::Orilla), 3);
			TestEqual(TEXT("Otra página no"), Other.State.Progress.Stage, EBeaconStage::Orilla);
			TestFalse(TEXT("Página 9 no existe"), FBeaconDirectorModel::ReadPage(FBeaconRunState(), 9).bOk);
		});

		It(TEXT("la balsa empieza el día 2 solo con la página 1 leída"), [this]()
		{
			const FBeaconRunState Faro = InStage(EBeaconStage::Faro);
			TestEqual(TEXT("Día 2 sin página"), FBeaconDirectorModel::Step(Faro, FactsAt(At(2, 6.5))).State.Progress.Stage, EBeaconStage::Faro);
			const FBeaconRunState Read = FBeaconDirectorModel::ReadPage(Faro, 1).State;
			TestEqual(TEXT("Día 1 con página"), FBeaconDirectorModel::Step(Read, FactsAt(At(1, 19.0))).State.Progress.Stage, EBeaconStage::Faro);
			TestEqual(TEXT("Día 2 con página"), FBeaconDirectorModel::Step(Read, FactsAt(At(2, 6.0))).State.Progress.Stage, EBeaconStage::Balsa);
		});

		It(TEXT("en el islote se pasa al carguero y la lente lleva a LenteEnMano"), [this]()
		{
			FBeaconFacts Facts = FactsAt(At(2, 10.0));
			Facts.MetersToFreighter = 40.0f;
			const FBeaconStepResult Islet = FBeaconDirectorModel::Step(InStage(EBeaconStage::Balsa), Facts);
			TestEqual(TEXT("Carguero"), Islet.State.Progress.Stage, EBeaconStage::Carguero);
			Facts.bHasLens = true;
			const FBeaconStepResult Lens = FBeaconDirectorModel::Step(Islet.State, Facts);
			TestEqual(TEXT("Lente en mano"), Lens.State.Progress.Stage, EBeaconStage::LenteEnMano);
			TestTrue(TEXT("Pensamiento de la lente"), Lens.Thoughts.Contains(EIriaThought::TakeLens));
			TestFalse(TEXT("Una sola vez"), FBeaconDirectorModel::Step(Lens.State, Facts).Thoughts.Contains(EIriaThought::TakeLens));
		});

		It(TEXT("el ciclón llega a las 19:00 del día 2 y la preparación a las 02:00 del día 3"), [this]()
		{
			TestEqual(TEXT("18:59 aún no"), FBeaconDirectorModel::Step(InStage(EBeaconStage::LenteEnMano), FactsAt(At(2, 18.98))).State.Progress.Stage, EBeaconStage::LenteEnMano);
			TestEqual(TEXT("19:00 ciclón"), FBeaconDirectorModel::Step(InStage(EBeaconStage::LenteEnMano), FactsAt(At(2, 19.0))).State.Progress.Stage, EBeaconStage::Ciclon);
			TestEqual(TEXT("Sin lente también"), FBeaconDirectorModel::Step(InStage(EBeaconStage::Balsa), FactsAt(At(2, 20.0))).State.Progress.Stage, EBeaconStage::Ciclon);
			TestEqual(TEXT("02:00 preparación"), FBeaconDirectorModel::Step(InStage(EBeaconStage::Ciclon), FactsAt(At(3, 2.0))).State.Progress.Stage, EBeaconStage::Preparacion);
			TestEqual(TEXT("Del prólogo al día 3 de un salto"), FBeaconDirectorModel::Step(FBeaconRunState(), FactsAt(At(3, 10.0))).State.Progress.Stage, EBeaconStage::Preparacion);
		});

		It(TEXT("la etapa nunca retrocede aunque el reloj vaya hacia atrás"), [this]()
		{
			TestEqual(TEXT("Preparación a las 08:00 del día 1"), FBeaconDirectorModel::Step(InStage(EBeaconStage::Preparacion), FactsAt(At(1, 8.0))).State.Progress.Stage, EBeaconStage::Preparacion);
		});
	});

	Describe(TEXT("hitos desde la mochila y el mundo"), [this]()
	{
		It(TEXT("cuenta hasta 2 tarros, la mecha, la hoguera y el pez asado"), [this]()
		{
			FBeaconFacts Facts = FactsAt(At(3, 9.0));
			Facts.OilJarsInPack = 1;
			const FBeaconStepResult One = FBeaconDirectorModel::Step(InStage(EBeaconStage::Preparacion), Facts);
			TestEqual(TEXT("Un tarro"), One.State.Progress.OilJars, 1);
			Facts.OilJarsInPack = 3;
			Facts.bHasWick = true;
			Facts.bFireLit = true;
			Facts.bHasCookedFish = true;
			FBeaconRunState Fresh = InStage(EBeaconStage::Preparacion);
			Fresh.Progress.bFirstFireLit = false;
			const FBeaconStepResult All = FBeaconDirectorModel::Step(Fresh, Facts);
			TestEqual(TEXT("Tope de 2 tarros"), All.State.Progress.OilJars, FBeaconProgressModel::RequiredOilJars);
			TestTrue(TEXT("Mecha"), All.State.Progress.bWickMade);
			TestTrue(TEXT("Hoguera"), All.State.Progress.bFirstFireLit);
			TestTrue(TEXT("Pensamiento de la hoguera"), All.Thoughts.Contains(EIriaThought::FirstFire));
		});
	});

	Describe(TEXT("hito de la balsa"), [this]()
	{
		It(TEXT("se registra cuando la balsa está atada y no antes"), [this]()
		{
			FBeaconRunState State = InStage(EBeaconStage::Balsa);
			State.Progress.bRaftBuilt = false;
			FBeaconFacts Facts = FactsAt(At(2, 9.0));
			const FBeaconStepResult Without = FBeaconDirectorModel::Step(State, Facts);
			TestFalse(TEXT("Sin balsa no hay hito"), Without.State.Progress.bRaftBuilt);
			Facts.bRaftBuilt = true;
			const FBeaconStepResult With = FBeaconDirectorModel::Step(State, Facts);
			TestTrue(TEXT("Con la balsa atada, hito"), With.State.Progress.bRaftBuilt);
			TestEqual(TEXT("El objetivo pasa a cruzar"), FBeaconProgressModel::CurrentObjective(With.State.Progress, FBeaconObjectiveContext()),
				FName(TEXT("obj_cruza_bajo")));
		});
	});

	Describe(TEXT("pensamientos"), [this]()
	{
		It(TEXT("se muestran una vez y con su umbral"), [this]()
		{
			FBeaconFacts Facts = FactsAt(At(1, 7.2));
			Facts.MovedMeters = 1.0f;
			Facts.MetersToTower = 120.0f;
			Facts.BarometerHpa = 1000.0f;
			const FBeaconStepResult Quiet = FBeaconDirectorModel::Step(FBeaconRunState(), Facts);
			TestEqual(TEXT("Nada por debajo de los umbrales"), Quiet.Thoughts.Num(), 0);
			Facts.MovedMeters = 2.0f;
			Facts.MetersToTower = 100.0f;
			Facts.MetersToHouse = 4.0f;
			Facts.MetersToFreighter = 125.0f;
			Facts.BarometerHpa = 999.0f;
			Facts.bCycloneEye = true;
			const FBeaconStepResult All = FBeaconDirectorModel::Step(FBeaconRunState(), Facts);
			for (const EIriaThought Thought : { EIriaThought::FirstMove, EIriaThought::SeeLighthouse, EIriaThought::EnterHouse,
				EIriaThought::SeeFreighter, EIriaThought::BarometerLow, EIriaThought::CycloneEye })
			{
				TestTrue(*FString::Printf(TEXT("Pensamiento %d"), static_cast<int32>(Thought)), All.Thoughts.Contains(Thought));
			}
			TestEqual(TEXT("La segunda vez, ninguno"), FBeaconDirectorModel::Step(All.State, Facts).Thoughts.Num(), 0);
		});

		It(TEXT("los externos (dormir, balsa) también son de una sola vez"), [this]()
		{
			const FBeaconStepResult First = FBeaconDirectorModel::TriggerThought(FBeaconRunState(), EIriaThought::FirstNight);
			TestTrue(TEXT("Primera vez"), First.bOk && First.Thoughts.Num() == 1);
			TestFalse(TEXT("Segunda vez"), FBeaconDirectorModel::TriggerThought(First.State, EIriaThought::FirstNight).bOk);
			TestFalse(TEXT("Fuera de rango"), FBeaconDirectorModel::TriggerThought(FBeaconRunState(), EIriaThought::Count).bOk);
		});

		It(TEXT("cada pensamiento tiene su id de texto"), [this]()
		{
			TSet<FName> Ids;
			for (int32 Index = 0; Index < static_cast<int32>(EIriaThought::Count); ++Index)
			{
				const FName Id = FBeaconDirectorModel::ThoughtTextId(static_cast<EIriaThought>(Index));
				TestFalse(TEXT("Con id"), Id.IsNone());
				Ids.Add(Id);
			}
			TestEqual(TEXT("12 distintos"), Ids.Num(), 12);
			TestTrue(TEXT("Count sin id"), FBeaconDirectorModel::ThoughtTextId(EIriaThought::Count).IsNone());
		});
	});

	Describe(TEXT("autoguardado"), [this]()
	{
		It(TEXT("al amanecer del día 2 y del 3 y el día 3 a las 20:00, una vez"), [this]()
		{
			TestTrue(TEXT("Amanecer día 2"), FBeaconDirectorModel::CrossesAutosave(At(2, 5.9), At(2, 6.0)));
			TestFalse(TEXT("Justo después no"), FBeaconDirectorModel::CrossesAutosave(At(2, 6.0), At(2, 6.1)));
			TestTrue(TEXT("Amanecer día 3"), FBeaconDirectorModel::CrossesAutosave(At(3, 5.0), At(3, 7.0)));
			TestTrue(TEXT("Día 3, 20:00"), FBeaconDirectorModel::CrossesAutosave(At(3, 19.9), At(3, 20.0)));
			TestFalse(TEXT("Día 1, 20:00 no"), FBeaconDirectorModel::CrossesAutosave(At(1, 19.9), At(1, 20.1)));
			TestFalse(TEXT("Hacia atrás no"), FBeaconDirectorModel::CrossesAutosave(At(2, 7.0), At(2, 5.0)));
		});

		It(TEXT("la primera muestra no guarda y las siguientes sí al cruzar"), [this]()
		{
			const FBeaconStepResult First = FBeaconDirectorModel::Step(InStage(EBeaconStage::Faro), FactsAt(At(2, 6.0)));
			TestFalse(TEXT("Sin hora anterior"), First.bAutosave);
			FBeaconRunState Before = InStage(EBeaconStage::Faro);
			Before.LastAbsHours = At(2, 5.95);
			TestTrue(TEXT("Cruza el amanecer"), FBeaconDirectorModel::Step(Before, FactsAt(At(2, 6.0))).bAutosave);
		});
	});

	Describe(TEXT("linterna, encendido y Mistral"), [this]()
	{
		It(TEXT("las ranuras exigen lo suyo y la tercera monta la linterna"), [this]()
		{
			FBeaconRunState Run = InStage(EBeaconStage::Preparacion);
			TestFalse(TEXT("Aceite sin tarros"), FBeaconDirectorModel::FillSlot(Run, ELanternSlot::Oil).bOk);
			TestFalse(TEXT("Encender sin montar"), FBeaconDirectorModel::Light(Run, At(3, 22.0)).bOk);
			Run.Progress.OilJars = 2;
			Run.Progress.bWickMade = true;
			Run = FBeaconDirectorModel::FillSlot(Run, ELanternSlot::Lens).State;
			Run = FBeaconDirectorModel::FillSlot(Run, ELanternSlot::Oil).State;
			const FBeaconStepResult Mounted = FBeaconDirectorModel::FillSlot(Run, ELanternSlot::Wick);
			TestTrue(TEXT("Tercera ranura"), Mounted.bOk);
			TestEqual(TEXT("Linterna montada"), Mounted.State.Progress.Stage, EBeaconStage::LinternaMontada);
			TestTrue(TEXT("Pensamiento de montar"), Mounted.Thoughts.Contains(EIriaThought::MountLantern));
			TestFalse(TEXT("Ranura repetida"), FBeaconDirectorModel::FillSlot(Mounted.State, ELanternSlot::Wick).bOk);
			const FBeaconStepResult Lit = FBeaconDirectorModel::Light(Mounted.State, At(3, 22.0));
			TestTrue(TEXT("Enciende"), Lit.bOk && Lit.State.Progress.bBeaconLit);
			TestEqual(TEXT("Final"), Lit.State.Progress.Stage, EBeaconStage::Final);
			TestEqual(TEXT("Hora del encendido"), Lit.State.LitAtHours, At(3, 22.0));
			TestTrue(TEXT("Pensamiento de encender"), Lit.Thoughts.Contains(EIriaThought::LightBeacon));
			TestFalse(TEXT("No se enciende dos veces"), FBeaconDirectorModel::Light(Lit.State, At(3, 22.5)).bOk);
		});

		It(TEXT("de noche el Mistral llega ya; de día, a las 21:00; una sola vez"), [this]()
		{
			TestEqual(TEXT("De noche, al momento"), FBeaconDirectorModel::MistralArrivalHours(At(3, 22.0)), At(3, 22.0));
			TestEqual(TEXT("De madrugada, al momento"), FBeaconDirectorModel::MistralArrivalHours(At(4, 3.0)), At(4, 3.0));
			TestEqual(TEXT("De día, a las 21:00"), FBeaconDirectorModel::MistralArrivalHours(At(3, 15.0)), At(3, 21.0));

			const FBeaconRunState Day = FBeaconDirectorModel::Light(InStage(EBeaconStage::LinternaMontada), At(3, 15.0)).State;
			TestFalse(TEXT("20:59 aún no"), FBeaconDirectorModel::Step(Day, FactsAt(At(3, 20.98))).bMistralDue);
			const FBeaconStepResult Arrives = FBeaconDirectorModel::Step(Day, FactsAt(At(3, 21.0)));
			TestTrue(TEXT("21:00 llega"), Arrives.bMistralDue);
			TestFalse(TEXT("Una sola vez"), FBeaconDirectorModel::Step(Arrives.State, FactsAt(At(3, 21.5))).bMistralDue);

			const FBeaconRunState Night = FBeaconDirectorModel::Light(InStage(EBeaconStage::LinternaMontada), At(3, 21.5)).State;
			TestTrue(TEXT("De noche, en la muestra siguiente"), FBeaconDirectorModel::Step(Night, FactsAt(At(3, 21.5))).bMistralDue);
			TestFalse(TEXT("Sin encender no llega"), FBeaconDirectorModel::Step(InStage(EBeaconStage::LinternaMontada), FactsAt(At(3, 23.0))).bMistralDue);
		});
	});

	Describe(TEXT("estados de depuración"), [this]()
	{
		It(TEXT("cada etapa tiene un estado coherente que pasa la validación del guardado"), [this]()
		{
			for (int32 Index = 0; Index < static_cast<int32>(EBeaconStage::Count); ++Index)
			{
				const EBeaconStage Stage = static_cast<EBeaconStage>(Index);
				const FBeaconRunState Run = FBeaconDirectorModel::StateForStage(Stage);
				const FBeaconProgressResult Back = FBeaconProgressModel::FromRecord(FBeaconProgressModel::ToRecord(Run.Progress));
				TestTrue(*FString::Printf(TEXT("%s es coherente"), FBeaconDirectorModel::StageName(Stage)), Back.bOk && Back.State == Run.Progress);
				TestEqual(TEXT("En su etapa"), Run.Progress.Stage, Stage);
			}
			TestTrue(TEXT("Linterna montada"), FBeaconProgressModel::IsLanternMounted(FBeaconDirectorModel::StateForStage(EBeaconStage::LinternaMontada).Progress));
			TestTrue(TEXT("Balsa con la página 1"), FNotebookModel::IsRead(FBeaconDirectorModel::StateForStage(EBeaconStage::Balsa).Notebook, 1));
		});

		It(TEXT("lee la etapa por nombre o por número"), [this]()
		{
			EBeaconStage Stage = EBeaconStage::Prologo;
			TestTrue(TEXT("Nombre"), FBeaconDirectorModel::ParseStage(TEXT("linternamontada"), Stage) && Stage == EBeaconStage::LinternaMontada);
			TestTrue(TEXT("Número"), FBeaconDirectorModel::ParseStage(TEXT(" 6 "), Stage) && Stage == EBeaconStage::Ciclon);
			TestFalse(TEXT("Fuera de rango"), FBeaconDirectorModel::ParseStage(TEXT("10"), Stage));
			TestFalse(TEXT("Desconocida"), FBeaconDirectorModel::ParseStage(TEXT("Zarpar"), Stage));
			TestFalse(TEXT("Vacía"), FBeaconDirectorModel::ParseStage(TEXT(""), Stage));
		});
	});
}

#endif
