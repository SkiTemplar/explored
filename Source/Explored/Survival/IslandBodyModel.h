#pragma once

#include "CoreMinimal.h"
#include "Survival/BodyEventsModel.h"

/**
 * Cuerpo de Iria en Baliza Siete (GDD v3 §5.1): cuatro barras de 0 a 100 (100 = bien) que bajan
 * por minuto real despierta, el calor que depende de la noche, la lluvia, el refugio y el fuego,
 * y las consecuencias de una barra baja (velocidad -20 % y desaturación bajo 25; aviso a 40).
 * El desmayo y el sueño son de FBodyEventsModel; este modelo solo hace el desgaste continuo.
 *
 * Funciones puras: devuelven barras nuevas. Los valores por defecto son los del GDD y
 * Tools/DataCheck los compara con Content/Data/survival_needs.json.
 */

enum class EBodyBar : uint8
{
	Thirst,
	Hunger,
	Energy,
	Warmth,
	Count
};

EXPLORED_API const TCHAR* LexToString(EBodyBar Bar);

struct EXPLORED_API FIslandBodyTuning
{
	static constexpr float DefaultThirstLossPerMinute = 4.5f;
	static constexpr float DefaultHungerLossPerMinute = 2.5f;
	static constexpr float DefaultEnergyLossPerMinute = 3.0f;
	static constexpr float DefaultNightWarmthLossPerMinute = 3.0f;
	static constexpr float DefaultRainWarmthLossPerMinute = 4.0f;
	static constexpr float DefaultFireWarmthGainPerMinute = 25.0f;
	static constexpr float DefaultLowThreshold = 25.0f;
	static constexpr float DefaultLowSpeedFactor = 0.8f;
	static constexpr float DefaultWarningAt = 40.0f;
	/** El aviso de una barra se vuelve a armar cuando se recupera por encima de WarningAt + esto. */
	static constexpr float DefaultWarningRearm = 20.0f;
	static constexpr double DefaultNightFromHour = 20.0;
	static constexpr double DefaultNightUntilHour = 6.0;

	FBodyBars Initial;
	float ThirstLossPerMinute = DefaultThirstLossPerMinute;
	float HungerLossPerMinute = DefaultHungerLossPerMinute;
	float EnergyLossPerMinute = DefaultEnergyLossPerMinute;
	float NightWarmthLossPerMinute = DefaultNightWarmthLossPerMinute;
	float RainWarmthLossPerMinute = DefaultRainWarmthLossPerMinute;
	float FireWarmthGainPerMinute = DefaultFireWarmthGainPerMinute;
	float LowThreshold = DefaultLowThreshold;
	float LowSpeedFactor = DefaultLowSpeedFactor;
	float WarningAt = DefaultWarningAt;
	float WarningRearm = DefaultWarningRearm;
	double NightFromHour = DefaultNightFromHour;
	double NightUntilHour = DefaultNightUntilHour;

	/** Desmayo y sueño (survival_needs.json «faint» y «sleep»). */
	FBodyEventsTuning Events;

	/** Valores razonables: tasas finitas y no negativas, umbrales dentro de [0, 100]. */
	bool IsValid() const;
};

/** Lo que rodea a Iria en un instante. */
struct EXPLORED_API FBodyEnvironment
{
	/** Hora del día (0-24). */
	double HourOfDay = 12.0;
	/** Dentro de un ShelterVolume (refugio techado o base del faro). */
	bool bSheltered = false;
	/** A 4 m o menos de una hoguera encendida. */
	bool bNearFire = false;
	/** Llueve (chubasco o muro del ciclón). */
	bool bRaining = false;
};

/** Barras que ya han avisado a 40 (bit por EBodyBar). */
struct EXPLORED_API FBodyWarnings
{
	uint8 WarnedMask = 0;

	bool HasWarned(EBodyBar Bar) const { return (WarnedMask & (1u << static_cast<uint8>(Bar))) != 0; }
	bool operator==(const FBodyWarnings& Other) const { return WarnedMask == Other.WarnedMask; }
};

struct EXPLORED_API FBodyStep
{
	FBodyBars Bars;
	FBodyWarnings Warnings;
	/** Barras que acaban de bajar de WarningAt en este paso (aviso de texto, una vez). */
	TArray<EBodyBar> NewWarnings;
};

struct EXPLORED_API FIslandBodyModel
{
	static float Get(const FBodyBars& Bars, EBodyBar Bar);
	/** Copia con la barra puesta a Value (limitada a [0, 100]). */
	static FBodyBars With(const FBodyBars& Bars, EBodyBar Bar, float Value);

	/** Suma deltas (comer, beber, el cruce forzoso) y limita a [0, 100]; no toca el calor. */
	static FBodyBars Apply(const FBodyBars& Bars, float Thirst, float Hunger, float Energy);

	static bool IsNight(double HourOfDay, const FIslandBodyTuning& Tuning = FIslandBodyTuning());

	/**
	 * Cambio de calor por minuto real: + fuego a ≤ 4 m; 0 bajo techo; fuera, la mayor pérdida
	 * entre la noche (-3) y la lluvia (-4); 0 de día sin lluvia.
	 */
	static float WarmthPerMinute(const FBodyEnvironment& Env, const FIslandBodyTuning& Tuning = FIslandBodyTuning());

	/**
	 * Avanza RealSeconds de desgaste. Un paso no finito o negativo no cambia nada. Las barras
	 * que bajan de WarningAt avisan una vez y se rearman al recuperarse por encima de
	 * WarningAt + WarningRearm.
	 */
	static FBodyStep Tick(const FBodyBars& Bars, const FBodyWarnings& Warnings, const FBodyEnvironment& Env, float RealSeconds,
		const FIslandBodyTuning& Tuning = FIslandBodyTuning());

	static bool IsLow(float Value, const FIslandBodyTuning& Tuning = FIslandBodyTuning());
	static bool AnyLow(const FBodyBars& Bars, const FIslandBodyTuning& Tuning = FIslandBodyTuning());

	/** Multiplicador de velocidad: LowSpeedFactor con alguna barra baja, 1 si no. */
	static float SpeedFactor(const FBodyBars& Bars, const FIslandBodyTuning& Tuning = FIslandBodyTuning());

	/** Desaturación del postproceso (0-1): 0 sin barras bajas; de 0,4 a 1 según baja la peor. */
	static float Desaturation01(const FBodyBars& Bars, const FIslandBodyTuning& Tuning = FIslandBodyTuning());
};
