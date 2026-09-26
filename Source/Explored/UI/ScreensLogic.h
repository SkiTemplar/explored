#pragma once

#include "CoreMinimal.h"
#include "Achievements/AchievementsModel.h"
#include "Carry/InventoryModel.h"
#include "Ruins/MuseumModel.h"
#include "Save/SaveSlots.h"

/**
 * Lógica pura de las pantallas de P-UI2 (solo modelos puros y CoreMinimal):
 * qué filas muestra cada una y en qué estado, para que los widgets Slate
 * (SExploredMuseum, SExploredAchievements, SExploredSaveSlots,
 * SExploredInventoryPanel) se limiten a pintar. ScreensLogicSpec las prueba
 * en el editor y en Tools/HostTests.
 */
namespace ExploredScreens
{
	// --- Museo y catálogo (GDD §7) -----------------------------------------------

	/** Qué sabe el jugador de un tesoro, de menos a más. */
	enum class EMuseumEntryState : uint8
	{
		/** Silueta: ni hallado ni fotografiado. */
		Unknown,
		/** Registrado por una foto de la cámara desechable, sin recogerlo. */
		Photographed,
		/** Recogido (en las manos, la mochila o un arcón). */
		Found,
		/** Recogido y colocado en un mueble del museo. */
		Exhibited
	};

	/** Una ficha del catálogo. */
	struct FMuseumEntry
	{
		FName ArtifactId;
		EMuseumEntryState State = EMuseumEntryState::Unknown;
		/** El catálogo trae silueta y procedencia de cada tesoro, completo o no (GDD §7). */
		EArtifactProvenance Provenance = EArtifactProvenance::Marae;
		EArtifactRarity Rarity = EArtifactRarity::Common;
		EArtifactSize Size = EArtifactSize::Small;
		/** Se muestra el nombre (registrado); si no, «???» y la silueta. */
		bool bShowName = false;
		/** Lugar y isla donde se recogió (solo si se ha recogido). */
		FName FoundAt;
		int32 FoundIsland = INDEX_NONE;
	};

	/** Fichas en el orden del catálogo (artifacts.json). */
	EXPLORED_API TArray<FMuseumEntry> BuildMuseumEntries(const FMuseumModel& Museum);

	/** Recuentos de la cabecera del museo. */
	struct FMuseumSummary
	{
		int32 Total = 0;
		int32 Registered = 0;
		int32 Found = 0;
		int32 Exhibited = 0;
		/** Tesoros expuestos para el logro «Coleccionista» (FMuseumModel::CollectorThreshold). */
		int32 CollectorTarget = FMuseumModel::CollectorThreshold;
		/** Expuestos / objetivo, en [0, 1]. */
		float CollectorProgress = 0.0f;
		bool bCollectorDone = false;
		/** Registrados / total, en [0, 1]. */
		float CatalogProgress = 0.0f;
	};

	EXPLORED_API FMuseumSummary SummarizeMuseum(const FMuseumModel& Museum);

	// --- Logros (GDD §16) --------------------------------------------------------

	struct FAchievementRow
	{
		FName Id;
		bool bUnlocked = false;
		/** Oculto y aún sin conseguir: nombre «???», sin descripción ni barra. */
		bool bMasked = false;
		/** Se puede conseguir en el modo de la partida actual (o fuera de partida). */
		bool bAvailableInMode = true;
		/** Barra de progreso en [0, 1]; 1 si está conseguido y 0 si está oculto. */
		float Progress = 0.0f;
	};

	/** Filas en el orden de achievements.json. */
	EXPLORED_API TArray<FAchievementRow> BuildAchievementRows(const FAchievementsModel& Model);

	struct FAchievementsSummary
	{
		int32 Unlocked = 0;
		int32 Total = 0;
		float Fraction = 0.0f;
	};

	EXPLORED_API FAchievementsSummary SummarizeAchievements(const FAchievementsModel& Model);

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
		FutureVersion
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
}
