#pragma once

#include "CoreMinimal.h"
#include "Ruins/RuinsModel.h"
#include "WorldGen/PointsOfInterest.h"

/** Dónde aparece un tesoro (GDD §7: marae, cuevas rituales y pecios; biblia §9.1). */
enum class EArtifactProvenance : uint8
{
	Marae,       // «marae»: sobre el altar de una ruina de isla.
	RitualCave,  // «cueva_ritual»: en una cueva ritual.
	Shipwreck,   // «pecio»: en el pecio del velero de Arenas Blancas.
	SunkenRuin,  // «ruina_sumergida»: bajo el agua junto a una ruina; solo con la marea viva extrema.
	Count
};

/** Id del JSON («marae», «cueva_ritual», «pecio», «ruina_sumergida»). */
EXPLORED_API const TCHAR* LexToString(EArtifactProvenance Provenance);

enum class EArtifactRarity : uint8
{
	Common,  // «comun»
	Rare,    // «raro»
	Unique,  // «unico»
	Count
};

EXPLORED_API const TCHAR* LexToString(EArtifactRarity Rarity);

/** Tamaño para exponerlo: un hueco admite su tamaño y los menores. */
enum class EArtifactSize : uint8
{
	Small,   // «Pequeno»: anzuelos, colgantes, figuras de mano.
	Medium,  // «Mediano»: collares largos, cartas de varillas.
	Large,   // «Grande»: remos, tapa.
	Count
};

EXPLORED_API const TCHAR* LexToString(EArtifactSize Size);

/** Definición de un tesoro (Content/Data/artifacts.json). */
struct EXPLORED_API FArtifactDef
{
	FName Id;
	/** Tipo de story_es.json/artifact_kinds («hook_bone», «stick_chart»…). */
	FName Kind;
	FString NameEs;
	FString NameEn;
	EArtifactProvenance Provenance = EArtifactProvenance::Marae;
	EArtifactRarity Rarity = EArtifactRarity::Common;
	EArtifactSize Size = EArtifactSize::Small;
	/** Malla de Tools/Blender/props/tesoros.py («SM_Treasure_…»). */
	FName Mesh;
};

/** Un hueco de un mueble de exposición. Posición local en centímetros. */
struct EXPLORED_API FDisplaySlotDef
{
	EArtifactSize MaxSize = EArtifactSize::Small;
	FVector Offset = FVector::ZeroVector;
};

/** Mueble de exposición: estantería, vitrina o panel (piezas de building_pieces.json). */
struct EXPLORED_API FDisplayDef
{
	/** Id de la pieza de construcción («estanteria_museo», «vitrina_museo»…). */
	FName Id;
	TArray<FDisplaySlotDef> Slots;
};

/** Catálogo de tesoros y muebles: los datos de artifacts.json ya parseados. */
struct EXPLORED_API FTreasureCatalog
{
	TArray<FArtifactDef> Artifacts;
	TArray<FDisplayDef> Displays;

	/** Parseo de los ids del JSON; devuelven false si el texto no es válido. */
	static bool ParseProvenance(const FString& Text, EArtifactProvenance& Out);
	static bool ParseRarity(const FString& Text, EArtifactRarity& Out);
	static bool ParseSize(const FString& Text, EArtifactSize& Out);

	const FArtifactDef* FindArtifact(FName Id) const;
	const FDisplayDef* FindDisplay(FName Id) const;
	/** Algún mueble tiene un hueco donde cabe el tesoro. */
	bool CanEverDisplay(const FArtifactDef& Artifact) const;
};

/** Dónde está cada tesoro en el mundo (metros). */
struct EXPLORED_API FArtifactPlacement
{
	FName ArtifactId;
	/** Lugar de procedencia: id de ruina («ruin_emerald»…) o «shipwreck». */
	FName PlaceId;
	int32 IslandIndex = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
	bool bUnderwater = false;
	/** Solo accesible con la marea viva extrema (ruinas sumergidas). */
	bool bNeedsSpringTide = false;
};

/** Reparte los tesoros por las ruinas, cuevas y pecios de forma determinista. */
struct EXPLORED_API FTreasurePlacement
{
	static TArray<FArtifactPlacement> Generate(const FRuinsLayout& Ruins, const TArray<FPointOfInterest>& Pois,
		const FTreasureCatalog& Catalog);
};

/** Qué sabe el jugador de un tesoro: hallado, fotografiado y de dónde viene. */
struct EXPLORED_API FArtifactRecord
{
	FName ArtifactId;
	bool bFound = false;
	bool bPhotographed = false;
	/** Procedencia real: dónde se recogió (id de lugar) y en qué isla. */
	FName FoundAt;
	int32 FoundIsland = INDEX_NONE;

	bool operator==(const FArtifactRecord& O) const
	{
		return ArtifactId == O.ArtifactId && bFound == O.bFound && bPhotographed == O.bPhotographed && FoundAt == O.FoundAt && FoundIsland == O.FoundIsland;
	}
};

/** Un mueble de exposición construido en la base. */
struct EXPLORED_API FDisplayState
{
	/** Clave del mueble colocado (la da la construcción; única en la partida). */
	int32 Key = INDEX_NONE;
	FName DisplayId;
	/** Tesoro de cada hueco (NAME_None si está vacío). */
	TArray<FName> Slots;

	bool operator==(const FDisplayState& O) const { return Key == O.Key && DisplayId == O.DisplayId && Slots == O.Slots; }
};

/** Estado guardable del catálogo y del museo: solo datos planos. */
struct EXPLORED_API FMuseumState
{
	TArray<FArtifactRecord> Records;
	TArray<FDisplayState> Displays;

	bool operator==(const FMuseumState& O) const { return Records == O.Records && Displays == O.Displays; }
};

enum class EMuseumResult : uint8
{
	Ok,
	UnknownArtifact,
	NotFound,          // Aún no se ha recogido.
	AlreadyExhibited,
	UnknownDisplay,    // No hay mueble con esa clave o el tipo no existe.
	DuplicateDisplay,  // Ya hay un mueble con esa clave.
	InvalidSlot,
	SlotOccupied,
	SlotEmpty,
	TooLarge,
};

EXPLORED_API const TCHAR* LexToString(EMuseumResult Result);

/**
 * Tesoros, catálogo y museo de la base (GDD §7, §8.11; biblia §7.4 y §10).
 * Cada tesoro es único: se recoge una vez, se puede fotografiar (lo registra en el
 * catálogo aunque no se recoja) y se expone en un hueco fijo de un mueble.
 */
class EXPLORED_API FMuseumModel
{
public:
	/** Logro «Coleccionista»: museo con 10 tesoros expuestos (GDD §16). */
	static constexpr int32 CollectorThreshold = 10;

	FMuseumModel() = default;
	explicit FMuseumModel(FTreasureCatalog InCatalog);

	const FTreasureCatalog& GetCatalog() const { return Catalog; }

	/** Recoge un tesoro. Devuelve true si es la primera vez. */
	bool MarkFound(FName ArtifactId, FName PlaceId, int32 IslandIndex);
	/** Registra una foto de la cámara desechable. Devuelve true si es nueva. */
	bool MarkPhotographed(FName ArtifactId);

	bool IsFound(FName ArtifactId) const;
	bool IsPhotographed(FName ArtifactId) const;
	/** Registrado en el catálogo: hallado o fotografiado (el resto sale en silueta). */
	bool IsRegistered(FName ArtifactId) const { return IsFound(ArtifactId) || IsPhotographed(ArtifactId); }
	/** Registro del tesoro o nullptr si no se sabe nada de él. */
	const FArtifactRecord* FindRecord(FName ArtifactId) const;

	/** Añade un mueble construido (vacío). */
	EMuseumResult AddDisplay(int32 Key, FName DisplayId);
	/** Quita un mueble; los tesoros que tenía vuelven al jugador (OutReturned). */
	EMuseumResult RemoveDisplay(int32 Key, TArray<FName>& OutReturned);

	EMuseumResult CanPlace(FName ArtifactId, int32 Key, int32 Slot) const;
	EMuseumResult Place(FName ArtifactId, int32 Key, int32 Slot);
	/** Retira el tesoro de un hueco (OutArtifact) y lo devuelve al jugador. */
	EMuseumResult Remove(int32 Key, int32 Slot, FName& OutArtifact);
	/** Primer hueco libre donde cabe el tesoro, en orden de muebles y huecos. */
	bool FindFreeSlot(FName ArtifactId, int32& OutKey, int32& OutSlot) const;

	bool IsExhibited(FName ArtifactId) const;
	int32 CountExhibited() const;
	bool HasCollectorAchievement() const { return CountExhibited() >= CollectorThreshold; }

	int32 CountFound() const;
	int32 CountRegistered() const;
	/** Fracción del catálogo registrada (0–1). */
	float CatalogCompletion() const;
	bool IsCatalogComplete() const;

	const FDisplayState* FindDisplay(int32 Key) const;
	const TArray<FDisplayState>& GetDisplays() const { return State.Displays; }

	const FMuseumState& GetState() const { return State; }
	/** Carga un estado guardado descartando tesoros y muebles desconocidos y huecos incoherentes. */
	void LoadState(const FMuseumState& Saved);

private:
	FArtifactRecord* FindOrAddRecord(FName ArtifactId);
	FDisplayState* FindDisplayMutable(int32 Key);

	FTreasureCatalog Catalog;
	FMuseumState State;
};
