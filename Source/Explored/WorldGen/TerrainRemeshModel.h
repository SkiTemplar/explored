#pragma once

#include "CoreMinimal.h"
#include "WorldGen/SurfaceNets.h"
#include "WorldGen/TerrainChunkBuilder.h"
#include "WorldGen/TerrainEditModel.h"

class FTerrainDensity;

/** Delta de una muestra de la rejilla de un chunk de edición (índice de `FDensityGrid`). */
struct EXPLORED_API FTerrainGridDelta
{
	int32 Index = 0;
	float Delta = 0.0f;
};

/**
 * Muestras globales que lee la rejilla de un chunk de edición: la primera y cuántas por eje.
 * La normal es [C·N − 1, C·N + N] (N + 2 muestras, convenio de BuildChunkGrid). En la cara
 * baja de un chunk de render sustituido se alarga hacia fuera una celda horneada (faldón):
 * la malla horneada del vecino acaba dentro de su última celda de 2 m y, sin faldón, entre
 * su último vértice y la malla fina quedaría una rendija por la que se ve el vacío.
 */
struct EXPLORED_API FTerrainGridWindow
{
	FIntVector First = FIntVector::ZeroValue;
	FIntVector Dims = FIntVector::ZeroValue;
};

/**
 * Remallado en tiempo de ejecución de los chunks de edición (8 m, rejilla de 0,25 m), puro
 * y seguro entre hilos salvo `GatherDeltas`, que lee el modelo y va en el hilo de juego
 * (docs/tecnico/terreno-editable.md, GDD v2 §7.3):
 *
 * 1. En el hilo de juego, `GatherDeltas` copia los deltas que lee la rejilla del chunk
 *    (como mucho sus 27 vecinos; cuesta lo que haya editado alrededor, no 34³).
 * 2. En una tarea, `BuildBaseGrid` evalúa el campo procedural (una columna por (X, Y)) —
 *    es lo caro y no cambia nunca, así que el llamante lo cachea —, `ApplyDeltas` suma los
 *    deltas y `BuildMesh` poligoniza con Surface Nets. Las normales salen del gradiente de
 *    la propia rejilla (sin evaluar más ruido); el color y las capas, de `FTerrainDensity`
 *    con el mismo convenio que `FTerrainChunkBuilder::Build`.
 *
 * Sustitución de un chunk horneado de 64 m: `SurfaceEditChunks` dice qué chunks de edición
 * de dentro pueden tener superficie (retícula de 2 m con margen) para mallarlos todos a
 * 0,25 m y ocultar el horneado sin dejar agujeros.
 */
class EXPLORED_API FTerrainRemeshModel
{
public:
	/**
	 * Margen (m de densidad) de la retícula gruesa: una celda de 2 m cuyas esquinas estén
	 * todas a más de esto del cero no contiene superficie aunque el ruido 3D tenga
	 * pendiente de hasta ~2 (la esquina más lejana está a 1,73 m).
	 */
	static constexpr float SurveyMargin = 4.0f;

	/** Chunks de edición por lado de un chunk de render (64 m / 8 m = 8); 0 si no encajan. */
	static int32 EditChunksPerRenderChunk(const FTerrainChunkSettings& Render, const FTerrainEditSettings& Edit);
	/** Chunk de render que contiene el chunk de edición. */
	static FIntVector RenderChunkOf(const FIntVector& EditChunk, int32 EditChunksPerRender);
	/** Esquina mínima del chunk de edición (m). */
	static FVector EditChunkOrigin(const FIntVector& EditChunk, const FTerrainEditSettings& Edit);

	/** Ventana normal del chunk: N + 2 muestras por eje. */
	static FTerrainGridWindow ChunkWindow(const FIntVector& Chunk, const FTerrainEditSettings& Edit);
	/**
	 * Ventana con faldón: en cada eje en el que el chunk es el primero de su chunk de render
	 * (coordenada múltiplo de EditChunksPerRender), SkirtSamples muestras más hacia abajo.
	 */
	static FTerrainGridWindow ChunkWindow(const FIntVector& Chunk, const FTerrainEditSettings& Edit, int32 EditChunksPerRender,
		int32 SkirtSamples);

	/** Rejilla del campo procedural en la ventana (una columna por (X, Y)). */
	static void BuildBaseGrid(const FTerrainDensity& Density, const FTerrainEditSettings& Edit, const FTerrainGridWindow& Window,
		FDensityGrid& Out);
	/** Deltas no nulos que lee la ventana (sin orden; índices sin repetir). Hilo de juego. */
	static void GatherDeltas(const FTerrainEditModel& Model, const FTerrainGridWindow& Window, TArray<FTerrainGridDelta>& Out);
	/** Lo mismo con la ventana normal del chunk. */
	static void GatherDeltas(const FTerrainEditModel& Model, const FIntVector& Chunk, TArray<FTerrainGridDelta>& Out);
	/** Suma los deltas a una rejilla de ese chunk (índices fuera de rango se ignoran). */
	static void ApplyDeltas(const TArray<FTerrainGridDelta>& Deltas, FDensityGrid& InOut);

	/** Gradiente normalizado de la celda que contiene P (m), hacia el aire. */
	static FVector GridNormal(const FDensityGrid& Grid, const FVector& P);
	/**
	 * Malla del chunk: posiciones en cm relativas a OriginMeters, normales del gradiente y,
	 * con Attributes, color y capas de superficie. Vacía si la rejilla no tiene superficie.
	 */
	static FTerrainMeshData BuildMesh(const FDensityGrid& Grid, const FVector& OriginMeters, const FTerrainDensity* Attributes);

	/**
	 * Chunks de edición del chunk de render que pueden tener superficie en el campo
	 * procedural, ordenados por (Z, Y, X). Evalúa la retícula de 2 m del chunk de render
	 * (33³ muestras, una columna por (X, Y)). Pensado para una tarea de fondo.
	 */
	static void SurfaceEditChunks(const FTerrainDensity& Density, const FTerrainChunkSettings& Render,
		const FTerrainEditSettings& Edit, const FIntVector& RenderChunk, TArray<FIntVector>& Out);
	/**
	 * Chunks de edición de ese chunk de render que tienen ediciones o leen muestras
	 * editadas (los editados y sus vecinos dentro del chunk de render). Hilo de juego.
	 */
	static void EditedEditChunks(const FTerrainEditModel& Model, int32 EditChunksPerRender, const FIntVector& RenderChunk,
		TArray<FIntVector>& Out);

	/** Coordenadas del chunk horneado a partir del nombre «SM_Terrain_X_Y_Z»; false si no casa. */
	static bool ParseBakedChunkName(const FString& Name, FIntVector& OutCoord);
};
