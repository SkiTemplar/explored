#include "Boats/RaftModel.h"

namespace RaftModelDetail
{
	/** Rumbo en (-180, 180]. */
	float NormalizeYaw(float Degrees)
	{
		if (!FMath::IsFinite(Degrees))
		{
			return 0.0f;
		}
		float Yaw = FMath::Fmod(Degrees, 360.0f);
		if (Yaw > 180.0f)
		{
			Yaw -= 360.0f;
		}
		else if (Yaw <= -180.0f)
		{
			Yaw += 360.0f;
		}
		return Yaw;
	}
}

const TCHAR* LexToString(ERaftSiteResult Result)
{
	switch (Result)
	{
	case ERaftSiteResult::Ok: return TEXT("Ok");
	case ERaftSiteResult::NoLog: return TEXT("NoLog");
	case ERaftSiteResult::SiteFull: return TEXT("SiteFull");
	case ERaftSiteResult::MissingLogs: return TEXT("MissingLogs");
	case ERaftSiteResult::AlreadyBuilt: return TEXT("AlreadyBuilt");
	default: return TEXT("?");
	}
}

const TCHAR* LexToString(ERaftBoard Result)
{
	switch (Result)
	{
	case ERaftBoard::Ok: return TEXT("Ok");
	case ERaftBoard::NotBuilt: return TEXT("NotBuilt");
	case ERaftBoard::NoPaddle: return TEXT("NoPaddle");
	case ERaftBoard::SeaClosed: return TEXT("SeaClosed");
	default: return TEXT("?");
	}
}

ERaftSiteResult FRaftModel::PlaceLog(const FRaftSiteState& In, bool bHoldingLog, FRaftSiteState& Out, const FRaftTuning& Tuning)
{
	Out = In;
	if (In.bBuilt)
	{
		return ERaftSiteResult::AlreadyBuilt;
	}
	if (In.LogsPlaced >= Tuning.LogsNeeded)
	{
		return ERaftSiteResult::SiteFull;
	}
	if (!bHoldingLog)
	{
		return ERaftSiteResult::NoLog;
	}
	Out.LogsPlaced = In.LogsPlaced + 1;
	return ERaftSiteResult::Ok;
}

ERaftSiteResult FRaftModel::CanTie(const FRaftSiteState& State, const FRaftTuning& Tuning)
{
	if (State.bBuilt)
	{
		return ERaftSiteResult::AlreadyBuilt;
	}
	return State.LogsPlaced >= Tuning.LogsNeeded ? ERaftSiteResult::Ok : ERaftSiteResult::MissingLogs;
}

ERaftSiteResult FRaftModel::Tie(const FRaftSiteState& In, FRaftSiteState& Out, const FRaftTuning& Tuning)
{
	Out = In;
	const ERaftSiteResult Check = CanTie(In, Tuning);
	if (Check == ERaftSiteResult::Ok)
	{
		Out.bBuilt = true;
	}
	return Check;
}

bool FRaftModel::IsValid(const FRaftSiteState& State, const FRaftTuning& Tuning)
{
	if (State.LogsPlaced < 0 || State.LogsPlaced > Tuning.LogsNeeded)
	{
		return false;
	}
	return !State.bBuilt || State.LogsPlaced == Tuning.LogsNeeded;
}

ERaftBoard FRaftModel::CanBoard(const FRaftSiteState& Site, bool bHasPaddle, double AbsHour, double RaftLastHour)
{
	if (!Site.bBuilt)
	{
		return ERaftBoard::NotBuilt;
	}
	if (!FMath::IsFinite(AbsHour) || AbsHour >= RaftLastHour)
	{
		return ERaftBoard::SeaClosed;
	}
	return bHasPaddle ? ERaftBoard::Ok : ERaftBoard::NoPaddle;
}

FVector2D FRaftModel::Forward(float YawDeg)
{
	const float Radians = FMath::DegreesToRadians(FMath::IsFinite(YawDeg) ? YawDeg : 0.0f);
	return FVector2D(FMath::Cos(Radians), FMath::Sin(Radians));
}

FRaftMotion FRaftModel::Step(const FRaftMotion& In, const FRaftControls& Controls, const FVector2D& CurrentMps, float DeltaSeconds,
	const FRaftTuning& Tuning)
{
	FRaftMotion Out = In;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return Out;
	}
	const float Throttle = FMath::IsFinite(Controls.Forward) ? FMath::Clamp(Controls.Forward, 0.0f, 1.0f) : 0.0f;
	const float Turn = FMath::IsFinite(Controls.Turn) ? FMath::Clamp(Controls.Turn, -1.0f, 1.0f) : 0.0f;
	const float Target = (Controls.bHasPaddle && !Controls.bGrounded) ? Throttle * Tuning.PaddleSpeedMps : 0.0f;
	const float MaxChange = Tuning.AccelerationMps2 * DeltaSeconds;
	Out.SpeedMps = In.SpeedMps + FMath::Clamp(Target - In.SpeedMps, -MaxChange, MaxChange);
	if (Controls.bGrounded)
	{
		Out.SpeedMps = 0.0f;
	}

	if (Controls.bHasPaddle)
	{
		Out.YawDeg = RaftModelDetail::NormalizeYaw(In.YawDeg + Turn * Tuning.TurnDegPerSecond * DeltaSeconds);
	}
	if (Controls.bGrounded)
	{
		return Out;
	}
	const FVector2D Current = (FMath::IsFinite(CurrentMps.X) && FMath::IsFinite(CurrentMps.Y)) ? CurrentMps : FVector2D::ZeroVector;
	Out.PositionM = In.PositionM + (Forward(Out.YawDeg) * Out.SpeedMps + Current) * DeltaSeconds;
	return Out;
}

FVector2D FRaftModel::LateralCurrent(float RouteYawDeg, const FRaftTuning& Tuning)
{
	// Izquierda del rumbo: -90° en Unreal (Y es la derecha de X).
	return Forward(RouteYawDeg - 90.0f) * Tuning.CurrentMps;
}

FVector2D FRaftModel::StableTilt(float SlopeForward, float SlopeRight, const FRaftTuning& Tuning)
{
	const auto Tilt = [&Tuning](float Slope)
	{
		if (!FMath::IsFinite(Slope))
		{
			return 0.0f;
		}
		// Amortiguado: la mitad del ángulo del agua, y nunca más del tope.
		const float Degrees = FMath::RadiansToDegrees(FMath::Atan(Slope)) * 0.5f;
		return FMath::Clamp(Degrees, -Tuning.MaxTiltDeg, Tuning.MaxTiltDeg);
	};
	return FVector2D(Tilt(SlopeForward), Tilt(SlopeRight));
}

bool FRaftModel::ShouldForceReturn(double AbsHour, double ForcedReturnHour, bool bOnIslet)
{
	return bOnIslet && FMath::IsFinite(AbsHour) && AbsHour >= ForcedReturnHour;
}

FBodyBars FRaftModel::ForcedReturnBars(const FBodyBars& Bars, const FRaftTuning& Tuning)
{
	FBodyBars Out = Bars;
	const auto Clamp = [](float Value) { return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 100.0f) : 0.0f; };
	Out.Thirst = Clamp(Bars.Thirst + Tuning.ForcedReturnThirst);
	Out.Hunger = Clamp(Bars.Hunger + Tuning.ForcedReturnHunger);
	return Out;
}
