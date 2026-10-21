#include "Misc/AutomationTest.h"

#include "Cooking/CampfireModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FCampfireSpec, "Explored.Campfire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FCampfireConfig Config;
END_DEFINE_SPEC(FCampfireSpec)

void FCampfireSpec::Define()
{
	BeforeEach([this]()
	{
		Config = FCampfireConfig::Default();
	});

	It(TEXT("encendida arde 5 min y cada leña suma 1,7 min (GDD §5.2)"), [this]()
	{
		const FCampfireState Lit = FCampfireModel::Ignite(FCampfireState(), Config);
		TestTrue(TEXT("Encendida"), Lit.bLit);
		TestEqual(TEXT("5 min"), Lit.BurnSeconds, 300.0f);
		FCampfireState Fed;
		TestTrue(TEXT("Admite leña"), FCampfireModel::AddFuel(Lit, Config, TEXT("lena"), Fed));
		TestEqual(TEXT("+102 s"), Fed.BurnSeconds, 402.0f);
		TestEqual(TEXT("La entrada no cambia"), Lit.BurnSeconds, 300.0f);
	});

	It(TEXT("apagada no admite leña y lo que no arde se rechaza"), [this]()
	{
		FCampfireState Out;
		TestFalse(TEXT("Apagada"), FCampfireModel::AddFuel(FCampfireState(), Config, TEXT("lena"), Out));
		const FCampfireState Lit = FCampfireModel::Ignite(FCampfireState(), Config);
		TestFalse(TEXT("Fibra"), FCampfireModel::AddFuel(Lit, Config, TEXT("fibra"), Out));
		TestEqual(TEXT("Sin cambios"), Out.BurnSeconds, 300.0f);
	});

	It(TEXT("no pasa del tope de combustible"), [this]()
	{
		FCampfireState S = FCampfireModel::Ignite(FCampfireState(), Config);
		for (int32 I = 0; I < 20; ++I)
		{
			FCampfireModel::AddFuel(S, Config, TEXT("lena"), S);
		}
		TestEqual(TEXT("Tope"), S.BurnSeconds, Config.MaxBurnSeconds);
	});

	It(TEXT("se apaga al acabarse el combustible"), [this]()
	{
		TArray<ECampfireEvent> Events;
		const FCampfireState S = FCampfireModel::Tick(FCampfireModel::Ignite(FCampfireState(), Config), 301.0f, false, Events);
		TestFalse(TEXT("Apagada"), S.bLit);
		TestTrue(TEXT("Evento"), Events.Contains(ECampfireEvent::BurnedOut));
	});

	It(TEXT("el ciclón la apaga al aire libre y no bajo techo"), [this]()
	{
		const FCampfireState Lit = FCampfireModel::Ignite(FCampfireState(), Config);
		TArray<ECampfireEvent> Events;
		TestFalse(TEXT("Expuesta"), FCampfireModel::Tick(Lit, 1.0f, true, Events).bLit);
		TestTrue(TEXT("Evento"), Events.Contains(ECampfireEvent::Quenched));
		Events.Reset();
		TestTrue(TEXT("Techada"), FCampfireModel::Tick(Lit, 1.0f, false, Events).bLit);
		TestEqual(TEXT("Sin eventos"), Events.Num(), 0);
	});

	It(TEXT("asa un pez en 20 s y deja el resultado listo para recoger"), [this]()
	{
		FCampfireState S = FCampfireModel::Ignite(FCampfireState(), Config);
		TestTrue(TEXT("Empieza"), FCampfireModel::StartJob(S, TEXT("pez_asado"), TEXT("pez_asado"), 1, 20.0f, S));
		FCampfireState Busy;
		TestFalse(TEXT("Un trabajo a la vez"), FCampfireModel::StartJob(S, TEXT("tarro_aceite"), TEXT("tarro_aceite"), 1, 40.0f, Busy));
		TArray<ECampfireEvent> Events;
		S = FCampfireModel::Tick(S, 19.0f, false, Events);
		FName Item;
		int32 Count = 0;
		FCampfireState Out;
		TestFalse(TEXT("A los 19 s no está"), FCampfireModel::CollectJob(S, Out, Item, Count));
		S = FCampfireModel::Tick(S, 1.0f, false, Events);
		TestTrue(TEXT("Evento"), Events.Contains(ECampfireEvent::JobDone));
		TestTrue(TEXT("Se recoge"), FCampfireModel::CollectJob(S, Out, Item, Count));
		TestEqual(TEXT("Pez asado"), Item, FName(TEXT("pez_asado")));
		TestEqual(TEXT("Uno"), Count, 1);
		TestFalse(TEXT("Sin trabajo"), Out.Job.IsActive());
	});

	It(TEXT("no cocina con el fuego apagado y el trabajo se pausa si se apaga"), [this]()
	{
		FCampfireState Out;
		TestFalse(TEXT("Apagada"), FCampfireModel::StartJob(FCampfireState(), TEXT("pez_asado"), TEXT("pez_asado"), 1, 20.0f, Out));
		FCampfireState S = FCampfireModel::Ignite(FCampfireState(), Config);
		S.BurnSeconds = 5.0f;
		FCampfireModel::StartJob(S, TEXT("pez_asado"), TEXT("pez_asado"), 1, 20.0f, S);
		TArray<ECampfireEvent> Events;
		S = FCampfireModel::Tick(S, 10.0f, false, Events);
		TestFalse(TEXT("Apagada"), S.bLit);
		TestEqual(TEXT("Solo avanzó lo que ardió"), S.Job.RemainingSeconds, 15.0f);
	});

	It(TEXT("calienta a 4 m o menos y solo encendida"), [this]()
	{
		const FCampfireState Lit = FCampfireModel::Ignite(FCampfireState(), Config);
		TestTrue(TEXT("A 4 m"), FCampfireModel::Warms(Lit, Config, 4.0f));
		TestFalse(TEXT("A 4,1 m"), FCampfireModel::Warms(Lit, Config, 4.1f));
		TestFalse(TEXT("Apagada"), FCampfireModel::Warms(FCampfireState(), Config, 1.0f));
		TestEqual(TEXT("Calor a 6 m"), FCampfireModel::HeatAt(Lit, Config, 6.0f), 0.5f);
	});
}

#endif
