#pragma once

#include "CoreMinimal.h"

/**
 * Colores del cielo low poly (GDD v3 §11) por momento del día y tormenta. Todos los
 * colores de superficie salen de Tools/Style/paleta.json (copias en SkyPaletteModel.cpp,
 * comprobadas por SkyPaletteSpec); las luces se tiñen con ellos y se escalan en
 * intensidad, pero no inventan tonos.
 *
 * Momentos: noche (sol bajo -0,2), amanecer/atardecer (sol en el horizonte), día (sol
 * por encima de 0,3) y ciclón (nubosidad alta), que se mezcla encima de cualquiera.
 */
struct EXPLORED_API FSkyLook
{
	/** Degradado del cielo (lineal): horizonte, franja media (≈ 20° de altura) y cénit. */
	FLinearColor Horizon = FLinearColor::Black;
	FLinearColor Middle = FLinearColor::Black;
	FLinearColor Zenith = FLinearColor::Black;
	/** Halo alrededor del sol en el horizonte (amanecer y atardecer). */
	FLinearColor Glow = FLinearColor::Black;
	float GlowStrength = 0.0f;
	/** Brillo general del cielo (la noche y el ciclón lo bajan). */
	float SkyBrightness = 1.0f;

	/** Color e intensidad relativa (0-1) del sol directo. */
	FLinearColor SunColor = FLinearColor::White;
	float SunIntensity = 1.0f;
	/** Tinte de la luz ambiente (sombras): nunca negro. */
	FLinearColor AmbientTint = FLinearColor::White;
	/** Tinte de las nubes K5 (multiplica su color de paleta) y de la niebla lejana. */
	FLinearColor CloudTint = FLinearColor::White;
	FLinearColor FogColor = FLinearColor::Black;
};

struct EXPLORED_API FSkyPaletteModel
{
	/** Pesos de los momentos (suman 1). */
	struct FWeights
	{
		float Night = 0.0f;
		float Twilight = 0.0f;
		float Day = 0.0f;
	};

	/** Reparto noche / crepúsculo / día según la altura del sol (componente Z de su dirección). */
	static FWeights MomentWeights(float SunZ);

	/** Aspecto del cielo para la altura del sol y la tormenta (0 = despejado, 1 = muro del ciclón). */
	static FSkyLook Evaluate(float SunZ, float Storm);

	/** Color de paleta (sRGB #RRGGBB de paleta.json) por nombre; negro si no existe. */
	static FLinearColor PaletteColor(const TCHAR* Name);

	/** Tormenta visual a partir de la nubosidad del clima (0 por debajo de 0,4, 1 desde 0,85). */
	static float StormFromCloudCover(float CloudCover);

	/**
	 * Paso (grados) con que giran el sol y la luna. Cualquier giro de una direccional invalida las
	 * páginas cacheadas de su VSM: girarla cada fotograma (0,004° con el día de 26 min) obliga a
	 * rasterizar la sombra entera siempre. 0,1° es ~6 % de la penumbra (LightSourceAngle 1,6°).
	 */
	static constexpr float LightRotationStepDeg = 0.1f;

	/** La luz aplicada (Applied, nula si no hay) debe girar hacia Wanted: se ha separado más del paso. */
	static bool ShouldRotateLight(const FVector& Applied, const FVector& Wanted);

	/** La luna proyecta sombra solo con el sol del todo fuera (SunUp ≈ 0): nunca dos VSM direccionales a la vez. */
	static bool MoonCastsShadows(float SunUp);

	/** Peso de la luz del sol según la altura del sol (Z de su dirección): 0 bajo -0,08 y 1 sobre 0,04. */
	static float SunUp(float SunZ);

	/**
	 * Peso de la luz de la luna según la altura del sol: 0 mientras el sol alumbra (SunUp > 0) y
	 * entra con fundido hasta 1 en SunZ = -0,12. Sin Lumen, una direccional sin sombra atraviesa
	 * techos y paredes; como la luna no proyecta sombra con el sol encendido (MoonCastsShadows),
	 * tampoco debe alumbrar hasta que el sol se apague del todo.
	 */
	static float MoonLightFade(float SunZ);
};
