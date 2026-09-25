#pragma once

#include "CoreMinimal.h"

/**
 * Rejilla de muestras de densidad. La muestra (I, J, K) está en
 * Origin + (I, J, K) * VoxelSize. Densidad < 0 es sólido.
 */
struct EXPLORED_API FDensityGrid
{
	FIntVector Dims = FIntVector::ZeroValue;
	FVector Origin = FVector::ZeroVector;
	float VoxelSize = 1.0f;
	TArray<float> Values;

	void Init(const FIntVector& InDims, const FVector& InOrigin, float InVoxelSize);

	FORCEINLINE int32 Index(int32 X, int32 Y, int32 Z) const
	{
		return X + Dims.X * (Y + Dims.Y * Z);
	}

	FORCEINLINE float Get(int32 X, int32 Y, int32 Z) const { return Values[Index(X, Y, Z)]; }
	FORCEINLINE void Set(int32 X, int32 Y, int32 Z, float V) { Values[Index(X, Y, Z)] = V; }
	FORCEINLINE FVector SamplePosition(int32 X, int32 Y, int32 Z) const
	{
		return Origin + FVector(X, Y, Z) * VoxelSize;
	}

	/** True si todas las muestras tienen el mismo signo (no hay superficie). */
	bool IsUniform() const;
};

/** Malla resultante. Posiciones en las mismas unidades que la rejilla. */
struct EXPLORED_API FTerrainMeshData
{
	TArray<FVector3f> Positions;
	TArray<FVector3f> Normals;
	TArray<FLinearColor> Colors;
	TArray<uint32> Indices;

	bool IsEmpty() const { return Indices.IsEmpty(); }
	int32 NumTriangles() const { return Indices.Num() / 3; }
};

/**
 * Poligonización Surface Nets: un vértice por celda con cambio de signo y un
 * quad por arista de la rejilla que cruza la superficie.
 *
 * Reparto entre chunks sin grietas: la rejilla debe incluir una muestra extra
 * antes del inicio del chunk (índice 0 = coordenada global -1). Solo se emiten
 * las caras de aristas cuyo origen está en [1, Dims - 1) en los tres ejes, de
 * modo que dos chunks vecinos comparten vértices idénticos y no duplican caras.
 */
struct EXPLORED_API FSurfaceNets
{
	/**
	 * Construye posiciones e índices. Las caras se orientan con el gradiente de
	 * la densidad (hacia el aire). Normales y colores quedan vacíos para que el
	 * llamante los rellene con su campo exacto.
	 */
	static FTerrainMeshData Polygonize(const FDensityGrid& Grid);

	/**
	 * Convención de orientación de Unreal: un triángulo (A, B, C) es frontal
	 * cuando (B - A) x (C - A) apunta en sentido contrario a su normal exterior.
	 */
	static constexpr bool bFrontFaceCrossOpposesNormal = true;
};
