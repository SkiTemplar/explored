#pragma once

#include "CoreMinimal.h"
#include "WorldGen/SurfaceNets.h"

class FTerrainDensity;

/** Parámetros de troceado del terreno. */
struct EXPLORED_API FTerrainChunkSettings
{
	/** Celdas por lado de chunk. */
	int32 CellsPerChunk = 32;
	/** Tamaño de vóxel en metros. */
	float VoxelSize = 2.0f;

	float ChunkSizeMeters() const { return CellsPerChunk * VoxelSize; }
};

/** Construye la malla de un chunk a partir del campo de densidad. */
struct EXPLORED_API FTerrainChunkBuilder
{
	/** Origen del chunk en metros (esquina mínima). */
	static FVector ChunkOrigin(const FIntVector& Coord, const FTerrainChunkSettings& Settings);

	/**
	 * Evalúa la densidad y poligoniza. Las posiciones se devuelven en
	 * centímetros relativas al origen del chunk; normales y colores rellenos.
	 */
	static FTerrainMeshData Build(const FTerrainDensity& Density, const FIntVector& Coord,
		const FTerrainChunkSettings& Settings);

	/** Coordenadas de chunk (Z incluida) que pueden contener superficie. */
	static TArray<FIntVector> FindCandidateChunks(const FTerrainDensity& Density,
		const FTerrainChunkSettings& Settings, const FBox2D& WorldRectMeters);
};
