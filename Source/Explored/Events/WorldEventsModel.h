#pragma once

#include "CoreMinimal.h"
#include "Weather/WeatherModel.h"
#include "WorldGen/ArchipelagoLayout.h"

/** Eventos del mundo vivo (GDD §9.3, §10; biblia §6.3 y §11). */
enum class EWorldEventType : uint8
{
	TurtleNesting,       // Desove de tortugas en Arenas Blancas (noche de luna llena)
	TurtleHatchlings,    // Crías de tortuga hacia el mar (amanecer siguiente al desove)
	Bioluminescence,     // El mar brilla (noche de luna nueva)
	MeteorShower,        // Lluvia de estrellas (rara, solo noches despejadas)
	WhalePassage,        // Paso de ballenas (cada ~10 días, de día)
	ShipOnHorizon,       // Barco en el horizonte (ocasional)
	MinorEruption,       // Erupción menor en la Isla del Humo (azar)
	ExtremeSpringTide,   // Marea viva extrema (rara, con marea viva; descubre ruinas sumergidas)
	Count
};

EXPLORED_API const TCHAR* LexToString(EWorldEventType Type);

/**
 * Una aparición concreta de un evento. Todo se deriva de la semilla y del
 * tiempo, así que dos modelos con la misma semilla producen exactamente los
 * mismos eventos, con el mismo Id.
 */
struct EXPLORED_API FWorldEvent
{
	EWorldEventType Type = EWorldEventType::Count;

	/** Intervalo [Start, End) en días totales de juego. */
	float Start = 0.0f;
	float End = 0.0f;

	/**
	 * Identificador estable (tipo y día de anclaje): sirve para guardar
	 * resultados de un solo uso (paquete ya soltado, obsidiana ya depositada).
	 */
	uint64 Id = 0;

	/** Isla donde ocurre; EIslandArchetype::Count si es en todo el archipiélago o en el mar. */
	EIslandArchetype Island = EIslandArchetype::Count;

	/**
	 * Fuerza en [0, 1]: densidad de la lluvia de estrellas, tamaño del grupo de
	 * ballenas, violencia de la erupción, retirada extra de la marea viva
	 * extrema, brillo de la bioluminiscencia...
	 */
	float Intensity = 1.0f;

	/** Rumbo unitario (X norte, Y este) por donde aparece en el horizonte (ballenas, barco). */
	FVector2D Direction = FVector2D(1.0f, 0.0f);

	/** Semilla propia para que la capa de UE derive posiciones y variaciones del evento. */
	uint32 Seed = 0;

	bool IsActiveAt(float TotalDays) const { return TotalDays >= Start && TotalDays < End; }
	float Duration() const { return End - Start; }
	/** Avance en [0, 1] dentro del intervalo. */
	float ProgressAt(float TotalDays) const;

	bool operator==(const FWorldEvent& Other) const { return Id == Other.Id; }
};

/** Estado de una erupción menor en un instante (lo leen la cámara, el audio y las partículas). */
struct EXPLORED_API FEruptionSample
{
	/** Temblor en [0, 1]: aviso al principio, máximo en el estallido. */
	float Tremor = 0.0f;
	/** Ceniza en el aire en [0, 1]: sube tras el estallido y se posa despacio. */
	float Ash = 0.0f;
};

/** Qué hace el barco del horizonte ante la hoguera de señal. */
enum class EShipSignalOutcome : uint8
{
	NoShip,          // No hay barco a la vista en este instante
	NoSignal,        // Hay barco, pero no ve ninguna hoguera de señal
	AlreadyDropped,  // Este barco ya soltó su paquete
	DropPackage,     // Suelta un paquete a la deriva (la capa de UE lo crea y consume el Id)
};

/**
 * Datos mínimos que se guardan: todo el calendario sale de la semilla y del
 * tiempo, así que solo hace falta recordar los resultados de un solo uso ya
 * consumidos (paquete del barco soltado, obsidiana de una erupción ya
 * depositada).
 */
struct EXPLORED_API FWorldEventsState
{
	/** Ids (FWorldEvent::Id) de los resultados ya consumidos, en orden de consumo. */
	TArray<uint64> ConsumedOutcomes;

	bool IsConsumed(uint64 EventId) const { return ConsumedOutcomes.Contains(EventId); }
	void Consume(uint64 EventId) { ConsumedOutcomes.AddUnique(EventId); }
};

/**
 * Calendario determinista de eventos del mundo. Para una semilla, cualquier
 * instante da siempre los mismos eventos: se generan por día de anclaje (el
 * día en que empiezan) sin estado global. El clima se consulta con un
 * FWeatherModel propio construido con la semilla del clima del mundo, así que
 * coincide con el de UExploredWeatherSubsystem (salvo estados forzados de
 * depuración).
 *
 * Horario (horas locales; a 12° S el Sol sale entre 5:40 y 6:20):
 * - Noche de eventos: 19:00–05:00 (lluvia de estrellas: 20:00–04:00).
 * - Amanecer: 05:30–07:30. Día de ballenas: entre 09:00 y 17:00.
 */
class EXPLORED_API FWorldEventsModel
{
public:
	static constexpr float NightStartHour = 19.0f;
	static constexpr float NightEndHour = 5.0f;
	static constexpr float MeteorStartHour = 20.0f;
	static constexpr float MeteorEndHour = 4.0f;
	static constexpr float DawnStartHour = 5.5f;
	static constexpr float DawnEndHour = 7.5f;

	/** Cadencia media del paso de ballenas (días). */
	static constexpr int32 WhaleCadenceDays = 10;

	/** Probabilidades por candidato (antes del filtro del clima). */
	static constexpr float MeteorChancePerNight = 0.08f;
	static constexpr float ShipChancePerDay = 0.07f;
	static constexpr float EruptionChancePerDay = 0.045f;
	static constexpr float ExtremeTideChanceDry = 0.2f;
	static constexpr float ExtremeTideChanceOther = 0.07f;

	/** Días hacia delante que mira NextOccurrence por defecto (algo más de cinco años de juego). */
	static constexpr int32 DefaultSearchDays = 180;

	FWorldEventsModel(uint32 InSeed, uint32 InWeatherSeed);

	/** Eventos que empiezan el día indicado (días enteros de juego), ordenados por inicio. */
	TArray<FWorldEvent> EventsStartingOnDay(int32 Day) const;

	/** Eventos activos en el instante (Start <= T < End), ordenados por inicio. */
	TArray<FWorldEvent> ActiveAt(float TotalDays) const;

	/** ¿Hay un evento de ese tipo activo en el instante? Si lo hay y OutEvent no es nulo, lo copia. */
	bool IsActive(EWorldEventType Type, float TotalDays, FWorldEvent* OutEvent = nullptr) const;

	/**
	 * Siguiente aparición de un tipo que empiece estrictamente después de
	 * AfterDays, buscando como mucho SearchDays días; false si no la hay.
	 */
	bool NextOccurrence(EWorldEventType Type, float AfterDays, FWorldEvent& OutEvent, int32 SearchDays = DefaultSearchDays) const;

	/**
	 * Eventos que se solapan con [FromDays, ToDays), ordenados por inicio (para
	 * la UI y el audio). Con FromDays == ToDays equivale a ActiveAt(FromDays).
	 */
	TArray<FWorldEvent> EventsInWindow(float FromDays, float ToDays) const;

	/**
	 * Recorre en orden los eventos que se solapan con [FromDays, ToDays) sin
	 * reservar la lista entera; el visitante devuelve false para parar.
	 */
	void ForEachInWindow(float FromDays, float ToDays, TFunctionRef<bool(const FWorldEvent&)> Visitor) const;

	/**
	 * Decisión del barco del horizonte: si en TotalDays hay un barco a la vista
	 * y la hoguera de señal está encendida, suelta un paquete una sola vez.
	 * No modifica el estado: con DropPackage, quien llama crea el paquete y
	 * llama a State.Consume(OutShip.Id).
	 */
	EShipSignalOutcome DecideShipPackage(float TotalDays, bool bSignalFireLit, const FWorldEventsState& State, FWorldEvent& OutShip) const;

	/** Temblor y ceniza de una erupción en un instante (cero fuera de ella). */
	static FEruptionSample SampleEruption(const FWorldEvent& Eruption, float TotalDays);

	/**
	 * La erupción deja obsidiana nueva una vez terminada; true si ya terminó y
	 * aún no se ha depositado (después, State.Consume(Eruption.Id)).
	 */
	static bool ShouldDepositObsidian(const FWorldEvent& Eruption, float TotalDays, const FWorldEventsState& State);

	/**
	 * Retirada extra de la marea en [0, 1] por una marea viva extrema activa
	 * (0 si no hay): campana centrada en la bajamar, escalada por Intensity.
	 * El océano la suma a la bajamar normal para descubrir las ruinas.
	 */
	float ExtremeTideDrawdown(float TotalDays) const;

	/** Brillo del mar en [0, 1]: máximo en luna nueva y de noche (GDD §9.1). */
	static float BioluminescenceAt(float TotalDays);

	/** El clima que consulta el calendario (misma semilla que el del mundo). */
	const FWeatherModel& GetWeather() const { return Weather; }
	uint32 GetSeed() const { return Seed; }

	/** Id estable de un evento: tipo en los 8 bits altos y día de anclaje en los bajos. */
	static uint64 MakeEventId(EWorldEventType Type, int32 AnchorDay);

private:
	void GenerateDay(int32 Day, TArray<FWorldEvent>& Out) const;

	/** ¿La noche que empieza el día indicado es de desove (luna llena y sin temporal)? */
	bool HasTurtleNesting(int32 Day) const;

	/** Día y ventana del paso de ballenas del bloque de WhaleCadenceDays días; false si el clima lo impide. */
	bool WhaleForBlock(int32 Block, FWorldEvent& OutEvent) const;

	/** true si en ningún momento de [Start, End) el estado del tiempo es uno de los prohibidos. */
	bool IsWindowFreeOf(float Start, float End, std::initializer_list<EWeatherState> Forbidden) const;

	/** true si todo [Start, End) está en alguno de los estados permitidos. */
	bool IsWindowOnly(float Start, float End, std::initializer_list<EWeatherState> Allowed) const;

	uint32 Seed;
	FWeatherModel Weather;
};
