#pragma once

#include "CoreMinimal.h"
#include "WorldGen/TerrainMetricsModel.h"

class FTerrainDensity;

/** Métricas de realismo de una isla (ver FTerrainSurvey::Measure). */
struct EXPLORED_API FIslandRealism
{
	/** Varianza local fina (ventana de 3 celdas) en tierra por encima de 2 m, m². */
	float FineVariance = 0.0f;
	/** Orientación de los cauces (celdas hundidas respecto a su entorno) frente a la rejilla. */
	FOrientationStats Channels;
	/** Pozos por km² de tierra: mínimos locales estrictos de al menos 0,3 m. */
	float PitsPerKm2 = 0.0f;
	int32 LandCells = 0;
};

/** Informe de realismo del terreno del archipiélago completo. */
struct EXPLORED_API FTerrainRealismReport
{
	int32 UnexplainedBumps = 0;
	float HighestUnexplainedBump = -1000.0f;
	FSeafloorHistogram Seafloor;
	/** Mayor salto entre celdas vecinas del fondo (< -3 m), m. */
	float MaxSeafloorStep = 0.0f;
	TArray<FIslandRealism> Islands;
};

/**
 * Recorre el terreno de un FTerrainDensity y lo mide con FTerrainMetricsModel: lo usan las
 * specs de realismo y la herramienta Tools/HostTests/tools/TerrainDiagnostics.cpp.
 */
class EXPLORED_API FTerrainSurvey
{
public:
	/** Muestrea la altura y la isla dominante en una rejilla cuadrada centrada en el origen. */
	static FTerrainSampleGrid SampleWorld(const FTerrainDensity& Density, float HalfExtent, float Spacing);

	/**
	 * Tierra y rasgos declarados del layout: la celda más alta de cada isla, sus islotes, sus
	 * cayos y los islotes del fondo marino. Un bulto somero que no contenga ninguno es un
	 * artefacto.
	 */
	static TArray<FVector2D> DeclaredAnchors(const FTerrainDensity& Density, const FTerrainSampleGrid& Grid);

	static FTerrainRealismReport Measure(const FTerrainDensity& Density, const FTerrainSampleGrid& Grid);

	/** Profundidad por debajo de la cual una elevación suelta cuenta como bulto (m). */
	static constexpr float BumpThreshold = -3.0f;
};
