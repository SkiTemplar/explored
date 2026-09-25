#pragma once

#include "CoreMinimal.h"

/**
 * Generador PCG32 determinista. Misma semilla y secuencia de llamadas producen
 * exactamente los mismos valores en cualquier plataforma e hilo.
 */
struct EXPLORED_API FExploredRandom
{
	explicit FExploredRandom(uint64 InSeed, uint64 InStream = 0x5851F42D4C957F2DULL);

	uint32 NextUInt32();

	/** Entero en [Min, Max] (ambos incluidos). */
	int32 RangeInt(int32 Min, int32 Max);

	/** Real en [0, 1). */
	float NextFloat();

	/** Real en [Min, Max). */
	float RangeFloat(float Min, float Max);

	/** Devuelve true con probabilidad P. */
	bool Chance(float P);

	/** Punto uniforme dentro del disco unidad. */
	FVector2D InsideUnitDisc();

private:
	uint64 State = 0;
	uint64 Increment = 0;
};

namespace ExploredHash
{
	/** Hash de enteros de buena calidad (lowbias32). */
	EXPLORED_API uint32 Hash32(uint32 X);

	/** Combina una semilla con coordenadas enteras. */
	EXPLORED_API uint32 Hash2D(uint32 Seed, int32 X, int32 Y);
	EXPLORED_API uint32 Hash3D(uint32 Seed, int32 X, int32 Y, int32 Z);

	/** Convierte un hash a real en [0, 1). */
	EXPLORED_API float ToUnitFloat(uint32 H);
}
