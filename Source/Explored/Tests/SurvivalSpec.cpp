#include "Misc/AutomationTest.h"

#include "Survival/SurvivalModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SurvivalTest
{
	/** Simula en pasos de 6 minutos de juego. */
	void Simulate(FSurvivalState& S, const FSurvivalInputs& In, float Hours, ESurvivalMode Mode, TArray<ESurvivalEvent>& Events)
	{
		constexpr float Step = 0.1f;
		for (float T = 0.0f; T < Hours; T += Step)
		{
			FSurvivalModel::Tick(S, In, Step, Mode, 0.99f, Events);
		}
	}
}

BEGIN_DEFINE_SPEC(FSurvivalSpec, "Explored.Survival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSurvivalSpec)

void FSurvivalSpec::Define()
{
	using namespace SurvivalTest;

	It("vacía la sed antes que el hambre", [this]()
	{
		FSurvivalState S;
		S.Hunger = 100.0f;
		S.Thirst = 100.0f;
		TArray<ESurvivalEvent> Events;
		Simulate(S, FSurvivalInputs(), 12.0f, ESurvivalMode::Survivor, Events);
		TestTrue(TEXT("La sed baja más rápido"), S.Thirst < S.Hunger);
		TestTrue(TEXT("Sigue vivo tras 12 horas"), !S.IsDead());
	});

	It("mata por deshidratación en modo Superviviente pero no en Explorador", [this]()
	{
		TArray<ESurvivalEvent> Events;
		FSurvivalState Survivor;
		Simulate(Survivor, FSurvivalInputs(), 60.0f, ESurvivalMode::Survivor, Events);
		TestTrue(TEXT("Superviviente muere"), Survivor.IsDead());
		TestTrue(TEXT("Evento de deshidratación"), Events.Contains(ESurvivalEvent::Dehydrated));

		FSurvivalState Explorer;
		Events.Reset();
		Simulate(Explorer, FSurvivalInputs(), 200.0f, ESurvivalMode::Explorer, Events);
		TestFalse(TEXT("Explorador no muere"), Explorer.IsDead());
	});

	It("en el último paso de un estado solo daña el tiempo que le quedaba (L4)", [this]()
	{
		TArray<ESurvivalEvent> Events;
		FSurvivalState Clean;
		FSurvivalState Poisoned;
		Poisoned.AddCondition(ECondition::Poisoned, 0.5f);
		FSurvivalModel::Tick(Clean, FSurvivalInputs(), 2.0f, ESurvivalMode::Survivor, 0.99f, Events);
		FSurvivalModel::Tick(Poisoned, FSurvivalInputs(), 2.0f, ESurvivalMode::Survivor, 0.99f, Events);
		// 3 puntos por hora durante media hora, no durante las dos del paso.
		TestEqual(TEXT("Daño de media hora"), Clean.Health - Poisoned.Health, 1.5f, 0.05f);
		TestFalse(TEXT("Se le pasa"), Poisoned.HasCondition(ECondition::Poisoned));
	});

	It("enfría el cuerpo de noche, mojado y con viento, y el fuego lo recupera", [this]()
	{
		FSurvivalState S;
		FSurvivalInputs Cold;
		Cold.AirTemperature = 21.0f;
		Cold.Wind = 0.8f;
		Cold.Rain = 1.0f;
		TArray<ESurvivalEvent> Events;
		Simulate(S, Cold, 4.0f, ESurvivalMode::Survivor, Events);
		TestTrue(TEXT("Hipotermia"), S.BodyTemperature < 35.0f);
		TestTrue(TEXT("Mojado"), S.Wetness > 0.9f);

		FSurvivalInputs Fire;
		Fire.AirTemperature = 21.0f;
		Fire.FireHeat = 1.0f;
		Fire.bSheltered = true;
		Simulate(S, Fire, 3.0f, ESurvivalMode::Survivor, Events);
		TestTrue(TEXT("Recupera temperatura"), S.BodyTemperature > 36.5f);
		TestTrue(TEXT("Se seca"), S.Wetness < 0.2f);
	});

	It("recupera el sueño durmiendo y más rápido bajo techo", [this]()
	{
		FSurvivalState Outside;
		Outside.Rest = 10.0f;
		FSurvivalState Inside = Outside;
		FSurvivalInputs Sleep;
		Sleep.Activity = EActivity::Sleeping;
		TArray<ESurvivalEvent> Events;
		Simulate(Outside, Sleep, 3.0f, ESurvivalMode::Survivor, Events);
		Sleep.bSheltered = true;
		Simulate(Inside, Sleep, 3.0f, ESurvivalMode::Survivor, Events);
		TestTrue(TEXT("Duerme"), Outside.Rest > 40.0f);
		TestTrue(TEXT("Mejor bajo techo"), Inside.Rest > Outside.Rest);
	});

	It("intoxica según la toxicidad y el antídoto lo cura", [this]()
	{
		FSurvivalState S;
		TArray<ESurvivalEvent> Events;
		FConsumable RawCassava;
		RawCassava.Food = 20.0f;
		RawCassava.Toxicity = 0.8f;
		FSurvivalModel::Consume(S, RawCassava, 0.5f, Events);
		TestTrue(TEXT("Intoxicado"), S.HasCondition(ECondition::Poisoned));
		TestTrue(TEXT("Evento"), Events.Contains(ESurvivalEvent::GotPoisoned));

		FConsumable Charcoal;
		Charcoal.Cures = 1u << static_cast<uint32>(ECondition::Poisoned);
		FSurvivalModel::Consume(S, Charcoal, 0.5f, Events);
		TestFalse(TEXT("Curado"), S.HasCondition(ECondition::Poisoned));

		FSurvivalState Lucky;
		FSurvivalModel::Consume(Lucky, RawCassava, 0.95f, Events);
		TestFalse(TEXT("Sin intoxicación con tirada alta"), Lucky.HasCondition(ECondition::Poisoned));
	});

	It("reduce la energía máxima con hambre y mala dieta", [this]()
	{
		FSurvivalState Fed;
		FSurvivalState Starving;
		Starving.Hunger = 5.0f;
		Starving.Protein = 0.0f;
		Starving.Carbs = 0.0f;
		Starving.Vitamins = 0.0f;
		TestTrue(TEXT("Menos energía"), Starving.MaxEnergy() < Fed.MaxEnergy() - 30.0f);
		TestTrue(TEXT("Nunca por debajo de 20"), Starving.MaxEnergy() >= 20.0f);
	});

	It("gasta energía al correr con peso y la recupera al descansar", [this]()
	{
		TestTrue(TEXT("Correr gasta"), FSurvivalModel::EnergyDrainPerSecond(EActivity::Sprinting, 0.0f) > 0.0f);
		TestTrue(TEXT("El peso cuesta más"),
			FSurvivalModel::EnergyDrainPerSecond(EActivity::Sprinting, 1.5f) > FSurvivalModel::EnergyDrainPerSecond(EActivity::Sprinting, 0.0f));
		TestTrue(TEXT("Descansar recupera"), FSurvivalModel::EnergyDrainPerSecond(EActivity::Resting, 0.0f) < 0.0f);
	});

	It("sube el ánimo junto al fuego con compañía", [this]()
	{
		FSurvivalState S;
		S.Morale = 30.0f;
		FSurvivalInputs Camp;
		Camp.FireHeat = 1.0f;
		Camp.bCompanionNearby = true;
		Camp.bSheltered = true;
		TArray<ESurvivalEvent> Events;
		Simulate(S, Camp, 4.0f, ESurvivalMode::Survivor, Events);
		TestTrue(TEXT("Más ánimo"), S.Morale > 45.0f);
	});

	It("aguanta la apnea base unos 40 s y se recupera en un puñado de segundos al respirar", [this]()
	{
		const float Drain = FSurvivalModel::OxygenDrainPerSecond(0.0f, 1.0f, false);
		TestTrue(TEXT("Se gasta"), Drain > 0.0f);
		const float HoldSeconds = 100.0f / Drain;
		TestTrue(TEXT("Aguanta al menos media apnea razonable"), HoldSeconds > 25.0f && HoldSeconds < 55.0f);

		const float Recovery = FSurvivalModel::OxygenRecoveryPerSecond(1.0f);
		TestTrue(TEXT("Se recupera bastante más rápido de lo que se gasta"), Recovery > Drain * 5.0f);
	});

	It("el peso y el esfuerzo aceleran el consumo de oxígeno; el pulmón mejorado lo frena", [this]()
	{
		const float Base = FSurvivalModel::OxygenDrainPerSecond(0.0f, 1.0f, false);
		const float Loaded = FSurvivalModel::OxygenDrainPerSecond(1.5f, 1.0f, false);
		const float Exerting = FSurvivalModel::OxygenDrainPerSecond(0.0f, 1.0f, true);
		const float BetterLungs = FSurvivalModel::OxygenDrainPerSecond(0.0f, 1.6f, false);

		TestTrue(TEXT("Cargado gasta más"), Loaded > Base);
		TestTrue(TEXT("Esforzándose gasta más"), Exerting > Base);
		TestTrue(TEXT("Mejor pulmón gasta menos por segundo"), BetterLungs < Base);
	});
}

#endif
