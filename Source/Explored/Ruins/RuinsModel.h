#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/PointsOfInterest.h"

/** Técnicas de navegación tradicional que enseñan las ruinas (GDD §6.2, biblia §9.2). */
enum class EWayfindingTechnique : uint8
{
	StarPath,      // Camino de estrellas: rumbo nocturno hacia otra isla concreta.
	SwellReading,  // Lectura del oleaje: distancia y dirección a tierra por el mar de fondo.
	BirdsAtDusk,   // Aves al atardecer: las bandadas vuelven a la isla más cercana.
	FixedClouds,   // Nubes fijas: una nube estacionaria delata una isla lejana y alta.
	WaterColour,   // Color del agua: el cambio de tono marca bajíos y arrecifes.
	Count
};

/** Id estable (el mismo de Content/Data/ruins.json): «star_path», «swell_reading»… */
EXPLORED_API const TCHAR* LexToString(EWayfindingTechnique Technique);

/** Qué hay que descubrir en una ruina (GDD §6.1). */
enum class ERuinElementKind : uint8
{
	Petroglyph,       // Petroglifo del catálogo (id «petro_NN» de FPoiLayout).
	StatueAlignment,  // Estatua: se completa al mirar hacia donde mira ella.
	AltarOffering,    // Altar del marae: se completa al dejar una ofrenda.
	StarCompass,      // Brújula estelar de la cumbre del Humo.
	DoubleCanoe,      // Canoa doble fosilizada (Arenas Blancas).
	RitualCave,       // Cueva ritual con pinturas y ofrendas.
	Count
};

/** Id estable (el mismo de Content/Data/ruins.json): «petroglyph», «statue»… */
EXPLORED_API const TCHAR* LexToString(ERuinElementKind Kind);

/** Un elemento descubrible de una ruina. Posiciones en metros. */
struct EXPLORED_API FRuinElement
{
	/** Id del descubrimiento: el ContentId del petroglifo o «<ruina>_<tipo>». */
	FName Id;
	ERuinElementKind Kind = ERuinElementKind::Petroglyph;
	/**
	 * Posición aproximada. Los petroglifos, la brújula y la cueva usan la del punto
	 * de interés; estatua, altar y canoa se colocan alrededor del ancla de la ruina y
	 * la capa de Unreal los asienta en el suelo con una traza vertical.
	 */
	FVector Location = FVector::ZeroVector;
	/** Orientación (grados, como el yaw de Unreal): la estatua mira a su rumbo. */
	float Yaw = 0.0f;
	/** Los petroglifos no son obligatorios uno a uno: cuenta FRuinSite::RequiredPetroglyphs. */
	bool bRequired = true;
};

/** Una ruina del pueblo navegante con lo que enseña al completarla. */
struct EXPLORED_API FRuinSite
{
	/** «ruin_<isla>» (p. ej. «ruin_emerald») o «ruin_compass» para la brújula estelar. */
	FName Id;
	int32 IslandIndex = INDEX_NONE;
	/** Centro de la ruina (metros). */
	FVector Anchor = FVector::ZeroVector;
	TArray<FRuinElement> Elements;
	/** Cuántos petroglifos de la ruina (cualquiera) hay que descubrir además de los obligatorios. */
	int32 RequiredPetroglyphs = 0;
	EWayfindingTechnique Teaches = EWayfindingTechnique::StarPath;
	/** Solo para caminos de estrellas: isla destino o FRuinsLayout::HiddenIslandIndex. */
	int32 StarPathTarget = INDEX_NONE;

	int32 CountPetroglyphs() const;
	int32 FindElement(FName ElementId) const;
};

/**
 * Ruinas del archipiélago y su asignación de técnicas, derivadas de forma
 * determinista de la disposición de islas y de los puntos de interés:
 *
 * - una ruina (marae) por isla, con estatua, altar, los petroglifos de esa isla y el
 *   rasgo propio de la isla (cueva ritual, canoa doble…);
 * - la brújula estelar de la cumbre del Humo, que siempre enseña el camino de
 *   estrellas hacia la isla oculta;
 * - las cuatro técnicas que no son caminos de estrellas van a cuatro ruinas de isla
 *   barajadas con la semilla; el resto de ruinas enseña caminos de estrellas, así que
 *   siempre se pueden aprender las cinco técnicas y hay al menos RequiredStarPaths
 *   caminos (con siete islas hay tres, más el de la brújula).
 */
struct EXPLORED_API FRuinsLayout
{
	/** Caminos de estrellas necesarios para navegar hasta la isla oculta (GDD §6.3). */
	static constexpr int32 RequiredStarPaths = 3;
	/** Petroglifos (cualquiera de la ruina) que exige completar una ruina de isla. */
	static constexpr int32 PetroglyphsPerSite = 2;
	/** Destino de un camino de estrellas que lleva a la isla oculta. */
	static constexpr int32 HiddenIslandIndex = -2;
	/** Distancia (en el eje dominante) a la que queda la isla oculta: fuera del mapa jugable. */
	static constexpr float HiddenIslandDistance = FArchipelagoLayout::WorldHalfExtent * 1.3f;
	/** Altura mínima de una isla para que su cumbre retenga una nube fija. */
	static constexpr float FixedCloudMinHeight = 100.0f;

	uint32 Seed = 0;
	TArray<FRuinSite> Sites;
	/** Copia de las islas de la disposición: rumbos, cumbres y bajíos de las anotaciones. */
	TArray<FIslandDesc> Islands;
	/** Isla oculta (arrecife fósil): fuera del mapa, en el lado de mar más abierto. */
	FVector2D HiddenIslandCenter = FVector2D::ZeroVector;
	/** Isla desde la que se zarpa hacia la isla oculta (la más cercana a ella). */
	int32 DepartureIsland = INDEX_NONE;

	static FRuinsLayout Generate(const FArchipelagoLayout& Layout, const TArray<FPointOfInterest>& Pois);

	int32 FindSite(FName SiteId) const;
	/** Ruina a la que pertenece un elemento, o INDEX_NONE. */
	int32 FindSiteOfElement(FName ElementId) const;
	/** Cuántas ruinas enseñan un camino de estrellas. */
	int32 CountStarPathSites() const;
	/** Cuántas ruinas enseñan la técnica. */
	int32 CountSitesTeaching(EWayfindingTechnique Technique) const;
	/** Centro de una isla o de la isla oculta. */
	FVector2D TargetCenter(int32 Target) const;
};

/**
 * Anotación que una técnica añade al mapa dibujado a mano (GDD §6.2). Es un dato
 * plano para la cartografía: qué técnica, de dónde a dónde y con qué rumbo.
 */
struct EXPLORED_API FWayfindingAnnotation
{
	EWayfindingTechnique Technique = EWayfindingTechnique::StarPath;
	/** Ruina que la enseñó. */
	FName SourceSite;
	/** Isla de origen (INDEX_NONE si la anotación es de una sola isla). */
	int32 FromIsland = INDEX_NONE;
	/** Isla señalada, o FRuinsLayout::HiddenIslandIndex. */
	int32 TargetIsland = INDEX_NONE;
	/** Punto de partida y punto señalado (metros, plano del mapa). */
	FVector2D From = FVector2D::ZeroVector;
	FVector2D To = FVector2D::ZeroVector;
	/** Rumbo de From a To en grados [0, 360): horario desde +X (el yaw de Unreal). */
	float BearingDegrees = 0.0f;
	/** Distancia de From a To (metros); en nubes y color del agua, el radio de la marca. */
	float DistanceMeters = 0.0f;
};

/** Estado guardable de las ruinas: solo datos planos. */
struct EXPLORED_API FRuinsState
{
	/** Elementos descubiertos en el orden en que se descubrieron. */
	TArray<FName> DiscoveredElements;
	/** Ruinas completadas en el orden en que se completaron. */
	TArray<FName> CompletedSites;

	bool operator==(const FRuinsState& Other) const
	{
		return DiscoveredElements == Other.DiscoveredElements && CompletedSites == Other.CompletedSites;
	}
};

/** Resultado de registrar un descubrimiento. */
struct EXPLORED_API FRuinDiscovery
{
	/** El elemento pertenece a alguna ruina y no se había descubierto. */
	bool bNew = false;
	int32 SiteIndex = INDEX_NONE;
	/** Este descubrimiento completa la ruina. */
	bool bSiteCompleted = false;
	/** La técnica que enseña la ruina completada se aprende por primera vez. */
	bool bTechniqueLearned = false;
	EWayfindingTechnique Technique = EWayfindingTechnique::StarPath;
	/** Camino de estrellas aprendido (isla destino) si la ruina enseña uno. */
	int32 StarPathTarget = INDEX_NONE;
};

/** Reglas de las ruinas y del wayfinding de una partida. */
class EXPLORED_API FRuinsModel
{
public:
	FRuinsModel() = default;
	explicit FRuinsModel(FRuinsLayout InLayout);

	const FRuinsLayout& GetLayout() const { return Layout; }

	/** Registra un descubrimiento (petroglifo, estatua alineada, ofrenda…). */
	FRuinDiscovery Discover(FName ElementId);
	bool IsDiscovered(FName ElementId) const { return State.DiscoveredElements.Contains(ElementId); }

	bool IsSiteComplete(int32 SiteIndex) const;
	/** Obligatorios descubiertos y total de obligatorios (petroglifos incluidos). */
	void GetSiteProgress(int32 SiteIndex, int32& OutDone, int32& OutRequired) const;

	bool KnowsTechnique(EWayfindingTechnique Technique) const;
	int32 CountKnownTechniques() const;
	/** Logro «Wayfinder»: las cinco técnicas. */
	bool KnowsAllTechniques() const { return CountKnownTechniques() == static_cast<int32>(EWayfindingTechnique::Count); }

	/** Caminos de estrellas reunidos (uno por ruina completada que enseña uno). */
	int32 CountStarPaths() const;
	bool HasStarPathTo(int32 Target) const;

	/**
	 * Hay suficientes caminos de estrellas para llegar a la isla oculta. La noche
	 * despejada y el barco «Limón» los comprueba quien llama.
	 */
	bool CanSailToHiddenIsland() const { return CountStarPaths() >= FRuinsLayout::RequiredStarPaths; }

	/** Anotaciones de todas las técnicas aprendidas, en orden estable. */
	TArray<FWayfindingAnnotation> BuildAnnotations() const;
	/** Anotaciones que aporta una ruina concreta (aunque no esté completa). */
	TArray<FWayfindingAnnotation> AnnotationsForSite(int32 SiteIndex) const;

	const FRuinsState& GetState() const { return State; }
	/** Carga un estado guardado: descarta ids desconocidos y recalcula las ruinas completas. */
	void LoadState(const FRuinsState& Saved);

	/** Rumbo de A a B en grados [0, 360), horario desde +X (el yaw de Unreal). */
	static float BearingDegrees(const FVector2D& From, const FVector2D& To);

private:
	bool IsSiteCompleteFromDiscoveries(int32 SiteIndex) const;

	FRuinsLayout Layout;
	FRuinsState State;
};
