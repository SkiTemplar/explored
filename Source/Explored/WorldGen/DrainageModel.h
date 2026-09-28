#pragma once

#include "CoreMinimal.h"
#include "WorldGen/TerrainErosion.h"

/** Parámetros de la red de drenaje y del tallado de ríos. Alturas en metros, áreas en celdas. */
struct EXPLORED_API FDrainageParams
{
	float SeaLevel = 0.0f;
	/** Las depresiones cerradas más someras que esto se rellenan (pozos de erosión, no lagos). */
	float MaxFillDepth = 1.5f;
	/** Exponente del reparto multidireccional (Freeman 1991): más alto = cauces más estrechos. */
	float FlowExponent = 1.1f;
	/** Cuenca mínima (celdas que drenan a una celda) para que haya cauce. */
	float MinRiverArea = 300.0f;
	/** Profundidad del cauce por raíz de la cuenca (m / sqrt(celdas)). */
	float DepthPerSqrtArea = 0.12f;
	float MaxRiverDepth = 4.0f;
	/** Semiancho del cauce en celdas, más un término que crece con la raíz de la cuenca. */
	float BaseHalfWidth = 2.5f;
	float HalfWidthPerSqrtArea = 0.05f;
	float MaxHalfWidth = 6.0f;
	/** El lecho nunca baja de esta cota: la desembocadura entra poco bajo el mar. */
	float MinBedHeight = -0.8f;
};

/**
 * Red de drenaje sobre una rejilla de alturas: rellena los pozos someros (priority-flood,
 * Barnes 2014), acumula el caudal con reparto multidireccional (MFD, sin la anisotropía del
 * D8 que alinea los cauces con la rejilla) y talla ríos con sección en U cuya profundidad y
 * anchura crecen con la cuenca. Determinista y sin UObjects.
 */
class EXPLORED_API FDrainageModel
{
public:
	/**
	 * Superficie rellena: cada celda sube hasta la cota a la que el agua podría salir por el
	 * borde de la rejilla o por el mar (celdas a SeaLevel o menos), más una pendiente mínima.
	 */
	static TArray<float> FilledSurface(const FErosionHeightGrid& Grid, float SeaLevel);

	/** Rellena solo las depresiones cerradas que no pasan de MaxFillDepth; los lagos hondos quedan. */
	static void FillShallowDepressions(FErosionHeightGrid& Grid, float SeaLevel, float MaxFillDepth);

	/** Celdas que drenan a cada celda (ella incluida) sobre la superficie rellena. */
	static TArray<float> FlowAccumulation(const FErosionHeightGrid& Grid, float SeaLevel, float Exponent);

	/** Rellena pozos, acumula caudal y talla los ríos. Devuelve la profundidad tallada por celda. */
	static TArray<float> CarveRivers(FErosionHeightGrid& Grid, const FDrainageParams& Params);
};
