#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"

class FTerrainDensity;

/** Regla de colocación de una especie vegetal o de rocas. Distancias en metros. */
struct EXPLORED_API FScatterRule
{
	FName Species;
	/** Categoría del manifest de mallas (tree, palm, shrub, rock, grass) y filtro opcional por nombre. */
	FString ManifestCategory;
	FString NameFilter;
	/** Mallas candidatas (se elige una por instancia); las resuelve el horneado desde el manifest. */
	TArray<FSoftObjectPath> Meshes;
	/** Peso por arquetipo de isla (0 = no aparece). */
	TMap<EIslandArchetype, float> IslandWeight;
	float MinHeight = 0.5f;
	float MaxHeight = 500.0f;
	/** Normal Z mínima (1 = llano, 0 = vertical). */
	float MinNormalZ = 0.7f;
	/** Separación media entre instancias. */
	float Spacing = 8.0f;
	/** Escala del ruido de agrupación y umbral (−1..1) por encima del cual hay instancias. */
	float ClusterScale = 60.0f;
	float ClusterThreshold = -0.2f;
	float MinScale = 0.8f;
	float MaxScale = 1.2f;
	/** 0 = vertical, 1 = alineado con la normal del terreno. */
	float AlignToNormal = 0.0f;
	/** Hundimiento para que la base no flote (m). */
	float Sink = 0.3f;
	/** Inclinación máxima hacia el mar (grados), típica de las palmeras. */
	float LeanTowardsSea = 0.0f;
	/** Sin colisión (hierba, helechos pequeños). */
	bool bNoCollision = false;
	/** Distancia de desaparición en metros (0 = siempre visible; evitarlo salvo casos puntuales). */
	float CullDistance = 0.0f;
	/** Sombra dinámica; false para clutter pequeño (hierba, restos) que no aporta silueta. */
	bool bCastShadow = true;
};

/** Instancia resultante (transformación en centímetros, espacio de mundo). */
struct EXPLORED_API FScatterInstance
{
	int32 MeshIndex = 0;
	FTransform Transform;
};

struct EXPLORED_API FScatterResult
{
	/** Por regla: instancias. */
	TArray<TArray<FScatterInstance>> PerRule;
	int32 Total() const;
};

/** Genera la vegetación de forma determinista a partir del campo de densidad. */
struct EXPLORED_API FVegetationScatter
{
	/** Reglas por defecto del archipiélago (rutas de /Game/Generated/Meshes). */
	static TArray<FScatterRule> DefaultRules();

	/**
	 * Coloca las instancias dentro del rectángulo (metros).
	 *
	 * AvoidPoints (metros, XY) y ClearRadiusM excluyen instancias alrededor del punto de aparición
	 * y de los puntos de interés: sin esto el jugador puede aparecer dentro de un arbusto o helecho
	 * de primer plano (ClearRadiusM=0, el valor por defecto, no filtra nada).
	 */
	static FScatterResult Generate(const FTerrainDensity& Density, const TArray<FScatterRule>& Rules,
		const FBox2D& Region, uint32 Seed, const TArray<FVector>& AvoidPoints = {}, float ClearRadiusM = 0.0f);

	/**
	 * Altura de la superficie más alta en (X, Y) según la densidad exacta (con
	 * voladizos). Devuelve false si no hay superficie cerca de la altura de la columna.
	 */
	static bool FindSurface(const FTerrainDensity& Density, float X, float Y, float& OutZ, FVector& OutNormal);
};
