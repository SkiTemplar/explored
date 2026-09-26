#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"

#include "ExploredGameUserSettings.generated.h"

/** Canales de volumen que expone el menú de Audio (GDD §10). */
UENUM(BlueprintType)
enum class EExploredAudioChannel : uint8
{
	Master,
	Music,
	Effects,
	Ambient,
	Interface
};

/** Tamaño de texto de la interfaz. */
UENUM(BlueprintType)
enum class EExploredTextSize : uint8
{
	Small,
	Medium,
	Large
};

/** Idioma de la interfaz y los textos del diario. */
UENUM(BlueprintType)
enum class EExploredLanguage : uint8
{
	Spanish,
	English
};

/** Modo de daltonismo, en paralelo a EColorVisionDeficiency del renderer de Slate. */
UENUM(BlueprintType)
enum class EExploredColorblindMode : uint8
{
	None,
	Protanopia,
	Deuteranopia,
	Tritanopia
};

/**
 * Ajustes persistentes del juego (Config/GameUserSettings.ini). Extiende
 * UGameUserSettings: la parte de Gráficos (resolución, modo de ventana,
 * VSync, límite de FPS, calidad general y por categoría, escala de
 * resolución) ya la resuelve la clase base; aquí solo se añaden Audio,
 * Controles, Juego y Accesibilidad, según GDD §10.
 *
 * Registrada como GameUserSettingsClassName en Config/DefaultEngine.ini.
 * Acceso: Cast<UExploredGameUserSettings>(GEngine->GetGameUserSettings()).
 *
 * Cómo se aplican (H7):
 * - ApplyNonResolutionSettings() (la llama ApplySettings() y el motor al
 *   arrancar, tras LoadSettings()) re-aplica brillo, idioma y daltonismo,
 *   empuja el audio y la duración del día a los mundos de juego abiertos y
 *   emite OnSettingsApplied, al que se suscribe AExploredCharacter para FOV,
 *   sensibilidad, invertir Y, balanceo de cámara y agacharse.
 * - ApplyToWorld() aplica lo que depende de un mundo (mezcla de audio,
 *   UTimeOfDaySubsystem::SetDayLengthMinutes y volumen del ambiente). La
 *   llama también AExploredPlayerController::BeginPlay, porque al arrancar el
 *   motor todavía no hay mundo.
 * - Audio: una única USoundMix persistente fijada como mezcla base (idempotente,
 *   sin Push/Pop) con un ajuste por canal sobre las SoundClass del proyecto
 *   (/Game/Audio/Classes/SC_*, las crea Tools/Unreal/import_audio.py). Si
 *   SC_Master no existe todavía, el volumen maestro se aplica a la SoundClass
 *   por defecto del motor para que al menos ese control funcione.
 * - Validación (M14): LoadSettings() y ValidateSettings() recortan cada campo
 *   con ExploredSettingsLogic (UI/SettingsLogic.h).
 */
UCLASS(BlueprintType)
class EXPLORED_API UExploredGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UExploredGameUserSettings();

	static UExploredGameUserSettings* Get();

	virtual void SetToDefaults() override;
	virtual void LoadSettings(bool bForceReload = false) override;
	virtual void ValidateSettings() override;
	virtual void ApplyNonResolutionSettings() override;

	/**
	 * Re-aplica al instante los ajustes que el jugador ve mientras edita el
	 * panel: brillo, daltonismo y (si bIncludeLanguage) idioma. Lo usan
	 * «Restaurar valores por defecto» y «Volver» sin aplicar (M11).
	 */
	void ApplyPreviewSettings(bool bIncludeLanguage = true);

	/** Aplica lo que depende de un mundo: audio, duración del día y volumen del ambiente. */
	void ApplyToWorld(const UObject* WorldContextObject);

	/** SoundClass del proyecto para un canal (/Game/Audio/Classes/SC_*), o nullptr si aún no existe. */
	class USoundClass* GetSoundClass(EExploredAudioChannel Channel);

	/** Multiplicador que el subsistema de ambiente aplica a sus capas (ver ExploredSettingsLogic::AmbienceLayerGain). */
	float GetAmbienceLayerGain();

	/** Se emite al final de cada ApplyNonResolutionSettings() (Aplicar, descartar y arranque). */
	FSimpleMulticastDelegate OnSettingsApplied;

	// --- Gráficos (brillo; el resto ya lo cubre UGameUserSettings) ---
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Gráficos")
	float GetBrightness() const { return Brightness; }
	/** Gamma de pantalla (GEngine->DisplayGamma, ver M16); se aplica al instante para que el jugador vea el resultado mientras arrastra. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Gráficos")
	void SetBrightness(float NewBrightness);

	// --- Audio ---
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Audio")
	float GetVolume(EExploredAudioChannel Channel) const;
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Audio")
	void SetVolume(EExploredAudioChannel Channel, float Volume0To100);
	/** Fija la mezcla de ajustes como mezcla base y aplica el volumen de cada canal a su SoundClass. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Audio")
	void ApplyAudioSettings(const UObject* WorldContextObject);

	// --- Controles ---
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	float GetFOV() const { return FieldOfView; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	void SetFOV(float NewFOV);

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	float GetMouseSensitivity() const { return MouseSensitivity; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	void SetMouseSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	float GetGamepadSensitivity() const { return GamepadSensitivity; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	void SetGamepadSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	bool GetInvertY() const { return bInvertY; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	void SetInvertY(bool bNewInvertY) { bInvertY = bNewInvertY; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	bool GetCameraBobEnabled() const { return bCameraBobEnabled; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	void SetCameraBobEnabled(bool bEnabled) { bCameraBobEnabled = bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	bool GetHoldToCrouch() const { return bHoldToCrouch; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	void SetHoldToCrouch(bool bHold) { bHoldToCrouch = bHold; }

	// --- Juego ---
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Juego")
	float GetDayLengthMinutes() const { return DayLengthMinutes; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Juego")
	void SetDayLengthMinutes(float Minutes);

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Juego")
	bool GetSubtitlesEnabled() const { return bSubtitlesEnabled; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Juego")
	void SetSubtitlesEnabled(bool bEnabled) { bSubtitlesEnabled = bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Juego")
	EExploredTextSize GetTextSize() const { return TextSize; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Juego")
	void SetTextSize(EExploredTextSize NewSize) { TextSize = NewSize; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Juego")
	EExploredLanguage GetLanguage() const { return Language; }
	/** Cambia el idioma y aplica la cultura activa (FInternationalization). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Juego")
	void SetLanguage(EExploredLanguage NewLanguage);

	// --- Accesibilidad ---
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Accesibilidad")
	EExploredColorblindMode GetColorblindMode() const { return ColorblindMode; }
	/** Cambia el modo y lo aplica al renderer de Slate (afecta a toda la pantalla). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Accesibilidad")
	void SetColorblindMode(EExploredColorblindMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Accesibilidad")
	bool GetReduceMotion() const { return bReduceMotion; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Accesibilidad")
	void SetReduceMotion(bool bEnabled) { bReduceMotion = bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Accesibilidad")
	bool GetDisableFlashing() const { return bDisableFlashing; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Accesibilidad")
	void SetDisableFlashing(bool bEnabled) { bDisableFlashing = bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Accesibilidad")
	bool GetSoundVisualCues() const { return bSoundVisualCues; }
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Accesibilidad")
	void SetSoundVisualCues(bool bEnabled) { bSoundVisualCues = bEnabled; }

	/** Días de juego soportados en el desplegable de Ajustes > Juego. */
	static const TArray<float>& GetSupportedDayLengths();

private:
	/** Recorta cada campo propio a su rango (M14). */
	void SanitizeCustomSettings();
	void ApplyBrightness() const;
	void ApplyLanguage() const;
	void ApplyColorblindMode() const;
	/** SoundClass por defecto del motor (UAudioSettings), para el volumen maestro si SC_Master no existe. */
	class USoundClass* GetEngineDefaultSoundClass() const;

	UPROPERTY(Config)
	float Brightness = 2.2f;

	UPROPERTY(Config)
	float VolumeMaster = 100.0f;
	UPROPERTY(Config)
	float VolumeMusic = 100.0f;
	UPROPERTY(Config)
	float VolumeEffects = 100.0f;
	UPROPERTY(Config)
	float VolumeAmbient = 100.0f;
	UPROPERTY(Config)
	float VolumeInterface = 100.0f;

	UPROPERTY(Config)
	float FieldOfView = 90.0f;
	UPROPERTY(Config)
	float MouseSensitivity = 1.0f;
	UPROPERTY(Config)
	float GamepadSensitivity = 1.0f;
	UPROPERTY(Config)
	bool bInvertY = false;
	UPROPERTY(Config)
	bool bCameraBobEnabled = true;
	UPROPERTY(Config)
	bool bHoldToCrouch = true;

	UPROPERTY(Config)
	float DayLengthMinutes = 40.0f;
	UPROPERTY(Config)
	bool bSubtitlesEnabled = true;
	UPROPERTY(Config)
	EExploredTextSize TextSize = EExploredTextSize::Medium;
	UPROPERTY(Config)
	EExploredLanguage Language = EExploredLanguage::Spanish;

	UPROPERTY(Config)
	EExploredColorblindMode ColorblindMode = EExploredColorblindMode::None;
	UPROPERTY(Config)
	bool bReduceMotion = false;
	UPROPERTY(Config)
	bool bDisableFlashing = false;
	UPROPERTY(Config)
	bool bSoundVisualCues = false;

	UPROPERTY(Transient)
	TObjectPtr<class USoundClass> MasterSoundClass;
	UPROPERTY(Transient)
	TObjectPtr<class USoundClass> MusicSoundClass;
	UPROPERTY(Transient)
	TObjectPtr<class USoundClass> EffectsSoundClass;
	UPROPERTY(Transient)
	TObjectPtr<class USoundClass> AmbientSoundClass;
	UPROPERTY(Transient)
	TObjectPtr<class USoundClass> InterfaceSoundClass;
	/** Mezcla de ajustes: se crea una vez y se fija como mezcla base en cada ApplyAudioSettings(). */
	UPROPERTY(Transient)
	TObjectPtr<class USoundMix> SoundMix;
};
