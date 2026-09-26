#include "Misc/AutomationTest.h"

#include "Weather/WeatherModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FWeatherSpec, "Explored.Weather",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FWeatherSpec)

void FWeatherSpec::Define()
{
	It("recorre las cuatro estaciones de ocho días", [this]()
	{
		TestEqual(TEXT("Día 0"), FWeatherModel::SeasonForDay(0.5f), ESeason::Dry);
		TestEqual(TEXT("Día 9"), FWeatherModel::SeasonForDay(9.0f), ESeason::FirstRains);
		TestEqual(TEXT("Día 17"), FWeatherModel::SeasonForDay(17.2f), ESeason::Monsoon);
		TestEqual(TEXT("Día 31"), FWeatherModel::SeasonForDay(31.9f), ESeason::Cyclones);
		TestEqual(TEXT("Año siguiente"), FWeatherModel::SeasonForDay(33.0f), ESeason::Dry);
	});

	It("es determinista para la misma semilla", [this]()
	{
		const FWeatherModel A(99);
		const FWeatherModel B(99);
		for (float T = 0.0f; T < 64.0f; T += 0.37f)
		{
			if (A.StateAt(T) != B.StateAt(T))
			{
				AddError(FString::Printf(TEXT("Divergencia en %.2f"), T));
				return;
			}
		}
	});

	It("solo genera ciclones en su temporada y nunca en los primeros días", [this]()
	{
		const FWeatherModel Model(7);
		int32 Cyclones = 0;
		for (float T = 0.0f; T < 96.0f; T += 0.05f)
		{
			if (Model.StateAt(T) == EWeatherState::Cyclone)
			{
				++Cyclones;
				const ESeason S = FWeatherModel::SeasonForDay(T);
				// Un ciclón que empieza el último día puede cruzar la medianoche hacia la estación seca.
				if (S != ESeason::Cyclones && !(S == ESeason::Dry && FMath::Fmod(T, 32.0f) < 1.0f))
				{
					AddError(FString::Printf(TEXT("Ciclón fuera de temporada en %.2f"), T));
					return;
				}
				TestTrue(TEXT("No en los primeros días"), T > 6.0f);
			}
		}
		TestTrue(TEXT("Hay algún ciclón en tres años"), Cyclones > 0);
	});

	It("avisa de los temporales con presión en caída y mar de fondo", [this]()
	{
		const FWeatherModel Model(7);
		FWeatherSpan Severe;
		TestTrue(TEXT("Hay temporal"), Model.NextSevereEvent(0.0f, Severe));
		const FWeatherSample Calm = Model.SampleAt(Severe.Start - 3.0f);
		const FWeatherSample Before = Model.SampleAt(Severe.Start - 0.1f);
		TestTrue(TEXT("La presión baja antes del temporal"), Before.Pressure < Calm.Pressure - 5.0f);
		TestTrue(TEXT("Mar de fondo"), Before.SeaState >= 0.25f);
	});

	It("mantiene las magnitudes en rango y las transiciones continuas", [this]()
	{
		const FWeatherModel Model(3);
		FWeatherSample Prev = Model.SampleAt(0.0f);
		for (float T = 0.002f; T < 40.0f; T += 0.002f)
		{
			const FWeatherSample S = Model.SampleAt(T);
			if (S.Rain < 0.0f || S.Rain > 1.0f || S.CloudCover < 0.0f || S.CloudCover > 1.0f || S.SeaState < 0.0f || S.SeaState > 1.0f)
			{
				AddError(FString::Printf(TEXT("Fuera de rango en %.3f"), T));
				return;
			}
			if (FMath::Abs(S.Rain - Prev.Rain) > 0.35f)
			{
				AddError(FString::Printf(TEXT("Salto de lluvia en %.3f: %.2f -> %.2f"), T, Prev.Rain, S.Rain));
				return;
			}
			Prev = S;
		}
	});

	It("hace la estación seca más soleada que el monzón", [this]()
	{
		const FWeatherModel Model(11);
		float DryRain = 0.0f;
		float MonsoonRain = 0.0f;
		for (int32 Year = 0; Year < 4; ++Year)
		{
			for (float T = 0.0f; T < 8.0f; T += 0.05f)
			{
				DryRain += Model.SampleAt(Year * 32.0f + T).Rain;
				MonsoonRain += Model.SampleAt(Year * 32.0f + 16.0f + T).Rain;
			}
		}
		TestTrue(TEXT("Más lluvia en el monzón"), MonsoonRain > DryRain * 2.0f);
	});
}

#endif
