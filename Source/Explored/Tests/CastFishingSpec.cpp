#include "Misc/AutomationTest.h"

#include "Fishing/CastFishingModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FCastFishingSpec, "Explored.CastFishing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FCastFishingConfig Config;
END_DEFINE_SPEC(FCastFishingSpec)

void FCastFishingSpec::Define()
{
	It(TEXT("la espera media es de ~35 s a mediodía y 1,5 veces más rápida al amanecer"), [this]()
	{
		uint32 Seed = 7;
		double Noon = 0.0;
		double Dawn = 0.0;
		constexpr int32 Samples = 4000;
		for (int32 I = 0; I < Samples; ++I)
		{
			const float Wait = FCastFishingModel::DrawWaitSeconds(Config, 12.0f, Seed);
			TestTrue(TEXT("Nunca menos de la mínima"), Wait >= Config.MinBiteSeconds);
			Noon += Wait;
			Dawn += FCastFishingModel::DrawWaitSeconds(Config, 7.0f, Seed);
		}
		TestEqual(TEXT("Media a mediodía"), static_cast<float>(Noon / Samples), 35.0f, 1.0f);
		TestEqual(TEXT("Media al amanecer"), static_cast<float>(Dawn / Samples), 35.0f / 1.5f, 1.0f);
	});

	It(TEXT("lanzar, morder y pulsar a tiempo saca el pez"), [this]()
	{
		ECastFishingEvent Event = ECastFishingEvent::None;
		FCastFishingState S = FCastFishingModel::Cast(FCastFishingState(), Config, 10.0f, false, Event);
		TestEqual(TEXT("Lanzado"), Event, ECastFishingEvent::Cast);
		S = FCastFishingModel::Tick(S, Config, S.RemainingSeconds, 10.0f, false, Event);
		TestEqual(TEXT("Muerde"), Event, ECastFishingEvent::Bite);
		S = FCastFishingModel::Tick(S, Config, 2.0f, 10.0f, false, Event);
		TestEqual(TEXT("Aún hay tiempo"), S.Phase, ECastFishingPhase::Biting);
		S = FCastFishingModel::Pull(S, Event);
		TestEqual(TEXT("Pez"), Event, ECastFishingEvent::Caught);
		TestEqual(TEXT("Vuelve a reposo"), S.Phase, ECastFishingPhase::Idle);
	});

	It(TEXT("si no pulsa en 3 s el pez se escapa; si pulsa antes de la mordida, no saca nada"), [this]()
	{
		ECastFishingEvent Event = ECastFishingEvent::None;
		FCastFishingState S = FCastFishingModel::Cast(FCastFishingState(), Config, 10.0f, false, Event);
		FCastFishingState Early = FCastFishingModel::Pull(S, Event);
		TestEqual(TEXT("Recoge sin pez"), Event, ECastFishingEvent::Reeled);
		TestEqual(TEXT("Reposo"), Early.Phase, ECastFishingPhase::Idle);
		S = FCastFishingModel::Tick(S, Config, S.RemainingSeconds, 10.0f, false, Event);
		S = FCastFishingModel::Tick(S, Config, 3.0f, 10.0f, false, Event);
		TestEqual(TEXT("Se escapa"), Event, ECastFishingEvent::Escaped);
		FCastFishingModel::Pull(S, Event);
		TestEqual(TEXT("Pulsar tarde no da nada"), Event, ECastFishingEvent::None);
	});

	It(TEXT("no se pesca de noche ni con el ciclón, y la noche corta la espera"), [this]()
	{
		ECastFishingEvent Event = ECastFishingEvent::None;
		FCastFishingModel::Cast(FCastFishingState(), Config, 22.0f, false, Event);
		TestEqual(TEXT("Noche"), Event, ECastFishingEvent::Blocked);
		FCastFishingModel::Cast(FCastFishingState(), Config, 3.0f, false, Event);
		TestEqual(TEXT("Madrugada"), Event, ECastFishingEvent::Blocked);
		FCastFishingModel::Cast(FCastFishingState(), Config, 12.0f, true, Event);
		TestEqual(TEXT("Ciclón"), Event, ECastFishingEvent::Blocked);
		FCastFishingState S = FCastFishingModel::Cast(FCastFishingState(), Config, 19.9f, false, Event);
		S = FCastFishingModel::Tick(S, Config, 1.0f, 20.0f, false, Event);
		TestEqual(TEXT("Cae la noche"), Event, ECastFishingEvent::Blocked);
		TestEqual(TEXT("Reposo"), S.Phase, ECastFishingPhase::Idle);
	});

	It(TEXT("el factor de hora dorada solo cuenta al amanecer y al atardecer"), [this]()
	{
		TestEqual(TEXT("06:00"), FCastFishingModel::RateFactor(Config, 6.0f), 1.5f);
		TestEqual(TEXT("08:00"), FCastFishingModel::RateFactor(Config, 8.0f), 1.0f);
		TestEqual(TEXT("18:30"), FCastFishingModel::RateFactor(Config, 18.5f), 1.5f);
		TestEqual(TEXT("13:00"), FCastFishingModel::RateFactor(Config, 13.0f), 1.0f);
	});
}

#endif
