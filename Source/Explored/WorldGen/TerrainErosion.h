#pragma once

#include "CoreMinimal.h"

/**
 * Parámetros de la simulación de erosión hidráulica por gotas (Beyer 2015 / Lague) y de
 * la erosión térmica (talud de reposo) sobre una rejilla de alturas. Deterministas: la
 * misma semilla y la misma rejilla de entrada producen siempre la misma salida.
 */
struct EXPLORED_API FErosionParams
{
	uint32 Seed = 0;
	/** Tamaño de celda en metros; solo afecta a la pendiente física de la erosión térmica. */
	float CellSizeMeters = 4.0f;

	// --- Erosión hidráulica (gotas de lluvia) ---
	int32 DropletCount = 30000;
	int32 MaxDropletLifetime = 32;
	/** Cuánto conserva la gota su dirección previa frente al gradiente cuesta abajo, en [0, 1]. */
	float Inertia = 0.05f;
	float SedimentCapacityFactor = 4.5f;
	float MinSedimentCapacity = 0.01f;
	float ErodeSpeed = 0.32f;
	float DepositSpeed = 0.32f;
	float EvaporateSpeed = 0.015f;
	float Gravity = 9.8f;
	float InitialWaterVolume = 1.0f;
	float InitialSpeed = 0.6f;
	/** Radio del pincel de erosión, en celdas. */
	int32 ErosionRadius = 3;

	// --- Erosión térmica (talud de reposo) ---
	int32 ThermalIterations = 40;
	/** Tangente de la pendiente máxima estable; por encima, el material se desliza. */
	float TalusAngleTangent = 0.85f;
	/** Fracción del exceso sobre el talud que se transporta en cada pasada, en [0, 1]. */
	float ThermalTransferRate = 0.5f;
};

/**
 * Rejilla de alturas mutable sobre la que trabaja FTerrainErosionModel. Coordenadas de
 * celda continuas en [0, Width - 1] x [0, Height - 1]; fuera de rango se recorta al borde.
 */
struct EXPLORED_API FErosionHeightGrid
{
	int32 Width = 0;
	int32 Height = 0;
	TArray<float> Heights;

	void Init(int32 InWidth, int32 InHeight, float FillValue = 0.0f);

	bool IsValidCoord(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height; }
	float& At(int32 X, int32 Y) { return Heights[Y * Width + X]; }
	float At(int32 X, int32 Y) const { return Heights[Y * Width + X]; }

	/** Altura interpolada bilinealmente en coordenadas de celda continuas. */
	float Sample(float X, float Y) const;
	/** Gradiente (dH/dX, dH/dY) interpolado, en metros de altura por celda. */
	FVector2D Gradient(float X, float Y) const;

	/** Suma de todas las alturas; sirve para comprobar la conservación aproximada de masa. */
	double TotalHeight() const;
	/** Mayor pendiente entre celdas vecinas adyacentes, en tangente (altura / CellSizeMeters). */
	float MaxNeighborSlope(float CellSizeMeters) const;
};

/**
 * Erosión hidráulica por gotas y erosión térmica sobre una rejilla de alturas. Modelo
 * puro (solo CoreMinimal.h), sin UObjects: testeable en host y seguro de llamar fuera
 * del editor.
 */
class EXPLORED_API FTerrainErosionModel
{
public:
	/** Aplica erosión hidráulica seguida de térmica sobre Grid, in-place. */
	static void Erode(FErosionHeightGrid& Grid, const FErosionParams& Params);

	/** Solo la pasada hidráulica (gotas). Expuesta para tests y para depurar por separado. */
	static void ErodeHydraulic(FErosionHeightGrid& Grid, const FErosionParams& Params);

	/** Solo la pasada térmica (talud de reposo). Expuesta para tests. */
	static void ErodeThermal(FErosionHeightGrid& Grid, const FErosionParams& Params);
};
