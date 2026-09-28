#pragma once

#include "CoreMinimal.h"

/** Zonas llanas de una isla: donde el jugador puede montar base, huerto y construcciones. */
struct EXPLORED_API FFlatPatchStats
{
	/** Celdas de tierra medidas (máscara) y cuántas son llanas. */
	int32 LandCells = 0;
	int32 FlatCells = 0;
	/** FlatCells / LandCells, en [0, 1]. */
	float FlatFraction = 0.0f;
	/** Área (m²) de cada parche llano conexo (4-vecindad), de mayor a menor. */
	TArray<float> PatchAreas;

	/** Parches de al menos MinAreaM2 metros cuadrados. */
	int32 CountAtLeast(float MinAreaM2) const;
};

/** Forma de la red de drenaje de una isla (ver FPlayabilityMetricsModel::DrainagePattern). */
struct EXPLORED_API FDrainagePattern
{
	int32 RiverCells = 0;
	/** Desembocaduras: celdas de río que vierten al mar. */
	int32 Mouths = 0;
	/**
	 * Longitud media resultante (0-1) de los rumbos de las desembocaduras vistas desde el
	 * centro, ponderada por la cuenca: ~0 si salen repartidas en estrella, alta si drenan hacia
	 * un lado de la isla.
	 */
	float MouthResultant = 0.0f;
	/** Coseno medio entre la dirección del agua y la radial hacia fuera: ~1 en estrella. */
	float Radiality = 0.0f;
	/** Longitud del cauce principal / distancia en línea recta, media ponderada por la cuenca. */
	float Sinuosity = 1.0f;
};

/**
 * Métricas de jugabilidad del terreno: llanos construibles, dispersión de rasgos sueltos
 * (Clark-Evans), variación a lo largo de un perímetro y forma de la red de drenaje. Modelo
 * puro (solo CoreMinimal.h y otros modelos puros); lo usan FTerrainPlayabilitySurvey y las
 * specs de realismo.
 */
class EXPLORED_API FPlayabilityMetricsModel
{
public:
	/**
	 * Llanos: celdas de LandMask con pendiente (diferencias centradas) menor que MaxSlopeDeg y
	 * sus parches conexos. Spacing en metros por celda. Las celdas del borde no cuentan.
	 */
	static FFlatPatchStats FlatPatches(const TArray<float>& Heights, int32 Width, int32 Height, float Spacing,
		const TArray<uint8>& LandMask, float MaxSlopeDeg);

	/**
	 * Índice de vecino más cercano de Clark y Evans (1954) en un área AreaM2: 1 = aleatorio
	 * (Poisson), < 1 agrupado o alineado, > 1 regular (hasta 2,15 en una malla hexagonal).
	 * 0 con menos de dos puntos o área no válida.
	 */
	static float ClarkEvans(const TArray<FVector2D>& Points, float AreaM2);

	/** Desviación típica / media (0 si la media es ~0 o hay menos de dos valores). */
	static float CoefficientOfVariation(const TArray<float>& Values);

	/**
	 * Dentado de una serie cíclica (valores a lo largo de un perímetro): media de la segunda
	 * diferencia |v[i-1] - 2 v[i] + v[i+1]| dividida por Scale (o por la media de v si Scale
	 * <= 0). Una variación amplia y suave (o una forma alargada) da poco; un borde en sierra,
	 * mucho. Los valores negativos son muestras que faltan y no cuentan.
	 */
	static float Jaggedness(const TArray<float>& Cyclic, float Scale = 0.0f);

	/** Longitud media resultante de unos ángulos (radianes) con pesos: 0 repartidos, 1 iguales. */
	static float MeanResultantLength(const TArray<float>& Angles, const TArray<float>& Weights);

	/**
	 * Red de drenaje de una rejilla de alturas (m): rellena depresiones (priority-flood) y
	 * sigue el agua por la máxima pendiente del relleno (D8), acumulando el caudal por ese árbol.
	 * Río = cuenca de al menos MinRiverCells celdas sobre SeaLevel; Center en celdas.
	 */
	static FDrainagePattern DrainagePattern(const TArray<float>& Heights, int32 Width, int32 Height,
		const FVector2D& Center, float MinRiverCells, float SeaLevel);
};
