#include "Misc/AutomationTest.h"

#include "Survival/BodyEventsModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace BodyEventsTest
{
	/** Hora absoluta del día Day (1-based) a la hora Hour. */
	double At(int32 Day, double Hour)
	{
		return (Day - 1) * 24.0 + Hour;
	}

	FBodyBars WithThirst(float Thirst)
	{
		FBodyBars Bars;
		Bars.Thirst = Thirst;
		return Bars;
	}
}

BEGIN_DEFINE_SPEC(FBodyEventsSpec, "Explored.BodyEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBodyEventsSpec)

void FBodyEventsSpec::Define()
{
	using namespace BodyEventsTest;

	Describe(TEXT("desmayo"), [this]()
	{
		It(TEXT("una barra a 0 durante 20 s desmaya"), [this]()
		{
			const FBodyBars Bars = WithThirst(0.0f);
			const FFaintTick Almost = FBodyEventsModel::TickFaint(FFaintTracker(), Bars, 19.5f);
			TestFalse(TEXT("A los 19,5 s aún no"), Almost.bFainted);
			const FFaintTick Now = FBodyEventsModel::TickFaint(Almost.Tracker, Bars, 0.5f);
			TestTrue(TEXT("A los 20 s sí"), Now.bFainted);
			TestEqual(TEXT("Cuenta reiniciada"), Now.Tracker.SecondsAtZero, 0.0f);
		});

		It(TEXT("a 60 Hz se dispara una sola vez en 20 s"), [this]()
		{
			FFaintTracker Tracker;
			FBodyBars Bars;
			Bars.Warmth = 0.0f;
			int32 Faints = 0;
			int32 FirstFrame = INDEX_NONE;
			for (int32 Frame = 0; Frame < 20 * 60 + 30; ++Frame)
			{
				const FFaintTick Tick = FBodyEventsModel::TickFaint(Tracker, Bars, 1.0f / 60.0f);
				Tracker = Tick.Tracker;
				if (Tick.bFainted)
				{
					++Faints;
					FirstFrame = FirstFrame == INDEX_NONE ? Frame : FirstFrame;
				}
			}
			TestEqual(TEXT("Un desmayo"), Faints, 1);
			TestTrue(TEXT("Hacia el segundo 20"), FirstFrame >= 20 * 60 - 2 && FirstFrame <= 20 * 60);
		});

		It(TEXT("si la barra se recupera antes, la cuenta vuelve a 0"), [this]()
		{
			const FFaintTick Low = FBodyEventsModel::TickFaint(FFaintTracker(), WithThirst(0.0f), 15.0f);
			const FFaintTick Drank = FBodyEventsModel::TickFaint(Low.Tracker, WithThirst(25.0f), 0.5f);
			TestFalse(TEXT("Sin desmayo"), Drank.bFainted);
			TestEqual(TEXT("Cuenta a 0"), Drank.Tracker.SecondsAtZero, 0.0f);
			TestFalse(TEXT("Otros 15 s no bastan"), FBodyEventsModel::TickFaint(Drank.Tracker, WithThirst(0.0f), 15.0f).bFainted);
		});

		It(TEXT("pasos no finitos o negativos no cuentan"), [this]()
		{
			FFaintTracker Tracker;
			Tracker.SecondsAtZero = 19.0f;
			const FBodyBars Bars = WithThirst(0.0f);
			const float Bad[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -5.0f};
			for (const float Delta : Bad)
			{
				const FFaintTick Tick = FBodyEventsModel::TickFaint(Tracker, Bars, Delta);
				TestFalse(TEXT("Sin desmayo"), Tick.bFainted);
				TestEqual(TEXT("Cuenta igual"), Tick.Tracker.SecondsAtZero, 19.0f);
			}
		});

		It(TEXT("una barra no finita cuenta como vacía"), [this]()
		{
			TestTrue(TEXT("NaN"), FBodyEventsModel::AnyBarEmpty(WithThirst(std::numeric_limits<float>::quiet_NaN())));
			TestFalse(TEXT("Barras de inicio"), FBodyEventsModel::AnyBarEmpty(FBodyBars()));
		});

		It(TEXT("al despertar: 40/40/40/60 y +3 h"), [this]()
		{
			const FFaintOutcome Out = FBodyEventsModel::Faint(At(2, 14.0));
			TestEqual(TEXT("Sed"), Out.Bars.Thirst, 40.0f);
			TestEqual(TEXT("Hambre"), Out.Bars.Hunger, 40.0f);
			TestEqual(TEXT("Energía"), Out.Bars.Energy, 40.0f);
			TestEqual(TEXT("Calor"), Out.Bars.Warmth, 60.0f);
			TestEqual(TEXT("Hora"), Out.WakeHour, At(2, 17.0));
		});
	});

	Describe(TEXT("dormir"), [this]()
	{
		It(TEXT("en refugio de noche duerme hasta las 06:00 con los deltas fijos"), [this]()
		{
			FBodyBars Bars;
			Bars.Thirst = 60.0f;
			Bars.Hunger = 70.0f;
			Bars.Energy = 12.0f;
			Bars.Warmth = 33.0f;
			const FSleepOutcome Out = FBodyEventsModel::Sleep(Bars, At(1, 22.0), true);
			TestTrue(TEXT("Duerme"), Out.bSlept);
			TestEqual(TEXT("Despierta D2 06:00"), Out.WakeHour, At(2, 6.0));
			TestEqual(TEXT("Sed -25"), Out.Bars.Thirst, 35.0f);
			TestEqual(TEXT("Hambre -15"), Out.Bars.Hunger, 55.0f);
			TestEqual(TEXT("Energía a 100"), Out.Bars.Energy, 100.0f);
			TestEqual(TEXT("Calor igual"), Out.Bars.Warmth, 33.0f);
			TestFalse(TEXT("Sin aviso"), Out.bThirstWarning);
		});

		It(TEXT("después de medianoche despierta el mismo día a las 06:00"), [this]()
		{
			const FSleepOutcome Out = FBodyEventsModel::Sleep(FBodyBars(), At(3, 2.5), true);
			TestTrue(TEXT("Duerme"), Out.bSlept);
			TestEqual(TEXT("D3 06:00"), Out.WakeHour, At(3, 6.0));
		});

		It(TEXT("respeta los límites: 20:00 y 04:59 sí; 05:00 y 19:59 no"), [this]()
		{
			TestEqual(TEXT("20:00"), FBodyEventsModel::CanSleep(At(1, 20.0), true), ESleepCheck::Allowed);
			TestEqual(TEXT("04:59"), FBodyEventsModel::CanSleep(At(2, 5.0 - 1.0 / 60.0), true), ESleepCheck::Allowed);
			TestEqual(TEXT("05:00"), FBodyEventsModel::CanSleep(At(2, 5.0), true), ESleepCheck::WrongHour);
			TestEqual(TEXT("19:59"), FBodyEventsModel::CanSleep(At(1, 20.0 - 1.0 / 60.0), true), ESleepCheck::WrongHour);
			TestEqual(TEXT("Mediodía"), FBodyEventsModel::CanSleep(At(2, 12.0), true), ESleepCheck::WrongHour);
		});

		It(TEXT("fuera de horario o de refugio no duerme y no cambia nada"), [this]()
		{
			const FBodyBars Bars = WithThirst(50.0f);
			const FSleepOutcome Day = FBodyEventsModel::Sleep(Bars, At(1, 15.0), true);
			TestFalse(TEXT("De día"), Day.bSlept);
			TestEqual(TEXT("Motivo hora"), Day.Check, ESleepCheck::WrongHour);
			TestTrue(TEXT("Barras iguales"), Day.Bars == Bars);
			TestEqual(TEXT("Hora igual"), Day.WakeHour, At(1, 15.0));

			const FSleepOutcome Outside = FBodyEventsModel::Sleep(Bars, At(1, 23.0), false);
			TestFalse(TEXT("A la intemperie"), Outside.bSlept);
			TestEqual(TEXT("Motivo refugio"), Outside.Check, ESleepCheck::NotSheltered);
			TestTrue(TEXT("Barras iguales"), Outside.Bars == Bars);
		});

		It(TEXT("con sed < 30 avisa pero duerme, y la sed no baja de 0"), [this]()
		{
			const FSleepOutcome Out = FBodyEventsModel::Sleep(WithThirst(10.0f), At(1, 21.0), true);
			TestTrue(TEXT("Duerme"), Out.bSlept);
			TestTrue(TEXT("Aviso"), Out.bThirstWarning);
			TestEqual(TEXT("Sed a 0"), Out.Bars.Thirst, 0.0f);
			TestTrue(TEXT("El desmayo lo corregirá"), FBodyEventsModel::AnyBarEmpty(Out.Bars));
		});

		It(TEXT("una hora no finita no duerme"), [this]()
		{
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			TestEqual(TEXT("NaN"), FBodyEventsModel::CanSleep(NaN, true), ESleepCheck::WrongHour);
			TestFalse(TEXT("No duerme"), FBodyEventsModel::Sleep(FBodyBars(), NaN, true).bSlept);
		});
	});
}

#endif
