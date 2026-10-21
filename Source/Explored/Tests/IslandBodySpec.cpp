#include "Misc/AutomationTest.h"

#include "Survival/IslandBodyModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace IslandBodyTest
{
	FBodyEnvironment Day()
	{
		FBodyEnvironment Env;
		Env.HourOfDay = 12.0;
		return Env;
	}

	FBodyEnvironment Night()
	{
		FBodyEnvironment Env;
		Env.HourOfDay = 22.0;
		return Env;
	}
}

BEGIN_DEFINE_SPEC(FIslandBodySpec, "Explored.IslandBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FIslandBodySpec)

void FIslandBodySpec::Define()
{
	using namespace IslandBodyTest;

	Describe(TEXT("desgaste"), [this]()
	{
		It(TEXT("empieza con 55/70/70/100 y pierde por minuto real lo del GDD"), [this]()
		{
			const FIslandBodyTuning Tuning;
			TestEqual(TEXT("Sed inicial"), Tuning.Initial.Thirst, 55.0f);
			TestEqual(TEXT("Hambre inicial"), Tuning.Initial.Hunger, 70.0f);
			TestEqual(TEXT("Energía inicial"), Tuning.Initial.Energy, 70.0f);
			TestEqual(TEXT("Calor inicial"), Tuning.Initial.Warmth, 100.0f);
			const FBodyStep Step = FIslandBodyModel::Tick(Tuning.Initial, FBodyWarnings(), Day(), 60.0f);
			TestEqual(TEXT("Sed -4,5"), Step.Bars.Thirst, 50.5f, 1e-4f);
			TestEqual(TEXT("Hambre -2,5"), Step.Bars.Hunger, 67.5f, 1e-4f);
			TestEqual(TEXT("Energía -3"), Step.Bars.Energy, 67.0f, 1e-4f);
			TestEqual(TEXT("Calor de día sin cambio"), Step.Bars.Warmth, 100.0f);
		});

		It(TEXT("el calor baja de noche fuera, no bajo techo, y sube junto al fuego"), [this]()
		{
			const FBodyBars Bars;
			TestEqual(TEXT("Noche fuera -3"), FIslandBodyModel::Tick(Bars, FBodyWarnings(), Night(), 60.0f).Bars.Warmth, 97.0f, 1e-4f);
			FBodyEnvironment Roof = Night();
			Roof.bSheltered = true;
			TestEqual(TEXT("Bajo techo"), FIslandBodyModel::Tick(Bars, FBodyWarnings(), Roof, 60.0f).Bars.Warmth, 100.0f);
			FBodyEnvironment Rain = Night();
			Rain.bRaining = true;
			TestEqual(TEXT("Lluvia de noche -4 (no se suman)"), FIslandBodyModel::WarmthPerMinute(Rain), -4.0f);
			FBodyEnvironment Fire = Rain;
			Fire.bNearFire = true;
			const FBodyBars Cold = FIslandBodyModel::With(Bars, EBodyBar::Warmth, 50.0f);
			TestEqual(TEXT("Fuego +25"), FIslandBodyModel::Tick(Cold, FBodyWarnings(), Fire, 60.0f).Bars.Warmth, 75.0f, 1e-4f);
			TestTrue(TEXT("06:00 ya es de día"), !FIslandBodyModel::IsNight(6.0) && FIslandBodyModel::IsNight(5.99) && FIslandBodyModel::IsNight(20.0));
		});

		It(TEXT("las barras no bajan de 0 ni un paso no válido cambia nada"), [this]()
		{
			const FBodyBars Bars;
			const FBodyStep Long = FIslandBodyModel::Tick(Bars, FBodyWarnings(), Night(), 3600.0f);
			TestEqual(TEXT("Sed a 0"), Long.Bars.Thirst, 0.0f);
			TestEqual(TEXT("Calor a 0"), Long.Bars.Warmth, 0.0f);
			const float Bad[] = {-1.0f, 0.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
			for (const float Seconds : Bad)
			{
				TestTrue(TEXT("Sin cambio"), FIslandBodyModel::Tick(Bars, FBodyWarnings(), Night(), Seconds).Bars == Bars);
			}
		});
	});

	Describe(TEXT("avisos y barras bajas"), [this]()
	{
		It(TEXT("avisa una vez al bajar de 40 y se rearma por encima de 60"), [this]()
		{
			const FBodyBars Bars = FIslandBodyModel::With(FBodyBars(), EBodyBar::Thirst, 40.5f);
			const FBodyStep First = FIslandBodyModel::Tick(Bars, FBodyWarnings(), Day(), 10.0f);
			TestEqual(TEXT("Un aviso"), First.NewWarnings.Num(), 1);
			TestTrue(TEXT("De la sed"), First.NewWarnings.Num() == 1 && First.NewWarnings[0] == EBodyBar::Thirst);
			const FBodyStep Second = FIslandBodyModel::Tick(First.Bars, First.Warnings, Day(), 10.0f);
			TestEqual(TEXT("No se repite"), Second.NewWarnings.Num(), 0);
			const FBodyBars Drunk = FIslandBodyModel::Apply(Second.Bars, 30.0f, 0.0f, 0.0f);
			const FBodyStep Rearmed = FIslandBodyModel::Tick(Drunk, Second.Warnings, Day(), 1.0f);
			TestFalse(TEXT("Rearmado"), Rearmed.Warnings.HasWarned(EBodyBar::Thirst));
		});

		It(TEXT("bajo 25 frena un 20 % y desatura; a 25 justo no"), [this]()
		{
			const FBodyBars Fine;
			TestEqual(TEXT("Velocidad normal"), FIslandBodyModel::SpeedFactor(Fine), 1.0f);
			TestEqual(TEXT("Sin desaturar"), FIslandBodyModel::Desaturation01(Fine), 0.0f);
			const FBodyBars Edge = FIslandBodyModel::With(Fine, EBodyBar::Hunger, 25.0f);
			TestEqual(TEXT("A 25 no frena"), FIslandBodyModel::SpeedFactor(Edge), 1.0f);
			const FBodyBars Low = FIslandBodyModel::With(Fine, EBodyBar::Energy, 24.0f);
			TestEqual(TEXT("Frena"), FIslandBodyModel::SpeedFactor(Low), 0.8f);
			const FBodyBars Empty = FIslandBodyModel::With(Fine, EBodyBar::Energy, 0.0f);
			TestTrue(TEXT("Desatura más cuanto más baja"),
				FIslandBodyModel::Desaturation01(Empty) > FIslandBodyModel::Desaturation01(Low) && FIslandBodyModel::Desaturation01(Low) >= 0.4f);
		});

		It(TEXT("comer y beber suma con tope en 100 y no toca el calor"), [this]()
		{
			const FBodyBars Bars;
			const FBodyBars After = FIslandBodyModel::Apply(Bars, 25.0f, 45.0f, -10.0f);
			TestEqual(TEXT("Sed"), After.Thirst, 80.0f);
			TestEqual(TEXT("Hambre con tope"), After.Hunger, 100.0f);
			TestEqual(TEXT("Energía"), After.Energy, 60.0f);
			TestEqual(TEXT("Calor igual"), After.Warmth, Bars.Warmth);
			TestTrue(TEXT("NaN no cuenta"), FIslandBodyModel::Apply(Bars, std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f) == Bars);
		});
	});

	Describe(TEXT("ajustes"), [this]()
	{
		It(TEXT("los del GDD son válidos y una tasa negativa no"), [this]()
		{
			FIslandBodyTuning Tuning;
			TestTrue(TEXT("GDD"), Tuning.IsValid());
			Tuning.ThirstLossPerMinute = -1.0f;
			TestFalse(TEXT("Negativa"), Tuning.IsValid());
			FIslandBodyTuning Speed;
			Speed.LowSpeedFactor = 0.0f;
			TestFalse(TEXT("Velocidad 0"), Speed.IsValid());
		});
	});
}

#endif
