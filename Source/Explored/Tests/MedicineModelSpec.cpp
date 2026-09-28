#include "Misc/AutomationTest.h"

#include "Core/SystemLinks.h"
#include "Survival/BodyModel.h"
#include "Survival/MedicineModel.h"
#include "Survival/SurvivalModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MedicineModelTest
{
	constexpr float Step = 0.1f;

	/** Mantiene cubiertas hambre, sed y sueño para aislar lo que se prueba. */
	void KeepFed(FSurvivalState& S)
	{
		S.Hunger = FMath::Max(S.Hunger, 80.0f);
		S.Thirst = FMath::Max(S.Thirst, 80.0f);
		S.Rest = FMath::Max(S.Rest, 80.0f);
	}

	/** Cuerpo sano y seco: la partida nueva empieza empapada y aquí estorbaría. */
	FSurvivalState Dry()
	{
		FSurvivalState S;
		S.Wetness = 0.0f;
		return S;
	}

	/** Templado, a la sombra, sin viento ni lluvia. */
	FSurvivalInputs Mild()
	{
		FSurvivalInputs In;
		In.AirTemperature = 25.0f;
		In.Wind = 0.0f;
		return In;
	}

	FSurvivalModeSettings Survivor()
	{
		return FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor);
	}

	void Simulate(FSurvivalState& S, float Hours, TArray<ESurvivalEvent>& Events,
		const FSurvivalModeSettings& Mode = Survivor(), const FSurvivalInputs& In = Mild())
	{
		// Pasos enteros: sumar 0.1 en float se desvía y cambiaría el número de pasos.
		const int32 Steps = FMath::RoundToInt(Hours / Step);
		for (int32 I = 0; I < Steps; ++I)
		{
			KeepFed(S);
			FSurvivalModel::Tick(S, In, Step, Mode, 0.99f, Events);
		}
	}

	float Hours(const FSurvivalState& S, ECondition C)
	{
		return S.ConditionTime[static_cast<int32>(C)];
	}

	bool Apply(FSurvivalState& S, const TCHAR* ItemId)
	{
		TArray<ESurvivalEvent> Events;
		return FMedicineModel::Apply(S, FName(ItemId), Events);
	}

	/** Un cuerpo con todos los estados activos a la vez, 10 horas cada uno. */
	FSurvivalState AllConditions()
	{
		FSurvivalState S = Dry();
		for (int32 C = 0; C < static_cast<int32>(ECondition::Count); ++C)
		{
			S.AddCondition(static_cast<ECondition>(C), 10.0f);
		}
		return S;
	}

	/** Todos los números finitos y en rango: lo mínimo tras cualquier combinación. */
	bool Sane(const FSurvivalState& S)
	{
		auto In = [](float V, float Lo, float Hi) { return FMath::IsFinite(V) && V >= Lo && V <= Hi; };
		bool bOk = In(S.Health, 0.0f, 100.0f) && In(S.Hunger, 0.0f, 100.0f) && In(S.Thirst, 0.0f, 100.0f)
			&& In(S.Morale, 0.0f, 100.0f) && In(S.Wetness, 0.0f, 1.0f) && In(S.BodyTemperature, 25.0f, 45.0f);
		for (const float T : S.ConditionTime)
		{
			bOk = bOk && In(T, 0.0f, 1000.0f);
		}
		for (const FWound& W : S.Wounds)
		{
			bOk = bOk && In(W.Depth, 0.0f, 1.0f) && In(W.Bleeding, 0.0f, 1.0f) && In(W.Healed, 0.0f, 1.0f);
		}
		return bOk;
	}
}

BEGIN_DEFINE_SPEC(FMedicineModelSpec, "Explored.Medicine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FMedicineModelSpec)

void FMedicineModelSpec::Define()
{
	using namespace MedicineModelTest;

	Describe("Partida nueva", [this]()
	{
		It("empieza empapada (biblia 01 §4) y se seca al sol en pocas horas", [this]()
		{
			FSurvivalState S;
			TestEqual(TEXT("Wetness inicial"), S.Wetness, 1.0f);
			FSurvivalInputs Sun = Mild();
			Sun.SunExposure = 1.0f;
			TArray<ESurvivalEvent> Events;
			Simulate(S, 1.0f, Events, Survivor(), Sun);
			TestEqual(TEXT("Una hora al sol basta"), S.Wetness, 0.0f);
			TestEqual(TEXT("Reaparecer no deja empapado"), ExploredLinks::MakeRespawnState(FSurvivalState()).Wetness, 0.0f);
		});

		It("el agua deja empapado otra vez, al instante", [this]()
		{
			FSurvivalState S = Dry();
			FSurvivalInputs Water = Mild();
			Water.bInWater = true;
			TArray<ESurvivalEvent> Events;
			FSurvivalModel::Tick(S, Water, Step, Survivor(), 0.99f, Events);
			TestEqual(TEXT("Empapado"), S.Wetness, 1.0f);
		});
	});

	Describe("La tabla de medicinas", [this]()
	{
		It("tiene las seis de la porción vertical, sin repetir", [this]()
		{
			const TCHAR* Expected[] = { TEXT("vendaje_tela"), TEXT("antidoto_corteza"), TEXT("carbon_activado"),
				TEXT("te_corteza_sauce"), TEXT("ferula_bambu"), TEXT("gel_aloe") };
			for (const TCHAR* Id : Expected)
			{
				TestNotNull(*FString::Printf(TEXT("Existe %s"), Id), FMedicineModel::Find(FName(Id)));
			}
			TArray<FName> Seen;
			for (const FMedicineDef& M : FMedicineModel::All())
			{
				TestFalse(TEXT("Sin repetir"), Seen.Contains(M.ItemId));
				Seen.Add(M.ItemId);
				TestTrue(TEXT("Algo hace"), M.Effects.Cures != 0 || M.bTreatsWounds || M.Effects.Healing > 0.0f);
				TestEqual(TEXT("Ninguna es tóxica"), M.Effects.Toxicity, 0.0f);
			}
			TestEqual(TEXT("Seis"), FMedicineModel::All().Num(), 6);
		});

		It("el antídoto de corteza es el mismo remedio que FBodyModel::Antidote(Ray)", [this]()
		{
			const FMedicineDef* Bark = FMedicineModel::Find(FName(TEXT("antidoto_corteza")));
			const FConsumable Remedy = FBodyModel::Antidote(EStingKind::Ray);
			TestTrue(TEXT("Existe"), Bark != nullptr);
			if (Bark)
			{
				TestEqual(TEXT("Mismas curas"), Bark->Effects.Cures, Remedy.Cures);
				TestEqual(TEXT("Misma salud"), Bark->Effects.Healing, Remedy.Healing);
			}
		});

		It("con un objeto que no es medicina no hace nada", [this]()
		{
			FSurvivalState S = AllConditions();
			const FSurvivalState Before = S;
			TestFalse(TEXT("Coco"), Apply(S, TEXT("coco_maduro")));
			TestFalse(TEXT("Id vacío"), FMedicineModel::Find(NAME_None) != nullptr);
			TestFalse(TEXT("Un id parecido no vale"), Apply(S, TEXT("gel_aloe_x")));
			for (int32 C = 0; C < static_cast<int32>(ECondition::Count); ++C)
			{
				TestEqual(TEXT("Estados intactos"), S.ConditionTime[C], Before.ConditionTime[C]);
			}
		});

		It("no cura a un muerto", [this]()
		{
			FSurvivalState S = Dry();
			S.AddCondition(ECondition::Poisoned, 5.0f);
			S.Health = 0.0f;
			TestFalse(TEXT("Rechazada"), Apply(S, TEXT("carbon_activado")));
			TestTrue(TEXT("Sigue intoxicado"), S.HasCondition(ECondition::Poisoned));
			TestEqual(TEXT("Sigue muerto"), S.Health, 0.0f);
		});

		It("cada medicina cura exactamente lo que dicen los datos y nada más", [this]()
		{
			for (const FMedicineDef& M : FMedicineModel::All())
			{
				FSurvivalState S = AllConditions();
				FBodyModel::AddCut(S, 0.5f);
				TArray<ESurvivalEvent> Events;
				FBodyModel::ApplyContactBurn(S, Survivor(), Events);
				const FSurvivalState Before = S;
				TestTrue(TEXT("Se aplica"), FMedicineModel::Apply(S, M.ItemId, Events));
				for (int32 C = 0; C < static_cast<int32>(ECondition::Count); ++C)
				{
					const ECondition Cond = static_cast<ECondition>(C);
					const FString What = FString::Printf(TEXT("%s / estado %d"), *M.ItemId.ToString(), C);
					if (!M.Cures(Cond))
					{
						TestEqual(*(What + TEXT(" intacto")), S.ConditionTime[C], Before.ConditionTime[C]);
					}
					else if (Cond == ECondition::ContactBurn)
					{
						// La quemadura no se borra: cicatriza a mitad de tiempo.
						TestTrue(*(What + TEXT(" sigue, más corta")), S.HasCondition(Cond) && S.ConditionTime[C] < Before.ConditionTime[C]);
					}
					else
					{
						TestFalse(*(What + TEXT(" curado")), S.HasCondition(Cond));
					}
				}
				TestEqual(TEXT("Salud según los datos"), S.Health, FMath::Min(100.0f, Before.Health + M.Effects.Healing));
			}
		});
	});

	Describe("Quemadura de contacto", [this]()
	{
		It("quita 8 de salud de golpe y abre una herida de 0.4 que no sangra", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			const float Lost = FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			TestEqual(TEXT("Daño"), Lost, 8.0f);
			TestEqual(TEXT("Salud"), S.Health, 92.0f);
			TestEqual(TEXT("Una herida"), S.Wounds.Num(), 1);
			TestTrue(TEXT("Es quemadura"), S.Wounds.Num() == 1 && S.Wounds[0].bBurn);
			TestEqual(TEXT("Profundidad"), S.Wounds.Num() == 1 ? S.Wounds[0].Depth : -1.0f, 0.4f);
			TestEqual(TEXT("No sangra"), FBodyModel::TotalBleeding(S), 0.0f);
			TestEqual(TEXT("Ni resta salud por sangrado"), FBodyModel::BleedingDamagePerHour(S), 0.0f);
			TestTrue(TEXT("Estado ContactBurn"), S.HasCondition(ECondition::ContactBurn));
			TestEqual(TEXT("Dura lo que tarda en cicatrizar"), Hours(S, ECondition::ContactBurn), 24.0f);
			TestFalse(TEXT("No es quemadura solar"), S.HasCondition(ECondition::SunBurn));
			TestTrue(TEXT("Evento Burned"), Events.Contains(ESurvivalEvent::Burned));
			TestEqual(TEXT("Golpe de ánimo de herido"), S.Morale, 60.0f + FBodyModel::MoraleEventDelta(EMoraleEvent::Injured));
			TestTrue(TEXT("Duele"), FBodyModel::Pain(S) > 0.0f);
		});

		It("no se infecta aunque nadie la cure, y cicatriza sola en 24 h", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			Events.Reset();
			Simulate(S, 12.0f, Events);
			TestEqual(TEXT("A las 12 h le quedan 12"), Hours(S, ECondition::ContactBurn), 12.0f, 0.05f);
			Simulate(S, 11.5f, Events);
			TestTrue(TEXT("A las 23.5 h sigue"), S.HasCondition(ECondition::ContactBurn) && FBodyModel::NumBurns(S) == 1);
			Simulate(S, 1.0f, Events);
			TestFalse(TEXT("A las 24.5 h ya no"), S.HasCondition(ECondition::ContactBurn));
			TestEqual(TEXT("Sin herida"), FBodyModel::NumBurns(S), 0);
			TestFalse(TEXT("Nunca infectada"), Events.Contains(ESurvivalEvent::WoundInfected));
			TestFalse(TEXT("Sin infección"), S.HasCondition(ECondition::Infection));
			TestFalse(TEXT("Sin fiebre"), S.HasCondition(ECondition::Fever));
			TestTrue(TEXT("La salud se recupera: la quemadura no resta por hora"), S.Health > 92.0f);
		});

		It("con aloe nada más quemarse cicatriza en 12 h", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			TestTrue(TEXT("Aloe"), Apply(S, TEXT("gel_aloe")));
			TestEqual(TEXT("Quedan 12 h"), Hours(S, ECondition::ContactBurn), 12.0f);
			TestTrue(TEXT("Aliviada"), S.Wounds.Num() == 1 && S.Wounds[0].bMedicinal);
			Simulate(S, 11.5f, Events);
			TestTrue(TEXT("A las 11.5 h sigue"), S.HasCondition(ECondition::ContactBurn));
			Simulate(S, 1.0f, Events);
			TestFalse(TEXT("A las 12.5 h ya no"), S.HasCondition(ECondition::ContactBurn));
			TestEqual(TEXT("Sin herida"), S.Wounds.Num(), 0);
		});

		It("con aloe a mitad solo acelera lo que queda (6 h + 9 h)", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			Simulate(S, 6.0f, Events);
			Apply(S, TEXT("gel_aloe"));
			TestEqual(TEXT("Quedan 9 h"), Hours(S, ECondition::ContactBurn), 9.0f, 0.05f);
			Simulate(S, 8.5f, Events);
			TestTrue(TEXT("A las 14.5 h sigue"), S.HasCondition(ECondition::ContactBurn));
			Simulate(S, 1.0f, Events);
			TestFalse(TEXT("A las 15.5 h ya no"), S.HasCondition(ECondition::ContactBurn));
		});

		It("el aloe no se acumula y alivia el dolor", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			const float PainRaw = FBodyModel::Pain(S);
			TestEqual(TEXT("Alivia una"), FBodyModel::SootheBurns(S), 1);
			const float Left = Hours(S, ECondition::ContactBurn);
			TestEqual(TEXT("La segunda vez no hay nada nuevo que aliviar"), FBodyModel::SootheBurns(S), 0);
			TestEqual(TEXT("Mismo tiempo"), Hours(S, ECondition::ContactBurn), Left);
			TestTrue(TEXT("Duele menos"), FBodyModel::Pain(S) < PainRaw);
		});

		It("sin herida detrás, el aloe quita el estado directamente", [this]()
		{
			FSurvivalState S = Dry();
			S.AddCondition(ECondition::ContactBurn, 20.0f);
			TestTrue(TEXT("Aloe"), Apply(S, TEXT("gel_aloe")));
			TestFalse(TEXT("Curada"), S.HasCondition(ECondition::ContactBurn));
		});

		It("sin herida detrás, el estado se consume con el tiempo como los demás", [this]()
		{
			FSurvivalState S = Dry();
			S.AddCondition(ECondition::ContactBurn, 2.0f);
			TArray<ESurvivalEvent> Events;
			Simulate(S, 1.0f, Events);
			TestEqual(TEXT("Le queda 1 h"), Hours(S, ECondition::ContactBurn), 1.0f, 0.01f);
			Simulate(S, 1.5f, Events);
			TestFalse(TEXT("Pasada"), S.HasCondition(ECondition::ContactBurn));
		});

		It("ni el agua ni las vendas la tratan", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			TestEqual(TEXT("Agua: nada"), FBodyModel::TreatWounds(S, EWoundTreatment::CleanWater), 0);
			TestEqual(TEXT("Hojas: nada"), FBodyModel::TreatWounds(S, EWoundTreatment::LeafBandage), 0);
			TestTrue(TEXT("Venda de tela: se usa igual"), Apply(S, TEXT("vendaje_tela")));
			TestFalse(TEXT("Sigue sin aloe"), S.Wounds[0].bMedicinal);
			TestEqual(TEXT("Sigue a 24 h"), Hours(S, ECondition::ContactBurn), 24.0f);
		});

		It("cada contacto abre su quemadura y el estado dura lo de la peor", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			Simulate(S, 10.0f, Events);
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			TestEqual(TEXT("Dos quemaduras"), FBodyModel::NumBurns(S), 2);
			TestEqual(TEXT("Manda la nueva"), Hours(S, ECondition::ContactBurn), 24.0f);
			Simulate(S, 14.5f, Events);
			TestEqual(TEXT("La primera ya cerró"), FBodyModel::NumBurns(S), 1);
			TestTrue(TEXT("La segunda sigue"), S.HasCondition(ECondition::ContactBurn));
			Simulate(S, 10.0f, Events);
			TestFalse(TEXT("Las dos cerradas"), S.HasCondition(ECondition::ContactBurn));
		});

		It("el escorbuto pleno la cicatriza a mitad de velocidad", [this]()
		{
			FSurvivalState S = Dry();
			S.Vitamins = 0.0f;
			S.ScurvySeverity = 1.0f;
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			Simulate(S, 30.0f, Events);
			TestEqual(TEXT("A las 30 h sigue abierta"), FBodyModel::NumBurns(S), 1);
			TestTrue(TEXT("Y el estado activo"), S.HasCondition(ECondition::ContactBurn));
		});

		It("en Explorador no mata; en Náufrago sí; a un muerto no le pasa nada", [this]()
		{
			TArray<ESurvivalEvent> Events;
			FSurvivalState Explorer = Dry();
			Explorer.Health = 12.0f;
			FBodyModel::ApplyContactBurn(Explorer, FSurvivalModeSettings::FromMode(ESurvivalMode::Explorer), Events);
			TestEqual(TEXT("Explorador: suelo de 10"), Explorer.Health, 10.0f);
			FSurvivalState Custom = Dry();
			Custom.Health = 5.0f;
			FBodyModel::ApplyContactBurn(Custom, FSurvivalModeSettings::MakeCustom(1.0f, false), Events);
			TestEqual(TEXT("Personalizado sin morir: no baja"), Custom.Health, 5.0f);

			FSurvivalState Castaway = Dry();
			Castaway.Health = 5.0f;
			Events.Reset();
			FBodyModel::ApplyContactBurn(Castaway, FSurvivalModeSettings::FromMode(ESurvivalMode::Castaway), Events);
			TestTrue(TEXT("Náufrago: muere"), Castaway.IsDead() && Events.Contains(ESurvivalEvent::Died));

			const int32 WoundsBefore = Castaway.Wounds.Num();
			Events.Reset();
			TestEqual(TEXT("Muerto: sin daño"), FBodyModel::ApplyContactBurn(Castaway, Survivor(), Events), 0.0f);
			TestEqual(TEXT("Ni herida nueva"), Castaway.Wounds.Num(), WoundsBefore);
			TestEqual(TEXT("Ni sucesos"), Events.Num(), 0);
		});

		It("reaparecer la limpia, como el resto de heridas", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			const FSurvivalState Respawned = ExploredLinks::MakeRespawnState(S);
			TestEqual(TEXT("Sin quemaduras"), FBodyModel::NumBurns(Respawned), 0);
			TestFalse(TEXT("Sin estado"), Respawned.HasCondition(ECondition::ContactBurn));
		});

		It("su bit de cura no pisa a los demás", [this]()
		{
			TestTrue(TEXT("Cabe en Cures"), static_cast<int32>(ECondition::Count) <= 32);
			for (int32 C = 0; C < static_cast<int32>(ECondition::Count); ++C)
			{
				const ECondition Cond = static_cast<ECondition>(C);
				if (Cond != ECondition::ContactBurn)
				{
					TestEqual(TEXT("Bits distintos"), SurvivalCureBit(Cond) & SurvivalCureBit(ECondition::ContactBurn), 0u);
				}
			}
			FSurvivalState S = Dry();
			S.AddCondition(ECondition::SunBurn, 10.0f);
			FConsumable OnlyContact;
			OnlyContact.Cures = SurvivalCureBit(ECondition::ContactBurn);
			TArray<ESurvivalEvent> Events;
			FSurvivalModel::Consume(S, OnlyContact, 1.0f, Events);
			TestTrue(TEXT("La solar sigue"), S.HasCondition(ECondition::SunBurn));
		});
	});

	Describe("Cada estado, su efecto y su cura", [this]()
	{
		It("Bleeding: 6 de salud por hora mientras dura; se cura con lo que lleve su bit", [this]()
		{
			FSurvivalState S = Dry();
			S.AddCondition(ECondition::Bleeding, 2.0f);
			TArray<ESurvivalEvent> Events;
			Simulate(S, 1.0f, Events);
			TestEqual(TEXT("-6 en una hora"), S.Health, 94.0f, 0.01f);
			FConsumable Stop;
			Stop.Cures = SurvivalCureBit(ECondition::Bleeding);
			FSurvivalModel::Consume(S, Stop, 1.0f, Events);
			TestFalse(TEXT("Curado"), S.HasCondition(ECondition::Bleeding));
		});

		It("Poisoned: salud, sed y energía máxima; el carbón activado lo corta", [this]()
		{
			FSurvivalState S = Dry();
			FSurvivalState Clean = Dry();
			S.AddCondition(ECondition::Poisoned, 5.0f);
			TArray<ESurvivalEvent> Events;
			FSurvivalModel::Tick(S, Mild(), 1.0f, Survivor(), 0.99f, Events);
			FSurvivalModel::Tick(Clean, Mild(), 1.0f, Survivor(), 0.99f, Events);
			TestEqual(TEXT("-3 de salud"), S.Health, 97.0f, 0.01f);
			TestEqual(TEXT("-4 de sed de más"), Clean.Thirst - S.Thirst, 4.0f, 0.01f);
			TestEqual(TEXT("Energía máxima -20"), Clean.MaxEnergy() - S.MaxEnergy(), 20.0f, 0.01f);
			TestTrue(TEXT("Carbón activado"), Apply(S, TEXT("carbon_activado")));
			TestFalse(TEXT("Curado"), S.HasCondition(ECondition::Poisoned));
		});

		It("Fever: sube la temperatura objetivo 1.6 °C; el té de sauce la baja", [this]()
		{
			FSurvivalState S = Dry();
			S.AddCondition(ECondition::Fever, 10.0f);
			TArray<ESurvivalEvent> Events;
			Simulate(S, 4.0f, Events);
			TestEqual(TEXT("37.2 + 1.6"), S.BodyTemperature, 38.8f, 0.05f);
			TestTrue(TEXT("Té de corteza de sauce"), Apply(S, TEXT("te_corteza_sauce")));
			TestFalse(TEXT("Curada"), S.HasCondition(ECondition::Fever));
			Simulate(S, 4.0f, Events);
			TestEqual(TEXT("Vuelve a 37.2"), S.BodyTemperature, 37.2f, 0.05f);
		});

		It("SunBurn: -1 de ánimo por hora y algo de dolor; el aloe la quita", [this]()
		{
			FSurvivalState S = Dry();
			FSurvivalState Clean = Dry();
			S.AddCondition(ECondition::SunBurn, 10.0f);
			TArray<ESurvivalEvent> Events;
			FSurvivalModel::Tick(S, Mild(), 1.0f, Survivor(), 0.99f, Events);
			FSurvivalModel::Tick(Clean, Mild(), 1.0f, Survivor(), 0.99f, Events);
			// 1 del estado + 2 × 0.2 de dolor.
			TestEqual(TEXT("-1.4 de ánimo"), Clean.Morale - S.Morale, 1.4f, 0.01f);
			TestTrue(TEXT("Aloe"), Apply(S, TEXT("gel_aloe")));
			TestFalse(TEXT("Curada"), S.HasCondition(ECondition::SunBurn));
		});

		It("Sprain: trabaja peor y más torpe; la férula lo cura", [this]()
		{
			FSurvivalState S = Dry();
			const float Work = S.WorkEfficiency();
			const float Clumsy = FBodyModel::Clumsiness(S);
			S.AddCondition(ECondition::Sprain, 20.0f);
			TestEqual(TEXT("-0.15 de eficiencia"), Work - S.WorkEfficiency(), 0.15f, 0.001f);
			TestEqual(TEXT("+0.25 de torpeza"), FBodyModel::Clumsiness(S) - Clumsy, 0.25f, 0.001f);
			TestTrue(TEXT("Férula"), Apply(S, TEXT("ferula_bambu")));
			TestFalse(TEXT("Curado"), S.HasCondition(ECondition::Sprain));
			TestEqual(TEXT("Eficiencia de vuelta"), S.WorkEfficiency(), Work);
		});

		It("Infection: 1.5 de salud por hora; ninguna medicina de la porción la cura aún", [this]()
		{
			FSurvivalState S = Dry();
			S.AddCondition(ECondition::Infection, 5.0f);
			TArray<ESurvivalEvent> Events;
			Simulate(S, 1.0f, Events);
			TestEqual(TEXT("-1.5"), S.Health, 98.5f, 0.01f);
			// La pasta de cúrcuma (biblia 01 §6.14) no está en la porción vertical.
			for (const FMedicineDef& M : FMedicineModel::All())
			{
				TestFalse(*FString::Printf(TEXT("%s no cura la infección"), *M.ItemId.ToString()), M.Cures(ECondition::Infection));
			}
		});

		It("JellyfishSting: 1 de salud por hora durante 6 h; el vinagre la quita", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplySting(S, EStingKind::Jellyfish, Events);
			TestEqual(TEXT("6 h"), Hours(S, ECondition::JellyfishSting), 6.0f);
			Simulate(S, 1.0f, Events);
			TestEqual(TEXT("-1"), S.Health, 99.0f, 0.01f);
			FSurvivalModel::Consume(S, FBodyModel::Antidote(EStingKind::Jellyfish), 1.0f, Events);
			TestFalse(TEXT("Curada"), S.HasCondition(ECondition::JellyfishSting));
		});

		It("RaySting: 3 por hora más el corte; el antídoto cura la picadura pero no el corte", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplySting(S, EStingKind::Ray, Events);
			TestEqual(TEXT("12 h"), Hours(S, ECondition::RaySting), 12.0f);
			TestEqual(TEXT("Un corte punzante"), S.Wounds.Num(), 1);
			const float HealthBefore = S.Health;
			TestTrue(TEXT("Antídoto de corteza"), Apply(S, TEXT("antidoto_corteza")));
			TestFalse(TEXT("Picadura curada"), S.HasCondition(ECondition::RaySting));
			TestEqual(TEXT("+5 de salud"), S.Health, FMath::Min(100.0f, HealthBefore + 5.0f));
			TestEqual(TEXT("El corte sigue"), S.Wounds.Num(), 1);
			TestTrue(TEXT("Y sangra"), FBodyModel::TotalBleeding(S) > 0.0f);
			TestTrue(TEXT("La venda de tela lo para"), Apply(S, TEXT("vendaje_tela")));
			TestEqual(TEXT("Ya no sangra"), FBodyModel::TotalBleeding(S), 0.0f);
		});

		It("Hallucinating: visión plena 4 h con la seta y luego se pasa", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FSurvivalModel::Consume(S, FBodyModel::WithHazard(FConsumable(), EFoodHazard::HallucinogenicMushroom), 0.99f, Events);
			TestEqual(TEXT("Plena"), FBodyModel::HallucinationIntensity(S), 1.0f);
			Simulate(S, 4.1f, Events);
			TestEqual(TEXT("Pasada"), FBodyModel::HallucinationIntensity(S), 0.0f);
		});
	});

	Describe("Combinaciones", [this]()
	{
		It("fiebre e intoxicación comparten el mismo -20 de energía máxima", [this]()
		{
			FSurvivalState Both = Dry();
			Both.AddCondition(ECondition::Fever, 5.0f);
			Both.AddCondition(ECondition::Poisoned, 5.0f);
			FSurvivalState OnlyFever = Dry();
			OnlyFever.AddCondition(ECondition::Fever, 5.0f);
			TestEqual(TEXT("No se acumulan"), Both.MaxEnergy(), OnlyFever.MaxEnergy());
			TestEqual(TEXT("80"), Both.MaxEnergy(), 80.0f);
		});

		It("el té baja la fiebre de una herida infectada, pero la infección sigue haciendo daño", [this]()
		{
			FSurvivalState S = Dry();
			FBodyModel::AddCut(S, 0.3f);
			TArray<ESurvivalEvent> Events;
			Simulate(S, 13.0f, Events);
			TestTrue(TEXT("Infectada"), S.HasCondition(ECondition::Infection) && S.HasCondition(ECondition::Fever));
			Apply(S, TEXT("te_corteza_sauce"));
			TestFalse(TEXT("Sin fiebre"), S.HasCondition(ECondition::Fever));
			TestTrue(TEXT("Con infección"), S.HasCondition(ECondition::Infection));
			const float Health = S.Health;
			Simulate(S, 1.0f, Events);
			TestTrue(TEXT("Sigue restando"), S.Health < Health);
		});

		It("picadura de raya y quemadura: cada remedio a lo suyo y el dolor no pasa de 1", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplySting(S, EStingKind::Ray, Events);
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			TestTrue(TEXT("Dolor acotado"), FBodyModel::Pain(S) <= 1.0f);
			Apply(S, TEXT("gel_aloe"));
			TestTrue(TEXT("El aloe no toca la raya"), S.HasCondition(ECondition::RaySting));
			Apply(S, TEXT("antidoto_corteza"));
			TestFalse(TEXT("El antídoto sí"), S.HasCondition(ECondition::RaySting));
			TestTrue(TEXT("Las quemaduras siguen"), S.HasCondition(ECondition::ContactBurn) && FBodyModel::NumBurns(S) == 2);
			TestEqual(TEXT("El aloe no venda el corte de la raya"), S.Wounds.Num() == 3 ? S.Wounds[0].bBandaged : true, false);
		});

		It("quemadura solar y de contacto a la vez: el aloe cura una y acorta la otra", [this]()
		{
			FSurvivalState S = Dry();
			S.AddCondition(ECondition::SunBurn, 10.0f);
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			Apply(S, TEXT("gel_aloe"));
			TestFalse(TEXT("Solar curada"), S.HasCondition(ECondition::SunBurn));
			TestEqual(TEXT("Contacto a 12 h"), Hours(S, ECondition::ContactBurn), 12.0f);
		});

		It("con todos los estados a la vez el cuerpo sigue en rango en cada modo", [this]()
		{
			const FSurvivalModeSettings Modes[] = { FSurvivalModeSettings::FromMode(ESurvivalMode::Explorer), Survivor(),
				FSurvivalModeSettings::FromMode(ESurvivalMode::Castaway), FSurvivalModeSettings::MakeCustom(3.0f, false) };
			for (const FSurvivalModeSettings& Mode : Modes)
			{
				FSurvivalState S = AllConditions();
				TArray<ESurvivalEvent> Events;
				FBodyModel::AddCut(S, 0.9f);
				FBodyModel::ApplySting(S, EStingKind::Ray, Events);
				FBodyModel::ApplyContactBurn(S, Mode, Events);
				bool bSane = true;
				for (int32 I = 0; I < 480; ++I)
				{
					FSurvivalModel::Tick(S, Mild(), Step, Mode, 0.5f, Events);
					bSane = bSane && Sane(S);
				}
				TestTrue(TEXT("En rango"), bSane);
				if (!Mode.NeedsCanKill())
				{
					TestTrue(TEXT("Sin muerte si el modo no mata"), S.Health >= 10.0f - 1.0e-3f);
				}
			}
		});

		It("con tiempos de estado corruptos no se rompe nada", [this]()
		{
			FSurvivalState S = Dry();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, Survivor(), Events);
			S.Wounds[0].Healed = 5.0f;  // cargado de una partida dañada
			S.ConditionTime[static_cast<int32>(ECondition::ContactBurn)] = 1.0e6f;
			Simulate(S, Step, Events);
			TestEqual(TEXT("La quemadura sobrecicatrizada se cierra"), FBodyModel::NumBurns(S), 0);
			TestFalse(TEXT("Y el estado se apaga con ella"), S.HasCondition(ECondition::ContactBurn));
			TestTrue(TEXT("En rango"), Sane(S));

			FSurvivalState Nan = Dry();
			FBodyModel::ApplyContactBurn(Nan, Survivor(), Events);
			Nan.Wounds[0].Healed = std::numeric_limits<float>::quiet_NaN();
			Simulate(Nan, Step, Events);
			TestEqual(TEXT("Una quemadura con NaN no se queda abierta para siempre"), FBodyModel::NumBurns(Nan), 0);
			TestFalse(TEXT("Ni su estado"), Nan.HasCondition(ECondition::ContactBurn));
		});

		It("es determinista: el mismo cuerpo y los mismos pasos dan lo mismo", [this]()
		{
			auto Run = []()
			{
				FSurvivalState S = AllConditions();
				TArray<ESurvivalEvent> Events;
				FBodyModel::ApplyContactBurn(S, Survivor(), Events);
				FBodyModel::AddCut(S, 0.4f);
				for (int32 I = 0; I < 200; ++I)
				{
					FSurvivalModel::Tick(S, Mild(), Step, Survivor(), (I % 7) / 7.0f, Events);
					if (I == 50)
					{
						TArray<ESurvivalEvent> Ignored;
						FMedicineModel::Apply(S, FName(TEXT("gel_aloe")), Ignored);
					}
				}
				return S;
			};
			const FSurvivalState A = Run();
			const FSurvivalState B = Run();
			TestEqual(TEXT("Salud"), A.Health, B.Health);
			TestEqual(TEXT("Heridas"), A.Wounds.Num(), B.Wounds.Num());
			for (int32 C = 0; C < static_cast<int32>(ECondition::Count); ++C)
			{
				TestEqual(TEXT("Estados"), A.ConditionTime[C], B.ConditionTime[C]);
			}
		});
	});
}

#endif
