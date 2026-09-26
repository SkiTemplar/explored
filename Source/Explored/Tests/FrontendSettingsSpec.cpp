#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UI/ExploredGameUserSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FExploredFrontendSettingsSpec, "Explored.Frontend.Settings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FExploredFrontendSettingsSpec)

void FExploredFrontendSettingsSpec::Define()
{
	// NewObject copia el CDO, que ya leyó el GameUserSettings.ini real del
	// usuario: cada caso parte de SetToDefaults() para no depender de ese ini.
	// (No se llama a ValidateSettings: la versión base puede borrar el ini real.)
	Describe("UExploredGameUserSettings", [this]()
	{
		It("tiene valores por defecto válidos", [this]()
		{
			UExploredGameUserSettings* Settings = NewObject<UExploredGameUserSettings>();
			Settings->SetToDefaults();
			TestTrue(TEXT("FOV por defecto en rango"), Settings->GetFOV() >= 70.0f && Settings->GetFOV() <= 110.0f);
			TestTrue(TEXT("Volumen maestro por defecto en rango"),
				Settings->GetVolume(EExploredAudioChannel::Master) >= 0.0f && Settings->GetVolume(EExploredAudioChannel::Master) <= 100.0f);
			TestTrue(TEXT("Duración de día por defecto soportada"),
				UExploredGameUserSettings::GetSupportedDayLengths().Contains(Settings->GetDayLengthMinutes()));
		});

		It("respeta el límite de FOV [70, 110]", [this]()
		{
			UExploredGameUserSettings* Settings = NewObject<UExploredGameUserSettings>();
			Settings->SetToDefaults();
			Settings->SetFOV(10.0f);
			TestEqual(TEXT("Se recorta al mínimo"), Settings->GetFOV(), 70.0f);
			Settings->SetFOV(500.0f);
			TestEqual(TEXT("Se recorta al máximo"), Settings->GetFOV(), 110.0f);
			Settings->SetFOV(95.0f);
			TestEqual(TEXT("Valor dentro de rango se conserva"), Settings->GetFOV(), 95.0f);
		});

		It("respeta el límite de volumen [0, 100] en todos los canales", [this]()
		{
			UExploredGameUserSettings* Settings = NewObject<UExploredGameUserSettings>();
			Settings->SetToDefaults();
			const EExploredAudioChannel Channels[] = {
				EExploredAudioChannel::Master, EExploredAudioChannel::Music, EExploredAudioChannel::Effects,
				EExploredAudioChannel::Ambient, EExploredAudioChannel::Interface
			};
			for (const EExploredAudioChannel Channel : Channels)
			{
				Settings->SetVolume(Channel, -50.0f);
				TestEqual(TEXT("Se recorta a 0"), Settings->GetVolume(Channel), 0.0f);
				Settings->SetVolume(Channel, 250.0f);
				TestEqual(TEXT("Se recorta a 100"), Settings->GetVolume(Channel), 100.0f);
			}
		});

		It("el brillo se recorta a [1,7; 2,7] y su valor por defecto es 2,2", [this]()
		{
			UExploredGameUserSettings* Settings = NewObject<UExploredGameUserSettings>();
			Settings->SetToDefaults();
			TestEqual(TEXT("Por defecto"), Settings->GetBrightness(), 2.2f);
			const float Previous = GEngine ? GEngine->DisplayGamma : 2.2f;
			Settings->SetBrightness(9.0f);
			TestEqual(TEXT("Se recorta al máximo"), Settings->GetBrightness(), 2.7f);
			if (GEngine)
			{
				TestEqual(TEXT("Se aplica a DisplayGamma (M16)"), GEngine->DisplayGamma, 2.7f);
				GEngine->DisplayGamma = Previous;
			}
		});

		It("SetDayLengthMinutes ajusta a la duración soportada más cercana", [this]()
		{
			UExploredGameUserSettings* Settings = NewObject<UExploredGameUserSettings>();
			Settings->SetToDefaults();
			Settings->SetDayLengthMinutes(45.0f); // más cerca de 40 que de 60
			TestEqual(TEXT("Redondea a 40"), Settings->GetDayLengthMinutes(), 40.0f);
			Settings->SetDayLengthMinutes(85.0f); // más cerca de 90
			TestEqual(TEXT("Redondea a 90"), Settings->GetDayLengthMinutes(), 90.0f);
		});

		It("SetToDefaults restaura todos los valores modificados", [this]()
		{
			UExploredGameUserSettings* Settings = NewObject<UExploredGameUserSettings>();
			Settings->SetToDefaults();
			Settings->SetFOV(70.0f);
			Settings->SetVolume(EExploredAudioChannel::Music, 0.0f);
			Settings->SetToDefaults();
			TestEqual(TEXT("FOV vuelve a 90"), Settings->GetFOV(), 90.0f);
			TestEqual(TEXT("Volumen de música vuelve a 100"), Settings->GetVolume(EExploredAudioChannel::Music), 100.0f);
		});

		It("aplicar y guardar persiste los valores (ida y vuelta por config)", [this]()
		{
			// Usa un .ini de prueba propio para no tocar el GameUserSettings.ini real.
			const FString TestIni = FPaths::ProjectSavedDir() / TEXT("Automation/ExploredFrontendSettingsSpec.ini");
			IFileManager::Get().Delete(*TestIni);

			UExploredGameUserSettings* Written = NewObject<UExploredGameUserSettings>();
			Written->SetToDefaults();
			Written->SetFOV(101.0f);
			Written->SetVolume(EExploredAudioChannel::Ambient, 42.0f);
			Written->SaveConfig(CPF_Config, *TestIni);

			UExploredGameUserSettings* Loaded = NewObject<UExploredGameUserSettings>();
			Loaded->SetToDefaults();
			Loaded->LoadConfig(UExploredGameUserSettings::StaticClass(), *TestIni);

			TestEqual(TEXT("El FOV persiste"), Loaded->GetFOV(), 101.0f);
			TestEqual(TEXT("El volumen de ambiente persiste"), Loaded->GetVolume(EExploredAudioChannel::Ambient), 42.0f);

			IFileManager::Get().Delete(*TestIni);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
