#pragma once

#include "CoreMinimal.h"

/** Estado de una prueba de recorrido a pie. */
enum class EWalkProbeStatus : uint8
{
	Walking,
	Arrived,
	Stuck,
	TimedOut,
};

EXPLORED_API const TCHAR* LexToString(EWalkProbeStatus Status);

struct EXPLORED_API FWalkProbeConfig
{
	/** Un punto de paso se da por alcanzado a esta distancia horizontal (m). */
	float ArriveRadius = 2.0f;
	/** Atascado: recorre menos de StuckDistance metros en StuckSeconds segundos. */
	float StuckSeconds = 6.0f;
	float StuckDistance = 1.5f;
	/** Tope de duración del recorrido (s). */
	float TimeoutSeconds = 300.0f;
};

/**
 * Prueba funcional de «se puede ir andando de A a B» (F3a: Cala Varada → base del faro). Modelo
 * puro: recibe la posición del personaje en cada paso y dice hacia dónde andar y si ha llegado,
 * se ha atascado o se le ha acabado el tiempo. UExploredWalkProbe es la capa de UE que mueve al
 * personaje con AddMovementInput (lo mismo que el teclado).
 */
class EXPLORED_API FWalkProbeModel
{
public:
	/** Ruta en metros (plano XY). Una ruta vacía se da por recorrida. */
	void Start(const TArray<FVector2D>& InWaypoints, const FWalkProbeConfig& InConfig = FWalkProbeConfig());

	/** Avanza DeltaSeconds con el personaje en Position (m) y devuelve el estado. */
	EWalkProbeStatus Step(const FVector2D& Position, float DeltaSeconds);

	/** Dirección unitaria hacia el punto de paso actual (cero si ya no anda). */
	FVector2D DesiredDirection(const FVector2D& Position) const;

	EWalkProbeStatus GetStatus() const { return Status; }
	int32 GetWaypointIndex() const { return WaypointIndex; }
	int32 GetWaypointCount() const { return Waypoints.Num(); }
	float GetElapsedSeconds() const { return Elapsed; }
	/** Metros recorridos (suma de desplazamientos horizontales entre pasos). */
	float GetTravelledMeters() const { return Travelled; }

private:
	TArray<FVector2D> Waypoints;
	FWalkProbeConfig Config;
	EWalkProbeStatus Status = EWalkProbeStatus::Arrived;
	int32 WaypointIndex = 0;
	float Elapsed = 0.0f;
	float Travelled = 0.0f;
	bool bHasLast = false;
	FVector2D LastPosition = FVector2D::ZeroVector;
	/** Ventana de atasco: posición y tiempo al empezarla. */
	FVector2D WindowStart = FVector2D::ZeroVector;
	float WindowElapsed = 0.0f;
};
