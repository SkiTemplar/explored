#include "Misc/AutomationTest.h"

#include "Ocean/OceanWaves.h"
#include "Sky/TimeOfDaySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FSkyOceanSpec, "Explored.SkyOcean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSkyOceanSpec)

void FSkyOceanSpec::Define()
{
	Describe("ExploredSky", [this]()
	{
		It("pone el Sol alto a mediodía y bajo el horizonte a medianoche", [this]()
		{
			for (float Day = 0.0f; Day < ExploredSky::DaysPerYear; Day += 4.0f)
			{
				TestTrue(TEXT("Mediodía alto"), ExploredSky::SunDirection(12.0f, Day).Z > 0.75f);
				TestTrue(TEXT("Medianoche bajo"), ExploredSky::SunDirection(0.0f, Day).Z < -0.5f);
			}
		});

		It("hace salir el Sol por el este y ponerse por el oeste", [this]()
		{
			const FVector Morning = ExploredSky::SunDirection(7.0f, 0.0f);
			const FVector Evening = ExploredSky::SunDirection(17.0f, 0.0f);
			TestTrue(TEXT("Mañana al este (+Y)"), Morning.Y > 0.5f);
			TestTrue(TEXT("Tarde al oeste (-Y)"), Evening.Y < -0.5f);
		});

		It("cruza el horizonte cerca de las 6 y las 18 en el equinoccio", [this]()
		{
			TestTrue(TEXT("Amanecer"), FMath::Abs(ExploredSky::SunDirection(6.0f, 0.0f).Z) < 0.05f);
			TestTrue(TEXT("Anochecer"), FMath::Abs(ExploredSky::SunDirection(18.0f, 0.0f).Z) < 0.05f);
		});

		It("devuelve direcciones unitarias", [this]()
		{
			for (float H = 0.0f; H < 24.0f; H += 0.5f)
			{
				TestTrue(TEXT("Unitaria"), FMath::IsNearlyEqual(ExploredSky::SunDirection(H, 10.0f).Size(), 1.0, 1e-4));
			}
		});

		It("recorre las fases lunares y la iluminación", [this]()
		{
			TestTrue(TEXT("Nueva al inicio"), ExploredSky::MoonIllumination(ExploredSky::MoonPhase(0.0f)) < 0.01f);
			TestTrue(TEXT("Llena a mitad de ciclo"),
				ExploredSky::MoonIllumination(ExploredSky::MoonPhase(ExploredSky::DaysPerLunarCycle * 0.5f)) > 0.99f);
			const float Phase = ExploredSky::MoonPhase(123.4f);
			TestTrue(TEXT("Fase en [0, 1)"), Phase >= 0.0f && Phase < 1.0f);
		});
	});

	Describe("FOceanWaves", [this]()
	{
		It("mantiene la altura dentro de la suma de amplitudes", [this]()
		{
			const FOceanWaves Waves = FOceanWaves::Make(0.3f);
			float MaxAmplitude = 0.0f;
			for (const FGerstnerWave& W : Waves.Waves)
			{
				MaxAmplitude += W.Amplitude;
			}
			for (int32 I = 0; I < 200; ++I)
			{
				const float H = Waves.HeightAt(FVector2D(I * 137.0f, I * -71.0f), I * 0.37f);
				if (FMath::Abs(H) > MaxAmplitude + 0.01f)
				{
					AddError(FString::Printf(TEXT("Altura %.2f supera %.2f"), H, MaxAmplitude));
					return;
				}
			}
		});

		It("hace olas más altas con mar más fuerte", [this]()
		{
			const FOceanWaves Calm = FOceanWaves::Make(0.0f);
			const FOceanWaves Rough = FOceanWaves::Make(1.0f);
			TestTrue(TEXT("Más amplitud"), Rough.Waves[0].Amplitude > Calm.Waves[0].Amplitude * 3.0f);
		});

		It("resuelve la altura de forma coherente con el desplazamiento", [this]()
		{
			const FOceanWaves Waves = FOceanWaves::Make(0.5f);
			const FVector2D Rest(1234.0f, -567.0f);
			const float T = 3.21f;
			const FVector D = Waves.Displacement(Rest, T);
			const FVector2D Displaced = Rest + FVector2D(D.X, D.Y);
			TestTrue(TEXT("Altura coherente"), FMath::IsNearlyEqual(Waves.HeightAt(Displaced, T), static_cast<float>(D.Z), 1.0f));
		});
	});
}

#endif
