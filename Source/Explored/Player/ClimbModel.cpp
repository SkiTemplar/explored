#include "Player/ClimbModel.h"

#include "Survival/SurvivalModel.h"

namespace
{
	/** Por debajo de este eje vertical se considera quieto (ruido del mando). */
	constexpr float VerticalDeadZone = 0.05f;

	/** Holgura numérica para comparar alturas (m). */
	constexpr float HeightEpsilon = 1e-4f;

	float Finite(float Value, float Fallback)
	{
		return FMath::IsFinite(Value) ? Value : Fallback;
	}
}

bool FClimbModel::IsClimbableSpecies(FName Species)
{
	return Species == FName(TEXT("Palm"));
}

bool FClimbModel::IsClimbableSlope(float SlopeDeg, const FClimbTuning& InTuning)
{
	return FMath::IsFinite(SlopeDeg) && SlopeDeg > InTuning.MinRockSlopeDeg;
}

float FClimbModel::ClimbDrainPerSecond(EClimbSurface Surface, const FClimberInfo& Who, const FClimbTuning& InTuning)
{
	const float Weight = FSurvivalModel::EnergyWeightFactor(FMath::Max(0.0f, Finite(Who.CarriedWeightRatio, 0.0f)));
	const bool bPalmFoot = Surface == EClimbSurface::Palm && Who.bHasPalmFoot;
	return (bPalmFoot ? InTuning.PalmFootDrainPerSecond : InTuning.ClimbDrainPerSecond) * Weight;
}

float FClimbModel::ReachM(const FClimbRoute& InRoute, const FClimbTuning& InTuning)
{
	const float Top = FMath::Max(0.0f, Finite(InRoute.TopHeightM, 0.0f));
	if (InRoute.Surface == EClimbSurface::Palm)
	{
		return Top;
	}

	// Anclajes: cada clavija y el final de la cuerda fija abren un tramo libre más,
	// pero solo si se llega a ellos desde abajo.
	TArray<float> Anchors;
	for (const float Piton : InRoute.PitonHeightsM)
	{
		if (FMath::IsFinite(Piton) && Piton >= 0.0f)
		{
			Anchors.Add(Piton);
		}
	}
	float Reach = InTuning.FreeReachM;
	const float RopeTop = FMath::Max(0.0f, Finite(InRoute.FixedRopeTopM, 0.0f));
	if (RopeTop > 0.0f)
	{
		Anchors.Add(RopeTop);
		Reach = FMath::Max(Reach, RopeTop);
	}
	Anchors.Sort();
	for (const float Anchor : Anchors)
	{
		if (Anchor > Reach + HeightEpsilon)
		{
			break;
		}
		Reach = FMath::Max(Reach, Anchor + InTuning.FreeReachM);
	}
	return FMath::Min(Reach, Top);
}

EClimbNotice FClimbModel::PitonNotice(float SlopeDeg, const FClimbTuning& InTuning)
{
	return IsClimbableSlope(SlopeDeg, InTuning) ? EClimbNotice::None : EClimbNotice::NoPitonSpot;
}

EClimbNotice FClimbModel::NoticeFor(EClimbReject Reject)
{
	switch (Reject)
	{
	case EClimbReject::CarryingSledge: return EClimbNotice::CarryingSledge;
	case EClimbReject::NoEnergy: return EClimbNotice::Exhausted;
	default: return EClimbNotice::None;
	}
}

FText FClimbModel::NoticeText(EClimbNotice Notice)
{
	switch (Notice)
	{
	case EClimbNotice::Exhausted: return NSLOCTEXT("ExploredClimb", "Exhausted", "No llego más arriba así.");
	case EClimbNotice::NoPitonSpot: return NSLOCTEXT("ExploredClimb", "NoPitonSpot", "Aquí no hay donde clavar nada.");
	case EClimbNotice::CarryingSledge: return NSLOCTEXT("ExploredClimb", "CarryingSledge", "Con las angarillas no puedo trepar.");
	default: return FText::GetEmpty();
	}
}

EClimbReject FClimbModel::Start(const FClimbRoute& InRoute, const FClimberInfo& Who, float Energy)
{
	if (State != EClimbState::None)
	{
		return EClimbReject::IllegalTransition;
	}
	if (!FMath::IsFinite(InRoute.TopHeightM) || InRoute.TopHeightM <= 0.0f || !FMath::IsFinite(InRoute.SlopeDeg)
		|| !FMath::IsFinite(InRoute.FixedRopeTopM) || InRoute.FixedRopeTopM < 0.0f)
	{
		return EClimbReject::InvalidRoute;
	}
	if (Who.bHasSledge)
	{
		return EClimbReject::CarryingSledge;
	}
	if (InRoute.Surface == EClimbSurface::Rock && !IsClimbableSlope(InRoute.SlopeDeg, Tuning))
	{
		return EClimbReject::SlopeTooGentle;
	}
	if (!(Energy > 0.0f))
	{
		return EClimbReject::NoEnergy;
	}

	Route = InRoute;
	Route.FixedRopeTopM = Route.Surface == EClimbSurface::Rock ? FMath::Min(Route.FixedRopeTopM, Route.TopHeightM) : 0.0f;
	Route.PitonHeightsM.Reset();
	if (Route.Surface == EClimbSurface::Rock)
	{
		for (const float Piton : InRoute.PitonHeightsM)
		{
			if (FMath::IsFinite(Piton) && Piton >= 0.0f && Piton <= Route.TopHeightM)
			{
				Route.PitonHeightsM.Add(Piton);
			}
		}
		Route.PitonHeightsM.Sort();
	}
	Climber = Who;
	Climber.CarriedWeightRatio = FMath::Max(0.0f, Finite(Who.CarriedWeightRatio, 0.0f));
	State = EClimbState::Grabbing;
	Height = 0.0f;
	FallFromM = 0.0f;
	bCrownReported = false;
	return EClimbReject::None;
}

bool FClimbModel::IsAtAnchor() const
{
	if (!IsOnWall())
	{
		return false;
	}
	const float Snap = Tuning.AnchorSnapM;
	if (Route.Surface == EClimbSurface::Palm)
	{
		return Height >= Route.TopHeightM - Snap;
	}
	if (Route.FixedRopeTopM > 0.0f && Height <= Route.FixedRopeTopM + Snap)
	{
		return true;
	}
	for (const float Piton : Route.PitonHeightsM)
	{
		if (FMath::Abs(Height - Piton) <= Snap)
		{
			return true;
		}
	}
	return false;
}

EClimbReject FClimbModel::Rest()
{
	if (State != EClimbState::Grabbing && State != EClimbState::Climbing)
	{
		return EClimbReject::IllegalTransition;
	}
	if (!IsAtAnchor())
	{
		return EClimbReject::NoAnchor;
	}
	State = EClimbState::Resting;
	return EClimbReject::None;
}

EClimbReject FClimbModel::Resume()
{
	if (State != EClimbState::Resting)
	{
		return EClimbReject::IllegalTransition;
	}
	State = EClimbState::Grabbing;
	return EClimbReject::None;
}

EClimbReject FClimbModel::Release()
{
	if (!IsOnWall())
	{
		return EClimbReject::IllegalTransition;
	}
	State = EClimbState::Falling;
	FallFromM = Height;
	return EClimbReject::None;
}

EClimbReject FClimbModel::DrivePiton()
{
	if (State != EClimbState::Grabbing && State != EClimbState::Resting)
	{
		return EClimbReject::IllegalTransition;
	}
	if (Route.Surface != EClimbSurface::Rock)
	{
		return EClimbReject::NotRock;
	}
	for (const float Piton : Route.PitonHeightsM)
	{
		if (FMath::Abs(Height - Piton) <= Tuning.AnchorSnapM)
		{
			return EClimbReject::None; // ya hay una aquí: no se gasta otra
		}
	}
	Route.PitonHeightsM.Add(Height);
	Route.PitonHeightsM.Sort();
	return EClimbReject::None;
}

FFallResult FClimbModel::Land(ELandingSurface Surface, float ExtraDropM)
{
	if (State != EClimbState::Falling)
	{
		return FFallResult();
	}
	const float Drop = FallFromM + FMath::Max(0.0f, Finite(ExtraDropM, 0.0f));
	State = EClimbState::None;
	Height = 0.0f;
	FallFromM = 0.0f;
	return FBodyModel::FallDamage(Drop, Surface);
}

float FClimbModel::SpeedMps() const
{
	if (OnFixedRope())
	{
		return Tuning.FixedRopeSpeedMps;
	}
	if (Route.Surface == EClimbSurface::Palm)
	{
		return Climber.bHasPalmFoot ? Tuning.PalmFootSpeedMps : Tuning.PalmSpeedMps;
	}
	return Tuning.RockSpeedMps;
}

bool FClimbModel::OnFixedRope() const
{
	return Route.FixedRopeTopM > 0.0f && Height < Route.FixedRopeTopM - HeightEpsilon;
}

void FClimbModel::BeginFall(FClimbStep& Out)
{
	State = EClimbState::Falling;
	FallFromM = Height;
	Out.bStartedFalling = true;
	Out.Notice = EClimbNotice::Exhausted;
}

FClimbStep FClimbModel::Tick(const FClimbInput& Input, float DeltaSeconds)
{
	FClimbStep Out;
	Out.HeightM = Height;
	if (!IsOnWall() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return Out;
	}

	if (State == EClimbState::Resting)
	{
		Out.EnergyDelta = Tuning.RestRecoveryPerSecond * DeltaSeconds;
		return Out;
	}

	float Energy = FMath::Max(0.0f, Finite(Input.Energy, 0.0f));
	const float Vertical = FMath::Clamp(Finite(Input.Vertical, 0.0f), -1.0f, 1.0f);
	const float Effort = FMath::Abs(Vertical) > VerticalDeadZone ? FMath::Abs(Vertical) : 0.0f;
	const float Direction = Effort > 0.0f ? FMath::Sign(Vertical) : 0.0f;

	// En la cuerda fija ni se gasta Energía ni hay caída por agotamiento (§13.3).
	const bool bRope = OnFixedRope() || (Direction < 0.0f && Route.FixedRopeTopM > 0.0f && Height <= Route.FixedRopeTopM + HeightEpsilon);
	const float MoveDrain = bRope ? 0.0f : ClimbDrainPerSecond(Route.Surface, Climber, Tuning) * FMath::Max(Effort, Tuning.HoldDrainFraction);
	const float HoldDrain = bRope ? 0.0f : ClimbDrainPerSecond(Route.Surface, Climber, Tuning) * Tuning.HoldDrainFraction;

	if (Energy <= 0.0f && HoldDrain > 0.0f)
	{
		BeginFall(Out);
		return Out;
	}

	// Tramo en movimiento: hasta el tope, la base, el final de la cuerda o el final del tick.
	const float Reach = ReachM(Route, Tuning);
	float Target = Height;
	if (Direction > 0.0f)
	{
		Target = bRope ? FMath::Min(Route.FixedRopeTopM, Reach) : Reach;
	}
	else if (Direction < 0.0f)
	{
		Target = Route.FixedRopeTopM > 0.0f && Height > Route.FixedRopeTopM + HeightEpsilon ? Route.FixedRopeTopM : 0.0f;
	}
	const float Speed = SpeedMps() * Effort;
	const float Distance = FMath::Abs(Target - Height);
	bool bArrives = Speed > 0.0f && Distance <= Speed * DeltaSeconds;
	float MoveTime = Speed > 0.0f ? (bArrives ? Distance / Speed : DeltaSeconds) : 0.0f;

	bool bExhausted = false;
	if (MoveDrain > 0.0f && MoveTime * MoveDrain >= Energy)
	{
		MoveTime = Energy / MoveDrain;
		bArrives = bArrives && Distance <= Speed * MoveTime;
		bExhausted = true;
	}
	Energy -= MoveTime * MoveDrain;
	Out.EnergyDelta -= MoveTime * MoveDrain;
	Height = bArrives ? Target : FMath::Clamp(Height + Direction * Speed * MoveTime, 0.0f, Reach);
	Out.HeightM = Height;
	State = Direction != 0.0f ? EClimbState::Climbing : EClimbState::Grabbing;

	if (bExhausted)
	{
		BeginFall(Out);
		return Out;
	}

	// Llegadas al final del tramo.
	if (Direction < 0.0f && Height <= HeightEpsilon)
	{
		Height = 0.0f;
		Out.HeightM = 0.0f;
		Out.bReachedBase = true;
		State = EClimbState::None;
		return Out;
	}
	if (Direction > 0.0f && Height >= Reach - HeightEpsilon)
	{
		if (Reach >= Route.TopHeightM - HeightEpsilon)
		{
			if (Route.Surface == EClimbSurface::Palm)
			{
				// En la copa: abrazado a ella se descansa y se cogen los cocos verdes.
				State = EClimbState::Resting;
				Out.bReachedCrown = !bCrownReported;
				bCrownReported = true;
			}
			else
			{
				Out.bToppedOut = true;
				State = EClimbState::None;
			}
			return Out;
		}
		Out.bAtReachLimit = true;
		State = EClimbState::Grabbing;
	}

	// Lo que queda del tick agarrado y quieto.
	const float HoldTime = DeltaSeconds - MoveTime;
	if (HoldTime > 0.0f && HoldDrain > 0.0f)
	{
		const float Spent = FMath::Min(Energy, HoldTime * HoldDrain);
		Energy -= Spent;
		Out.EnergyDelta -= Spent;
	}
	if (Energy <= 0.0f && HoldDrain > 0.0f)
	{
		BeginFall(Out);
	}
	return Out;
}

FClimbSnapshot FClimbModel::Snapshot() const
{
	FClimbSnapshot S;
	S.State = State;
	S.HeightCm = FMath::RoundToInt(Height * 100.0f);
	return S;
}

EClimbValidation FClimbModel::ValidateClientMove(const FClimbSnapshot& Server, const FClimbSnapshot& Client,
	const FClimbTuning& InTuning)
{
	if (Server.State != Client.State)
	{
		return EClimbValidation::Correct;
	}
	const int32 ToleranceCm = FMath::RoundToInt(InTuning.CorrectionToleranceM * 100.0f);
	return FMath::Abs(Server.HeightCm - Client.HeightCm) > ToleranceCm ? EClimbValidation::Correct : EClimbValidation::Accept;
}
