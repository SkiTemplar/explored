#include "UI/ExploredGameUserSettings.h"

#include "Audio/ExploredAmbienceSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/SlateRenderer.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UI/SettingsLogic.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPath.h"

namespace ExploredGameUserSettingsDetail
{
	/**
	 * SoundClass del proyecto por canal. Las crea Tools/Unreal/import_audio.py
	 * (SC_Master es la madre de las otras cuatro) y asigna cada SoundWave a la
	 * suya según su categoría del manifiesto.
	 */
	const TCHAR* SoundClassPath(EExploredAudioChannel Channel)
	{
		switch (Channel)
		{
		case EExploredAudioChannel::Master: return TEXT("/Game/Audio/Classes/SC_Master.SC_Master");
		case EExploredAudioChannel::Music: return TEXT("/Game/Audio/Classes/SC_Music.SC_Music");
		case EExploredAudioChannel::Effects: return TEXT("/Game/Audio/Classes/SC_Effects.SC_Effects");
		case EExploredAudioChannel::Ambient: return TEXT("/Game/Audio/Classes/SC_Ambient.SC_Ambient");
		case EExploredAudioChannel::Interface: return TEXT("/Game/Audio/Classes/SC_Interface.SC_Interface");
		default: return nullptr;
		}
	}

	constexpr EExploredAudioChannel AllChannels[] = {
		EExploredAudioChannel::Master, EExploredAudioChannel::Music, EExploredAudioChannel::Effects,
		EExploredAudioChannel::Ambient, EExploredAudioChannel::Interface
	};

	EColorVisionDeficiency ToEngineDeficiency(EExploredColorblindMode Mode)
	{
		switch (Mode)
		{
		case EExploredColorblindMode::Protanopia: return EColorVisionDeficiency::Protanope;
		case EExploredColorblindMode::Deuteranopia: return EColorVisionDeficiency::Deuteranope;
		case EExploredColorblindMode::Tritanopia: return EColorVisionDeficiency::Tritanope;
		default: return EColorVisionDeficiency::NormalVision;
		}
	}

	/** Número de valores de un UENUM sin contar el _MAX que añade UHT. */
	template <typename TEnum>
	int32 NumEnumValues()
	{
		const UEnum* Enum = StaticEnum<TEnum>();
		return Enum ? FMath::Max(Enum->NumEnums() - 1, 1) : 1;
	}

	template <typename TEnum>
	TEnum SanitizeEnum(TEnum Value, TEnum Default)
	{
		return static_cast<TEnum>(ExploredSettingsLogic::SanitizeEnumIndex(
			static_cast<int32>(Value), NumEnumValues<TEnum>(), static_cast<int32>(Default)));
	}
}

UExploredGameUserSettings::UExploredGameUserSettings()
{
}

UExploredGameUserSettings* UExploredGameUserSettings::Get()
{
	return Cast<UExploredGameUserSettings>(GEngine ? GEngine->GetGameUserSettings() : nullptr);
}

void UExploredGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	// SetToDefaults() solo corre cuando el jugador pulsa «restaurar valores predeterminados» en el
	// menú de Opciones (el arranque en frío nunca la llama: LoadSettings() lee Saved/Config o, si no
	// existe, cae directo en el nivel que ya trae compilado UGameUserSettings — Épico/3 — sin pasar
	// por aquí). El default de fábrica real está en Config/DefaultGameUserSettings.ini
	// ([ScalabilityGroups] a Alto/2); este SetOverallScalabilityLevel solo mantiene «restaurar
	// predeterminados» consistente con esa misma elección en vez de devolver al jugador a Épico.
	SetOverallScalabilityLevel(2);

	using namespace ExploredSettingsLogic;
	Brightness = BrightnessRange.Default;
	VolumeMaster = VolumeMusic = VolumeEffects = VolumeAmbient = VolumeInterface = VolumeRange.Default;
	FieldOfView = FieldOfViewRange.Default;
	MouseSensitivity = GamepadSensitivity = SensitivityRange.Default;
	bInvertY = false;
	bCameraBobEnabled = true;
	bHoldToCrouch = true;
	DayLengthMinutes = DefaultDayLengthMinutes;
	bSubtitlesEnabled = true;
	TextSize = EExploredTextSize::Medium;
	Language = EExploredLanguage::Spanish;
	ColorblindMode = EExploredColorblindMode::None;
	bReduceMotion = false;
	bDisableFlashing = false;
	bSoundVisualCues = false;
}

void UExploredGameUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);
	// El ini lo puede editar el jugador a mano: VolumeMaster=500 o DayLengthMinutes=45
	// no deben llegar al juego (M14).
	SanitizeCustomSettings();
}

void UExploredGameUserSettings::ValidateSettings()
{
	Super::ValidateSettings();
	SanitizeCustomSettings();
}

void UExploredGameUserSettings::SanitizeCustomSettings()
{
	using namespace ExploredSettingsLogic;
	using ExploredGameUserSettingsDetail::SanitizeEnum;

	Brightness = ClampToRange(Brightness, BrightnessRange);
	VolumeMaster = ClampToRange(VolumeMaster, VolumeRange);
	VolumeMusic = ClampToRange(VolumeMusic, VolumeRange);
	VolumeEffects = ClampToRange(VolumeEffects, VolumeRange);
	VolumeAmbient = ClampToRange(VolumeAmbient, VolumeRange);
	VolumeInterface = ClampToRange(VolumeInterface, VolumeRange);
	FieldOfView = ClampToRange(FieldOfView, FieldOfViewRange);
	MouseSensitivity = ClampToRange(MouseSensitivity, SensitivityRange);
	GamepadSensitivity = ClampToRange(GamepadSensitivity, SensitivityRange);
	DayLengthMinutes = SnapDayLength(DayLengthMinutes);
	TextSize = SanitizeEnum(TextSize, EExploredTextSize::Medium);
	Language = SanitizeEnum(Language, EExploredLanguage::Spanish);
	ColorblindMode = SanitizeEnum(ColorblindMode, EExploredColorblindMode::None);
}

void UExploredGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	SanitizeCustomSettings();

	// En el editor no se toca la cultura al cargar: cambiaría el idioma de toda
	// la interfaz del editor solo por abrir el proyecto. En PIE el selector de
	// idioma sigue aplicándolo al instante (SetLanguage).
	ApplyPreviewSettings(!GIsEditor);

	// Al arrancar el motor todavía no hay mundo: entonces lo aplica
	// AExploredPlayerController::BeginPlay con ApplyToWorld().
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld())
			{
				ApplyToWorld(World);
			}
		}
	}

	OnSettingsApplied.Broadcast();
}

void UExploredGameUserSettings::ApplyPreviewSettings(bool bIncludeLanguage)
{
	ApplyBrightness();
	ApplyColorblindMode();
	if (bIncludeLanguage)
	{
		ApplyLanguage();
	}
}

void UExploredGameUserSettings::ApplyToWorld(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	ApplyAudioSettings(World);

	if (UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Time->SetDayLengthMinutes(DayLengthMinutes);
	}
	if (UExploredAmbienceSubsystem* Ambience = World->GetSubsystem<UExploredAmbienceSubsystem>())
	{
		Ambience->SetAmbienceVolume(GetAmbienceLayerGain());
	}
}

float UExploredGameUserSettings::GetVolume(EExploredAudioChannel Channel) const
{
	switch (Channel)
	{
	case EExploredAudioChannel::Master: return VolumeMaster;
	case EExploredAudioChannel::Music: return VolumeMusic;
	case EExploredAudioChannel::Effects: return VolumeEffects;
	case EExploredAudioChannel::Ambient: return VolumeAmbient;
	case EExploredAudioChannel::Interface: return VolumeInterface;
	default: return ExploredSettingsLogic::VolumeRange.Default;
	}
}

void UExploredGameUserSettings::SetVolume(EExploredAudioChannel Channel, float Volume0To100)
{
	const float Clamped = ExploredSettingsLogic::ClampToRange(Volume0To100, ExploredSettingsLogic::VolumeRange);
	switch (Channel)
	{
	case EExploredAudioChannel::Master: VolumeMaster = Clamped; break;
	case EExploredAudioChannel::Music: VolumeMusic = Clamped; break;
	case EExploredAudioChannel::Effects: VolumeEffects = Clamped; break;
	case EExploredAudioChannel::Ambient: VolumeAmbient = Clamped; break;
	case EExploredAudioChannel::Interface: VolumeInterface = Clamped; break;
	default: break;
	}
}

USoundClass* UExploredGameUserSettings::GetSoundClass(EExploredAudioChannel Channel)
{
	TObjectPtr<USoundClass>* Slot = nullptr;
	switch (Channel)
	{
	case EExploredAudioChannel::Master: Slot = &MasterSoundClass; break;
	case EExploredAudioChannel::Music: Slot = &MusicSoundClass; break;
	case EExploredAudioChannel::Effects: Slot = &EffectsSoundClass; break;
	case EExploredAudioChannel::Ambient: Slot = &AmbientSoundClass; break;
	case EExploredAudioChannel::Interface: Slot = &InterfaceSoundClass; break;
	default: return nullptr;
	}
	// Se reintenta mientras no exista: el asset puede crearse con el editor abierto
	// (import_audio.py) y Aplicar es poco frecuente.
	if (!*Slot)
	{
		if (const TCHAR* Path = ExploredGameUserSettingsDetail::SoundClassPath(Channel))
		{
			*Slot = LoadObject<USoundClass>(nullptr, Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
		}
	}
	return *Slot;
}

USoundClass* UExploredGameUserSettings::GetEngineDefaultSoundClass() const
{
	const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
	return AudioSettings ? Cast<USoundClass>(AudioSettings->DefaultSoundClassName.TryLoad()) : nullptr;
}

float UExploredGameUserSettings::GetAmbienceLayerGain()
{
	return ExploredSettingsLogic::AmbienceLayerGain(VolumeAmbient, GetSoundClass(EExploredAudioChannel::Ambient) != nullptr);
}

void UExploredGameUserSettings::ApplyAudioSettings(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	if (!SoundMix)
	{
		SoundMix = NewObject<USoundMix>(this, TEXT("Explored_SettingsMix"));
		// Sin fundidos: el cambio de volumen se oye en cuanto se pulsa Aplicar.
		SoundMix->FadeInTime = 0.0f;
		SoundMix->FadeOutTime = 0.0f;
	}

	// Antes se hacía PushSoundMixModifier en cada Aplicar sin su Pop, y el
	// contador de referencias de la mezcla crecía sin fin. La mezcla base es
	// única por dispositivo de audio y SetBaseSoundMix es idempotente, así que
	// se puede llamar en cada Aplicar y en cada BeginPlay sin acumular nada.
	// (El proyecto no define DefaultBaseSoundMix en AudioSettings; si algún día
	// lo hace, esa mezcla quedaría sustituida por esta.)
	UGameplayStatics::SetBaseSoundMix(World, SoundMix);

	for (const EExploredAudioChannel Channel : ExploredGameUserSettingsDetail::AllChannels)
	{
		USoundClass* SoundClass = GetSoundClass(Channel);
		if (!SoundClass && Channel == EExploredAudioChannel::Master)
		{
			SoundClass = GetEngineDefaultSoundClass();
		}
		if (SoundClass)
		{
			// bApplyToChildren: el maestro multiplica a sus hijas (música, efectos...).
			UGameplayStatics::SetSoundMixClassOverride(World, SoundMix, SoundClass,
				ExploredSettingsLogic::VolumeToGain(GetVolume(Channel)), 1.0f, 0.0f, true);
		}
	}
}

void UExploredGameUserSettings::SetBrightness(float NewBrightness)
{
	Brightness = ExploredSettingsLogic::ClampToRange(NewBrightness, ExploredSettingsLogic::BrightnessRange);
	ApplyBrightness();
}

void UExploredGameUserSettings::ApplyBrightness() const
{
	// M16: antes se escribía r.Gamma. Esa variable es, hasta donde sabemos, una
	// gamma ADICIONAL del tonemapper con valor por defecto 1,0, así que escribir
	// 2,2 lavaba la imagen en cuanto se tocaba el slider; como su semántica no
	// está clara, se deja de tocar. En su lugar se usa GEngine->DisplayGamma:
	// la gamma de pantalla que devuelve FViewport::GetDisplayGamma() al
	// renderer, 2,2 por defecto en BaseEngine.ini y la misma que cambia el
	// comando de consola «gamma». El tonemapper aplica 2,2 / DisplayGamma como
	// exponente, así que valores mayores aclaran y 2,2 deja la imagen como está.
	// El rango [1,7; 2,7] deja el valor por defecto en el centro del slider.
	// Verificar en local que el brillo cambia de forma suave en ambos sentidos.
	if (GEngine)
	{
		GEngine->DisplayGamma = Brightness;
	}
}

void UExploredGameUserSettings::SetFOV(float NewFOV)
{
	FieldOfView = ExploredSettingsLogic::ClampToRange(NewFOV, ExploredSettingsLogic::FieldOfViewRange);
}

void UExploredGameUserSettings::SetMouseSensitivity(float NewSensitivity)
{
	MouseSensitivity = ExploredSettingsLogic::ClampToRange(NewSensitivity, ExploredSettingsLogic::SensitivityRange);
}

void UExploredGameUserSettings::SetGamepadSensitivity(float NewSensitivity)
{
	GamepadSensitivity = ExploredSettingsLogic::ClampToRange(NewSensitivity, ExploredSettingsLogic::SensitivityRange);
}

void UExploredGameUserSettings::SetDayLengthMinutes(float Minutes)
{
	DayLengthMinutes = ExploredSettingsLogic::SnapDayLength(Minutes);
}

const TArray<float>& UExploredGameUserSettings::GetSupportedDayLengths()
{
	return ExploredSettingsLogic::GetSupportedDayLengths();
}

void UExploredGameUserSettings::SetLanguage(EExploredLanguage NewLanguage)
{
	Language = NewLanguage;
	ApplyLanguage();
}

void UExploredGameUserSettings::ApplyLanguage() const
{
	const FString Culture = (Language == EExploredLanguage::English) ? TEXT("en") : TEXT("es");
	FInternationalization& I18n = FInternationalization::Get();
	// Cambiar de cultura reconstruye todos los textos: solo si de verdad cambia.
	if (I18n.GetCurrentCulture()->GetName() != Culture)
	{
		I18n.SetCurrentCulture(Culture);
	}
}

void UExploredGameUserSettings::SetColorblindMode(EExploredColorblindMode NewMode)
{
	ColorblindMode = NewMode;
	ApplyColorblindMode();
}

void UExploredGameUserSettings::ApplyColorblindMode() const
{
	// FSlateApplication::GetRenderer() devuelve un FSlateRenderer* crudo (no un
	// TSharedPtr), de ahí el chequeo por nulidad en vez de IsValid().
	if (FSlateApplication::IsInitialized())
	{
		if (FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer())
		{
			Renderer->SetColorVisionDeficiencyType(ExploredGameUserSettingsDetail::ToEngineDeficiency(ColorblindMode), 10, true, false);
		}
	}
}
