#pragma once

#include "CoreMinimal.h"

#include "Achievements/AchievementsModel.h"
#include "Boats/BoatTypes.h"
#include "Building/BuildingTypes.h"
#include "Carry/InventoryModel.h"
#include "Cartography/CartographyModel.h"
#include "Cooking/CookingModel.h"
#include "Cooking/FireModel.h"
#include "Events/WorldEventsModel.h"
#include "Farming/FarmModel.h"
#include "Fishing/FishingModel.h"
#include "Ruins/MuseumModel.h"
#include "Ruins/RuinsModel.h"
#include "Save/SaveArchive.h"
#include "Survival/SurvivalModel.h"
#include "Weather/WeatherModel.h"

/**
 * Secciones de guardado de cada sistema (docs/tecnico/guardado.md): conversión
 * de los estados planos de los modelos puros a FSaveArchive y de vuelta.
 *
 * Contrato común:
 * - Save escribe todos los campos con claves en inglés y camelCase; los enums
 *   van por nombre (reordenarlos no rompe partidas).
 * - Load parte SIEMPRE del estado por defecto y lee lo que haya: una clave que
 *   falta o no encaja deja el valor por defecto, y un elemento ilegible de una
 *   lista se descarta sin tirar el resto.
 *
 * Todo es puro (solo CoreMinimal y modelos puros): los viajes de ida y vuelta se
 * prueban en Tools/HostTests (Explored.Save.Systems).
 */
namespace ExploredSaveStates
{
	// --- Construcción («building») ------------------------------------------------
	EXPLORED_API void SaveBuilding(FSaveArchive& Ar, const FBuildingSaveState& State);
	EXPLORED_API void LoadBuilding(const FSaveArchive& Ar, FBuildingSaveState& OutState);

	// --- Huerto («farm») -----------------------------------------------------------
	EXPLORED_API void SaveFarm(FSaveArchive& Ar, const FFarmState& State);
	EXPLORED_API void LoadFarm(const FSaveArchive& Ar, FFarmState& OutState);

	// --- Mapa dibujado a mano («cartography») --------------------------------------
	EXPLORED_API void SaveCartography(FSaveArchive& Ar, const FCartographyState& State);
	EXPLORED_API void LoadCartography(const FSaveArchive& Ar, FCartographyState& OutState);

	// --- Ruinas y museo («ruins») ---------------------------------------------------
	EXPLORED_API void SaveRuins(FSaveArchive& Ar, const FRuinsState& Ruins, const FMuseumState& Museum);
	EXPLORED_API void LoadRuins(const FSaveArchive& Ar, FRuinsState& OutRuins, FMuseumState& OutMuseum);

	// --- Un fuego del mundo (dentro de la sección «cooking») -----------------------
	/** Lo que se guarda de un AExploredFire; el utensilio va aparte como objeto aplanado. */
	struct EXPLORED_API FSavedFire
	{
		/** Posición del fuego en el mundo (cm): identifica el fuego al cargar. */
		FVector Location = FVector::ZeroVector;
		FFireState Fire;
		FCookingPot Pot;
	};
	EXPLORED_API void SaveFire(FSaveArchive& Ar, const FSavedFire& Fire);
	EXPLORED_API void LoadFire(const FSaveArchive& Ar, FSavedFire& OutFire);

	// --- Eventos del mundo («events») -----------------------------------------------
	EXPLORED_API void SaveWorldEvents(FSaveArchive& Ar, const FWorldEventsState& State);
	EXPLORED_API void LoadWorldEvents(const FSaveArchive& Ar, FWorldEventsState& OutState);

	// --- Logros («achievements») ----------------------------------------------------
	EXPLORED_API void SaveAchievements(FSaveArchive& Ar, const FAchievementsState& State);
	EXPLORED_API void LoadAchievements(const FSaveArchive& Ar, FAchievementsState& OutState);
	/**
	 * Combina el estado en memoria con el de una partida que se carga: la partida
	 * trae sus estadísticas de partida (Run, RunMode); el perfil nunca retrocede
	 * (máximo de cada número, unión de conjuntos, marcas y logros conseguidos).
	 * Así cargar una partida antigua no borra logros ni progreso del perfil.
	 */
	EXPLORED_API FAchievementsState MergeLoadedAchievements(const FAchievementsState& Current, const FAchievementsState& Loaded);

	// --- Inventario del jugador («inventory») ---------------------------------------
	EXPLORED_API void SaveInventory(FSaveArchive& Ar, const FInventoryState& State);
	EXPLORED_API void LoadInventory(const FSaveArchive& Ar, FInventoryState& OutState);
	EXPLORED_API void SaveInventoryContainer(FSaveArchive& Ar, const FInventoryContainer& Container);
	EXPLORED_API void LoadInventoryContainer(const FSaveArchive& Ar, FInventoryContainer& OutContainer);

	/**
	 * Un objeto de un árbol aplanado (FItemInstance::Components no se serializa
	 * con la reflexión, ver Items/ItemTypes.h): las piezas van en la misma lista
	 * que su padre, después de él, con el índice de su padre (INDEX_NONE = raíz).
	 */
	struct EXPLORED_API FSavedItemNode
	{
		int32 Parent = INDEX_NONE;
		FName DefinitionId;
		int32 Quality = 3;
		float Durability = 1.0f;
		int32 Count = 1;
		float LiquidLiters = 0.0f;
		/** Nombre generado por la fabricación (texto ya formateado); vacío = el de la definición. */
		FString GeneratedName;

		bool operator==(const FSavedItemNode& Other) const
		{
			return Parent == Other.Parent && DefinitionId == Other.DefinitionId && Quality == Other.Quality &&
				Durability == Other.Durability && Count == Other.Count && LiquidLiters == Other.LiquidLiters &&
				GeneratedName == Other.GeneratedName;
		}
	};
	EXPLORED_API void SaveItemNodes(FSaveArchive& Ar, const FString& Key, const TArray<FSavedItemNode>& Nodes);
	/** Lee la lista; descarta los nodos cuyo padre no esté antes que ellos (árbol roto). */
	EXPLORED_API void LoadItemNodes(const FSaveArchive& Ar, const FString& Key, TArray<FSavedItemNode>& OutNodes);

	/**
	 * Aplana un árbol de objetos (T = FItemInstance en el juego; cualquier tipo con
	 * un TArray<T> Components en los tests) en preorden. ToNode rellena los datos
	 * del nodo; el padre lo pone esta función.
	 */
	template <typename T, typename FToNode>
	void FlattenItemTree(const T& Root, FToNode&& ToNode, TArray<FSavedItemNode>& Out)
	{
		struct FVisit
		{
			const T* Item;
			int32 Parent;
		};
		TArray<FVisit> Stack;
		Stack.Add({&Root, INDEX_NONE});
		while (Stack.Num() > 0)
		{
			const FVisit Visit = Stack.Pop();
			FSavedItemNode Node = ToNode(*Visit.Item);
			Node.Parent = Visit.Parent;
			const int32 Index = Out.Add(MoveTemp(Node));
			// Al revés en la pila para que las piezas salgan en su orden.
			for (int32 I = Visit.Item->Components.Num() - 1; I >= 0; --I)
			{
				Stack.Add({&Visit.Item->Components[I], Index});
			}
		}
	}

	/**
	 * Reconstruye los árboles de una lista aplanada: devuelve una raíz por cada
	 * nodo con Parent == INDEX_NONE, en orden. FromNode crea el objeto sin piezas.
	 * Los nodos con un padre inválido (no anterior a ellos) se descartan.
	 */
	template <typename T, typename FFromNode>
	TArray<T> RebuildItemTrees(const TArray<FSavedItemNode>& Nodes, FFromNode&& FromNode)
	{
		// Se construye de las hojas a la raíz: cada nodo se añade a su padre cuando ya tiene sus piezas.
		TArray<T> Built;
		TArray<uint8> Valid;
		Built.Reserve(Nodes.Num());
		Valid.Reserve(Nodes.Num());
		for (int32 I = 0; I < Nodes.Num(); ++I)
		{
			const int32 Parent = Nodes[I].Parent;
			const bool bValid = Parent == INDEX_NONE || (Parent >= 0 && Parent < I && Valid[Parent] != 0);
			Valid.Add(bValid ? 1 : 0);
			Built.Add(FromNode(Nodes[I]));
		}
		for (int32 I = Nodes.Num() - 1; I >= 0; --I)
		{
			const int32 Parent = Nodes[I].Parent;
			if (Valid[I] != 0 && Parent != INDEX_NONE)
			{
				Built[Parent].Components.Insert(MoveTemp(Built[I]), 0);
			}
		}
		TArray<T> Roots;
		for (int32 I = 0; I < Nodes.Num(); ++I)
		{
			if (Valid[I] != 0 && Nodes[I].Parent == INDEX_NONE)
			{
				Roots.Add(MoveTemp(Built[I]));
			}
		}
		return Roots;
	}

	// --- Cuerpo del jugador («body») ------------------------------------------------
	EXPLORED_API void SaveSurvival(FSaveArchive& Ar, const FSurvivalState& State, ESurvivalMode Mode);
	EXPLORED_API void LoadSurvival(const FSaveArchive& Ar, FSurvivalState& OutState, ESurvivalMode& OutMode);

	// --- Embarcaciones («boats») ------------------------------------------------------
	EXPLORED_API void SaveBoat(FSaveArchive& Ar, const FBoatSaveData& Boat);
	EXPLORED_API void LoadBoat(const FSaveArchive& Ar, FBoatSaveData& OutBoat);

	// --- Pesca («fishing») -----------------------------------------------------------
	EXPLORED_API void SaveFishing(FSaveArchive& Ar, const FFishingSaveState& State);
	EXPLORED_API void LoadFishing(const FSaveArchive& Ar, FFishingSaveState& OutState);

	// --- Hora y clima («time») --------------------------------------------------------
	struct EXPLORED_API FSavedClock
	{
		int32 Day = 4;
		float Hours = 7.5f;
		/** Estado forzado del clima (Count = ninguno) y hasta cuándo (días totales). */
		EWeatherState ForcedWeather = EWeatherState::Count;
		float ForcedUntilDays = 0.0f;
	};
	EXPLORED_API void SaveClock(FSaveArchive& Ar, const FSavedClock& Clock);
	EXPLORED_API void LoadClock(const FSaveArchive& Ar, FSavedClock& OutClock);
}
