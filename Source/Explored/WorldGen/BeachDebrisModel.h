#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"

class FTerrainDensity;

/**
 * Microdetalle de playa (docs/diseno/exploracion.md §4.4): conchas, cocos, troncos a la deriva
 * y algas en la línea de marea, con densidad baja y agrupados, nunca uniformes. Modelo puro,
 * como FVegetationScatter y FFormationPlacementModel: solo depende de CoreMinimal.h y de
 * FTerrainDensity (adelantada).
 *
 * Reutiliza mallas ya existentes de Art/Export/Meshes/manifest.json (categorías «palm» y
 * «debris») donde ya hay asset. Las categorías «shell» y «seaweed» se declaran sin malla
 * todavía; igual que FVegetationScatter, Generate() no depende de si Meshes está resuelto
 * (así se puede probar en el host con FBeachDebrisModel::DefaultRules() sin resolver nada
 * contra el editor): quien hornea filtra antes, con Rules.RemoveAll si Meshes está vacío
 * (mismo patrón que ResolveScatterMeshes en WorldGenCommandlet.cpp). Añadir esas mallas el
 * día que existan no requiere tocar este fichero, solo el manifiesto.
 */

struct EXPLORED_API FBeachDebrisRule
{
	FName Species;
	/** Categoría y filtro de nombre del manifiesto de mallas (Art/Export/Meshes/manifest.json). */
	FString ManifestCategory;
	FString NameFilter;
	/** Mallas candidatas; las resuelve el horneado. Vacío = la regla no produce instancias. */
	TArray<FSoftObjectPath> Meshes;
	/** Peso por arquetipo de isla (0 = no aparece en esa isla). */
	TMap<EIslandArchetype, float> IslandWeight;
	float MinScale = 0.6f;
	float MaxScale = 1.15f;
	/** 0 = siempre vertical (cocos), 1 = tumbado siguiendo la arena (troncos). */
	float AlignToNormal = 0.0f;
};

/** Instancia resultante (transformación en centímetros, espacio de mundo, como FScatterInstance). */
struct EXPLORED_API FBeachDebrisInstance
{
	FName Species;
	int32 MeshIndex = 0;
	FTransform Transform;
};

struct EXPLORED_API FBeachDebrisModel
{
	/** Reglas por defecto del archipiélago. */
	static TArray<FBeachDebrisRule> DefaultRules();

	/**
	 * Coloca microdetalle en la franja de marea de todo el archipiélago.
	 * @param AvoidPoints Puntos a proteger en metros (spawn, POI, rutas); ninguna instancia se
	 *        coloca a menos de AvoidRadius de ninguno de ellos.
	 */
	static TArray<FBeachDebrisInstance> Generate(const FTerrainDensity& Density, const TArray<FBeachDebrisRule>& Rules,
		uint32 Seed, const TArray<FVector>& AvoidPoints, float AvoidRadius = 15.0f);
};
