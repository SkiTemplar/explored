#pragma once

#include "CoreMinimal.h"

/**
 * Lluvia visible low poly (GDD v3 §11, F6): trazos geométricos que caen en una caja alrededor de la
 * cámara. Modelo puro: dada la hora, la intensidad, el viento y la cámara devuelve la posición de
 * cada trazo y la inclinación común; AExploredRainStreaks (UExploredWeatherFxSubsystem) los dibuja
 * con instancias de un prisma fino y M_RainStreak, sin sprites ni texturas.
 *
 * Las posiciones son de mundo y se repliegan en la caja centrada en la cámara: al caminar, la lluvia
 * no viaja con el jugador. Determinista: la misma semilla da los mismos trazos.
 */
struct EXPLORED_API FRainStreaksTuning
{
	int32 MaxStreaks = 900;
	/** Media anchura de la caja (cm) en X e Y. */
	float HalfExtentCm = 1800.0f;
	/** Altura de la caja (cm); la cámara queda a un tercio desde abajo. */
	float HeightCm = 1500.0f;
	/** Velocidad de caída (cm/s). */
	float FallCmS = 1100.0f;
	/** Viento máximo (cm/s) con Wind = 1. */
	float MaxWindCmS = 700.0f;
	/** Largo del trazo (cm) con lluvia débil y con lluvia fuerte. */
	float LengthLightCm = 45.0f;
	float LengthHeavyCm = 110.0f;
	/** Radio (cm) alrededor de la cámara sin trazos: de cerca se verían como barras. */
	float NearClearCm = 300.0f;
	/** Por debajo de esta intensidad no se dibuja nada. */
	float MinIntensity = 0.05f;
};

struct EXPLORED_API FRainStreaksFrame
{
	TArray<FVector> Positions;
	/** Eje del trazo (unitario, hacia abajo y a favor del viento). */
	FVector Direction = FVector(0.0, 0.0, -1.0);
	float LengthCm = 0.0f;
};

class EXPLORED_API FRainStreaksModel
{
public:
	FRainStreaksModel(const FRainStreaksTuning& InTuning, uint32 Seed);

	/** Trazos visibles con una intensidad (0-1): proporcional, 0 por debajo del mínimo. */
	int32 CountFor(float Intensity) const;

	/**
	 * Fotograma de la lluvia. Time en segundos (reloj real), Intensity 0-1, Wind 0-1 en la dirección
	 * WindDir (XY, se normaliza; cero = sin deriva), Camera en cm de mundo.
	 */
	FRainStreaksFrame Evaluate(double Time, float Intensity, float Wind, const FVector2D& WindDir, const FVector& Camera) const;

	const FRainStreaksTuning& GetTuning() const { return Tuning; }

	/** Repliega Value en [Center - Half, Center + Half). */
	static double Wrap(double Value, double Center, double Half);

private:
	FRainStreaksTuning Tuning;
	/** Posición base de cada trazo en [0, 1)^3. */
	TArray<FVector> Seeds;
};
