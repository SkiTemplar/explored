#pragma once

#include "CoreMinimal.h"
#include "Items/BackpackModel.h"

/**
 * Barril de lluvia de la base del faro (GDD v3 §5.5): 4 cargas; cada carga llena la cantimplora
 * (4 sorbos de +20). El chubasco guiado del día 1 (90 s) llena 2 cargas y el ciclón hasta 4.
 * Versión por cargas de la captación de lluvia: FRainCatchModel (litros por minuto de juego) queda
 * para los recipientes abiertos de v1. Modelo puro e inmutable; AExploredRainBarrel es la capa fina.
 */
struct EXPLORED_API FRainBarrelState
{
	int32 Charges = 0;
	/** Segundos de lluvia acumulados hacia la siguiente carga. */
	float RainSeconds = 0.0f;

	bool operator==(const FRainBarrelState& Other) const { return Charges == Other.Charges && RainSeconds == Other.RainSeconds; }
};

enum class ERainBarrelResult : uint8
{
	Ok,
	/** El barril no tiene agua. */
	Empty,
	/** No lleva la cantimplora. */
	NoCanteen,
	/** La cantimplora ya está llena. */
	CanteenFull,
};

struct EXPLORED_API FRainBarrelModel
{
	static constexpr int32 MaxCharges = 4;
	static constexpr int32 ShowerCharges = 2;
	/** El chubasco guiado (2 h de juego, 120 s reales) da 2 cargas: una cada 45 s de lluvia. */
	static constexpr float ShowerSeconds = 90.0f;
	static constexpr float RainSecondsPerCharge = ShowerSeconds / ShowerCharges;
	/** Lluvia (0-1 de FWeatherSample::Rain) a partir de la cual cuenta. */
	static constexpr float RainThreshold = 0.3f;

	static FRainBarrelState AddCharges(const FRainBarrelState& In, int32 Charges);

	/** Estado leído de la partida dentro de rango: cargas en [0, MaxCharges] y segundos finitos en [0, RainSecondsPerCharge). */
	static FRainBarrelState Sanitize(const FRainBarrelState& In);

	/** Suma segundos de lluvia; cada RainSecondsPerCharge es una carga (hasta MaxCharges). */
	static FRainBarrelState Collect(const FRainBarrelState& In, float RainSeconds);

	/** Gasta una carga y deja la cantimplora a 4 sorbos. No toca los de entrada. */
	static ERainBarrelResult RefillCanteen(const FRainBarrelState& InBarrel, const FBackpackState& InPack,
		FRainBarrelState& OutBarrel, FBackpackState& OutPack);
};
