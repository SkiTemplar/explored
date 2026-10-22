#pragma once

#include "CoreMinimal.h"
#include "Carry/InventoryModel.h"
#include "Save/SaveSlots.h"

/**
 * Lógica pura de las pantallas de P-UI2 (solo modelos puros y CoreMinimal):
 * qué filas muestra cada una y en qué estado, para que los widgets Slate
 * (SExploredSaveSlots, SExploredInventoryPanel) se limiten a pintar. ScreensLogicSpec las prueba
 * en el editor y en Tools/HostTests.
 */
namespace ExploredScreens
{
	// --- Ranuras de guardado (GDD §15) -------------------------------------------

	/** Para qué se abre el selector: «Guardar» (pausa) o «Cargar» (menú principal). */
	enum class ESaveSlotsMode : uint8
	{
		Save,
		Load
	};

	enum class ESaveSlotRowState : uint8
	{
		/** Sin fichero. */
		Empty,
		/** Se lee bien. */
		Ok,
		/** La principal está dañada y se lee su copia de seguridad. */
		Recovered,
		/** No se puede leer (ni la principal ni la copia). */
		Damaged,
		/** De una versión del juego más nueva. */
		FutureVersion,
		/** De una versión anterior que ya no se puede cargar (historia anterior, formato 1). */
		Obsolete
	};

	struct FSaveSlotRow
	{
		FString SlotId;
		bool bIsAuto = false;
		/** 1…3 en las manuales; 0 en la automática. */
		int32 ManualIndex = 0;
		ESaveSlotRowState State = ESaveSlotRowState::Empty;
		/** Cabecera leída (válida con Ok y Recovered). */
		FSaveHeader Header;
		/** Hay copia de seguridad «.bak» de la ranura. */
		bool bHasBackup = false;
		/** Se puede elegir en este modo. */
		bool bSelectable = false;
		/** Guardar aquí pisa una partida: pide confirmación. */
		bool bNeedsOverwriteConfirm = false;
	};

	/**
	 * Siempre las cuatro ranuras en orden fijo (automática y manuales 1–3),
	 * estén o no en Listed (FSaveSlotStore::List solo trae las que tienen
	 * fichero). Guardando solo se eligen las manuales (la automática la escribe
	 * el juego al dormir y en hogueras) y pisar una con fichero pide
	 * confirmación; cargando solo se eligen las legibles.
	 */
	EXPLORED_API TArray<FSaveSlotRow> BuildSaveSlotRows(const TArray<FSaveSlotInfo>& Listed, const TArray<FString>& SlotsWithBackup,
		ESaveSlotsMode Mode);

	/** Tiempo jugado en horas y minutos enteros (negativo o no finito = 0). */
	struct FPlayTime
	{
		int32 Hours = 0;
		int32 Minutes = 0;
	};

	EXPLORED_API FPlayTime SplitPlayTime(double Seconds);

	// --- Inventario diegético (GDD §8.2) -----------------------------------------

	/** Cómo se lleva la carga, para el color y el texto del peso. */
	enum class ELoadBand : uint8
	{
		/** Dentro de la capacidad cómoda. */
		Comfortable,
		/** Por encima de la cómoda: cansa y hace ruido. */
		Heavy,
		/** En el tope (FInventoryModel::MaxLoadRatio): no se puede cargar más. */
		Overloaded
	};

	EXPLORED_API ELoadBand LoadBandFor(float CarriedWeightRatio);

	/** Un contenedor del cuerpo tal como se pinta en el panel. */
	struct FInventorySection
	{
		EInventorySlot Slot = EInventorySlot::None;
		/** Objetos, ordenados por su hueco visible. */
		TArray<int64> ItemIds;
		/** Huecos del contenedor (0 = sin límite de huecos: la mochila va por volumen). */
		int32 Capacity = 0;
		float UsedWeightKg = 0.0f;
		float MaxWeightKg = 0.0f;
		float UsedVolumeLiters = 0.0f;
		float MaxVolumeLiters = 0.0f;
	};

	/**
	 * Secciones visibles en orden: bolsillos, cinturón, bolsa estanca (si la
	 * hay, bHasPouch = FInventoryModel::HasPouch(), o si aún guarda algo),
	 * mochila (si se lleva) y angarillas (si van enganchadas). Las manos se
	 * pintan aparte (HandLeft/HandRight del estado).
	 */
	EXPLORED_API TArray<FInventorySection> BuildInventorySections(const FInventoryState& State, bool bHasPouch);

	// --- Cuaderno de guardia (J, GDD v3 §9.2) ----------------------------------------

	constexpr int32 NotebookPages = 8;

	struct FNotebookPageRow
	{
		/** 1-8. */
		int32 Page = 0;
		bool bRead = false;
		bool bSelected = false;
	};

	struct FNotebookView
	{
		/** Siempre las 8 páginas, en orden. */
		TArray<FNotebookPageRow> Rows;
		int32 ReadCount = 0;
		/** Página que se enseña (1-8); 0 si no hay ninguna leída. */
		int32 Shown = 0;
	};

	/**
	 * Lo que pinta el cuaderno con la máscara de páginas leídas (bit i = página i + 1). Se enseña
	 * Requested si está leída; si no, la última leída; sin ninguna, 0.
	 */
	EXPLORED_API FNotebookView BuildNotebookView(uint8 PagesMask, int32 Requested);

	/** Siguiente página leída en Direction (+1 o -1) desde Current, sin dar la vuelta; Current si no hay. */
	EXPLORED_API int32 StepNotebookPage(uint8 PagesMask, int32 Current, int32 Direction);

	// --- Menú de pausa: «Salir al menú» y «Salir del juego» (GDD v3 §9.2) ---------------

	/** Página del conmutador de SExploredPauseMenu (el orden de sus ranuras). */
	enum class EPauseMenuPage : uint8
	{
		Buttons = 0,
		ConfirmExit = 1,
		ConfirmQuit = 2
	};

	/** Delegado que lanza el paso. */
	enum class EPauseMenuAction : uint8
	{
		None,
		ExitToMenu,
		Quit
	};

	/** Adónde va el foco tras el paso. */
	enum class EPauseMenuFocus : uint8
	{
		Unchanged,
		/** «No» de la pregunta que se acaba de abrir. */
		ConfirmNo,
		ExitButton,
		QuitButton
	};

	struct FPauseMenuStep
	{
		EPauseMenuPage Page = EPauseMenuPage::Buttons;
		EPauseMenuAction Action = EPauseMenuAction::None;
		EPauseMenuFocus Focus = EPauseMenuFocus::Unchanged;
	};

	/**
	 * «Salir al menú» (bQuit = false) o «Salir del juego» (true) desde los botones: con progreso sin
	 * guardar (bConfirmExit) abre su pregunta con el foco en «No» y no sale; si no, sale al momento.
	 */
	EXPLORED_API FPauseMenuStep PauseRequestExit(bool bQuit, bool bConfirmExit);
	/** «Sí» en la pregunta de la página Current: sale al menú o del juego según cuál sea; en los botones, nada. */
	EXPLORED_API FPauseMenuStep PauseConfirmYes(EPauseMenuPage Current);
	/** «No»: vuelve a los botones con el foco en el que abrió la pregunta. */
	EXPLORED_API FPauseMenuStep PauseConfirmNo(EPauseMenuPage Current);
}
