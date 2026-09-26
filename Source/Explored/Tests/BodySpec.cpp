#include "Misc/AutomationTest.h"

#include "Survival/BodyModel.h"
#include "Survival/BodySignals.h"
#include "Survival/SurvivalModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BodyTest
{
	constexpr float Step = 0.1f;

	/** Mantiene cubiertas hambre, sed, sueño y temperatura para aislar lo que se prueba. */
	void KeepFed(FSurvivalState& S)
	{
		S.Hunger = FMath::Max(S.Hunger, 80.0f);
		S.Thirst = FMath::Max(S.Thirst, 80.0f);
		S.Rest = FMath::Max(S.Rest, 80.0f);
	}

	/** Simula en pasos de 6 minutos; con bFed repone las necesidades básicas en cada paso. */
	void Simulate(FSurvivalState& S, const FSurvivalInputs& In, float Hours, const FSurvivalModeSettings& Mode,
		TArray<ESurvivalEvent>& Events, bool bFed = true)
	{
		for (float T = 0.0f; T < Hours - 1.0e-4f; T += Step)
		{
			if (bFed)
			{
				KeepFed(S);
			}
			FSurvivalModel::Tick(S, In, Step, Mode, 0.99f, Events);
		}
	}

	FSurvivalModeSettings Survivor()
	{
		return FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor);
	}

	/** Entorno templado y a la sombra: ni frío, ni sol, ni lluvia. */
	FSurvivalInputs Mild()
	{
		FSurvivalInputs In;
		In.AirTemperature = 25.0f;
		In.Wind = 0.0f;
		return In;
	}

	/** Generador determinista para barrer estados al azar. */
	struct FLcg
	{
		uint32 Seed = 12345u;
		float Next()
		{
			Seed = Seed * 1664525u + 1013904223u;
			return static_cast<float>(Seed >> 8) / static_cast<float>(1u << 24);
		}
	};

	bool InRange01(float V)
	{
		return FMath::IsFinite(V) && V >= 0.0f && V <= 1.0f;
	}

	bool AllInRange(const FBodySignals& B)
	{
		return InRange01(B.Breathing) && InRange01(B.Heartbeat) && InRange01(B.Shivering) && InRange01(B.StomachGrowl)
			&& InRange01(B.Vignette) && InRange01(B.Blur) && InRange01(B.HandTremor) && InRange01(B.Desaturation)
			&& InRange01(B.BleedingPulse) && InRange01(B.Hallucination)
			&& InRange01(B.VignetteTint.R) && InRange01(B.VignetteTint.G) && InRange01(B.VignetteTint.B);
	}
}

BEGIN_DEFINE_SPEC(FBodySpec, "Explored.Body",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBodySpec)

void FBodySpec::Define()
{
	using namespace BodyTest;

	Describe("Escorbuto", [this]()
	{
		It("aparece por etapas tras días sin fruta: encías, visión y sangrado leve", [this]()
		{
			FSurvivalState S;
			S.Vitamins = 0.0f;
			TArray<ESurvivalEvent> Events;
			Simulate(S, Mild(), 24.0f, Survivor(), Events);
			TestEqual(TEXT("Un día sin vitamina C aún no se nota"), (int32)FBodyModel::ScurvyStage(S.ScurvySeverity), (int32)EScurvyStage::None);

			Simulate(S, Mild(), 24.0f * 3.0f, Survivor(), Events);
			TestEqual(TEXT("A los cuatro días duelen las encías"), (int32)FBodyModel::ScurvyStage(S.ScurvySeverity), (int32)EScurvyStage::Gums);
			TestTrue(TEXT("Evento de empeoramiento"), Events.Contains(ESurvivalEvent::ScurvyWorse));

			Simulate(S, Mild(), 24.0f * 2.0f, Survivor(), Events);
			TestEqual(TEXT("A los seis días falla la vista"), (int32)FBodyModel::ScurvyStage(S.ScurvySeverity), (int32)EScurvyStage::Vision);

			Simulate(S, Mild(), 24.0f * 3.0f, Survivor(), Events);
			TestEqual(TEXT("A los nueve días sangra"), (int32)FBodyModel::ScurvyStage(S.ScurvySeverity), (int32)EScurvyStage::Bleeding);
			TestTrue(TEXT("Sangrado leve visible"), FBodyModel::TotalBleeding(S) > 0.0f);
			TestTrue(TEXT("Leve: sigue vivo con salud"), !S.IsDead() && S.Health > 50.0f);
		});

		It("con la reserva inicial no aparece antes de una semana", [this]()
		{
			FSurvivalState S;
			TArray<ESurvivalEvent> Events;
			Simulate(S, Mild(), 24.0f * 7.0f, Survivor(), Events);
			TestEqual(TEXT("Sin escorbuto la primera semana"), (int32)FBodyModel::ScurvyStage(S.ScurvySeverity), (int32)EScurvyStage::None);
		});

		It("los limones lo curan y lo previenen", [this]()
		{
			FSurvivalState S;
			S.Vitamins = 0.0f;
			S.ScurvySeverity = 0.8f;
			FConsumable Lemon;
			Lemon.Vitamins = 50.0f;
			Lemon.Water = 5.0f;
			TArray<ESurvivalEvent> Events;
			for (int32 Day = 0; Day < 3; ++Day)
			{
				FSurvivalModel::Consume(S, Lemon, 0.5f, Events);
				Simulate(S, Mild(), 24.0f, Survivor(), Events);
			}
			TestEqual(TEXT("Curado en unos días"), S.ScurvySeverity, 0.0f);

			FSurvivalState Prevented;
			Prevented.Vitamins = 0.0f;
			for (int32 Day = 0; Day < 20; ++Day)
			{
				if (Day % 4 == 0)
				{
					FSurvivalModel::Consume(Prevented, Lemon, 0.5f, Events);
				}
				Simulate(Prevented, Mild(), 24.0f, Survivor(), Events);
			}
			TestEqual(TEXT("Un limón cada pocos días lo previene"), (int32)FBodyModel::ScurvyStage(Prevented.ScurvySeverity), (int32)EScurvyStage::None);
		});
	});

	Describe("Cortes e infección", [this]()
	{
		It("un corte sangra y la venda de tela lo para", [this]()
		{
			FSurvivalState Open;
			FBodyModel::AddCut(Open, 0.6f);
			FSurvivalState Bandaged = Open;
			TestEqual(TEXT("Vendada una herida"), FBodyModel::TreatWounds(Bandaged, EWoundTreatment::ClothBandage), 1);
			TestEqual(TEXT("La venda para la sangre"), FBodyModel::BleedingDamagePerHour(Bandaged), 0.0f);

			TArray<ESurvivalEvent> Events;
			Simulate(Open, Mild(), 1.0f, Survivor(), Events);
			Simulate(Bandaged, Mild(), 1.0f, Survivor(), Events);
			TestTrue(TEXT("Sin vendar pierde salud"), Open.Health < 95.0f);
			TestTrue(TEXT("Vendado no"), Bandaged.Health > Open.Health + 3.0f);
		});

		It("un corte muy profundo sigue sangrando con venda y la sutura lo cierra", [this]()
		{
			FSurvivalState S;
			FBodyModel::AddCut(S, 0.95f);
			FBodyModel::TreatWounds(S, EWoundTreatment::ClothBandage);
			TestTrue(TEXT("La venda no basta"), FBodyModel::BleedingDamagePerHour(S) > 0.0f);
			FBodyModel::TreatWounds(S, EWoundTreatment::Suture);
			TestEqual(TEXT("La sutura lo cierra"), FBodyModel::BleedingDamagePerHour(S), 0.0f);
		});

		It("se infecta si no se cura en unas horas y da fiebre", [this]()
		{
			FSurvivalState S;
			FBodyModel::AddCut(S, 0.2f);
			TArray<ESurvivalEvent> Events;
			Simulate(S, Mild(), 11.0f, Survivor(), Events);
			TestFalse(TEXT("Aún no infectada"), S.HasCondition(ECondition::Infection));
			Simulate(S, Mild(), 2.0f, Survivor(), Events);
			TestTrue(TEXT("Infectada"), S.HasCondition(ECondition::Infection));
			TestTrue(TEXT("Fiebre"), S.HasCondition(ECondition::Fever));
			TestTrue(TEXT("Evento"), Events.Contains(ESurvivalEvent::WoundInfected));

			FSurvivalState Treated;
			FBodyModel::AddCut(Treated, 0.2f);
			Simulate(Treated, Mild(), 4.0f, Survivor(), Events);
			FBodyModel::TreatWounds(Treated, EWoundTreatment::LeafBandage);
			Simulate(Treated, Mild(), 24.0f, Survivor(), Events);
			TestFalse(TEXT("Vendada a tiempo no se infecta"), Treated.HasCondition(ECondition::Infection));
		});

		It("vendado cicatriza y desaparece; las hojas medicinales van más rápido", [this]()
		{
			FSurvivalState Cloth;
			FBodyModel::AddCut(Cloth, 0.5f);
			FSurvivalState Leaves = Cloth;
			FBodyModel::TreatWounds(Cloth, EWoundTreatment::ClothBandage);
			FBodyModel::TreatWounds(Leaves, EWoundTreatment::LeafBandage);
			TArray<ESurvivalEvent> Events;
			Simulate(Cloth, Mild(), 40.0f, Survivor(), Events);
			Simulate(Leaves, Mild(), 40.0f, Survivor(), Events);
			TestEqual(TEXT("Con hojas ya ha cerrado"), Leaves.Wounds.Num(), 0);
			TestEqual(TEXT("Con tela aún no"), Cloth.Wounds.Num(), 1);
			Simulate(Cloth, Mild(), 20.0f, Survivor(), Events);
			TestEqual(TEXT("Con tela cierra después"), Cloth.Wounds.Num(), 0);
		});

		It("la pasta de cúrcuma cura la infección", [this]()
		{
			FSurvivalState S;
			FBodyModel::AddCut(S, 0.2f);
			TArray<ESurvivalEvent> Events;
			Simulate(S, Mild(), 13.0f, Survivor(), Events);
			TestTrue(TEXT("Infectada"), S.HasCondition(ECondition::Infection));
			FConsumable Turmeric;
			Turmeric.Cures = SurvivalCureBit(ECondition::Infection);
			FSurvivalModel::Consume(S, Turmeric, 0.5f, Events);
			FBodyModel::TreatWounds(S, EWoundTreatment::LeafBandage);
			Simulate(S, Mild(), 1.0f, Survivor(), Events);
			TestFalse(TEXT("Sin infección"), S.HasCondition(ECondition::Infection));
			TestFalse(TEXT("La herida ya no está infectada"), S.Wounds.Num() > 0 && S.Wounds[0].bInfected);
		});
	});

	Describe("Caídas", [this]()
	{
		It("no dañan por debajo de 3 m y el daño crece con la altura", [this]()
		{
			TestEqual(TEXT("Salto pequeño"), FBodyModel::FallDamage(2.5f, ELandingSurface::Ground).Damage, 0.0f);
			float Previous = 0.0f;
			for (float H = 3.0f; H <= 20.0f; H += 0.5f)
			{
				const float D = FBodyModel::FallDamage(H, ELandingSurface::Ground).Damage;
				TestTrue(FString::Printf(TEXT("Monótono a %.1f m"), H), D >= Previous);
				Previous = D;
			}
			TestTrue(TEXT("12 m sobre tierra matan"), FBodyModel::FallDamage(12.0f, ELandingSurface::Ground).Damage >= 100.0f);
			TestTrue(TEXT("6 m duelen pero no matan"), FBodyModel::FallDamage(6.0f, ELandingSurface::Ground).Damage < 50.0f);
		});

		It("la arena y el agua amortiguan; la roca castiga más", [this]()
		{
			const float Rock = FBodyModel::FallDamage(7.0f, ELandingSurface::Rock).Damage;
			const float Ground = FBodyModel::FallDamage(7.0f, ELandingSurface::Ground).Damage;
			const float Sand = FBodyModel::FallDamage(7.0f, ELandingSurface::Sand).Damage;
			TestTrue(TEXT("Roca > tierra > arena"), Rock > Ground && Ground > Sand);
			TestEqual(TEXT("Al agua desde 10 m, sin daño"), FBodyModel::FallDamage(10.0f, ELandingSurface::Water).Damage, 0.0f);
			TestEqual(TEXT("Altura no válida"), FBodyModel::FallDamage(-4.0f, ELandingSurface::Rock).Damage, 0.0f);
		});

		It("tuercen el tobillo desde unos 4.5 m y la férula lo cura", [this]()
		{
			FSurvivalState S;
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyFall(S, 4.0f, ELandingSurface::Ground, Survivor(), Events);
			TestFalse(TEXT("4 m: sin esguince"), S.HasCondition(ECondition::Sprain));
			FBodyModel::ApplyFall(S, 5.0f, ELandingSurface::Ground, Survivor(), Events);
			TestTrue(TEXT("5 m: esguince"), S.HasCondition(ECondition::Sprain));
			TestTrue(TEXT("Evento"), Events.Contains(ESurvivalEvent::Sprained));
			TestTrue(TEXT("Más torpe"), FBodyModel::Clumsiness(S) > 0.2f);

			FConsumable Splint;
			Splint.Cures = SurvivalCureBit(ECondition::Sprain);
			FSurvivalModel::Consume(S, Splint, 0.5f, Events);
			TestFalse(TEXT("Férula"), S.HasCondition(ECondition::Sprain));
		});

		It("en Explorador no matan", [this]()
		{
			FSurvivalState Explorer;
			FSurvivalState Survivor;
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyFall(Explorer, 30.0f, ELandingSurface::Rock, FSurvivalModeSettings::FromMode(ESurvivalMode::Explorer), Events);
			FBodyModel::ApplyFall(Survivor, 30.0f, ELandingSurface::Rock, FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor), Events);
			TestFalse(TEXT("Explorador vive"), Explorer.IsDead());
			TestTrue(TEXT("Superviviente muere"), Survivor.IsDead());
			TestTrue(TEXT("Evento de muerte"), Events.Contains(ESurvivalEvent::Died));
		});
	});

	Describe("Picaduras e intoxicaciones", [this]()
	{
		It("la medusa escuece y el vinagre la cura", [this]()
		{
			FSurvivalState S;
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplySting(S, EStingKind::Jellyfish, Events);
			TestTrue(TEXT("Picado"), S.HasCondition(ECondition::JellyfishSting));
			TestTrue(TEXT("Evento"), Events.Contains(ESurvivalEvent::Stung));
			TestTrue(TEXT("Duele"), FBodyModel::Pain(S) > 0.3f);
			FSurvivalModel::Consume(S, FBodyModel::Antidote(EStingKind::Jellyfish), 0.5f, Events);
			TestFalse(TEXT("Vinagre"), S.HasCondition(ECondition::JellyfishSting));
		});

		It("la raya duele más, deja herida y el antídoto de corteza la cura", [this]()
		{
			FSurvivalState Ray;
			FSurvivalState Jelly;
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplySting(Ray, EStingKind::Ray, Events);
			FBodyModel::ApplySting(Jelly, EStingKind::Jellyfish, Events);
			TestTrue(TEXT("Más dolor que la medusa"), FBodyModel::Pain(Ray) > FBodyModel::Pain(Jelly));
			TestEqual(TEXT("Herida punzante"), Ray.Wounds.Num(), 1);

			FSurvivalState Untreated = Ray;
			FSurvivalModel::Consume(Ray, FBodyModel::Antidote(EStingKind::Ray), 0.5f, Events);
			FBodyModel::TreatWounds(Ray, EWoundTreatment::ClothBandage);
			FBodyModel::TreatWounds(Untreated, EWoundTreatment::ClothBandage);
			TestFalse(TEXT("Antídoto"), Ray.HasCondition(ECondition::RaySting));
			Simulate(Ray, Mild(), 6.0f, Survivor(), Events);
			Simulate(Untreated, Mild(), 6.0f, Survivor(), Events);
			TestTrue(TEXT("Sin antídoto pierde más salud"), Untreated.Health < Ray.Health - 5.0f);
		});

		It("cada fuente tiene su riesgo: agua sin hervir leve, charca y yuca cruda graves, mar deshidrata", [this]()
		{
			FConsumable Water;
			Water.Water = 20.0f;
			const FConsumable River = FBodyModel::WithHazard(Water, EFoodHazard::UnboiledWater);
			const FConsumable Swamp = FBodyModel::WithHazard(Water, EFoodHazard::SwampWater);
			const FConsumable Sea = FBodyModel::WithHazard(Water, EFoodHazard::SeaWater);
			TestTrue(TEXT("Río: algo de riesgo"), River.Toxicity > 0.0f && River.Toxicity < 0.5f);
			TestTrue(TEXT("Charca: mucho más"), Swamp.Toxicity > River.Toxicity);
			TestTrue(TEXT("El mar quita sed"), Sea.Water < 0.0f);
			TestEqual(TEXT("Hervida: sin riesgo"), FBodyModel::WithHazard(Water, EFoodHazard::None).Toxicity, 0.0f);

			FConsumable Food;
			Food.Food = 20.0f;
			TestTrue(TEXT("Yuca cruda"), FBodyModel::WithHazard(Food, EFoodHazard::RawCassava).Toxicity >= 0.5f);
			TestTrue(TEXT("Seta tóxica"), FBodyModel::WithHazard(Food, EFoodHazard::ToxicMushroom).Toxicity >= 0.5f);

			FSurvivalState S;
			TArray<ESurvivalEvent> Events;
			FSurvivalModel::Consume(S, FBodyModel::WithHazard(Food, EFoodHazard::RawCassava), 0.1f, Events);
			TestTrue(TEXT("Intoxicado con mala tirada"), S.HasCondition(ECondition::Poisoned));
		});

		It("la seta alucinógena alucina unas horas", [this]()
		{
			FSurvivalState S;
			FConsumable Mushroom;
			Mushroom.Food = 5.0f;
			TArray<ESurvivalEvent> Events;
			FSurvivalModel::Consume(S, FBodyModel::WithHazard(Mushroom, EFoodHazard::HallucinogenicMushroom), 0.99f, Events);
			TestEqual(TEXT("Alucinación plena"), FBodyModel::HallucinationIntensity(S), 1.0f);
			Simulate(S, Mild(), 5.0f, Survivor(), Events);
			TestEqual(TEXT("Se pasa"), FBodyModel::HallucinationIntensity(S), 0.0f);
		});

		It("el sol sin sombrero quema en un par de horas; sombrero y sombra lo evitan", [this]()
		{
			FSurvivalInputs Sun = Mild();
			Sun.SunExposure = 1.0f;
			Sun.AirTemperature = 30.0f;
			FSurvivalState Bare;
			FSurvivalState Hat;
			FSurvivalState Shade;
			TArray<ESurvivalEvent> Events;
			Simulate(Bare, Sun, 3.0f, Survivor(), Events);
			FSurvivalInputs WithHat = Sun;
			WithHat.bHasHat = true;
			Simulate(Hat, WithHat, 3.0f, Survivor(), Events);
			FSurvivalInputs InShade = Sun;
			InShade.SunExposure = 0.15f;
			Simulate(Shade, InShade, 8.0f, Survivor(), Events);
			TestTrue(TEXT("Sin sombrero se quema"), Bare.HasCondition(ECondition::SunBurn));
			TestTrue(TEXT("Evento"), Events.Contains(ESurvivalEvent::SunBurned));
			TestFalse(TEXT("Con sombrero no"), Hat.HasCondition(ECondition::SunBurn));
			TestFalse(TEXT("A la sombra no"), Shade.HasCondition(ECondition::SunBurn));
		});
	});

	Describe("Nutrición", [this]()
	{
		It("comer solo cocos días seguidos tiene consecuencias leves", [this]()
		{
			FConsumable Coconut;
			Coconut.Food = 15.0f;
			Coconut.Water = 20.0f;
			Coconut.Carbs = 12.0f;
			Coconut.Protein = 1.0f;
			Coconut.Vitamins = 1.0f;
			FConsumable Fish;
			Fish.Food = 20.0f;
			Fish.Protein = 12.0f;
			FConsumable Fruit;
			Fruit.Food = 8.0f;
			Fruit.Water = 5.0f;
			Fruit.Carbs = 8.0f;
			Fruit.Vitamins = 6.0f;

			FSurvivalState Coconuts;
			FSurvivalState Varied;
			TArray<ESurvivalEvent> Events;
			for (int32 Meal = 0; Meal < 30; ++Meal) // cinco días, una comida cada cuatro horas
			{
				FSurvivalModel::Consume(Coconuts, Coconut, 0.5f, Events);
				FSurvivalModel::Consume(Varied, Meal % 2 == 0 ? Fish : Fruit, 0.5f, Events);
				FSurvivalModel::Consume(Varied, Coconut, 0.5f, Events);
				Coconuts.Rest = Varied.Rest = 90.0f; // duerme bien: solo cambia la dieta
				Simulate(Coconuts, Mild(), 4.0f, Survivor(), Events, false);
				Simulate(Varied, Mild(), 4.0f, Survivor(), Events, false);
			}
			TestTrue(TEXT("Dieta desequilibrada"), Coconuts.DietBalance() < 0.5f);
			TestTrue(TEXT("Dieta variada equilibrada"), Varied.DietBalance() > Coconuts.DietBalance());
			TestTrue(TEXT("Se nota la monotonía"), FBodyModel::MonotonyFactor(Coconuts) > 0.5f);
			TestTrue(TEXT("Menos energía máxima que con dieta variada"), Coconuts.MaxEnergy() < Varied.MaxEnergy());
			TestTrue(TEXT("Pero leve"), Coconuts.MaxEnergy() > 70.0f);
			TestTrue(TEXT("Sin daño a la salud"), Coconuts.Health > 95.0f);
			TestTrue(TEXT("Menos ánimo"), Coconuts.Morale < Varied.Morale);
		});

		It("el hambre no cuenta como monotonía", [this]()
		{
			FSurvivalState S;
			S.Hunger = 10.0f;
			S.Protein = 0.0f;
			S.Carbs = 90.0f;
			TArray<ESurvivalEvent> Events;
			Simulate(S, Mild(), 10.0f, Survivor(), Events, false);
			TestEqual(TEXT("Sin monotonía con el estómago vacío"), S.MonotonyHours, 0.0f);
		});
	});

	Describe("Sueño y ánimo", [this]()
	{
		It("la falta de sueño da torpeza y alucinaciones leves", [this]()
		{
			FSurvivalState Rested;
			FSurvivalState Tired;
			Tired.Rest = 0.0f;
			TestEqual(TEXT("Descansado: nada"), FBodyModel::Clumsiness(Rested), 0.0f);
			TestEqual(TEXT("Descansado: sin alucinar"), FBodyModel::HallucinationIntensity(Rested), 0.0f);
			TestTrue(TEXT("Torpe"), FBodyModel::Clumsiness(Tired) > 0.5f);
			const float H = FBodyModel::HallucinationIntensity(Tired);
			TestTrue(TEXT("Alucinaciones leves"), H > 0.0f && H <= 0.35f);
			TestTrue(TEXT("Trabaja peor"), Tired.WorkEfficiency() < Rested.WorkEfficiency());
		});

		It("fuego, descubrimientos, música y mapa lo suben; soledad, heridas y tormentas lo bajan", [this]()
		{
			FSurvivalState S;
			S.Morale = 40.0f;
			FBodyModel::ApplyMoraleEvent(S, EMoraleEvent::Discovery);
			FBodyModel::ApplyMoraleEvent(S, EMoraleEvent::MapProgress);
			TestTrue(TEXT("Sube"), S.Morale > 50.0f);

			FSurvivalInputs Music = Mild();
			Music.bPlayingMusic = true;
			FSurvivalInputs Storm = Mild();
			Storm.StormIntensity = 1.0f;
			Storm.Rain = 1.0f;
			FSurvivalState WithMusic;
			FSurvivalState InStorm;
			FSurvivalState Wounded;
			FBodyModel::AddCut(Wounded, 0.5f);
			FBodyModel::TreatWounds(Wounded, EWoundTreatment::ClothBandage);
			FSurvivalState Alone;
			TArray<ESurvivalEvent> Events;
			Simulate(WithMusic, Music, 3.0f, Survivor(), Events);
			Simulate(InStorm, Storm, 3.0f, Survivor(), Events);
			Simulate(Wounded, Mild(), 3.0f, Survivor(), Events);
			Simulate(Alone, Mild(), 3.0f, Survivor(), Events);
			TestTrue(TEXT("La música sube"), WithMusic.Morale > Alone.Morale);
			TestTrue(TEXT("La soledad pesa"), Alone.Morale < 60.0f);
			TestTrue(TEXT("La tormenta baja más"), InStorm.Morale < Alone.Morale);
			TestTrue(TEXT("Las heridas bajan"), Wounded.Morale < Alone.Morale);
		});

		It("el ánimo nunca mata", [this]()
		{
			FSurvivalState S;
			S.Morale = 0.0f;
			FSurvivalInputs Grim = Mild();
			Grim.StormIntensity = 1.0f;
			Grim.bSheltered = true;
			TArray<ESurvivalEvent> Events;
			for (int32 Day = 0; Day < 30; ++Day)
			{
				S.Protein = S.Carbs = S.Vitamins = 60.0f;
				FBodyModel::ApplyMoraleEvent(S, EMoraleEvent::StormHit);
				Simulate(S, Grim, 24.0f, Survivor(), Events);
			}
			TestEqual(TEXT("Ánimo por los suelos"), S.Morale, 0.0f);
			TestEqual(TEXT("Salud intacta"), S.Health, 100.0f);
			TestFalse(TEXT("Vivo"), S.IsDead());
		});
	});

	Describe("Modos", [this]()
	{
		It("escalan la velocidad de las necesidades y el Personalizado la multiplica", [this]()
		{
			auto ThirstAfter = [](const FSurvivalModeSettings& Mode)
			{
				FSurvivalState S;
				S.Thirst = 100.0f;
				TArray<ESurvivalEvent> Events;
				Simulate(S, Mild(), 6.0f, Mode, Events, false);
				return 100.0f - S.Thirst;
			};
			const float Explorer = ThirstAfter(FSurvivalModeSettings::FromMode(ESurvivalMode::Explorer));
			const float Survivor = ThirstAfter(FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor));
			const float Castaway = ThirstAfter(FSurvivalModeSettings::FromMode(ESurvivalMode::Castaway));
			const float Fast = ThirstAfter(FSurvivalModeSettings::MakeCustom(2.0f, true));
			const float Slow = ThirstAfter(FSurvivalModeSettings::MakeCustom(0.5f, true));
			TestTrue(TEXT("Explorador < Superviviente < Náufrago"), Explorer < Survivor && Survivor < Castaway);
			TestEqual(TEXT("Personalizado ×2"), Fast, Survivor * 2.0f, 0.5f);
			TestEqual(TEXT("Personalizado ×0.5"), Slow, Survivor * 0.5f, 0.5f);
			TestEqual(TEXT("El multiplicador se limita a [0.25, 3]"), FSurvivalModeSettings::MakeCustom(10.0f, true).NeedScale(), 3.0f);
		});

		It("el Personalizado puede impedir que las necesidades maten; solo el Náufrago no reaparece", [this]()
		{
			FSurvivalState Gentle;
			FSurvivalState Harsh;
			TArray<ESurvivalEvent> Events;
			Simulate(Gentle, Mild(), 200.0f, FSurvivalModeSettings::MakeCustom(1.0f, false), Events, false);
			Simulate(Harsh, Mild(), 200.0f, FSurvivalModeSettings::MakeCustom(1.0f, true), Events, false);
			TestFalse(TEXT("Sin muerte por necesidades"), Gentle.IsDead());
			TestTrue(TEXT("Con muerte por necesidades"), Harsh.IsDead());
			TestTrue(TEXT("Náufrago: permadeath"), FSurvivalModeSettings::FromMode(ESurvivalMode::Castaway).HasPermadeath());
			TestFalse(TEXT("Superviviente reaparece"), FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor).HasPermadeath());
		});

		It("la firma con ESurvivalMode equivale a FromMode", [this]()
		{
			FSurvivalState A;
			FSurvivalState B;
			TArray<ESurvivalEvent> Events;
			for (int32 I = 0; I < 50; ++I)
			{
				FSurvivalModel::Tick(A, Mild(), 0.5f, ESurvivalMode::Castaway, 0.99f, Events);
				FSurvivalModel::Tick(B, Mild(), 0.5f, FSurvivalModeSettings::FromMode(ESurvivalMode::Castaway), 0.99f, Events);
			}
			TestEqual(TEXT("Misma sed"), A.Thirst, B.Thirst);
			TestEqual(TEXT("Misma salud"), A.Health, B.Health);
		});
	});

	Describe("Señales del cuerpo", [this]()
	{
		It("todas las señales quedan en [0, 1] para cualquier estado", [this]()
		{
			FLcg Rng;
			for (int32 I = 0; I < 500; ++I)
			{
				FSurvivalState S;
				S.Health = Rng.Next() * 100.0f;
				S.Hunger = Rng.Next() * 100.0f;
				S.Thirst = Rng.Next() * 100.0f;
				S.Rest = Rng.Next() * 100.0f;
				S.BodyTemperature = 30.0f + Rng.Next() * 13.0f;
				S.ScurvySeverity = Rng.Next();
				S.MonotonyHours = Rng.Next() * 100.0f;
				for (int32 C = 0; C < static_cast<int32>(ECondition::Count); ++C)
				{
					S.ConditionTime[C] = Rng.Next() < 0.3f ? 5.0f : 0.0f;
				}
				const int32 Cuts = static_cast<int32>(Rng.Next() * 4.0f);
				for (int32 W = 0; W < Cuts; ++W)
				{
					FBodyModel::AddCut(S, Rng.Next());
				}
				FBodySignalContext Ctx;
				Ctx.EnergyRatio = Rng.Next();
				Ctx.Oxygen01 = Rng.Next();
				Ctx.Exertion = Rng.Next() * 1.5f;
				const FBodySignals B = FBodySignalsModel::Evaluate(S, Ctx);
				if (!TestTrue(FString::Printf(TEXT("Rango en la muestra %d"), I), AllInRange(B)))
				{
					break;
				}
			}
		});

		It("un cuerpo sano y descansado apenas manda señales", [this]()
		{
			const FBodySignals B = FBodySignalsModel::Evaluate(FSurvivalState(), FBodySignalContext());
			TestEqual(TEXT("Sin viñeta"), B.Vignette, 0.0f);
			TestEqual(TEXT("Sin causa"), (int32)B.TintCause, (int32)EBodyTintCause::None);
			TestEqual(TEXT("Sin borrosidad"), B.Blur, 0.0f);
			TestEqual(TEXT("Sin temblor"), B.Shivering, 0.0f);
			TestTrue(TEXT("Latido discreto"), B.Heartbeat < 0.2f);
		});

		It("cada señal crece con su causa (monotonía)", [this]()
		{
			const FBodySignalContext Ctx;
			float Prev[6] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
			for (int32 I = 0; I <= 40; ++I)
			{
				const float T = I / 40.0f;
				FSurvivalState Thirsty;
				Thirsty.Thirst = 100.0f * (1.0f - T);
				FSurvivalState Cold;
				Cold.BodyTemperature = 37.0f - 5.0f * T;
				FSurvivalState Hungry;
				Hungry.Hunger = 100.0f * (1.0f - T);
				FSurvivalState Scurvy;
				Scurvy.ScurvySeverity = T;
				FSurvivalState Bleeding;
				FBodyModel::AddCut(Bleeding, T);
				FSurvivalState Hurt;
				Hurt.Health = 100.0f * (1.0f - T);

				const float Now[6] = {
					FBodySignalsModel::Evaluate(Thirsty, Ctx).Blur,
					FBodySignalsModel::Evaluate(Cold, Ctx).Shivering,
					FBodySignalsModel::Evaluate(Hungry, Ctx).StomachGrowl,
					FBodySignalsModel::Evaluate(Scurvy, Ctx).Desaturation,
					FBodySignalsModel::Evaluate(Bleeding, Ctx).BleedingPulse,
					FBodySignalsModel::Evaluate(Hurt, Ctx).Heartbeat,
				};
				for (int32 K = 0; K < 6; ++K)
				{
					TestTrue(FString::Printf(TEXT("Señal %d monótona en %.2f"), K, T), Now[K] >= Prev[K]);
					Prev[K] = Now[K];
				}
			}
			TestTrue(TEXT("Sed extrema emborrona"), Prev[0] > 0.5f);
			TestTrue(TEXT("Hipotermia tiembla"), Prev[1] > 0.5f);
			TestTrue(TEXT("Hambre suena"), Prev[2] > 0.5f);
			TestTrue(TEXT("Escorbuto destiñe"), Prev[3] > 0.5f);
			TestTrue(TEXT("Corte profundo late en rojo"), Prev[4] > 0.5f);
		});

		It("el esfuerzo y la apnea aceleran respiración y latido", [this]()
		{
			const FSurvivalState S;
			FBodySignalContext Calm;
			FBodySignalContext Running;
			Running.Exertion = 1.0f;
			FBodySignalContext Diving;
			Diving.Oxygen01 = 0.1f;
			const FBodySignals A = FBodySignalsModel::Evaluate(S, Calm);
			const FBodySignals B = FBodySignalsModel::Evaluate(S, Running);
			const FBodySignals C = FBodySignalsModel::Evaluate(S, Diving);
			TestTrue(TEXT("Correr: más respiración"), B.Breathing > A.Breathing + 0.4f);
			TestTrue(TEXT("Correr: más latido"), B.Heartbeat > A.Heartbeat);
			TestTrue(TEXT("Apnea: más latido"), C.Heartbeat > A.Heartbeat);
			TestTrue(TEXT("Pulsaciones en rango"), FBodySignalsModel::HeartRateBpm(B.Heartbeat) > 80.0f
				&& FBodySignalsModel::HeartRateBpm(1.0f) <= 160.0f);
		});

		It("la causa más intensa decide el tinte del borde", [this]()
		{
			FSurvivalState Cold;
			Cold.BodyTemperature = 34.0f;
			FSurvivalState Bleeding;
			FBodyModel::AddCut(Bleeding, 0.9f);
			FSurvivalState Poisoned;
			Poisoned.AddCondition(ECondition::Poisoned, 5.0f);
			TestEqual(TEXT("Frío: azul"), (int32)FBodySignalsModel::Evaluate(Cold, FBodySignalContext()).TintCause, (int32)EBodyTintCause::Cold);
			TestEqual(TEXT("Sangre: rojo"), (int32)FBodySignalsModel::Evaluate(Bleeding, FBodySignalContext()).TintCause, (int32)EBodyTintCause::Bleeding);
			TestEqual(TEXT("Intoxicación: verde"), (int32)FBodySignalsModel::Evaluate(Poisoned, FBodySignalContext()).TintCause, (int32)EBodyTintCause::Poison);
			const FBodySignals B = FBodySignalsModel::Evaluate(Bleeding, FBodySignalContext());
			TestTrue(TEXT("El tinte es el de la causa"), B.VignetteTint.Equals(FBodySignalsModel::TintFor(EBodyTintCause::Bleeding)));
			TestTrue(TEXT("La viñeta nunca tapa del todo"), B.Vignette <= 0.75f);
		});

		It("el suavizado se acerca sin pasarse y no se mueve con dt = 0", [this]()
		{
			FBodySignals Target;
			Target.Heartbeat = 1.0f;
			Target.Desaturation = 1.0f;
			Target.VignetteTint = FLinearColor(1.0f, 0.0f, 0.0f);
			FBodySignals Current;
			const FBodySignals Frozen = FBodySignalsModel::Smooth(Current, Target, 0.0f);
			TestEqual(TEXT("dt = 0 no cambia"), Frozen.Heartbeat, 0.0f);
			float Previous = 0.0f;
			for (int32 Frame = 0; Frame < 600; ++Frame)
			{
				Current = FBodySignalsModel::Smooth(Current, Target, 1.0f / 60.0f);
				TestTrue(TEXT("Crece sin pasarse"), Current.Heartbeat >= Previous && Current.Heartbeat <= 1.0f);
				Previous = Current.Heartbeat;
			}
			TestTrue(TEXT("El latido llega en segundos"), Current.Heartbeat > 0.99f);
			TestTrue(TEXT("El color va más despacio que el latido"), Current.Desaturation < Current.Heartbeat);
			TestTrue(TEXT("El tinte se acerca"), Current.VignetteTint.R > 0.9f);
			const FBodySignals Huge = FBodySignalsModel::Smooth(FBodySignals(), Target, 1.0e6f);
			TestEqual(TEXT("Un dt enorme llega justo al objetivo"), Huge.Heartbeat, 1.0f);
		});
	});

	Describe("Reloj de pulsera", [this]()
	{
		It("da la hora con minutos y envuelve el día", [this]()
		{
			const FSurvivalState S;
			const FWristWatchReadout A = FBodySignalsModel::WristWatch(S, 13.5f, false);
			TestEqual(TEXT("13 h"), A.Hour, 13);
			TestEqual(TEXT("30 min"), A.Minute, 30);
			const FWristWatchReadout B = FBodySignalsModel::WristWatch(S, 24.25f, false);
			TestEqual(TEXT("00:15"), B.Hour * 100 + B.Minute, 15);
			const FWristWatchReadout C = FBodySignalsModel::WristWatch(S, -1.0f, false);
			TestEqual(TEXT("23:00"), C.Hour * 100 + C.Minute, 2300);
			const FWristWatchReadout D = FBodySignalsModel::WristWatch(S, 23.9999f, false);
			TestTrue(TEXT("Nunca 24:00"), D.Hour <= 23 && D.Minute <= 59);
		});

		It("los indicadores son gruesos y solo si se piden", [this]()
		{
			FSurvivalState S;
			S.Thirst = 5.0f;
			S.Hunger = 45.0f;
			S.Rest = 20.0f;
			S.BodyTemperature = 34.0f;
			const FWristWatchReadout Hidden = FBodySignalsModel::WristWatch(S, 8.0f, false);
			TestFalse(TEXT("Oculto por defecto"), Hidden.bShowNeeds);
			const FWristWatchReadout R = FBodySignalsModel::WristWatch(S, 8.0f, true);
			TestTrue(TEXT("Visible si se pide"), R.bShowNeeds);
			TestEqual(TEXT("Sed crítica"), (int32)R.Thirst, (int32)ENeedLevel::Critical);
			TestEqual(TEXT("Hambre regular"), (int32)R.Hunger, (int32)ENeedLevel::Fair);
			TestEqual(TEXT("Sueño bajo"), (int32)R.Rest, (int32)ENeedLevel::Low);
			TestEqual(TEXT("Frío crítico"), (int32)R.Warmth, (int32)ENeedLevel::Critical);
		});
	});
}

#endif
