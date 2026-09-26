#pragma once

#include "CoreMinimal.h"

/**
 * Lógica pura de Ajustes y de la navegación del frontend (solo CoreMinimal).
 *
 * Aquí vive todo lo que se puede decidir sin UObject ni Slate: rangos y
 * validación de cada ajuste, conversión slider ↔ valor, detección de
 * conflictos al remapear y a qué pantalla lleva «Volver»/Escape. La capa de
 * Unreal (UExploredGameUserSettings, UExploredInputSettingsSubsystem,
 * SExploredSettingsPanel, AExploredPlayerController) solo llama a estas
 * funciones, así que se prueban en el editor y en Tools/HostTests con el
 * mismo SettingsLogicSpec.
 *
 * Las teclas se representan por su FName (el de EKeys, p. ej. "SpaceBar"),
 * porque FKey no forma parte de CoreMinimal; en Unreal, FKey(Nombre) y
 * FKey::GetFName() convierten en ambos sentidos.
 */
namespace ExploredSettingsLogic
{
	// --- Ajustes continuos ---------------------------------------------------

	/** Rango de un ajuste continuo y su valor por defecto. */
	struct FSettingRange
	{
		float Min;
		float Max;
		float Default;
	};

	/**
	 * Brillo = gamma de pantalla (GEngine->DisplayGamma, por defecto 2,2 en
	 * BaseEngine.ini). Valores mayores aclaran la imagen.
	 */
	inline constexpr FSettingRange BrightnessRange{ 1.7f, 2.7f, 2.2f };
	/** Volumen de cada canal en porcentaje. */
	inline constexpr FSettingRange VolumeRange{ 0.0f, 100.0f, 100.0f };
	/** Campo de visión horizontal en grados. */
	inline constexpr FSettingRange FieldOfViewRange{ 70.0f, 110.0f, 90.0f };
	/** Multiplicador de sensibilidad (ratón y mando). */
	inline constexpr FSettingRange SensitivityRange{ 0.1f, 5.0f, 1.0f };

	/** Recorta al rango; un valor no finito (NaN o infinito leído del ini) vuelve al valor por defecto. */
	EXPLORED_API float ClampToRange(float Value, const FSettingRange& Range);

	/** Posición del slider en [0, 1] para un valor del ajuste (recortado al rango). */
	EXPLORED_API float ValueToSlider(float Value, const FSettingRange& Range);

	/** Valor del ajuste para una posición del slider (la posición se recorta a [0, 1]). */
	EXPLORED_API float SliderToValue(float Slider01, const FSettingRange& Range);

	/** Ganancia lineal [0, 1] de un volumen en porcentaje. */
	EXPLORED_API float VolumeToGain(float Volume0To100);

	/**
	 * Multiplicador que aplica el subsistema de ambiente a sus capas. Si las
	 * capas pasan por la SoundClass de Ambiente, la mezcla de ajustes ya aplica
	 * el volumen y aquí se devuelve 1 (para no atenuar dos veces); si esa
	 * SoundClass no existe todavía, el propio subsistema aplica el porcentaje.
	 */
	EXPLORED_API float AmbienceLayerGain(float AmbientVolume0To100, bool bRoutedThroughSoundClass);

	// --- Duración del día ------------------------------------------------------

	inline constexpr float DefaultDayLengthMinutes = 40.0f;

	/** Duraciones de día soportadas en Ajustes > Juego (GDD §10), en minutos reales. */
	EXPLORED_API const TArray<float>& GetSupportedDayLengths();

	/** Índice de la duración soportada más cercana (nunca INDEX_NONE; un valor no finito da la de por defecto). */
	EXPLORED_API int32 DayLengthIndex(float Minutes);

	/** Duración soportada más cercana. */
	EXPLORED_API float SnapDayLength(float Minutes);

	// --- Enumerados ------------------------------------------------------------

	/** Devuelve Value si está en [0, Count); si no, Default. */
	EXPLORED_API int32 SanitizeEnumIndex(int32 Value, int32 Count, int32 Default);

	// --- Remapeo -----------------------------------------------------------------

	/** Acción remapeable de AExploredCharacter y su tecla de teclado/ratón por defecto. */
	struct FRemappableAction
	{
		FName ActionName;
		FName DefaultKey;
	};

	/**
	 * Todas las acciones remapeables, en el orden en que se muestran en
	 * Ajustes > Controles. Es la única fuente de verdad: el personaje construye
	 * sus mapeos con esta tabla y el subsistema de entrada detecta conflictos
	 * contra ella.
	 */
	EXPLORED_API const TArray<FRemappableAction>& GetRemappableActions();

	/** Tecla por defecto de una acción de la tabla (NAME_None si no está). */
	EXPLORED_API FName GetDefaultKeyFor(FName ActionName);

	/**
	 * Teclas fijas que no se pueden asignar a una acción remapeable: el
	 * movimiento (WASD), Escape (pausa/volver) y F8 (vuelo de depuración).
	 */
	EXPLORED_API bool IsReservedKey(FName KeyName);

	/** Tecla efectiva de una acción. */
	struct FKeyBinding
	{
		FName ActionName;
		FName Key;
	};

	enum class ERemapCheck : uint8
	{
		Ok,
		InvalidKey,
		ReservedKey,
		InUse
	};

	struct FRemapCheckResult
	{
		ERemapCheck Result = ERemapCheck::Ok;
		/** Acción que ya usa la tecla (solo con InUse). */
		FName ConflictingAction;
	};

	/**
	 * Comprueba si NewKey se puede asignar a ActionName dadas las teclas
	 * efectivas de todas las acciones. Reasignar a una acción la tecla que ya
	 * tiene es válido (no hace nada).
	 */
	EXPLORED_API FRemapCheckResult CheckRemap(const TArray<FKeyBinding>& EffectiveBindings, FName ActionName, FName NewKey);

	// --- Navegación del frontend --------------------------------------------------

	/** Pantalla del frontend visible. None = jugando, sin menú encima. */
	enum class EMenuScreen : uint8
	{
		None,
		MainMenu,
		ModeSelect,
		Settings,
		Credits,
		Pause
	};

	/**
	 * A qué pantalla lleva «Volver» (botón, Escape o B del mando) desde Current.
	 * Ajustes vuelve a la pausa o al menú principal según desde dónde se abrió.
	 * En el menú principal no hace nada (devuelve MainMenu).
	 */
	EXPLORED_API EMenuScreen ScreenAfterBack(EMenuScreen Current, bool bSettingsOpenedFromPause);

	/** Como ScreenAfterBack, pero jugando (None) Escape abre la pausa. */
	EXPLORED_API EMenuScreen ScreenAfterEscape(EMenuScreen Current, bool bSettingsOpenedFromPause);

	/** Teclas que equivalen a «Volver» en cualquier menú: Escape y B del mando. */
	EXPLORED_API bool IsMenuBackKey(FName KeyName);

	/** Teclas que abren y cierran la pausa: Escape y Start (Menu) del mando. */
	EXPLORED_API bool IsPauseToggleKey(FName KeyName);
}
