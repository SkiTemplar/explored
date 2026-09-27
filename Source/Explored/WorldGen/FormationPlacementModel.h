#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"

class FTerrainDensity;

/**
 * Colocación determinista del kit de rocas y acantilados (docs/diseno/exploracion.md §4.3,
 * manifiesto Art/Export/Props/manifest.json: grupos «AcantiladoFormaciones» y
 * «AcantiladoBloques», importados a /Game/Generated/Meshes/<grupo>/<nombre> por
 * Tools/Unreal/import_props.py). Modelo puro: solo depende de CoreMinimal.h y de
 * FTerrainDensity (adelantada, sin incluirla aquí), igual que FVegetationScatter.
 *
 * No sustituye a FVegetationScatter ni a FPoiLayout: coloca formaciones geológicas
 * (paredes, espolones, farallones, el arco marino, bloques y losas), no vegetación
 * ni puntos de interés jugables.
 */

/** Tipo de formación, uno por filtro de nombre dentro de su grupo del manifiesto. */
enum class EFormationKind : uint8
{
	CliffWall,     // AcantiladoFormaciones/CliffWall_Basalt* o CliffWall_Sandstone*: pared en ladera empinada.
	CliffSpur,     // AcantiladoFormaciones/CliffSpur*: espolón menor, rompe la silueta sin alicatarla.
	SeaStack,      // AcantiladoFormaciones/SeaStack*: farallón en agua somera junto a un cayo rocoso.
	SeaArch,       // AcantiladoFormaciones/SeaArch01: arco marino sobre una cueva-arco ya generada en Los Dientes.
	Boulder,       // AcantiladoBloques/RockBoulder*: canto grande, pie de pared o extremo rocoso de playa.
	Cobble,        // AcantiladoBloques/RockCobble*: canto pequeño, relleno de detalle.
	LimestoneSlab, // AcantiladoBloques/LimestoneSlab*: losa suelta en la meseta.
};

EXPLORED_API const TCHAR* LexToString(EFormationKind Kind);

/** Instancia resultante (transformación en centímetros, espacio de mundo, como FScatterInstance). */
struct EXPLORED_API FFormationInstance
{
	EFormationKind Kind = EFormationKind::CliffWall;
	/**
	 * Filtro de subcadena dentro del grupo del manifiesto que resuelve la malla candidata
	 * (ver ResolveFormationMeshes en WorldGenCommandlet.cpp): p.ej. «CliffWall_Basalt»,
	 * «CliffWall_Sandstone», «CliffSpur», «SeaStack», «SeaArch», «RockBoulder», «RockCobble»,
	 * «LimestoneSlab». Vacío solo si el tipo no tiene variantes con nombre distinto.
	 */
	FName MeshFilter;
	/** Grupo del manifiesto: «AcantiladoFormaciones» o «AcantiladoBloques». */
	FName ManifestGroup;
	/** Índice determinista dentro de las mallas candidatas que casan con MeshFilter (módulo el recuento real). */
	int32 VariantIndex = 0;
	FTransform Transform;
};

/** Parámetros ajustables de la pasada (recuentos y umbrales), expuestos para tests y ajuste fino. */
struct EXPLORED_API FFormationPlacementParams
{
	/** Pendiente mínima (grados desde la horizontal) para colocar una pared o un espolón. */
	float MinCliffSlopeDeg = 55.0f;
	/** Pendiente mínima para un afloramiento rocoso en el extremo de una playa (más suave que un acantilado). */
	float MinBeachRockSlopeDeg = 32.0f;
	float MaxBeachRockSlopeDeg = 55.0f;
	/** Separación de la rejilla candidata: fina, para no perderse los tramos de 55°+, que son estrechos. */
	float WallSpacing = 30.0f;
	/** Umbral de ruido de agrupación: más alto, más raras (0..1 tras EdgeFade). Bajo a propósito:
	 * el propio requisito de pendiente ya hace escasos los candidatos («menos es más»); un umbral
	 * alto además del filtro de pendiente los dejaba casi a cero. */
	float WallClusterThreshold = 0.05f;
	float WallClusterScale = 90.0f;
	/**
	 * Cuánto se hunde la formación en la ladera para que no flote (m, a lo largo de -Normal
	 * horizontal). Corto a propósito: un espolón puede tener una cresta estrecha, y un
	 * hundimiento largo puede cruzarla y caer en la ladera opuesta, mucho menos empinada.
	 */
	float WallEmbed = 0.9f;
	/** Tope de paredes/espolones por isla: «menos es más» (exploracion.md §4.3). */
	int32 MaxWallsPerIsland = 14;

	/** Franja de agua somera para los farallones (m, negativos = bajo el nivel del mar). */
	float StackMinDepth = -8.0f;
	float StackMaxDepth = -1.0f;
	int32 StackMinPerCay = 2;
	int32 StackMaxPerCay = 4;

	/** Bloques y cantos por pared/espolón aceptado, y por punto rocoso de playa. */
	int32 DebrisPerWall = 3;
	float DebrisSpreadRadius = 11.0f;
	int32 DebrisPerBeachPoint = 2;
	float BeachRockSpacing = 55.0f;
	float BeachRockClusterThreshold = 0.3f;

	/** Losas de caliza: separación y umbral de agrupación en la meseta. */
	float SlabSpacing = 32.0f;
	float SlabClusterThreshold = 0.15f;
	int32 MaxSlabs = 40;
	/** Franja de altura de la meseta (fracción de Island.MaxHeight) donde se considera «llano alto». */
	float SlabMinHeightFrac = 0.25f;
	float SlabMaxHeightFrac = 0.9f;
	float SlabMinNormalZ = 0.85f;

	/** Radio en torno a cada punto de exclusión (spawn, POI, rutas) que ninguna formación puede invadir (m). */
	float AvoidRadius = 22.0f;
};

/** Coloca formaciones rocosas deterministas a partir del campo de densidad ya generado. */
struct EXPLORED_API FFormationPlacementModel
{
	/**
	 * @param AvoidPoints Puntos a proteger en metros (spawn de Landing, POI, anclas de rutas):
	 *        ninguna formación se coloca a menos de Params.AvoidRadius de ninguno de ellos.
	 */
	static TArray<FFormationInstance> Generate(const FTerrainDensity& Density, uint32 Seed,
		const TArray<FVector>& AvoidPoints, const FFormationPlacementParams& Params = FFormationPlacementParams());
};
