#include "Misc/AutomationTest.h"

#include "Audio/ExploredAmbienceSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FAmbienceSpec, "Explored.Audio.Ambience",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	float Volumes[static_cast<int32>(EAmbienceLayer::Count)];
	float Get(EAmbienceLayer Layer) const { return Volumes[static_cast<int32>(Layer)]; }
END_DEFINE_SPEC(FAmbienceSpec)

void FAmbienceSpec::Define()
{
	It("suena el mar en la playa y la selva de día tierra adentro", [this]()
	{
		FAmbienceEnvironment Beach;
		Beach.Coast = 1.0f;
		FAmbienceMixer::Mix(Beach, Volumes);
		TestTrue(TEXT("Olas en la playa"), Get(EAmbienceLayer::OceanCalm) > 0.8f);

		FAmbienceEnvironment Jungle;
		Jungle.Vegetation = 1.0f;
		FAmbienceMixer::Mix(Jungle, Volumes);
		TestTrue(TEXT("Selva de día"), Get(EAmbienceLayer::JungleDay) > 0.8f);
		TestTrue(TEXT("Sin selva nocturna de día"), Get(EAmbienceLayer::JungleNight) < 0.05f);
		TestTrue(TEXT("Sin olas tierra adentro"), Get(EAmbienceLayer::OceanCalm) < 0.05f);
	});

	It("cambia a los sonidos nocturnos de noche", [this]()
	{
		FAmbienceEnvironment Env;
		Env.Vegetation = 1.0f;
		Env.Night = 1.0f;
		FAmbienceMixer::Mix(Env, Volumes);
		TestTrue(TEXT("Noche"), Get(EAmbienceLayer::JungleNight) > 0.8f);
		TestTrue(TEXT("Día apagado"), Get(EAmbienceLayer::JungleDay) < 0.05f);
	});

	It("silencia el exterior bajo el agua", [this]()
	{
		FAmbienceEnvironment Env;
		Env.Coast = 1.0f;
		Env.Vegetation = 1.0f;
		Env.Underwater = 1.0f;
		FAmbienceMixer::Mix(Env, Volumes);
		TestTrue(TEXT("Capa submarina"), Get(EAmbienceLayer::Underwater) > 0.9f);
		TestTrue(TEXT("Sin olas"), Get(EAmbienceLayer::OceanCalm) < 0.01f);
		TestTrue(TEXT("Sin selva"), Get(EAmbienceLayer::JungleDay) < 0.01f);
	});

	It("pasa de lluvia ligera a fuerte y del mar en calma al temporal", [this]()
	{
		FAmbienceEnvironment Env;
		Env.Coast = 1.0f;
		Env.Rain = 0.2f;
		FAmbienceMixer::Mix(Env, Volumes);
		TestTrue(TEXT("Lluvia ligera"), Get(EAmbienceLayer::RainLight) > Get(EAmbienceLayer::RainHeavy));
		Env.Rain = 1.0f;
		Env.SeaState = 1.0f;
		FAmbienceMixer::Mix(Env, Volumes);
		TestTrue(TEXT("Lluvia fuerte"), Get(EAmbienceLayer::RainHeavy) > Get(EAmbienceLayer::RainLight));
		TestTrue(TEXT("Mar agitado"), Get(EAmbienceLayer::OceanRough) > Get(EAmbienceLayer::OceanCalm));
	});

	It("mantiene todos los volúmenes en [0, 1]", [this]()
	{
		for (int32 I = 0; I < 200; ++I)
		{
			FAmbienceEnvironment Env;
			Env.Coast = FMath::FRand();
			Env.Altitude = FMath::FRand();
			Env.Vegetation = FMath::FRand();
			Env.Night = FMath::FRand();
			Env.Rain = FMath::FRand();
			Env.SeaState = FMath::FRand();
			FAmbienceMixer::Mix(Env, Volumes);
			for (const float V : Volumes)
			{
				if (V < 0.0f || V > 1.0f)
				{
					AddError(FString::Printf(TEXT("Volumen fuera de rango: %f"), V));
					return;
				}
			}
		}
	});
}

#endif
