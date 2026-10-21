#pragma once

#include "CoreMinimal.h"
#include "Survival/BodyEventsModel.h"

/**
 * Balsa de Baliza Siete (GDD v3 §5.6 receta 8 y §5.7): el sitio de Playa Norte donde se dejan los
 * 4 troncos y se ata la balsa, el movimiento a remo y las reglas del cruce al bajo.
 *
 * El movimiento es cinemático y en 2D (metros y grados, X norte, Y este como Unreal): sin
 * hidrostática, así que no puede volcar (estabilidad forzada). W avanza hacia la velocidad de
 * remo, A/D giran, y la corriente lateral se suma siempre. El cabeceo es solo visual y está
 * amortiguado (StableTilt). Funciones puras: AExploredRaft y AExploredRaftSite son la capa fina.
 */

struct EXPLORED_API FRaftTuning
{
	static constexpr int32 DefaultLogsNeeded = 4;
	static constexpr float DefaultPaddleSpeedMps = 2.0f;
	static constexpr float DefaultTurnDegPerSecond = 40.0f;
	static constexpr float DefaultAccelerationMps2 = 1.2f;
	static constexpr float DefaultCurrentMps = 0.3f;
	static constexpr float DefaultMaxTiltDeg = 4.0f;
	/** Pérdida de sed y hambre al cruzar como se pueda a las 18:45 del día 2 (GDD §5.7). */
	static constexpr float DefaultForcedReturnThirst = -20.0f;
	static constexpr float DefaultForcedReturnHunger = -20.0f;

	int32 LogsNeeded = DefaultLogsNeeded;
	float PaddleSpeedMps = DefaultPaddleSpeedMps;
	float TurnDegPerSecond = DefaultTurnDegPerSecond;
	float AccelerationMps2 = DefaultAccelerationMps2;
	float CurrentMps = DefaultCurrentMps;
	float MaxTiltDeg = DefaultMaxTiltDeg;
	float ForcedReturnThirst = DefaultForcedReturnThirst;
	float ForcedReturnHunger = DefaultForcedReturnHunger;
};

/** Sitio de la balsa: troncos dejados y si ya está atada. */
struct EXPLORED_API FRaftSiteState
{
	int32 LogsPlaced = 0;
	bool bBuilt = false;

	bool operator==(const FRaftSiteState& Other) const { return LogsPlaced == Other.LogsPlaced && bBuilt == Other.bBuilt; }
};

enum class ERaftSiteResult : uint8
{
	Ok,
	/** No lleva un tronco en las manos. */
	NoLog,
	/** Ya hay los 4 troncos. */
	SiteFull,
	/** Faltan troncos para atar. */
	MissingLogs,
	/** La balsa ya está hecha. */
	AlreadyBuilt,
};

EXPLORED_API const TCHAR* LexToString(ERaftSiteResult Result);

/** Estado cinemático en el plano del mar. */
struct EXPLORED_API FRaftMotion
{
	FVector2D PositionM = FVector2D::ZeroVector;
	float YawDeg = 0.0f;
	/** Velocidad propia a remo (sin la corriente), m/s. */
	float SpeedMps = 0.0f;
};

struct EXPLORED_API FRaftControls
{
	/** W: 0-1 (S no rema hacia atrás: sin marcha atrás en el GDD). */
	float Forward = 0.0f;
	/** A/D: -1 izquierda, +1 derecha. */
	float Turn = 0.0f;
	bool bHasPaddle = true;
	/** Encallada: no avanza ni la mueve la corriente (puede girar para salir). */
	bool bGrounded = false;
};

enum class ERaftBoard : uint8
{
	Ok,
	NotBuilt,
	NoPaddle,
	/** Desde las 18:00 del día 2 el mar está cerrado. */
	SeaClosed,
};

EXPLORED_API const TCHAR* LexToString(ERaftBoard Result);

struct EXPLORED_API FRaftModel
{
	/** Deja un tronco en el sitio (el que lleva en las manos). */
	static ERaftSiteResult PlaceLog(const FRaftSiteState& In, bool bHoldingLog, FRaftSiteState& Out,
		const FRaftTuning& Tuning = FRaftTuning());

	/** ¿Se puede atar? (Las cuerdas las comprueba FRecipeBook con la receta «balsa».) */
	static ERaftSiteResult CanTie(const FRaftSiteState& State, const FRaftTuning& Tuning = FRaftTuning());

	/** Ata la balsa: los troncos pasan a ser la balsa. */
	static ERaftSiteResult Tie(const FRaftSiteState& In, FRaftSiteState& Out, const FRaftTuning& Tuning = FRaftTuning());

	/** Estado coherente para cargar partidas. */
	static bool IsValid(const FRaftSiteState& State, const FRaftTuning& Tuning = FRaftTuning());

	/** ¿Puede embarcar? AbsHour en horas absolutas; RaftLastHour = D2 18:00 (FStormClockConfig). */
	static ERaftBoard CanBoard(const FRaftSiteState& Site, bool bHasPaddle, double AbsHour, double RaftLastHour);

	/** Dirección de avance para un rumbo (X norte, Y este). */
	static FVector2D Forward(float YawDeg);

	/**
	 * Un paso de movimiento. La velocidad propia tiende a Forward × PaddleSpeed (con remo) con
	 * aceleración limitada; el giro necesita el remo; la corriente (m/s, ya orientada) se suma.
	 * Un paso no finito o negativo no cambia nada.
	 */
	static FRaftMotion Step(const FRaftMotion& In, const FRaftControls& Controls, const FVector2D& CurrentMps, float DeltaSeconds,
		const FRaftTuning& Tuning = FRaftTuning());

	/** Corriente lateral «a la izquierda» de un rumbo de referencia, CurrentMps de módulo. */
	static FVector2D LateralCurrent(float RouteYawDeg, const FRaftTuning& Tuning = FRaftTuning());

	/** Cabeceo y balanceo visuales (grados) para una pendiente del agua, limitados a ±MaxTiltDeg. */
	static FVector2D StableTilt(float SlopeForward, float SlopeRight, const FRaftTuning& Tuning = FRaftTuning());

	/** A las 18:45 del día 2 (o después), si Iria sigue en el islote, fundido y vuelta a Playa Norte. */
	static bool ShouldForceReturn(double AbsHour, double ForcedReturnHour, bool bOnIslet);

	/** Barras tras el cruce forzoso: sed y hambre -20. */
	static FBodyBars ForcedReturnBars(const FBodyBars& Bars, const FRaftTuning& Tuning = FRaftTuning());
};
