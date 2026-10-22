#pragma once

#include "CoreMinimal.h"

/** Una nube K5 del cielo (metros, relativos al centro de la isla). */
struct EXPLORED_API FCloudInstance
{
	/** Variante SM_Nube_A..E (0-4). */
	int32 Variant = 0;
	FVector2D Position = FVector2D::ZeroVector;
	float Altitude = 200.0f;
	float YawDeg = 0.0f;
	float Scale = 1.0f;
	/** Umbral de nubosidad a partir del cual se ve (las primeras nubes salen con cielo despejado). */
	float CoverThreshold = 0.0f;
};

/**
 * Campo de nubes low poly (sustituye a las nubes volumétricas): ~32 nubes de K5 en un
 * anillo de 500-900 m alrededor de la isla que derivan con el viento y vuelven a entrar por el
 * lado contrario del cuadrado que lo contiene. Determinista para una semilla. Vuelan a 260-450 m
 * (169 m como mínimo con el techo bajo de la tormenta, x0,65): muy por encima de la linterna del
 * faro (unos 50 m), para que ninguna nube salga de la linterna ni asome por el cerro.
 */
struct EXPLORED_API FCloudFieldModel
{
	static constexpr int32 NumVariants = 5;
	static constexpr float InnerRadius = 500.0f;
	static constexpr float OuterRadius = 900.0f;
	static constexpr float MinAltitude = 260.0f;
	static constexpr float MaxAltitude = 450.0f;
	/** Factor de altura con tormenta plena: el techo de nubes baja. */
	static constexpr float StormCeilingFactor = 0.65f;
	/** Cota (m) que ninguna nube baja nunca: la linterna del faro con margen. */
	static constexpr float LanternClearanceM = 120.0f;
	/** Velocidad de deriva (m/s) con viento 1. */
	static constexpr float MaxDriftSpeed = 12.0f;

	TArray<FCloudInstance> Clouds;

	/** Genera «Count» nubes en el anillo. */
	static FCloudFieldModel Make(uint32 Seed, int32 Count);

	/** Desplaza todas las nubes con el viento (dirección unitaria XY, fuerza 0-1) y las envuelve en el cuadrado. */
	void Advance(const FVector2D& WindDir, float Wind, float DeltaSeconds);

	/**
	 * Escala visible de una nube (0-1): se encoge al entrar en el radio interior o salir del
	 * exterior (aparece y se disuelve en vez de saltar) y desaparece por debajo de su umbral de nubosidad.
	 */
	static float Visibility(const FCloudInstance& Cloud, float CloudCover);
};
