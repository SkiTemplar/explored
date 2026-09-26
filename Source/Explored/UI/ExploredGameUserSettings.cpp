#include "UI/ExploredGameUserSettings.h"

#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/SlateRenderer.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
	/** Duraciones de día soportadas en Ajustes > Juego (GDD §10). */
	const TArray<float> GDayLengths = { 20.0f, 40.0f, 60.0f, 90.0f };

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

	Brightness = 2.2f;
	VolumeMaster = VolumeMusic = VolumeEffects = VolumeAmbient = VolumeInterface = 100.0f;
	FieldOfView = 90.0f;
	MouseSensitivity = GamepadSensitivity = 1.0f;
	bInvertY = false;
	bCameraBobEnabled = true;
	bHoldToCrouch = true;
	DayLengthMinutes = 40.0f;
	bSubtitlesEnabled = true;
	TextSize = EExploredTextSize::Medium;
	Language = EExploredLanguage::Spanish;
	ColorblindMode = EExploredColorblindMode::None;
	bReduceMotion = false;
	bDisableFlashing = false;
	bSoundVisualCues = false;
}

const TArray<float>& UExploredGameUserSettings::GetSupportedDayLengths()
{
	return GDayLengths;
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
	default: return 100.0f;
	}
}

void UExploredGameUserSettings::SetVolume(EExploredAudioChannel Channel, float Volume0To100)
{
	const float Clamped = FMath::Clamp(Volume0To100, 0.0f, 100.0f);
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

USoundClass* UExploredGameUserSettings::GetOrCreateSoundClass(EExploredAudioChannel Channel)
{
	TObjectPtr<USoundClass>* Slot = nullptr;
	FName Name;
	switch (Channel)
	{
	case EExploredAudioChannel::Master: Slot = &MasterSoundClass; Name = TEXT("Explored_Master"); break;
	case EExploredAudioChannel::Music: Slot = &MusicSoundClass; Name = TEXT("Explored_Music"); break;
	case EExploredAudioChannel::Effects: Slot = &EffectsSoundClass; Name = TEXT("Explored_Effects"); break;
	case EExploredAudioChannel::Ambient: Slot = &AmbientSoundClass; Name = TEXT("Explored_Ambient"); break;
	case EExploredAudioChannel::Interface: Slot = &InterfaceSoundClass; Name = TEXT("Explored_Interface"); break;
	default: return nullptr;
	}
	if (!*Slot)
	{
		*Slot = NewObject<USoundClass>(GetTransientPackage(), Name);
	}
	return *Slot;
}

void UExploredGameUserSettings::ApplyAudioSettings(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return;
	}

	if (!SoundMix)
	{
		SoundMix = NewObject<USoundMix>(GetTransientPackage(), TEXT("Explored_SoundMix"));
	}
	UGameplayStatics::PushSoundMixModifier(WorldContextObject, SoundMix);

	static const EExploredAudioChannel Channels[] = {
		EExploredAudioChannel::Master, EExploredAudioChannel::Music, EExploredAudioChannel::Effects,
		EExploredAudioChannel::Ambient, EExploredAudioChannel::Interface
	};
	for (const EExploredAudioChannel Channel : Channels)
	{
		if (USoundClass* SoundClass = GetOrCreateSoundClass(Channel))
		{
			UGameplayStatics::SetSoundMixClassOverride(
				WorldContextObject, SoundMix, SoundClass, GetVolume(Channel) / 100.0f, 1.0f, 0.0f, true);
		}
	}
}

void UExploredGameUserSettings::SetBrightness(float NewBrightness)
{
	Brightness = FMath::Clamp(NewBrightness, 1.7f, 2.7f);
	if (IConsoleVariable* Gamma = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Gamma")))
	{
		Gamma->Set(Brightness);
	}
}

void UExploredGameUserSettings::SetFOV(float NewFOV)
{
	FieldOfView = FMath::Clamp(NewFOV, 70.0f, 110.0f);
}

void UExploredGameUserSettings::SetMouseSensitivity(float NewSensitivity)
{
	MouseSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 5.0f);
}

void UExploredGameUserSettings::SetGamepadSensitivity(float NewSensitivity)
{
	GamepadSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 5.0f);
}

void UExploredGameUserSettings::SetDayLengthMinutes(float Minutes)
{
	float Best = GDayLengths[0];
	float BestDist = FMath::Abs(Minutes - Best);
	for (const float Candidate : GDayLengths)
	{
		const float Dist = FMath::Abs(Minutes - Candidate);
		if (Dist < BestDist)
		{
			Best = Candidate;
			BestDist = Dist;
		}
	}
	DayLengthMinutes = Best;
}

void UExploredGameUserSettings::SetLanguage(EExploredLanguage NewLanguage)
{
	Language = NewLanguage;
	const FString Culture = (Language == EExploredLanguage::English) ? TEXT("en") : TEXT("es");
	FInternationalization::Get().SetCurrentCulture(Culture);
}

void UExploredGameUserSettings::SetColorblindMode(EExploredColorblindMode NewMode)
{
	ColorblindMode = NewMode;
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer().IsValid())
	{
		FSlateApplication::Get().GetRenderer()->SetColorVisionDeficiencyType(
			ToEngineDeficiency(ColorblindMode), 10, true, false);
	}
}
