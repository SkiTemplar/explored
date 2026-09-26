#pragma once

#include "CoreMinimal.h"

/** Una onda de Gerstner. Unidades: centímetros y segundos. */
struct EXPLORED_API FGerstnerWave
{
	FVector2D Direction = FVector2D(1.0f, 0.0f);
	float Wavelength = 2000.0f;
	float Amplitude = 20.0f;
	/** Inclinación de la cresta (0 = senoidal, 1 = cresta afilada). */
	float Steepness = 0.4f;
};

/**
 * Conjunto de olas compartido por el material del océano y la lógica de juego
 * (nado, flotación). Ambos deben evaluar exactamente la misma función.
 */
struct EXPLORED_API FOceanWaves
{
	static constexpr int32 NumWaves = 4;
	FGerstnerWave Waves[NumWaves];

	/** Olas por defecto, escaladas por la fuerza del mar (0 calma, 1 temporal). */
	static FOceanWaves Make(float SeaState);

	/** Desplazamiento de un punto de la superficie en reposo (X, Y) en el instante Time. */
	FVector Displacement(const FVector2D& Position, float Time) const;

	/** Altura aproximada del agua en (X, Y) (corrige el desplazamiento horizontal con dos iteraciones). */
	float HeightAt(const FVector2D& Position, float Time) const;

	/**
	 * Normal unitaria de la superficie en (X, Y), por diferencias finitas de
	 * HeightAt. La usa el nado en superficie para inclinar la cámara con la
	 * pendiente de la ola; el material evalúa su propia normal por separado,
	 * pero a partir de las mismas olas.
	 */
	FVector NormalAt(const FVector2D& Position, float Time) const;

	/** Gravedad en cm/s² para la relación de dispersión. */
	static constexpr float Gravity = 981.0f;
};
