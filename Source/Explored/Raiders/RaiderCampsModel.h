#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"

class FTerrainDensity;

/**
 * Campamentos piratas fijos [F3] (biblia 05 §2.5-2.6, biblia 04 §2.4 y §2.5). Modelo puro:
 * coloca los dos campamentos sobre el terreno real a partir de la semilla del archipiélago,
 * con su tienda, su hoguera, su cofre de botín y su barco. Misma semilla, mismos campamentos
 * y mismo botín: «fijos», no aleatorios entre partidas con el mismo mundo.
 *
 * Red (biblia 08 §2.7, §2.10): el servidor genera los campamentos con la semilla que ya
 * comparten todos los clientes, así que la geometría no se replica; solo el estado de cada
 * uno (cofre abierto, hoguera apagada, barco hundido) y los piratas, que son fauna terrestre
 * replicada.
 */

enum class ERaiderCampId : uint8
{
	BrokenCove,       // «cala_rota»: Cala Rota / Broken Cove, en Los Dientes.
	RottenAnchorage,  // «fondeadero_podrido»: Fondeadero Podrido / Rotten Anchorage, en el Manglar de las Voces.
	Count
};

EXPLORED_API const TCHAR* LexToString(ERaiderCampId Camp);

/** Barcos piratas (biblia 05 §2.5), sobre el mismo FBoatModel del resto de embarcaciones. */
enum class EPirateBoatType : uint8
{
	RaidingCanoe,  // Piragua de asalto / Raiding canoe: 2-3 tripulantes, varada en la playa.
	BlackSloop,    // Balandra negra / Black sloop: 5-6 con Capitán, fondeada.
	Count
};

EXPLORED_API const TCHAR* LexToString(EPirateBoatType Boat);

/** Una línea del cofre de botín. */
struct EXPLORED_API FRaiderLoot
{
	FName ItemId;
	int32 Count = 0;

	bool operator==(const FRaiderLoot& Other) const { return ItemId == Other.ItemId && Count == Other.Count; }
};

/** Un campamento colocado. Posiciones en metros, como FPointOfInterest. */
struct EXPLORED_API FRaiderCamp
{
	ERaiderCampId Id = ERaiderCampId::BrokenCove;
	EIslandArchetype Island = EIslandArchetype::Teeth;
	int32 IslandIndex = INDEX_NONE;
	/** Centro del campamento, en la playa o, sin playa, en la cornisa más baja. */
	FVector Location = FVector::ZeroVector;
	/** Grados; mira al mar (hacia fuera de la isla). */
	float Yaw = 0.0f;
	FVector TentLocation = FVector::ZeroVector;
	FVector FireLocation = FVector::ZeroVector;
	FVector ChestLocation = FVector::ZeroVector;
	EPirateBoatType Boat = EPirateBoatType::RaidingCanoe;
	/**
	 * La piragua, en la línea de agua (varada en una playa; al pie de la cornisa, a flote); la
	 * balandra, fondeada con calado. Z nunca por debajo del nivel del mar.
	 */
	FVector BoatLocation = FVector::ZeroVector;
	TArray<FRaiderLoot> Loot;
	/** Se encontró orilla (playa o cornisa); false si se usó el borde nominal de la isla. */
	bool bOnBeach = false;

	bool operator==(const FRaiderCamp& Other) const;
};

struct EXPLORED_API FRaiderCampsModel
{
	/** Separación mínima a cualquier punto de interés que se pasa en Avoid (m). */
	static constexpr float MinClearanceM = 60.0f;
	/** Playas sin agua para el barco que se prueban en cada pasada antes de aceptar un sitio peor. */
	static constexpr int32 MaxAttempts = 24;
	/** Rejilla de búsqueda de playa: celda (m) y alcance en radios de la isla. */
	static constexpr float SearchCellM = 8.0f;
	static constexpr float SearchRadiusFactor = 1.4f;
	/** Franja de altura de playa (la misma de FPoiLayout::FindBeach). */
	static constexpr float BeachMinHeightM = 1.2f;
	static constexpr float BeachMaxHeightM = 3.0f;
	/**
	 * Sin playa (Los Dientes no tienen ni una celda entre 0 y 3 m: pasan del agua al
	 * acantilado) se acepta la cornisa más baja, primero hasta 6 m y luego hasta 12 m, si es
	 * llana (normal con Z ≥ MinLedgeFlatness).
	 */
	static constexpr float LedgeMidHeightM = 6.0f;
	static constexpr float LedgeMaxHeightM = 12.0f;
	static constexpr float MinLedgeFlatness = 0.8f;
	/** Distancia máxima desde la playa hasta el sitio del barco (m). */
	static constexpr float MaxBoatDistanceM = 90.0f;
	/** Calado para fondear la balandra (profundidad mínima, m). */
	static constexpr float SloopDraftM = 2.5f;

	/** Isla y barco de cada campamento (tabla de biblia 05 §2.5). */
	static EIslandArchetype IslandOf(ERaiderCampId Camp);
	static EPirateBoatType BoatOf(ERaiderCampId Camp);
	/** Tripulación del barco: [2, 3] la piragua, [5, 6] la balandra. */
	static void CrewRange(EPirateBoatType Boat, int32& OutMin, int32& OutMax);

	/**
	 * Botín del cofre (biblia 05 §2.6), determinista por semilla y campamento: metal trabajado,
	 * cable, cuerda y a veces medicina. Nunca monedas, pólvora ni mapas de tesoro.
	 */
	static TArray<FRaiderLoot> RollLoot(uint32 WorldSeed, ERaiderCampId Camp);

	/**
	 * Coloca los dos campamentos sobre el terreno. Avoid son posiciones (m) de puntos de
	 * interés ya colocados (FPoiLayout), de los que se separa al menos MinClearanceM si hay
	 * playa para ello. Un campamento cuya isla no existe en el layout no se devuelve.
	 */
	static TArray<FRaiderCamp> Generate(const FTerrainDensity& Density, const TArray<FVector>& Avoid = TArray<FVector>());

	/** Un solo campamento; false si su isla no está en el layout. */
	static bool Place(const FTerrainDensity& Density, ERaiderCampId Camp, const TArray<FVector>& Avoid, FRaiderCamp& Out);
};
