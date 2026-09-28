#include "Misc/AutomationTest.h"

#include "Survival/BodyModel.h"
#include "Survival/InnerVoiceModel.h"
#include "Survival/SurvivalModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace InnerVoiceModelTest
{
	/** Cuerpo sano y seco, con los avisos enganchados a lo que ya pasa. */
	FSurvivalState Calm()
	{
		FSurvivalState S;
		S.Wetness = 0.0f;
		return S;
	}

	/** El siguiente aviso, o Count si no toca ninguno. */
	EInnerVoiceLine Next(FInnerVoiceState& Voice, const FSurvivalState& S, const TArray<ESurvivalEvent>& Events = {})
	{
		EInnerVoiceLine Line = EInnerVoiceLine::Count;
		return FInnerVoiceModel::Evaluate(Voice, S, Events, Line) ? Line : EInnerVoiceLine::Count;
	}

	/** Todos los avisos pendientes, en el orden en que saldrían. */
	TArray<EInnerVoiceLine> Drain(FInnerVoiceState& Voice, const FSurvivalState& S)
	{
		TArray<EInnerVoiceLine> Lines;
		for (EInnerVoiceLine Line = Next(Voice, S); Line != EInnerVoiceLine::Count; Line = Next(Voice, S))
		{
			Lines.Add(Line);
			if (Lines.Num() > static_cast<int32>(EInnerVoiceLine::Count))
			{
				break; // nunca debería repetir
			}
		}
		return Lines;
	}

	int32 Count(const TArray<EInnerVoiceLine>& Lines, EInnerVoiceLine Line)
	{
		int32 N = 0;
		for (const EInnerVoiceLine L : Lines)
		{
			N += L == Line ? 1 : 0;
		}
		return N;
	}
}

BEGIN_DEFINE_SPEC(FInnerVoiceModelSpec, "Explored.InnerVoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FInnerVoiceModelSpec)

void FInnerVoiceModelSpec::Define()
{
	using namespace InnerVoiceModelTest;

	Describe("Los textos", [this]()
	{
		It("cada aviso tiene id único y frase en español e inglés", [this]()
		{
			TArray<FName> Ids;
			for (int32 I = 0; I < static_cast<int32>(EInnerVoiceLine::Count); ++I)
			{
				const EInnerVoiceLine Line = static_cast<EInnerVoiceLine>(I);
				const FInnerVoiceText& Text = FInnerVoiceModel::Text(Line);
				TestFalse(*FString::Printf(TEXT("Id del aviso %d"), I), Text.Id.IsNone());
				TestFalse(TEXT("Id único"), Ids.Contains(Text.Id));
				Ids.Add(Text.Id);
				TestFalse(*FString::Printf(TEXT("%s en español"), *Text.Id.ToString()), Text.TextEs.IsEmpty());
				TestFalse(*FString::Printf(TEXT("%s en inglés"), *Text.Id.ToString()), Text.TextEn.IsEmpty());
				TestTrue(TEXT("Idiomas distintos"), Text.TextEs != Text.TextEn);
				EInnerVoiceLine Back = EInnerVoiceLine::Count;
				TestTrue(TEXT("Se encuentra por id"), FInnerVoiceModel::FromId(Text.Id, Back) && Back == Line);
			}
		});

		It("recoge las frases de biblia 01 §6 tal cual", [this]()
		{
			TestEqual(TEXT("Quemadura ES"), FInnerVoiceModel::Text(EInnerVoiceLine::ContactBurn).TextEs,
				FString(TEXT("Eso ha dolido. Cuidado con las brasas.")));
			TestEqual(TEXT("Quemadura EN"), FInnerVoiceModel::Text(EInnerVoiceLine::ContactBurn).TextEn,
				FString(TEXT("That hurt. Watch the embers.")));
			TestEqual(TEXT("Hambre"), FInnerVoiceModel::Id(EInnerVoiceLine::HungerLow), FName(TEXT("hambre_aprieta")));
		});

		It("fuera de rango o con un id desconocido no revienta", [this]()
		{
			TestTrue(TEXT("Sin id"), FInnerVoiceModel::Id(EInnerVoiceLine::Count).IsNone());
			TestTrue(TEXT("Texto vacío"), FInnerVoiceModel::Text(static_cast<EInnerVoiceLine>(200)).TextEs.IsEmpty());
			EInnerVoiceLine Line = EInnerVoiceLine::HungerLow;
			TestFalse(TEXT("Desconocido"), FInnerVoiceModel::FromId(FName(TEXT("no_existe")), Line));
			TestFalse(TEXT("Vacío"), FInnerVoiceModel::FromId(NAME_None, Line));
			TestEqual(TEXT("No toca la salida"), Line, EInnerVoiceLine::HungerLow);
		});
	});

	Describe("Umbrales", [this]()
	{
		It("una partida nueva no dice nada", [this]()
		{
			FInnerVoiceState Voice;
			const FSurvivalState S;
			FInnerVoiceModel::Prime(Voice, S);
			TestEqual(TEXT("Nada enganchado"), Voice.Latched, 0u);
			TestEqual(TEXT("Nada"), Next(Voice, S), EInnerVoiceLine::Count);
		});

		It("avisa al cruzar el umbral una vez y no repite al rozarlo", [this]()
		{
			FInnerVoiceState Voice;
			FSurvivalState S = Calm();
			S.Hunger = 45.0f;
			TestEqual(TEXT("45: nada"), Next(Voice, S), EInnerVoiceLine::Count);
			S.Hunger = 39.0f;
			TestEqual(TEXT("39: el estómago"), Next(Voice, S), EInnerVoiceLine::HungerLow);
			TestEqual(TEXT("Solo una vez"), Next(Voice, S), EInnerVoiceLine::Count);
			S.Hunger = 42.0f;
			TestEqual(TEXT("Un bocado: nada"), Next(Voice, S), EInnerVoiceLine::Count);
			S.Hunger = 39.0f;
			TestEqual(TEXT("Otra vez por debajo, dentro del margen: nada"), Next(Voice, S), EInnerVoiceLine::Count);
			S.Hunger = 46.0f;
			TestEqual(TEXT("Comer de verdad rearma"), Next(Voice, S), EInnerVoiceLine::Count);
			S.Hunger = 39.0f;
			TestEqual(TEXT("Y vuelve a avisar"), Next(Voice, S), EInnerVoiceLine::HungerLow);
		});

		It("dice lo más urgente primero, de uno en uno", [this]()
		{
			FInnerVoiceState Voice;
			FSurvivalState S = Calm();
			S.Thirst = 0.0f;
			S.Hunger = 30.0f;
			const TArray<EInnerVoiceLine> Lines = Drain(Voice, S);
			TestEqual(TEXT("Tres avisos"), Lines.Num(), 3);
			if (Lines.Num() == 3)
			{
				TestEqual(TEXT("Primero la sed crítica"), Lines[0], EInnerVoiceLine::ThirstZero);
				TestEqual(TEXT("Luego la sed"), Lines[1], EInnerVoiceLine::ThirstLow);
				TestEqual(TEXT("Luego el hambre"), Lines[2], EInnerVoiceLine::HungerLow);
			}
		});

		It("descarta lo que pasó antes de decirse", [this]()
		{
			FInnerVoiceState Voice;
			FSurvivalState S = Calm();
			S.Thirst = 30.0f;
			S.Hunger = 30.0f;
			TestEqual(TEXT("Primero la sed"), Next(Voice, S), EInnerVoiceLine::ThirstLow);
			S.Hunger = 90.0f;
			TestEqual(TEXT("Ya ha comido: el hambre no se dice"), Next(Voice, S), EInnerVoiceLine::Count);
		});

		It("la fiebre no se confunde con el sol", [this]()
		{
			FInnerVoiceState Voice;
			FSurvivalState S = Calm();
			S.BodyTemperature = 38.8f;
			S.AddCondition(ECondition::Fever, 10.0f);
			const TArray<EInnerVoiceLine> Lines = Drain(Voice, S);
			TestEqual(TEXT("Solo la fiebre"), Lines.Num(), 1);
			TestTrue(TEXT("Fiebre"), Lines.Contains(EInnerVoiceLine::Fever));
			S.ClearCondition(ECondition::Fever);
			TestEqual(TEXT("Sin fiebre y con 38.8: es el sol"), Next(Voice, S), EInnerVoiceLine::HeatHigh);
		});

		It("empapado y con frío: primero lo mojado, luego el frío", [this]()
		{
			FInnerVoiceState Voice;
			FSurvivalState S = Calm();
			S.Wetness = 0.9f;
			S.BodyTemperature = 35.6f;
			const TArray<EInnerVoiceLine> Lines = Drain(Voice, S);
			TestEqual(TEXT("Dos"), Lines.Num(), 2);
			TestTrue(TEXT("Orden"), Lines.Num() == 2 && Lines[0] == EInnerVoiceLine::Soaked && Lines[1] == EInnerVoiceLine::ColdLow);
		});

		It("cada estado del cuerpo avisa al empezar y se rearma al curarse", [this]()
		{
			const TPair<ECondition, EInnerVoiceLine> Cases[] = {
				{ ECondition::Poisoned, EInnerVoiceLine::Poisoned },
				{ ECondition::Fever, EInnerVoiceLine::Fever },
				{ ECondition::Sprain, EInnerVoiceLine::Sprain },
				{ ECondition::Infection, EInnerVoiceLine::WoundInfected },
				{ ECondition::JellyfishSting, EInnerVoiceLine::JellyfishSting },
				{ ECondition::RaySting, EInnerVoiceLine::RaySting },
				{ ECondition::Hallucinating, EInnerVoiceLine::Hallucinating },
				{ ECondition::Bleeding, EInnerVoiceLine::WoundBleeding },
			};
			for (const TPair<ECondition, EInnerVoiceLine>& Case : Cases)
			{
				FInnerVoiceState Voice;
				FSurvivalState S = Calm();
				S.AddCondition(Case.Key, 5.0f);
				const FString Id = FInnerVoiceModel::Id(Case.Value).ToString();
				TestEqual(*(Id + TEXT(": avisa")), Next(Voice, S), Case.Value);
				TestEqual(*(Id + TEXT(": una vez")), Next(Voice, S), EInnerVoiceLine::Count);
				S.ClearCondition(Case.Key);
				TestEqual(*(Id + TEXT(": curado, nada")), Next(Voice, S), EInnerVoiceLine::Count);
				S.AddCondition(Case.Key, 5.0f);
				TestEqual(*(Id + TEXT(": recaída, avisa otra vez")), Next(Voice, S), Case.Value);
			}
		});

		It("el escorbuto avisa por etapas", [this]()
		{
			FInnerVoiceState Voice;
			FSurvivalState S = Calm();
			S.ScurvySeverity = 0.2f;
			TestEqual(TEXT("Encías"), Next(Voice, S), EInnerVoiceLine::ScurvyGums);
			S.ScurvySeverity = 0.5f;
			TestEqual(TEXT("Visión"), Next(Voice, S), EInnerVoiceLine::ScurvyVision);
			S.ScurvySeverity = 0.8f;
			TestEqual(TEXT("Sangrado"), Next(Voice, S), EInnerVoiceLine::ScurvyBleeding);
			TestEqual(TEXT("Y nada más"), Next(Voice, S), EInnerVoiceLine::Count);
		});

		It("al cargar o reaparecer no recita lo que ya dolía", [this]()
		{
			FInnerVoiceState Voice;
			FSurvivalState S = Calm();
			S.Hunger = 10.0f;
			S.AddCondition(ECondition::Fever, 5.0f);
			S.MonotonyHours = 72.0f;
			FInnerVoiceModel::Prime(Voice, S);
			TestEqual(TEXT("Silencio"), Next(Voice, S), EInnerVoiceLine::Count);
			S.Hunger = 0.0f;
			TestEqual(TEXT("Lo nuevo sí"), Next(Voice, S), EInnerVoiceLine::HungerZero);
		});
	});

	Describe("Sucesos", [this]()
	{
		It("cada quemadura se dice, aunque la anterior siga abierta", [this]()
		{
			FInnerVoiceState Voice;
			FSurvivalState S = Calm();
			TArray<ESurvivalEvent> Events;
			FBodyModel::ApplyContactBurn(S, FSurvivalModeSettings(), Events);
			TestEqual(TEXT("Primera"), Next(Voice, S, Events), EInnerVoiceLine::ContactBurn);
			TestEqual(TEXT("Una quemadura no es un corte que sangra"), Next(Voice, S), EInnerVoiceLine::Count);
			Events.Reset();
			FBodyModel::ApplyContactBurn(S, FSurvivalModeSettings(), Events);
			TestEqual(TEXT("Segunda"), Next(Voice, S, Events), EInnerVoiceLine::ContactBurn);
		});

		It("un corte que sangra avisa; uno que ya coaguló, no", [this]()
		{
			FInnerVoiceState Voice;
			FSurvivalState S = Calm();
			FBodyModel::AddCut(S, 0.5f);
			TestEqual(TEXT("Sangra"), Next(Voice, S), EInnerVoiceLine::WoundBleeding);
			FBodyModel::TreatWounds(S, EWoundTreatment::ClothBandage);
			TestEqual(TEXT("Vendado"), Next(Voice, S), EInnerVoiceLine::Count);
			FBodyModel::AddCut(S, 0.3f);
			TestEqual(TEXT("Otro corte, otro aviso"), Next(Voice, S), EInnerVoiceLine::WoundBleeding);
		});

		It("ahogo bajo el agua y jadeo al salir", [this]()
		{
			FInnerVoiceState Voice;
			const FSurvivalState S = Calm();
			FInnerVoiceModel::ObserveBreath(Voice, 0.2f, true);
			TestEqual(TEXT("Aún aguanta"), Next(Voice, S), EInnerVoiceLine::Count);
			FInnerVoiceModel::ObserveBreath(Voice, 0.0f, true);
			TestEqual(TEXT("¡Aire!"), Next(Voice, S), EInnerVoiceLine::Drowning);
			FInnerVoiceModel::ObserveBreath(Voice, 0.0f, true);
			TestEqual(TEXT("No lo repite en bucle"), Next(Voice, S), EInnerVoiceLine::Count);
			FInnerVoiceModel::ObserveBreath(Voice, 0.1f, false);
			TestEqual(TEXT("Jadeo al salir"), Next(Voice, S), EInnerVoiceLine::OutOfBreath);
			FInnerVoiceModel::ObserveBreath(Voice, 1.0f, false);
			TestEqual(TEXT("Respira: nada"), Next(Voice, S), EInnerVoiceLine::Count);
			FInnerVoiceModel::ObserveBreath(Voice, 0.8f, true);
			FInnerVoiceModel::ObserveBreath(Voice, 0.5f, false);
			TestEqual(TEXT("Una zambullida corta no jadea"), Next(Voice, S), EInnerVoiceLine::Count);
		});

		It("si sale antes de decirlo, no grita «aire» en la orilla", [this]()
		{
			FInnerVoiceState Voice;
			const FSurvivalState S = Calm();
			FInnerVoiceModel::ObserveBreath(Voice, 0.0f, true);
			FInnerVoiceModel::ObserveBreath(Voice, 0.05f, false);
			const TArray<EInnerVoiceLine> Lines = Drain(Voice, S);
			TestEqual(TEXT("Solo el jadeo"), Lines.Num(), 1);
			TestTrue(TEXT("Jadeo"), Lines.Contains(EInnerVoiceLine::OutOfBreath));
		});

		It("un oxígeno corrupto no dispara nada", [this]()
		{
			FInnerVoiceState Voice;
			const FSurvivalState S = Calm();
			FInnerVoiceModel::ObserveBreath(Voice, std::numeric_limits<float>::quiet_NaN(), true);
			FInnerVoiceModel::ObserveBreath(Voice, -std::numeric_limits<float>::infinity(), false);
			TestEqual(TEXT("Nada"), Next(Voice, S), EInnerVoiceLine::Count);
		});
	});

	Describe("En una partida", [this]()
	{
		It("sin comer ni beber 30 horas cada aviso sale una sola vez", [this]()
		{
			FInnerVoiceState Voice;
			FSurvivalState S;
			FSurvivalInputs In;
			In.AirTemperature = 28.0f;
			In.Wind = 0.0f;
			FInnerVoiceModel::Prime(Voice, S);
			TArray<EInnerVoiceLine> Said;
			for (int32 I = 0; I < 300; ++I)
			{
				TArray<ESurvivalEvent> Events;
				FSurvivalModel::Tick(S, In, 0.1f, FSurvivalModeSettings(), 0.99f, Events);
				EInnerVoiceLine Line = EInnerVoiceLine::Count;
				if (FInnerVoiceModel::Evaluate(Voice, S, Events, Line))
				{
					TestTrue(TEXT("Lo dicho está pasando"), FInnerVoiceModel::IsEventLine(Line) || FInnerVoiceModel::IsActive(Line, S));
					Said.Add(Line);
				}
			}
			TestEqual(TEXT("Sed una vez"), Count(Said, EInnerVoiceLine::ThirstLow), 1);
			TestEqual(TEXT("Sed crítica una vez"), Count(Said, EInnerVoiceLine::ThirstZero), 1);
			TestEqual(TEXT("Hambre una vez"), Count(Said, EInnerVoiceLine::HungerLow), 1);
			TestEqual(TEXT("Sueño una vez"), Count(Said, EInnerVoiceLine::SleepLow), 1);
			TestTrue(TEXT("Sigue vivo"), !S.IsDead());
		});

		It("con cuerpos al azar solo dice lo que está pasando y no deja nada pendiente sin enganchar", [this]()
		{
			uint32 Seed = 777u;
			auto Rand = [&Seed]()
			{
				Seed = Seed * 1664525u + 1013904223u;
				return static_cast<float>(Seed >> 8) / static_cast<float>(1u << 24);
			};
			FInnerVoiceState Voice;
			for (int32 I = 0; I < 2000; ++I)
			{
				FSurvivalState S;
				S.Hunger = Rand() * 100.0f;
				S.Thirst = Rand() * 100.0f;
				S.Rest = Rand() * 100.0f;
				S.BodyTemperature = 33.0f + Rand() * 8.0f;
				S.Wetness = Rand();
				S.ScurvySeverity = Rand();
				S.MonotonyHours = Rand() * 80.0f;
				for (int32 C = 0; C < static_cast<int32>(ECondition::Count); ++C)
				{
					S.ConditionTime[C] = Rand() < 0.2f ? 5.0f : 0.0f;
				}
				EInnerVoiceLine Line = EInnerVoiceLine::Count;
				if (FInnerVoiceModel::Evaluate(Voice, S, {}, Line))
				{
					if (!FInnerVoiceModel::IsActive(Line, S))
					{
						AddError(FString::Printf(TEXT("Dijo %s sin que pase"), *FInnerVoiceModel::Id(Line).ToString()));
						return;
					}
					TestTrue(TEXT("Lo dicho queda enganchado"), Voice.IsLatched(Line));
				}
				TestEqual(TEXT("Nada pendiente que no pase"), Voice.Pending & ~Voice.Latched, 0u);
			}
		});
	});
}

#endif
