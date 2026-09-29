#pragma once

#include "CoreMinimal.h"
#include "WorldGen/PlayabilityMetricsModel.h"
#include "WorldGen/TerrainMetricsModel.h"

class FTerrainDensity;

/** Costa de una isla medida con rayos desde su centro (ver FTerrainPlayabilitySurvey::MeasureCoast). */
struct EXPLORED_API FCoastStats
{
	/** Rayos que han encontrado la costa de la isla. */
	int32 Rays = 0;
	/** Rayos con pendiente > 60° en los primeros 20 m tierra adentro: acantilado marino. */
	int32 CliffRays = 0;
	float CliffFraction = 0.0f;
	/** Mediana y máximo de la altura de esos acantilados (cota más alta en los primeros 30 m), m. */
	float CliffMedianHeight = 0.0f;
	float CliffMaxHeight = 0.0f;
	/** Rumbo en el mundo (radianes, desde +X) de cada rayo con acantilado. */
	TArray<float> CliffAngles;
	/** Anchura de la plataforma (costa → -10 m) y del talud (-10 → -20 m) por rayo, m. */
	TArray<float> ShelfWidths;
	TArray<float> SlopeWidths;
	/** Variación a lo largo del perímetro (desviación / media) y dentado de la plataforma. */
	float ShelfWidthCV = 0.0f;
	float SlopeWidthCV = 0.0f;
	float ShelfJaggedness = 0.0f;
};

/** Agua tierra adentro de una isla (lagunas interiores): extensión, cota y fondo llano. */
struct EXPLORED_API FLagoonStats
{
	/** Superficie de agua dentro de la costa de la isla (T < InnerCoastT), m². */
	float Area = 0.0f;
	/** Profundidad mediana (m, positiva). */
	float MedianDepth = 0.0f;
	/** Llanos del fondo (pendiente < 15°) dentro de esa agua. */
	FFlatPatchStats Floor;
};

/** Jugabilidad de una isla: llanos construibles, costa y red de drenaje. */
struct EXPLORED_API FIslandPlayability
{
	/** Tierra por encima de 0,5 m con pendiente < 15°, muestreada cada SampleSpacing metros. */
	FFlatPatchStats Flat;
	FCoastStats Coast;
	FDrainagePattern Drainage;
	FLagoonStats Lagoon;
};

/** Informe de jugabilidad del archipiélago. */
struct EXPLORED_API FPlayabilityReport
{
	TArray<FIslandPlayability> Islands;
	/** Motas en aguas profundas: montículos del fondo y cayos sin plataforma que los una a su isla. */
	TArray<FVector2D> SeaMotes;
	/** Índice de Clark-Evans de las motas sobre el mundo entero (< 1 agrupadas, > 1 regulares). */
	float SeaMoteClarkEvans = 0.0f;
};

/**
 * Mide la jugabilidad del terreno de un FTerrainDensity: dónde se puede construir (llanos), qué
 * parte de la costa es acantilado, cómo varía la plataforma submarina a lo largo del perímetro,
 * si las motas del mar están repartidas en malla y si los ríos salen en estrella. Lo usan
 * TerrainRealismSpec y Tools/HostTests/tools/TerrainDiagnostics.cpp.
 */
class EXPLORED_API FTerrainPlayabilitySurvey
{
public:
	/** Rejilla de alturas alrededor de una isla (1,5 radios), cada Spacing metros. */
	static FTerrainSampleGrid SampleIsland(const FTerrainDensity& Density, int32 IslandIdx, float Spacing);

	/** Llanos (pendiente < MaxSlopeDeg) en la tierra de la isla por encima de 0,5 m. */
	static FFlatPatchStats MeasureBuildable(const FTerrainSampleGrid& Grid, int32 IslandIdx);

	/**
	 * Red de drenaje de la tierra de la isla (el resto se trata como mar). El agua somera tierra
	 * adentro (esteros, canales de marea) cuenta como cauce hasta la costa.
	 */
	static FDrainagePattern MeasureDrainage(const FTerrainDensity& Density, const FTerrainSampleGrid& Grid, int32 IslandIdx);

	/**
	 * Agua dentro de la costa de la isla (lagunas interiores, bahías cerradas, esteros): la parte
	 * de la rejilla bajo el mar con distancia normalizada < InnerCoastT.
	 */
	static FLagoonStats MeasureLagoon(const FTerrainDensity& Density, const FTerrainSampleGrid& Grid, int32 IslandIdx);

	/** Acantilados y plataforma a lo largo de RayCount rayos repartidos alrededor de la isla. */
	static FCoastStats MeasureCoast(const FTerrainDensity& Density, int32 IslandIdx, int32 RayCount);

	/** Montículos del fondo y cayos cuyo camino a la isla baja de -8 m (sueltos en el mar). */
	static TArray<FVector2D> SeaMotes(const FTerrainDensity& Density);

	/** Todo lo anterior para todas las islas. */
	static FPlayabilityReport Measure(const FTerrainDensity& Density, float SampleSpacing);

	static constexpr float BuildableSlopeDeg = 15.0f;
	static constexpr float CliffSlopeDeg = 60.0f;
	/** Tramo tierra adentro (m) en el que se busca la pared del acantilado. */
	static constexpr float CliffBand = 20.0f;
	/** Agua más adentro que esta distancia normalizada a la costa cuenta como laguna interior. */
	static constexpr float InnerCoastT = 0.9f;
};
