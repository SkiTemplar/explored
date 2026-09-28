#pragma once

#include "CoreMinimal.h"

#include "Weather/WeatherModel.h"

/**
 * Paquete de reloj y clima del `GameState` (biblia 08 §2.8): **11 bytes** a 0,2 Hz con
 * el día, la hora, el `TimeScale`, la estación, el tiempo que hace, el viento, la lluvia,
 * el estado de mar y la tormenta. Se replica el reloj, no sus consecuencias: cielo, sol,
 * luna, mareas, olas y corrientes se derivan en cada cliente de estos 11 bytes.
 *
 * | Byte | Campo | Cuantización |
 * |---|---|---|
 * | 0–1 | `Day` | `uint16` |
 * | 2–3 | `Hours` | `uint16` × 0,001 h (3,6 s), 0–23 999 |
 * | 4 | `TimeScale` | `uint8` × 0,5 (0–127,5; el ×120 de dormir cabe) |
 * | 5 | estación (bits 0–1) + tiempo (bits 2–5) | bits 6–7 a cero |
 * | 6 | `Wind01` | `uint8` × 1/255 |
 * | 7 | `WindFromDeg` | `uint8`, vuelta completa en 256 pasos |
 * | 8 | `Rain01` | `uint8` × 1/255 |
 * | 9 | `SeaState01` | `uint8` × 1/255 |
 * | 10 | `StormIntensity01` (bits 0–4, × 1/31) + categoría de ciclón (bits 5–7, 0–5) | |
 *
 * Modelo puro (solo CoreMinimal): el `AExploredGameState` solo llama a Encode/Decode y a
 * la corrección de reloj, y la replicación real la hace Unreal con estos bytes.
 */
struct EXPLORED_API FWorldClockNetState
{
	int32 Day = 4;
	/** Hora local en [0, 24). */
	float Hours = 7.5f;
	float TimeScale = 1.0f;
	ESeason Season = ESeason::Dry;
	EWeatherState Weather = EWeatherState::Clear;
	float Wind01 = 0.2f;
	/** De dónde sopla el viento, en grados (0 = norte, sentido horario). */
	float WindFromDeg = 90.0f;
	float Rain01 = 0.0f;
	float SeaState01 = 0.15f;
	float StormIntensity01 = 0.0f;
	/** 0 sin ciclón; 1–5 durante un ciclón (FWeatherModel::CycloneCategoryAt). */
	int32 CycloneCategory = 0;

	/** Horas totales de juego (Day × 24 + Hours). */
	double TotalHours() const { return static_cast<double>(Day) * 24.0 + static_cast<double>(Hours); }
};

/** Cómo debe mover el cliente su reloj local hacia el del servidor. */
struct EXPLORED_API FWorldClockCorrection
{
	/** Servidor − cliente en horas de juego (positivo: el cliente va atrasado). */
	double ErrorHours = 0.0;
	/** Error mayor que el umbral: saltar directamente al reloj del servidor. */
	bool bHardSnap = false;
	/** Multiplicador local sobre el `TimeScale` replicado, entre 0,95 y 1,05 (1 si hay salto). */
	float LocalRateFactor = 1.0f;
};

class EXPLORED_API FWorldClockNetModel
{
public:
	static constexpr int32 PacketBytes = 11;
	/** Envío periódico: cada 5 s (0,2 Hz). */
	static constexpr double SendIntervalSeconds = 5.0;
	/** Salto duro por encima de 6 minutos de juego de error. */
	static constexpr double HardSnapErrorHours = 0.1;
	/** Corrección suave: nunca más de un 5 % más rápido o más lento. */
	static constexpr float MaxRateDeviation = 0.05f;
	static constexpr int32 MaxCycloneCategory = 5;
	static constexpr float MaxTimeScale = 127.5f;

	/** Escribe exactamente PacketBytes bytes (sustituye el contenido de Out). */
	static void Encode(const FWorldClockNetState& State, TArray<uint8>& Out);

	/**
	 * Lee un paquete. Devuelve false, sin tocar OutState, si no mide PacketBytes, si la
	 * hora pasa de 23,999 h, si la estación, el tiempo o la categoría de ciclón no existen
	 * o si los bits reservados no van a cero (paquete corrupto o de otra versión).
	 */
	static bool Decode(const TArray<uint8>& In, FWorldClockNetState& OutState);

	/** El estado tal como lo verá el cliente (Encode + Decode). */
	static FWorldClockNetState Quantize(const FWorldClockNetState& State);

	/**
	 * ¿Hay que mandar ya, sin esperar a los 5 s? Sí cuando cambia algo de lo que se manda
	 * «al cambiar» (`TimeScale`, estación, tiempo, tormenta o categoría de ciclón),
	 * comparado ya cuantizado para que el ruido por debajo de un paso no dispare envíos.
	 */
	static bool NeedsImmediateSend(const FWorldClockNetState& LastSent, const FWorldClockNetState& Now);

	/** Envío periódico o inmediato; SecondsSinceLastSend < 0 = nunca enviado. */
	static bool ShouldSend(const FWorldClockNetState& LastSent, const FWorldClockNetState& Now, double SecondsSinceLastSend);

	/**
	 * Corrección del reloj local del cliente hacia el recibido: por encima de 6 minutos de
	 * juego, salto duro; por debajo, un multiplicador proporcional al error entre 0,95 y
	 * 1,05 que lo cierra sin mover el sol ni la marea a tirones. Horas no finitas → salto.
	 */
	static FWorldClockCorrection Correct(double LocalTotalHours, double ServerTotalHours);
};
