#pragma once

#include "CoreMinimal.h"

/** Estaciones del año del archipiélago (biblia de contenido §6.1). */
enum class ESeason : uint8
{
	Dry,          // Seca
	FirstRains,   // Primeras lluvias
	Monsoon,      // Monzón
	Cyclones,     // Temporada de ciclones
	Count
};

/** Estados del tiempo (biblia de contenido §6.2). */
enum class EWeatherState : uint8
{
	Clear,
	Cloudy,
	MorningFog,
	LightRain,
	Shower,
	Thunderstorm,
	HeatWave,
	Gale,
	Cyclone,
	Count
};

EXPLORED_API const TCHAR* LexToString(ESeason Season);
EXPLORED_API const TCHAR* LexToString(EWeatherState State);

/** Magnitudes físicas que consumen el cielo, el océano, el audio y la supervivencia. */
struct EXPLORED_API FWeatherSample
{
	float CloudCover = 0.2f;     // 0–1
	float Rain = 0.0f;           // 0–1
	float Wind = 0.2f;           // 0–1
	float SeaState = 0.15f;      // 0–1
	float Fog = 0.0f;            // 0–1
	float Lightning = 0.0f;      // probabilidad por segundo de rayo
	float Temperature = 28.0f;   // °C del aire
	float Pressure = 1012.0f;    // hPa (el barómetro del Albatros)

	static FWeatherSample Lerp(const FWeatherSample& A, const FWeatherSample& B, float Alpha);
};

/** Un bloque de tiempo planificado. */
struct EXPLORED_API FWeatherSpan
{
	EWeatherState State = EWeatherState::Clear;
	/** Inicio y fin en días totales de juego. */
	float Start = 0.0f;
	float End = 0.0f;
};

/**
 * Planificador determinista del clima: para una semilla y un día cualquiera
 * produce siempre la misma secuencia de estados. Los ciclones y galernas
 * vienen precedidos de señales (presión, viento, mar de fondo) con 1–2 días
 * de antelación.
 */
class EXPLORED_API FWeatherModel
{
public:
	static constexpr int32 DaysPerSeason = 8;
	static constexpr int32 DaysPerYear = DaysPerSeason * static_cast<int32>(ESeason::Count);
	/**
	 * Reloj máximo admitido (10 000 días de juego, como FRainCatchModel::MaxSupportedMinute).
	 * Un reloj mayor (estado corrupto) se trata como este: pasarlo a int32 desbordaría.
	 */
	static constexpr int32 MaxSupportedDays = 10000;

	explicit FWeatherModel(uint32 InSeed);

	static ESeason SeasonForDay(float TotalDays);

	/** Estado planificado en un instante (días totales). */
	EWeatherState StateAt(float TotalDays) const;

	/** Muestra física en un instante, con transiciones suaves entre bloques y aviso previo de temporales. */
	FWeatherSample SampleAt(float TotalDays) const;

	/**
	 * Categoría del ciclón activo en un instante (1–3, biblia §6.2); 0 si no hay
	 * ciclón planificado. Sale de su propio hash de la semilla y del día en que
	 * empieza, así que no altera la secuencia de estados: 50 % de categoría 1,
	 * 35 % de 2 y 15 % de 3.
	 */
	int32 CycloneCategoryAt(float TotalDays) const;

	/** Categoría del ciclón que empieza la noche de un día (aunque ese día no haya ciclón). */
	int32 CycloneCategoryForDay(int32 Day) const;

	/** Siguiente temporal grave (ciclón o galerna) a partir de un instante; false si no hay en 40 días. */
	bool NextSevereEvent(float FromDays, FWeatherSpan& OutSpan) const;

	/** Condiciones base de un estado en una estación. */
	static FWeatherSample BaseSample(EWeatherState State, ESeason Season);

	/** Bloques planificados que cubren un día (se generan por día y se cachean). */
	const TArray<FWeatherSpan>& SpansForDay(int32 Day) const;

private:
	void GenerateDay(int32 Day, TArray<FWeatherSpan>& Out) const;

	uint32 Seed;
	mutable TMap<int32, TArray<FWeatherSpan>> Cache;
	mutable FCriticalSection CacheLock;
};
