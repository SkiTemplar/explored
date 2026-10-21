#pragma once

#include "CoreMinimal.h"

#include "Items/BackpackModel.h"
#include "Narrative/BeaconDirectorModel.h"
#include "Save/SaveArchive.h"

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
}
