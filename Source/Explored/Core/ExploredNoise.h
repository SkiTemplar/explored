#pragma once

#include "CoreMinimal.h"

/**
 * Ruido de gradiente determinista sin estado global. Cada instancia se
 * identifica por su semilla; es seguro usarla desde varios hilos.
 */
struct EXPLORED_API FExploredNoise
{
	explicit FExploredNoise(uint32 InSeed = 0) : Seed(InSeed) {}

	/** Ruido de gradiente 2D en [-1, 1]. Vale 0 en los nodos enteros. */
	float Gradient2D(float X, float Y) const;

	/** Ruido de gradiente 3D en [-1, 1]. Vale 0 en los nodos enteros. */
	float Gradient3D(float X, float Y, float Z) const;

	/** Movimiento browniano fraccional 2D normalizado a [-1, 1]. */
	float Fbm2D(float X, float Y, int32 Octaves, float Lacunarity = 2.0f, float Gain = 0.5f) const;

	/** fBm 3D normalizado a [-1, 1]. */
	float Fbm3D(float X, float Y, float Z, int32 Octaves, float Lacunarity = 2.0f, float Gain = 0.5f) const;

	/** Ruido «ridged» 2D en [0, 1]: crestas afiladas para cordilleras. */
	float Ridged2D(float X, float Y, int32 Octaves, float Lacunarity = 2.0f, float Gain = 0.5f) const;

	/** Desplaza (X, Y) con dos campos de fBm para romper la regularidad. */
	FVector2D Warp2D(float X, float Y, float Strength, int32 Octaves) const;

	uint32 GetSeed() const { return Seed; }

private:
	uint32 Seed;
};
