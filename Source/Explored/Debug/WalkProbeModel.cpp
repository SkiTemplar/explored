#include "Debug/WalkProbeModel.h"

const TCHAR* LexToString(EWalkProbeStatus Status)
{
	switch (Status)
	{
	case EWalkProbeStatus::Walking: return TEXT("walking");
	case EWalkProbeStatus::Arrived: return TEXT("arrived");
	case EWalkProbeStatus::Stuck: return TEXT("stuck");
	default: return TEXT("timeout");
	}
}

void FWalkProbeModel::Start(const TArray<FVector2D>& InWaypoints, const FWalkProbeConfig& InConfig)
{
	Waypoints = InWaypoints;
	Config = InConfig;
	Status = Waypoints.IsEmpty() ? EWalkProbeStatus::Arrived : EWalkProbeStatus::Walking;
	WaypointIndex = 0;
	Elapsed = 0.0f;
	Travelled = 0.0f;
	bHasLast = false;
	WindowElapsed = 0.0f;
}

EWalkProbeStatus FWalkProbeModel::Step(const FVector2D& Position, float DeltaSeconds)
{
	if (Status != EWalkProbeStatus::Walking)
	{
		return Status;
	}
	const float Dt = FMath::IsFinite(DeltaSeconds) ? FMath::Clamp(DeltaSeconds, 0.0f, 1.0f) : 0.0f;
	Elapsed += Dt;
	if (bHasLast)
	{
		Travelled += static_cast<float>(FVector2D::Distance(Position, LastPosition));
	}
	else
	{
		WindowStart = Position;
		bHasLast = true;
	}
	LastPosition = Position;

	// Salta todos los puntos de paso ya alcanzados (puede atajar por una curva).
	while (WaypointIndex < Waypoints.Num() && FVector2D::Distance(Position, Waypoints[WaypointIndex]) <= Config.ArriveRadius)
	{
		++WaypointIndex;
		WindowStart = Position;
		WindowElapsed = 0.0f;
	}
	if (WaypointIndex >= Waypoints.Num())
	{
		Status = EWalkProbeStatus::Arrived;
		return Status;
	}

	WindowElapsed += Dt;
	if (WindowElapsed >= Config.StuckSeconds)
	{
		if (FVector2D::Distance(Position, WindowStart) < Config.StuckDistance)
		{
			Status = EWalkProbeStatus::Stuck;
			return Status;
		}
		WindowStart = Position;
		WindowElapsed = 0.0f;
	}
	if (Elapsed >= Config.TimeoutSeconds)
	{
		Status = EWalkProbeStatus::TimedOut;
	}
	return Status;
}

FVector2D FWalkProbeModel::DesiredDirection(const FVector2D& Position) const
{
	if (Status != EWalkProbeStatus::Walking || !Waypoints.IsValidIndex(WaypointIndex))
	{
		return FVector2D::ZeroVector;
	}
	return (Waypoints[WaypointIndex] - Position).GetSafeNormal();
}
