#pragma once

#include "CoreMinimal.h"

/**
 * Rejilla regular de alturas muestreada del mundo (metros), con la isla dominante de cada
 * celda. Es la entrada de las métricas de realismo de FTerrainMetricsModel.
 */
struct EXPLORED_API FTerrainSampleGrid
{
	int32 Width = 0;
	int32 Height = 0;
	/** Posición en el mundo de la celda (0, 0). */
	FVector2D Origin = FVector2D::ZeroVector;
	float Spacing = 1.0f;
	TArray<float> Heights;
	TArray<int32> IslandIndex;

	void Init(int32 InWidth, int32 InHeight, const FVector2D& InOrigin, float InSpacing);
	int32 Index(int32 X, int32 Y) const { return Y * Width + X; }
	bool IsValid(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height; }
	FVector2D WorldPosition(int32 X, int32 Y) const { return Origin + FVector2D(X, Y) * Spacing; }
	/** Celda más cercana a un punto del mundo, o INDEX_NONE fuera de la rejilla. */
	int32 CellAt(const FVector2D& World) const;
};

/** Componente conexa de fondo somero que no pertenece a ninguna tierra o rasgo declarado. */
struct EXPLORED_API FTerrainBump
{
	FVector2D Centroid = FVector2D::ZeroVector;
	int32 CellCount = 0;
	float MaxHeight = 0.0f;
};

/** Histograma del fondo marino: detecta cotas recortadas (mesetas) y escalones. */
struct EXPLORED_API FSeafloorHistogram
{
	int32 SampleCount = 0;
	/** Fracción de las celdas de fondo en el intervalo de 0,1 m más poblado. */
	float ModeFraction = 0.0f;
	float ModeHeight = 0.0f;
	/**
	 * Mayor cociente entre un intervalo de 1 m y el mayor de sus dos vecinos (solo intervalos con
	 * al menos el 0,2 % de las muestras): ~1 en un fondo continuo, varias veces más en una cota
	 * recortada o un escalón.
	 */
	float MaxSpike = 0.0f;
	float SpikeHeight = 0.0f;
};

/** Reparto de orientaciones de la pendiente respecto a los ejes y diagonales de la rejilla. */
struct EXPLORED_API FOrientationStats
{
	int32 SampleCount = 0;
	/** Fracción a ±7,5° de 0°/90° dividida por la esperada si fuera isótropa (1/6): 1 = isótropo. */
	float AxisExcess = 0.0f;
	/** Igual para las diagonales (45°/135°). */
	float DiagonalExcess = 0.0f;
};

/**
 * Métricas de realismo del terreno: patrones artificiales que se ven en el juego (bultos
 * sueltos en el mar, fondos recortados, escalones, cauces alineados con la rejilla, pozos de
 * gotas). Modelo puro (solo CoreMinimal.h): lo usan las specs y Tools/HostTests/tools.
 */
class EXPLORED_API FTerrainMetricsModel
{
public:
	/**
	 * Componentes conexas (4-vecindad) por encima de Threshold que no contienen ninguno de los
	 * anclajes (cumbre de cada isla, cayos, islotes declarados). Ignora las de menos de MinCells.
	 */
	static TArray<FTerrainBump> FindUnexplainedBumps(const FTerrainSampleGrid& Grid, float Threshold, int32 MinCells,
		const TArray<FVector2D>& Anchors);

	/** Histograma de las celdas de fondo por debajo de BelowHeight. */
	static FSeafloorHistogram SeafloorHistogram(const FTerrainSampleGrid& Grid, float BelowHeight);

	/** Varianza local media (ventana de 2·Radius+1 celdas) en la tierra de una isla por encima de MinHeight. */
	static float LocalVariance(const FTerrainSampleGrid& Grid, int32 IslandIdx, float MinHeight, int32 Radius);

	/**
	 * Fracción de la tierra de una isla (por encima de MinHeight) cuya ventana 3x3 tiene una
	 * varianza menor que MaxVariance: mesetas lisas o cotas recortadas, que en el terreno
	 * natural no aparecen. Una playa en rampa o un llano con microrrelieve no cuentan.
	 */
	static float SmoothFraction(const FTerrainSampleGrid& Grid, int32 IslandIdx, float MinHeight, float MaxVariance);

	/** Mayor salto de altura entre celdas vecinas cuando ambas están por debajo de BelowHeight. */
	static float MaxStepBelow(const FTerrainSampleGrid& Grid, float BelowHeight, FVector2D* OutWhere = nullptr);

	/**
	 * Orientación del gradiente en las celdas con Mask != 0 y pendiente de al menos MinGradient
	 * (altura por celda). Mask vacía = todas las celdas.
	 */
	static FOrientationStats GradientOrientation(const TArray<float>& Heights, int32 Width, int32 Height,
		const TArray<uint8>& Mask, float MinGradient);

	/** Pozos: mínimos locales estrictos por encima de MinHeight, al menos MinDepth bajo sus 8 vecinos. */
	static int32 CountPits(const TArray<float>& Heights, int32 Width, int32 Height, float MinHeight, float MinDepth);
};
