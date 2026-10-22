#pragma once

#include "CoreMinimal.h"

#include "Boats/RaftModel.h"
#include "Items/BackpackModel.h"
#include "Narrative/BeaconDirectorModel.h"
#include "Save/SaveArchive.h"
#include "Survival/BodyEventsModel.h"

/**
 * Secciones de guardado de Baliza Siete (GDD v3 §5.10) que no son de un sistema heredado:
 * progreso de la historia («progress»), páginas del cuaderno («pages»), mochila e ids
 * recogidos (dentro de «island»). Puro: los viajes de ida y vuelta se prueban en el host
 * (Explored.Save.Beacon).
 *
 * Mismo contrato que SaveSystemStates.h: Load parte del estado por defecto y un dato que no
 * encaja deja el valor por defecto. Load devuelve false si la sección trae datos incoherentes
 * (etapa sin su requisito, más de 2 tarros...): el sistema se queda en el estado por defecto.
 */
template <>
struct TSaveEnumNames<EBeaconStage>
{
	static constexpr const TCHAR* Names[] = {
		TEXT("Prologo"), TEXT("Orilla"), TEXT("Faro"), TEXT("Balsa"), TEXT("Carguero"),
		TEXT("LenteEnMano"), TEXT("Ciclon"), TEXT("Preparacion"), TEXT("LinternaMontada"), TEXT("Final"),
	};
};

namespace ExploredSaveStates
{
	inline const TCHAR* const SectionProgress = TEXT("progress");
	inline const TCHAR* const SectionPages = TEXT("pages");
	inline const TCHAR* const SectionIsland = TEXT("island");

	// --- Progreso de la historia («progress») ---------------------------------------
	EXPLORED_API void SaveBeaconProgress(FSaveArchive& Ar, const FBeaconRunState& Run);
	/** Lee etapa, hitos, ranuras, pensamientos y el encendido; no toca Notebook. */
	EXPLORED_API bool LoadBeaconProgress(const FSaveArchive& Ar, FBeaconRunState& InOutRun);

	// --- Páginas del cuaderno («pages») --------------------------------------------
	EXPLORED_API void SavePages(FSaveArchive& Ar, const FNotebookState& Notebook);
	EXPLORED_API bool LoadPages(const FSaveArchive& Ar, FNotebookState& OutNotebook);

	// --- Mochila e ids recogidos (dentro de «island») -------------------------------
	EXPLORED_API void SaveBackpack(FSaveArchive& Ar, const FBackpackState& Pack);
	EXPLORED_API bool LoadBackpack(const FSaveArchive& Ar, FBackpackState& OutPack);
	/** Ids en orden alfabético: el mismo conjunto da siempre el mismo texto. */
	EXPLORED_API void SaveCollectedIds(FSaveArchive& Ar, const TSet<FName>& Ids);
	EXPLORED_API void LoadCollectedIds(const FSaveArchive& Ar, TSet<FName>& OutIds);

	// --- Objetos soltados (dentro de «island») ------------------------------------------
	/** Distancia horizontal máxima (cm) al origen de la isla de un objeto soltado que se acepta al cargar. */
	inline constexpr double MaxDroppedRadiusCm = 200000.0;
	/** Por debajo del suelo más de esto (cm) un objeto se considera enterrado. */
	inline constexpr double BuriedToleranceCm = 50.0;
	/** Altura (cm) sobre el suelo a la que se deja un objeto que estaba enterrado. */
	inline constexpr double RestLiftCm = 30.0;
	/**
	 * Objeto soltado que se puede recrear: id de items.json, cantidad entre 1 y lo que cabe en la
	 * mochila entera, y posición finita a menos de MaxDroppedRadiusCm. Si no, false y el motivo.
	 */
	EXPLORED_API bool IsSaneDropped(FName ItemId, int32 Count, const FVector& LocationCm, const FBackpackRules& Rules, FString& OutReason);
	/** Un objeto más de BuriedToleranceCm bajo el suelo (metros) sube a RestLiftCm sobre él; el resto no se toca. */
	EXPLORED_API FVector RestOnGround(const FVector& LocationCm, float GroundMeters);

	/** Objeto soltado tal como se guarda (lista «dropped» de «island»). */
	struct EXPLORED_API FDroppedSaveEntry
	{
		FName ItemId;
		int32 Count = 1;
		/** Posición (cm). */
		FVector Location = FVector::ZeroVector;
	};
	EXPLORED_API void SaveDroppedItems(FSaveArchive& Ar, const TArray<FDroppedSaveEntry>& Items);
	/**
	 * Lee «dropped»: descarta lo que no pasa IsSaneDropped (un motivo por descarte en OutRejected) y
	 * sube a la superficie lo enterrado (RestOnGround con GroundAt(X, Y), en metros). Las partidas de
	 * antes del arreglo de la leña de deriva la guardaron bajo la arena.
	 */
	EXPLORED_API void LoadDroppedItems(const FSaveArchive& Ar, const FBackpackRules& Rules,
		TFunctionRef<float(double XMeters, double YMeters)> GroundAt, TArray<FDroppedSaveEntry>& OutItems,
		TArray<FString>& OutRejected);

	// --- Cuerpo de Iria (sección «body»): 4 barras y el último refugio ---------------
	EXPLORED_API void SaveBodyBars(FSaveArchive& Ar, const FBodyBars& Bars, FName LastShelter);
	/** false si falta una barra o alguna está fuera de [0, 100]: se queda el cuerpo por defecto. */
	EXPLORED_API bool LoadBodyBars(const FSaveArchive& Ar, FBodyBars& OutBars, FName& OutLastShelter);

	// --- Balsa (dentro de «island»): troncos, atada y dónde quedó ----------------------
	struct EXPLORED_API FRaftSaveData
	{
		FRaftSiteState Site;
		bool bHasPose = false;
		/** Posición (cm) y rumbo de la balsa si ya existe. */
		FVector Location = FVector::ZeroVector;
		float YawDeg = 0.0f;
	};
	EXPLORED_API void SaveRaft(FSaveArchive& Ar, const FRaftSaveData& Raft);
	/** false si el sitio no es coherente (FRaftModel::IsValid): balsa por hacer. */
	EXPLORED_API bool LoadRaft(const FSaveArchive& Ar, FRaftSaveData& OutRaft);
}
