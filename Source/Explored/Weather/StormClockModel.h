#pragma once

#include "CoreMinimal.h"

/**
 * Reloj de tormenta de Baliza Siete (GDD v3 §5.8): curva fija del barómetro,
 * chubasco guiado del día 1 y ciclón de la noche del día 2. Sin azar.
 *
 * El tiempo es absoluto, en horas de juego desde el día 1 a las 00:00
 * (día 2 a las 18:00 = 42). La presión se interpola linealmente entre los
 * puntos de la curva y se mantiene constante fuera de ellos. Los estados son
 * ventanas [inicio, fin); fuera de toda ventana el cielo está despejado.
 *
 * Los valores por defecto son los del GDD (FStormClockConfig::Gdd); el
 * subsistema puede construir el modelo con los de storm_curve.json. Una
 * configuración no válida se sustituye por la del GDD (UsesFallback()).
 */

enum class EStormState : uint8
{
	Clear,         // despejado
	Squall,        // chubasco guiado (D1 15:00, 90 s reales)
	HighClouds,    // nubes altas (día 2)
	SeaClosed,     // el mar se cierra (D2 18:00): no se cruza en balsa
	CycloneWall1,  // muro 1: viento, olas, lluvia, fuegos al aire libre apagados
	CycloneEye,    // ojo: calma, sin lluvia
	CycloneWall2,  // muro 2: igual que el muro 1
	Count
};

enum class EBarometerTrend : uint8
{
	Steady,
	Falling,
	Rising,
};

struct EXPLORED_API FStormCurvePoint
{
	double Hour = 0.0;
	float Hpa = 1013.0f;
};

struct EXPLORED_API FStormWindow
{
	EStormState State = EStormState::Clear;
	double StartHour = 0.0;
	double EndHour = 0.0;
};

struct EXPLORED_API FStormClockConfig
{
	/** 24 h de juego = 18 min reales (GDD §3): 1 h = 45 s. */
	static constexpr double DayLengthMinutes = 18.0;
	static constexpr double RealSecondsPerHour = DayLengthMinutes * 60.0 / 24.0;
	/** Ojo del ciclón: 65 s reales (GDD §3 y §5.8). */
	static constexpr double EyeRealSeconds = 65.0;
	static constexpr double EyeHours = EyeRealSeconds / RealSecondsPerHour;

	/** Puntos de la curva con horas estrictamente crecientes. */
	TArray<FStormCurvePoint> Points;
	/** Ventanas de estado sin solaparse; ninguna es Clear. */
	TArray<FStormWindow> Windows;

	/** Intensidad de lluvia (0-1) en el chubasco y en los muros del ciclón. */
	float SquallRain = 0.6f;
	float CycloneRain = 1.0f;
	/** Cargas de barril por hora de juego (GDD §5.5: el chubasco de 2 h llena 2). */
	float SquallChargesPerHour = 1.0f;
	float CycloneChargesPerHour = 1.0f;

	/** Última hora en la que se puede cruzar en balsa (D2 18:00: «el mar se cierra»). */
	double RaftLastHour = 42.0;
	/** D2 18:45: si Iria sigue en el islote, fundido y vuelta a Playa Norte (GDD §5.7). */
	double ForcedReturnHour = 42.75;
	/** Por debajo de este cambio (hPa por hora) la flecha de tendencia queda estable. */
	float SteadyHpaPerHour = 0.1f;

	/** Valores del GDD §5.3, §5.5, §5.7 y §5.8. */
	static FStormClockConfig Gdd();

	bool IsValid() const;
};

/** Lluvia en un instante. */
struct EXPLORED_API FStormRain
{
	bool bRaining = false;
	float Intensity = 0.0f;       // 0-1
	float ChargesPerHour = 0.0f;  // cargas de barril por hora de juego
};

/** Aspecto del tiempo (0-1 salvo la presión); mismos campos que FWeatherSample. */
struct EXPLORED_API FWeatherLook
{
	float CloudCover = 0.15f;
	float Rain = 0.0f;
	float Wind = 0.2f;
	float SeaState = 0.15f;
	float Fog = 0.0f;
	float Lightning = 0.0f;
	float Temperature = 28.0f;
	float Pressure = 1013.0f;
};

/** Barril de lluvia (GDD §5.5): 4 cargas; las fracciones se acumulan. */
struct EXPLORED_API FStormBarrelState
{
	int32 Charges = 0;
	float Partial = 0.0f;  // fracción de la siguiente carga, [0, 1)

	bool operator==(const FStormBarrelState& Other) const { return Charges == Other.Charges && Partial == Other.Partial; }
};

class EXPLORED_API FStormClockModel
{
public:
	static constexpr int32 BarrelCapacity = 4;
	static constexpr double HoursPerDay = 24.0;

	/** Con la configuración del GDD. */
	FStormClockModel();
	/** Con una configuración propia; si no es válida se usa la del GDD. */
	explicit FStormClockModel(const FStormClockConfig& InConfig);

	/** Día 1-based y hora del día (0-24) a horas absolutas. */
	static double ToAbsoluteHours(int32 Day, double HourOfDay);

	const FStormClockConfig& GetConfig() const { return Config; }
	bool UsesFallback() const { return bFallback; }

	/** Lectura del barómetro en hPa. Un tiempo no finito devuelve la del primer punto. */
	float PressureAt(double Hour) const;
	EBarometerTrend TrendAt(double Hour) const;
	EStormState StateAt(double Hour) const;

	bool IsCyclone(double Hour) const;

	/** Solo los muros del ciclón apagan los fuegos al aire libre (GDD §5.2); los techados no. */
	bool ExtinguishesOutdoorFires(double Hour) const;

	/** Se puede cruzar al bajo: antes de que el mar se cierre (D2 18:00). */
	bool CanCrossByRaft(double Hour) const;

	/** Pasada la hora del fundido forzoso de vuelta desde el islote. */
	bool IsPastForcedReturn(double Hour) const;

	FStormRain RainAt(double Hour) const;

	/** Cargas de barril que caen entre FromHour y ToHour (0 si el intervalo no es válido). */
	float BarrelChargesBetween(double FromHour, double ToHour) const;

	/** Añade lluvia a un barril sin pasar de BarrelCapacity; lo que rebosa se pierde. */
	static FStormBarrelState AddRain(const FStormBarrelState& Barrel, float Charges);

	/** Saca una carga para rellenar la cantimplora; bOk falso si está vacío. */
	static FStormBarrelState TakeCharge(const FStormBarrelState& Barrel, bool& bOutOk);

	/** Muro 1 o muro 2 (viento, olas, lluvia, niebla, fuegos al aire libre apagados). */
	static bool IsWall(EStormState State);

	/**
	 * Cielo, mar y lluvia de un estado (lo reparte UExploredWeatherSubsystem al océano, al cielo y
	 * al ambiente). Pressure es la del barómetro en esa hora; los muros dan mar 1 (olas de 3-4 m)
	 * y niebla; el ojo, calma con mar de fondo.
	 */
	FWeatherLook LookAt(double Hour) const;
	static FWeatherLook LookFor(EStormState State, float RainIntensity);

private:
	float ChargesPerHourFor(EStormState State) const;

	FStormClockConfig Config;
	bool bFallback = false;
};
