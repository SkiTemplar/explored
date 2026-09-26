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
	 * movimiento (WASD), Escape (pausa/volver), M (mapa) y F8 (vuelo de depuración).
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

	/**
	 * Pantalla del frontend visible. None = jugando, sin menú encima. Los
	 * valores nuevos van al final para no cambiar los existentes.
	 */
	enum class EMenuScreen : uint8
	{
		None,
		MainMenu,
		ModeSelect,
		Settings,
		Credits,
		Pause,
		/** Mapa en las manos (GDD §5.6): se abre jugando (M o View) o desde la pausa. */
		Map,
		/** Museo y catálogo de tesoros (GDD §7): desde la pausa o desde el apartado de colección del mapa. */
		Museum,
		/** Lista de logros (GDD §15, §16): desde el menú principal o la pausa. */
		Achievements,
		/** Selector de ranura: «Guardar» en la pausa y «Cargar» en el menú principal. */
		SaveSlots
	};

	/**
	 * A qué pantalla lleva «Volver» (botón, Escape o B del mando) desde Current
	 * cuando solo se sabe si se abrió desde la pausa (tabla por defecto):
	 * - Ajustes, Logros y Ranuras vuelven a la pausa o al menú principal.
	 * - El mapa vuelve a la pausa o al juego (None).
	 * - El museo vuelve a la pausa o al mapa (su apartado de colección).
	 * En el menú principal no hace nada (devuelve MainMenu). Con varias
	 * pantallas encadenadas (juego → mapa → museo) manda FMenuNavigation.
	 */
	EXPLORED_API EMenuScreen ScreenAfterBack(EMenuScreen Current, bool bOpenedFromPause);

	/** Como ScreenAfterBack, pero jugando (None) Escape abre la pausa. */
	EXPLORED_API EMenuScreen ScreenAfterEscape(EMenuScreen Current, bool bOpenedFromPause);

	/** Si existe la transición «abrir To estando en From» (botones y teclas del frontend). */
	EXPLORED_API bool CanOpenScreenFrom(EMenuScreen From, EMenuScreen To);

	/**
	 * Pila de pantallas del frontend: recuerda desde dónde se abrió cada una
	 * para que «Volver» regrese allí aunque se encadenen (juego → mapa → museo
	 * → mapa → juego). La base es la raíz: None (jugando) o MainMenu.
	 */
	class EXPLORED_API FMenuNavigation
	{
	public:
		FMenuNavigation() { Stack.Add(EMenuScreen::None); }

		/** Vacía la pila y deja solo la raíz. */
		void Reset(EMenuScreen Root);

		EMenuScreen Current() const { return Stack.Last(); }
		EMenuScreen Root() const { return Stack[0]; }
		int32 Depth() const { return Stack.Num(); }

		/** Screen está en la pila (es la actual o una de las que llevan a ella). */
		bool IsInStack(EMenuScreen Screen) const { return Stack.Contains(Screen); }

		/**
		 * Abre Screen. Si ya es la actual, no cambia nada; si está más abajo en
		 * la pila, vuelve a ella (recorta lo de encima); si la transición existe
		 * (CanOpenScreenFrom), la apila. Si no, devuelve false y no cambia nada.
		 */
		bool Open(EMenuScreen Screen);

		/**
		 * Destino de «Volver» sin modificar la pila: la pantalla anterior o, en
		 * la raíz, la tabla por defecto (jugando y en el menú principal no hace nada).
		 */
		EMenuScreen BackTarget() const;

		/** Destino de Escape: jugando abre la pausa; en el resto, como BackTarget. */
		EMenuScreen EscapeTarget() const;

	private:
		TArray<EMenuScreen> Stack;
	};

	/** Teclas que equivalen a «Volver» en cualquier menú: Escape y B del mando. */
	EXPLORED_API bool IsMenuBackKey(FName KeyName);

	/** Teclas que abren y cierran la pausa: Escape y Start (Menu) del mando. */
	EXPLORED_API bool IsPauseToggleKey(FName KeyName);

	/** Teclas que sacan y guardan el mapa: M y View (Back) del mando. */
	EXPLORED_API bool IsMapToggleKey(FName KeyName);
}
