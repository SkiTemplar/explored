#pragma once

#include "CoreMinimal.h"
#include "Weather/WeatherModel.h"

/**
 * La lluvia llena los recipientes abiertos (biblia 02 §5.4, GDD v2 §3.16).
 *
 * Un recipiente dejado a la intemperie recoge lo que cae sobre su boca:
 * 1 mm de lluvia sobre 1 m² son 1 L. Si está a cubierto no recoge nada. Sin
 * lluvia, el agua se evapora poco a poco. Lo que no cabe rebosa y, si dentro
 * había agua sin tratar o de mar, la lluvia la va desplazando al rebosar, así
 * que un cuenco que se deja en un chaparrón acaba con agua limpia.
 *
 * Todo es entero (minutos de juego y microlitros) y el tiempo se muestrea en
 * franjas fijas de 10 minutos alineadas con el reloj absoluto, así que avanzar
 * 2 días de golpe o 2880 veces un minuto da exactamente el mismo resultado.
 * Por eso el motor solo simula los recipientes cercanos al jugador y los
 * lejanos se ponen al día al cargarse, con la misma cuenta.
 */

/** Qué hay en el recipiente además del agua de lluvia. */
enum class ERainCatchLiquid : uint8
{
	/** Solo lluvia (o agua ya limpia vertida desde otro recipiente). */
	None,
	/** Agua de río, charca o sin hervir (`agua_sin_tratar`). */
	Untreated,
	/** Agua de mar (`agua_mar`). Manda sobre la sin tratar al mezclarse. */
	Sea
};

/** Lo que el jugador ve y bebe. */
enum class ERainCatchQuality : uint8
{
	Empty,
	/** Solo lluvia: se bebe sin riesgo (biblia 01 §6.2, «lluvia recogida»). */
	Rain,
	/** Hay algo sin tratar (o un rastro de mar por debajo del umbral): cuenta como `agua_sin_tratar`. */
	Untreated,
	/** Salobre: al menos SeaFractionForSalty de agua de mar; cuenta como `agua_mar`. */
	Sea
};

EXPLORED_API const TCHAR* LexToString(ERainCatchQuality Quality);

/** Cómo es el recipiente puesto en el mundo. */
struct EXPLORED_API FRainCatchSpec
{
	int64 CapacityMicroL = 0;
	/** Superficie de la boca abierta al cielo, en m². 0 = cerrado (no recoge ni evapora). */
	double MouthAreaM2 = 0.0;
	/** Bajo un techo: no recoge lluvia y evapora menos (a la sombra). */
	bool bSheltered = false;
};

/** Ritmos constantes durante una franja. */
struct EXPLORED_API FRainCatchRates
{
	int64 InPerMinute = 0;
	int64 EvaporationPerMinute = 0;
};

/** Estado guardable de un recipiente del mundo. */
struct EXPLORED_API FRainCatchState
{
	int64 RainMicroL = 0;
	/** Agua ajena (sin tratar más la de mar). */
	int64 OtherMicroL = 0;
	/** Parte de OtherMicroL que es agua de mar: el umbral de salobre se mide con ella. */
	int64 SeaMicroL = 0;
	/** El tipo más fuerte presente: Sea si SeaMicroL > 0. Un estado antiguo sin SeaMicroL con Sea se toma todo como mar. */
	ERainCatchLiquid OtherKind = ERainCatchLiquid::None;
	int64 LastUpdateMinute = 0;

	/** Contadores para depurar y para los tests de conservación (no hace falta guardarlos). */
	int64 CaughtMicroL = 0;
	int64 SpilledMicroL = 0;
	int64 EvaporatedMicroL = 0;

	int64 TotalMicroL() const { return RainMicroL + OtherMicroL; }
	float TotalLiters() const;
};

/**
 * Muestras del tiempo por franja de 10 minutos, compartidas entre todos los
 * recipientes: ponerse al día 60 días son 8640 franjas y cada FWeatherModel::SampleAt
 * cuesta ~1,4 µs, así que sin caché cada recipiente pagaría ~12 ms. El subsistema
 * guarda una y la pasa a Advance. No es segura entre hilos (solo el hilo de juego).
 */
class EXPLORED_API FRainCatchSky
{
public:
	/** Franjas guardadas como mucho (algo más de 60 días); al pasarse se vacía entera. */
	static constexpr int32 MaxCachedSlots = 9000;

	explicit FRainCatchSky(const FWeatherModel& InWeather) : Weather(InWeather) {}

	/** Muestra en el centro de la franja Slot (minutos [Slot·10, Slot·10 + 10)). */
	FWeatherSample SampleForSlot(int64 Slot) const;

	int32 NumCached() const { return Cache.Num(); }

private:
	const FWeatherModel& Weather;
	mutable TMap<int64, FWeatherSample> Cache;
};

struct EXPLORED_API FRainCatchModel
{
	static constexpr int64 MicroLPerLiter = 1000000;
	static constexpr int64 MinutesPerDay = 1440;
	/** Tope de capacidad (1000 L): el reparto proporcional multiplica dos cantidades y tiene que caber en int64. */
	static constexpr int64 MaxCapacityMicroL = 1000 * MicroLPerLiter;
	/** Franja de muestreo del tiempo, alineada con el minuto 0 de la partida. */
	static constexpr int64 SlotMinutes = 10;
	/** Ponerse al día nunca recorre más de 60 días: lo anterior ya se ha llenado, evaporado o rebosado muchas veces. */
	static constexpr int64 MaxCatchUpMinutes = 60 * MinutesPerDay;
	/**
	 * Reloj máximo que se simula (10 000 días de juego, más de 300 años de 32 días).
	 * Más allá, FWeatherModel pierde precisión en float y desborda al pasar a int.
	 */
	static constexpr int64 MaxSupportedMinute = 10000 * MinutesPerDay;
	/** Fracción de agua de mar a partir de la cual la mezcla es salobre (~1 g/L de sal). */
	static constexpr double SeaFractionForSalty = 0.03;
	/** Evaporación a la sombra (a cubierto) frente al sol. */
	static constexpr double ShelteredEvaporationFactor = 0.3;
	/** Por debajo de esta lluvia (0–1) se considera que no llueve y hay evaporación. */
	static constexpr float RainThreshold = 0.02f;

	/** Intensidad en mm/h a partir de FWeatherSample::Rain (tabla del GDD v2 §3.16). */
	static float RainMmPerHour(float Rain01);

	/** Evaporación en mm/h de una superficie de agua al aire libre; 0 mientras llueve. */
	static float EvaporationMmPerHour(const FWeatherSample& Sample);

	static FRainCatchRates RatesFor(const FRainCatchSpec& Spec, const FWeatherSample& Sample);

	/** Boca de cada recipiente del catálogo (m²); 0 si no es un recipiente abierto (coco verde, cesta, mochila...). */
	static double MouthAreaM2ForItem(FName ItemId);

	/** Recipiente de catálogo puesto en el suelo; la capacidad es la del inventario (Recipiente × 0,25 L). */
	static FRainCatchSpec SpecForItem(FName ItemId, float RecipienteValue, bool bSheltered);

	/**
	 * Avanza NMinutes con ritmos constantes, minuto a minuto: entra la lluvia,
	 * se evapora (solo si no entra nada) y rebosa lo que no cabe. Lo que sale
	 * (evaporación o rebose) se reparte entre lluvia y el resto en proporción,
	 * redondeando hacia arriba lo que sale del resto para que un lavado largo lo
	 * deje en cero. Con solo lluvia dentro se resuelve de una vez, con el mismo
	 * resultado exacto. NMinutes se acota a MaxCatchUpMinutes.
	 */
	static void StepMinutes(FRainCatchState& State, const FRainCatchSpec& Spec, const FRainCatchRates& Rates, int64 NMinutes);

	/**
	 * Avanza hasta NowMinute con el tiempo del FWeatherModel, franja a franja
	 * (muestra en el centro de cada franja). Devuelve los microlitros de lluvia
	 * recogidos en esta llamada. Un reloj que retrocede no hace nada; uno fuera de
	 * ±MaxSupportedMinute tampoco (ni se adopta). Un LastUpdateMinute guardado más
	 * allá de MaxSupportedMinute se corrige a NowMinute sin simular.
	 */
	static int64 Advance(FRainCatchState& State, const FRainCatchSpec& Spec, const FRainCatchSky& Sky, int64 NowMinute);

	/** Igual, sin caché compartida (para un recipiente suelto o para tests). */
	static int64 Advance(FRainCatchState& State, const FRainCatchSpec& Spec, const FWeatherModel& Weather, int64 NowMinute);

	/** Vierte líquido dentro. Devuelve lo que cabe (el resto no entra). None = agua limpia. */
	static int64 Pour(FRainCatchState& State, const FRainCatchSpec& Spec, int64 MicroL, ERainCatchLiquid Kind);

	/** Saca (bebe o vierte) hasta MicroL, en proporción. Devuelve lo que sale de verdad. */
	static int64 Take(FRainCatchState& State, int64 MicroL, int64* OutRainMicroL = nullptr, int64* OutOtherMicroL = nullptr);

	static ERainCatchQuality Quality(const FRainCatchState& State);

	/** Deja un estado cargado dentro de rango: cantidades entre 0 y MaxCapacityMicroL y un tipo coherente. */
	static void Sanitize(FRainCatchState& State);
};
