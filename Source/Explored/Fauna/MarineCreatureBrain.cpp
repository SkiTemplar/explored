#include "Fauna/MarineCreatureBrain.h"

#include "Core/ExploredRandom.h"

namespace MarineBrainDetail
{
	constexpr float Gravity = 981.0f;
	/** Holguras bajo la superficie y sobre el fondo. */
	constexpr float SurfaceClearanceCm = 30.0f;
	constexpr float SeabedClearanceCm = 25.0f;
	/** La raya se entierra casi a ras del fondo. */
	constexpr float RayBuriedHeightCm = 5.0f;
	constexpr float RaySwimHeightCm = 30.0f;
	constexpr float RayStingDamage = 25.0f;
	constexpr float JellyStingDamage = 8.0f;
	constexpr float ReefSharkBiteDamage = 20.0f;
	constexpr float TigerSharkBiteDamage = 45.0f;
	constexpr float BiteReachCm = 170.0f;
	/** Olor a partir del cual un tiburón se da por avisado. */
	constexpr float SmellAwareness = 0.03f;

	FVector2D Flat(const FVector& V) { return FVector2D(V.X, V.Y); }

	/** Dirección horizontal de A a B (unitaria o cero). */
	FVector FlatDirection(const FVector& From, const FVector& To)
	{
		const FVector2D D = (Flat(To) - Flat(From)).GetSafeNormal();
		return FVector(D.X, D.Y, 0.0);
	}

	FVector ClampSize(const FVector& V, double Max)
	{
		const double S2 = V.SizeSquared();
		return S2 > Max * Max && S2 > 0.0 ? V * (Max / FMath::Sqrt(S2)) : V;
	}

	/** Z a una fracción de la columna de agua (0 fondo, 1 superficie) con holguras. */
	double WaterZ(const FFaunaWorldQuery& W, const FVector2D& XY, float Fraction)
	{
		const double Lo = W.SeabedZ(XY) + SeabedClearanceCm;
		const double Hi = W.SurfaceZ(XY) - SurfaceClearanceCm;
		if (Lo > Hi)
		{
			return 0.5 * (W.SeabedZ(XY) + W.SurfaceZ(XY));
		}
		return FMath::Lerp(Lo, Hi, static_cast<double>(FMath::Clamp(Fraction, 0.0f, 1.0f)));
	}

	bool PlayerReachable(const FFaunaStimuli& S)
	{
		return S.bHasPlayer && (S.bPlayerInWater || S.bPlayerInBoat);
	}
}

const TCHAR* LexToString(EMarineState State)
{
	switch (State)
	{
	case EMarineState::Wander: return TEXT("Wander");
	case EMarineState::Buried: return TEXT("Buried");
	case EMarineState::Flee: return TEXT("Flee");
	case EMarineState::Settle: return TEXT("Settle");
	case EMarineState::Drift: return TEXT("Drift");
	case EMarineState::Curious: return TEXT("Curious");
	case EMarineState::Circle: return TEXT("Circle");
	case EMarineState::Stalk: return TEXT("Stalk");
	case EMarineState::Attack: return TEXT("Attack");
	case EMarineState::Retreat: return TEXT("Retreat");
	case EMarineState::Accompany: return TEXT("Accompany");
	case EMarineState::Jump: return TEXT("Jump");
	case EMarineState::Breathe: return TEXT("Breathe");
	case EMarineState::Nesting: return TEXT("Nesting");
	case EMarineState::Travel: return TEXT("Travel");
	case EMarineState::Sounding: return TEXT("Sounding");
	default: return TEXT("Unknown");
	}
}

// ---------------------------------------------------------------------------
// Utilidades comunes
// ---------------------------------------------------------------------------

void FMarineCreatureBrain::Init(const FMarineBrainConfig& InConfig, const FFaunaWorldQuery& World)
{
	using namespace MarineBrainDetail;
	Config = InConfig;
	Position = Config.SpawnCm;
	Velocity = FVector::ZeroVector;
	bVisible = true;
	StateSeconds = 0.0f;
	Cooldown = 0.0f;
	Encounters = 0;
	bAttackDecided = false;
	bHasWanderGoal = false;
	bNestingRequested = false;
	bNestingDone = false;
	NotEscortingSeconds = 0.0f;
	StingCooldown = 0.0f;
	LocalTime = 0.0;
	RandomCounter = 0;
	TravelDirection = Config.TravelDirection.GetSafeNormal();
	if (TravelDirection.IsNearlyZero())
	{
		TravelDirection = FVector2D(1.0, 0.0);
	}
	const float Angle = NextRandom() * UE_TWO_PI;
	Heading = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	CircleSign = NextRandom() < 0.5f ? -1.0f : 1.0f;

	switch (Config.Species)
	{
	case EFaunaSpecies::Stingray:
		State = EMarineState::Buried;
		Position.Z = World.SeabedZ(Flat(Position)) + RayBuriedHeightCm;
		break;
	case EFaunaSpecies::Jellyfish:
		State = EMarineState::Drift;
		ClampToWater(World, false);
		break;
	case EFaunaSpecies::HumpbackWhale:
		State = EMarineState::Travel;
		Heading = TravelDirection;
		EventTimer = RandomRange(10.0f, 30.0f);
		ClampToWater(World, false);
		break;
	case EFaunaSpecies::SeaTurtle:
		State = EMarineState::Wander;
		Timer = RandomRange(60.0f, 120.0f);
		ClampToWater(World, false);
		break;
	case EFaunaSpecies::Dolphin:
		State = EMarineState::Wander;
		EventTimer = RandomRange(4.0f, 8.0f);
		ClampToWater(World, false);
		break;
	default:
		State = EMarineState::Wander;
		ClampToWater(World, false);
		break;
	}
}

float FMarineCreatureBrain::NextRandom()
{
	return ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Config.Seed, static_cast<int32>(RandomCounter++), 0x51));
}

void FMarineCreatureBrain::SetState(EMarineState NewState)
{
	if (State != NewState)
	{
		State = NewState;
		StateSeconds = 0.0f;
	}
}

FVector FMarineCreatureBrain::GetForward() const
{
	const FVector2D H = FVector2D(Velocity.X, Velocity.Y).SizeSquared() > 1.0 ? FVector2D(Velocity.X, Velocity.Y).GetSafeNormal() : Heading;
	return FVector(H.X, H.Y, 0.0);
}

bool FMarineCreatureBrain::SensesPlayer(const FFaunaStimuli& S, float SightRangeCm, float HalfAngleDeg, float HearingRangeCm) const
{
	if (!MarineBrainDetail::PlayerReachable(S))
	{
		return false;
	}
	const float Light = FFaunaActivity::Daylight(S.Hours);
	return FFaunaPerception::CanSee(Position, GetForward(), S.PlayerCm, SightRangeCm, HalfAngleDeg, Light)
		|| FFaunaPerception::CanHear(Position, S.PlayerCm, S.PlayerNoise01, HearingRangeCm);
}

FVector FMarineCreatureBrain::WanderVelocity(float Dt, const FFaunaWorldQuery& W, float Speed, float DepthFraction)
{
	using namespace MarineBrainDetail;
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Config.Species);
	WanderGoalSeconds += Dt;
	if (!bHasWanderGoal || FVector::Dist2D(Position, WanderGoal) < 200.0 || WanderGoalSeconds > 25.0f)
	{
		// Nuevo punto dentro de la zona con agua suficiente (unos pocos intentos deterministas).
		for (int32 Attempt = 0; Attempt < 6; ++Attempt)
		{
			const float A = NextRandom() * UE_TWO_PI;
			const float R = FMath::Sqrt(NextRandom()) * Config.HomeRadiusCm;
			const FVector2D XY = Flat(Config.HomeCm) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
			if (W.WaterDepth(XY) >= Info.MinWaterDepthCm || Attempt == 5)
			{
				WanderGoal = FVector(XY.X, XY.Y, 0.0);
				break;
			}
		}
		bHasWanderGoal = true;
		WanderGoalSeconds = 0.0f;
	}
	FVector Goal = WanderGoal;
	Goal.Z = WaterZ(W, Flat(Position), DepthFraction);
	FVector Desired = FlatDirection(Position, Goal) * Speed;
	Desired.Z = FMath::Clamp((Goal.Z - Position.Z) * 0.5, -Speed * 0.3, Speed * 0.3);
	return Desired;
}

void FMarineCreatureBrain::SteerTo(const FVector& DesiredVelocity, float MaxAccelCmS2, float Dt)
{
	using namespace MarineBrainDetail;
	const FVector Delta = ClampSize(DesiredVelocity - Velocity, MaxAccelCmS2 * Dt);
	Velocity = ClampSize(Velocity + Delta, FFaunaSpeciesInfo::Get(Config.Species).MaxSpeedCmS);
}

void FMarineCreatureBrain::ClampToWater(const FFaunaWorldQuery& W, bool bAllowAir)
{
	using namespace MarineBrainDetail;
	const FVector2D XY = Flat(Position);
	const double Seabed = W.SeabedZ(XY);
	const double Surface = W.SurfaceZ(XY);
	const double Lo = Seabed + FMath::Min(SeabedClearanceCm, static_cast<float>(FMath::Max(0.0, Surface - Seabed) * 0.25));
	const double Hi = Surface - FMath::Min(SurfaceClearanceCm, static_cast<float>(FMath::Max(0.0, Surface - Seabed) * 0.25));
	if (Lo > Hi)
	{
		Position.Z = 0.5 * (Seabed + Surface);
		return;
	}
	if (Position.Z < Lo)
	{
		Position.Z = Lo;
		Velocity.Z = FMath::Max(Velocity.Z, 0.0);
	}
	else if (!bAllowAir && Position.Z > Hi)
	{
		Position.Z = Hi;
		Velocity.Z = FMath::Min(Velocity.Z, 0.0);
	}
}

void FMarineCreatureBrain::Move(float Dt, const FFaunaWorldQuery& W, float MinDepthCm, bool bAllowAir)
{
	FVector Candidate = Position + Velocity * Dt;
	const FVector2D From(Position.X, Position.Y);
	const FVector2D To(Candidate.X, Candidate.Y);
	const float DepthTo = W.WaterDepth(To);
	// No se entra en agua más somera que la mínima, salvo para salir hacia agua más honda.
	if (DepthTo < MinDepthCm && DepthTo <= W.WaterDepth(From) && !To.Equals(From, 1.0e-3))
	{
		Candidate.X = Position.X;
		Candidate.Y = Position.Y;
		Velocity.X = -Velocity.X * 0.5;
		Velocity.Y = -Velocity.Y * 0.5;
		bHasWanderGoal = false;
	}
	Position = Candidate;
	ClampToWater(W, bAllowAir);
	const FVector2D H(Velocity.X, Velocity.Y);
	if (H.SizeSquared() > 1.0)
	{
		Heading = H.GetSafeNormal();
	}
}

bool FMarineCreatureBrain::IsStingHazard(const FVector& PointCm) const
{
	switch (Config.Species)
	{
	case EFaunaSpecies::Jellyfish:
		return FVector::Distance(PointCm, Position) <= JellyStingRadiusCm;
	case EFaunaSpecies::Stingray:
		return State == EMarineState::Buried && FVector::Dist2D(PointCm, Position) <= RayStingRadiusCm
			&& FMath::Abs(PointCm.Z - Position.Z) <= 150.0;
	default:
		return false;
	}
}

float FMarineCreatureBrain::GetStingRadiusCm() const
{
	switch (Config.Species)
	{
	case EFaunaSpecies::Jellyfish: return JellyStingRadiusCm;
	case EFaunaSpecies::Stingray: return RayStingRadiusCm;
	default: return 0.0f;
	}
}

void FMarineCreatureBrain::BeginNesting(const FVector& BeachCm)
{
	if (Config.Species != EFaunaSpecies::SeaTurtle)
	{
		return;
	}
	NestTarget = BeachCm;
	bNestingRequested = true;
	bNestingDone = false;
	SetState(EMarineState::Nesting);
}

void FMarineCreatureBrain::CancelNesting()
{
	bNestingRequested = false;
	bNestingDone = false;
	if (State == EMarineState::Nesting)
	{
		SetState(EMarineState::Wander);
	}
}

bool FMarineCreatureBrain::IsTigerSharkWater(const FFaunaWorldQuery& World, const FVector2D& PointCm)
{
	return World.WaterDepth(PointCm) >= FFaunaSpeciesInfo::Get(EFaunaSpecies::TigerShark).MinWaterDepthCm;
}

bool FMarineCreatureBrain::ReefSharkAttackRoll(uint32 Seed, int32 Encounter, float Smell, bool bPeaceful)
{
	if (bPeaceful)
	{
		return false;
	}
	const float Chance = Smell >= MarineBrainDetail::SmellAwareness ? ReefSharkBloodAttackChance : ReefSharkBaseAttackChance;
	return ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Seed, Encounter, 0xA77)) < Chance;
}

FMarineBrainEvents FMarineCreatureBrain::Tick(float DeltaSeconds, const FFaunaStimuli& Stimuli, const FFaunaWorldQuery& World)
{
	FMarineBrainEvents Events;
	if (DeltaSeconds <= 0.0f)
	{
		return Events;
	}
	// Pasos de como mucho 1/20 s para que las embestidas y los saltos no atraviesen nada.
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(DeltaSeconds / 0.05f));
	const float Dt = DeltaSeconds / Steps;
	for (int32 I = 0; I < Steps; ++I)
	{
		LocalTime += Dt;
		StateSeconds += Dt;
		Cooldown = FMath::Max(0.0f, Cooldown - Dt);
		StingCooldown = FMath::Max(0.0f, StingCooldown - Dt);
		switch (Config.Species)
		{
		case EFaunaSpecies::Stingray: TickStingray(Dt, Stimuli, World, Events); break;
		case EFaunaSpecies::Jellyfish: TickJellyfish(Dt, Stimuli, World, Events); break;
		case EFaunaSpecies::ReefShark: TickReefShark(Dt, Stimuli, World, Events); break;
		case EFaunaSpecies::TigerShark: TickTigerShark(Dt, Stimuli, World, Events); break;
		case EFaunaSpecies::Dolphin: TickDolphin(Dt, Stimuli, World, Events); break;
		case EFaunaSpecies::SeaTurtle: TickTurtle(Dt, Stimuli, World, Events); break;
		case EFaunaSpecies::HumpbackWhale: TickWhale(Dt, Stimuli, World, Events); break;
		default:
			// Bancos y bandadas no usan este cerebro: deambulan sin más.
			SteerTo(WanderVelocity(Dt, World, FFaunaSpeciesInfo::Get(Config.Species).CruiseSpeedCmS, 0.5f), 300.0f, Dt);
			Move(Dt, World, FFaunaSpeciesInfo::Get(Config.Species).MinWaterDepthCm);
			break;
		}
	}
	return Events;
}

// ---------------------------------------------------------------------------
// Raya: enterrada; se aleja ondulando si te acercas; pica si se pisa
// ---------------------------------------------------------------------------

void FMarineCreatureBrain::TickStingray(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out)
{
	using namespace MarineBrainDetail;
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Config.Species);
	const bool bPlayer = S.bHasPlayer && S.bPlayerInWater;
	const double Dist = bPlayer ? FVector::Distance(Position, S.PlayerCm) : 1.0e9;

	switch (State)
	{
	case EMarineState::Buried:
	{
		Velocity = FVector::ZeroVector;
		Position.Z = W.SeabedZ(Flat(Position)) + RayBuriedHeightCm;
		if (!bPlayer)
		{
			break;
		}
		// Pisarla pica (salvo arrastrando los pies, que la avisa antes).
		if (IsStingHazard(S.PlayerCm) && !S.bPlayerShuffling && StingCooldown <= 0.0f)
		{
			Out.bSting = true;
			Out.StingDamage = S.bPeaceful ? 0.0f : RayStingDamage;
			StingCooldown = 3.0f;
			Out.bStartled = true;
			SetState(EMarineState::Flee);
			break;
		}
		float Sense = 120.0f + 360.0f * FMath::Clamp(S.PlayerNoise01, 0.0f, 1.0f);
		if (S.bPlayerShuffling)
		{
			Sense = FMath::Max(Sense, RayShuffleSenseCm);
		}
		if (Dist < Sense)
		{
			Out.bStartled = true;
			SetState(EMarineState::Flee);
		}
		break;
	}
	case EMarineState::Flee:
	{
		FVector Away = bPlayer ? FlatDirection(S.PlayerCm, Position) : GetForward();
		if (Away.IsNearlyZero())
		{
			Away = GetForward();
		}
		FVector Desired = Away * Info.MaxSpeedCmS;
		const double Target = W.SeabedZ(Flat(Position)) + RaySwimHeightCm;
		Desired.Z = FMath::Clamp((Target - Position.Z) * 2.0, -100.0, 100.0);
		SteerTo(Desired, 900.0f, Dt);
		Move(Dt, W, Info.MinWaterDepthCm);
		if (StateSeconds > 3.0f && Dist > RayFleeDistanceCm)
		{
			SetState(EMarineState::Settle);
		}
		break;
	}
	case EMarineState::Settle:
	default:
	{
		SteerTo(FVector::ZeroVector, 150.0f, Dt);
		Move(Dt, W, Info.MinWaterDepthCm);
		const double Bottom = W.SeabedZ(Flat(Position)) + RayBuriedHeightCm;
		Position.Z = FMath::Max(Bottom, Position.Z - 20.0 * Dt);
		if (bPlayer && Dist < 300.0)
		{
			SetState(EMarineState::Flee);
		}
		else if (Velocity.SizeSquared() < 25.0 && StateSeconds > 2.0f)
		{
			SetState(EMarineState::Buried);
		}
		break;
	}
	}
}

// ---------------------------------------------------------------------------
// Medusa: deriva con la corriente
// ---------------------------------------------------------------------------

void FMarineCreatureBrain::TickJellyfish(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out)
{
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Config.Species);
	const FVector2D Current = W.CurrentAt(FVector2D(Position.X, Position.Y));
	// La deriva horizontal es exactamente la corriente; la pulsación solo sube y baja un poco.
	const double Bob = Info.CruiseSpeedCmS * FMath::Sin(static_cast<float>(LocalTime) * 1.2f + (Config.Seed % 97) * 0.1f);
	Velocity = FVector(Current.X, Current.Y, Bob);
	Move(Dt, W, Info.MinWaterDepthCm);

	if (S.bHasPlayer && S.bPlayerInWater && IsStingHazard(S.PlayerCm) && StingCooldown <= 0.0f)
	{
		Out.bSting = true;
		Out.StingDamage = S.bPeaceful ? 0.0f : MarineBrainDetail::JellyStingDamage;
		StingCooldown = 1.5f;
	}
}

// ---------------------------------------------------------------------------
// Tiburón de arrecife: curioso, da vueltas, rara vez ataca
// ---------------------------------------------------------------------------

void FMarineCreatureBrain::TickReefShark(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out)
{
	using namespace MarineBrainDetail;
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Config.Species);
	const float Activity = FFaunaActivity::Level(Config.Species, S.Hours);
	const float Cruise = Info.CruiseSpeedCmS * (0.6f + 0.4f * Activity);
	const bool bReachable = PlayerReachable(S);
	const double Dist = bReachable ? FVector::Distance(Position, S.PlayerCm) : 1.0e9;
	const float Smell = FFaunaPerception::SmellAt(Position, S.Blood, W);
	const bool bAware = bReachable && Cooldown <= 0.0f
		&& (SensesPlayer(S, 2500.0f * (0.5f + 0.5f * Activity), 70.0f, 3000.0f) || (Smell >= SmellAwareness && Dist < 6000.0));

	switch (State)
	{
	case EMarineState::Wander:
		SteerTo(WanderVelocity(Dt, W, Cruise, 0.4f), 250.0f, Dt);
		if (bAware)
		{
			SetState(EMarineState::Curious);
		}
		break;
	case EMarineState::Curious:
	{
		if (!bReachable || Dist > 5000.0)
		{
			SetState(EMarineState::Wander);
			break;
		}
		FVector Desired = (S.PlayerCm - Position).GetSafeNormal() * (Cruise * 1.3f);
		SteerTo(Desired, 300.0f, Dt);
		if (Dist < ReefSharkCircleRadiusCm * 1.3f)
		{
			bAttackDecided = ReefSharkAttackRoll(Config.Seed, Encounters++, Smell, S.bPeaceful);
			const FVector2D Rel = Flat(Position) - Flat(S.PlayerCm);
			CircleAngle = FMath::Atan2(static_cast<float>(Rel.Y), static_cast<float>(Rel.X));
			SetState(EMarineState::Circle);
		}
		break;
	}
	case EMarineState::Circle:
	{
		if (!bReachable)
		{
			Cooldown = 20.0f;
			SetState(EMarineState::Retreat);
			break;
		}
		const float Speed = Cruise * 1.4f;
		CircleAngle += CircleSign * Speed / ReefSharkCircleRadiusCm * Dt;
		const FVector2D Goal = Flat(S.PlayerCm) + FVector2D(FMath::Cos(CircleAngle), FMath::Sin(CircleAngle)) * ReefSharkCircleRadiusCm;
		FVector GoalP(Goal.X, Goal.Y, FMath::Min(S.PlayerCm.Z, WaterZ(W, Goal, 0.8f)));
		SteerTo(ClampSize((GoalP - Position) * 1.5, Speed), 500.0f, Dt);
		if (StateSeconds >= ReefSharkCircleSeconds)
		{
			if (bAttackDecided && !S.bPeaceful)
			{
				SetState(EMarineState::Attack);
			}
			else
			{
				Cooldown = 30.0f;
				SetState(EMarineState::Retreat);
			}
		}
		break;
	}
	case EMarineState::Attack:
		if (!bReachable || S.bPeaceful)
		{
			SetState(EMarineState::Retreat);
			break;
		}
		SteerTo((S.PlayerCm - Position).GetSafeNormal() * Info.MaxSpeedCmS, 1200.0f, Dt);
		if (Dist < BiteReachCm)
		{
			Out.bBite = true;
			Out.BiteDamage = ReefSharkBiteDamage;
			Cooldown = 45.0f;
			SetState(EMarineState::Retreat);
		}
		break;
	case EMarineState::Retreat:
	default:
	{
		FVector Away = bReachable ? FlatDirection(S.PlayerCm, Position) : FlatDirection(Position, Config.HomeCm);
		if (Away.IsNearlyZero())
		{
			Away = GetForward();
		}
		SteerTo(Away * (Cruise * 1.5f), 400.0f, Dt);
		if (StateSeconds > 8.0f)
		{
			SetState(EMarineState::Wander);
		}
		break;
	}
	}
	Move(Dt, W, Info.MinWaterDepthCm);
}

// ---------------------------------------------------------------------------
// Tiburón tigre: solo en aguas profundas; acecha y ataca en mar abierto
// ---------------------------------------------------------------------------

void FMarineCreatureBrain::TickTigerShark(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out)
{
	using namespace MarineBrainDetail;
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Config.Species);
	const float Activity = FFaunaActivity::Level(Config.Species, S.Hours);
	const float Cruise = Info.CruiseSpeedCmS * (0.6f + 0.4f * Activity);
	const bool bPlayerInDeep = PlayerReachable(S) && IsTigerSharkWater(W, Flat(S.PlayerCm));
	const double Dist = bPlayerInDeep ? FVector::Distance(Position, S.PlayerCm) : 1.0e9;
	const float Smell = FFaunaPerception::SmellAt(Position, S.Blood, W);
	const bool bAware = bPlayerInDeep
		&& (SensesPlayer(S, 4000.0f, 80.0f, 5000.0f) || (Smell >= SmellAwareness && Dist < 10000.0));

	if (!bPlayerInDeep && State != EMarineState::Wander && State != EMarineState::Retreat)
	{
		// El jugador ha vuelto a aguas someras: el tiburón no puede seguirle.
		SetState(EMarineState::Retreat);
	}

	switch (State)
	{
	case EMarineState::Wander:
		SteerTo(WanderVelocity(Dt, W, Cruise, 0.6f), 250.0f, Dt);
		if (bAware)
		{
			const FVector2D Rel = Flat(Position) - Flat(S.PlayerCm);
			CircleAngle = FMath::Atan2(static_cast<float>(Rel.Y), static_cast<float>(Rel.X));
			SetState(EMarineState::Stalk);
		}
		break;
	case EMarineState::Stalk:
	{
		// Círculos cada vez más cerrados; con sangre en el agua la paciencia se acaba antes.
		const float Duration = Smell >= SmellAwareness ? TigerSharkStalkSeconds * 0.5f : TigerSharkStalkSeconds;
		const float T = FMath::Clamp(StateSeconds / Duration, 0.0f, 1.0f);
		const float Radius = FMath::Lerp(1200.0f, 400.0f, T);
		const float Speed = Cruise * 1.3f;
		CircleAngle += CircleSign * Speed / Radius * Dt;
		const FVector2D Goal = Flat(S.PlayerCm) + FVector2D(FMath::Cos(CircleAngle), FMath::Sin(CircleAngle)) * Radius;
		FVector GoalP(Goal.X, Goal.Y, FMath::Min(S.PlayerCm.Z - 150.0, WaterZ(W, Goal, 0.85f)));
		SteerTo(ClampSize((GoalP - Position) * 1.5, Speed), 500.0f, Dt);
		if (StateSeconds >= Duration && !S.bPeaceful)
		{
			SetState(EMarineState::Attack);
		}
		break;
	}
	case EMarineState::Attack:
		if (S.bPeaceful)
		{
			SetState(EMarineState::Stalk);
			break;
		}
		SteerTo((S.PlayerCm - Position).GetSafeNormal() * Info.MaxSpeedCmS, 1500.0f, Dt);
		if (Dist < BiteReachCm)
		{
			Out.bBite = true;
			Out.BiteDamage = TigerSharkBiteDamage;
			SetState(EMarineState::Retreat);
		}
		break;
	case EMarineState::Retreat:
	default:
	{
		FVector Away = bPlayerInDeep ? FlatDirection(S.PlayerCm, Position) : GetForward();
		if (Away.IsNearlyZero())
		{
			Away = GetForward();
		}
		SteerTo(Away * Cruise, 400.0f, Dt);
		if (StateSeconds > 8.0f)
		{
			SetState(bPlayerInDeep ? EMarineState::Stalk : EMarineState::Wander);
		}
		break;
	}
	}
	// El límite natural del mundo: nunca entra en agua de menos de MinWaterDepthCm.
	Move(Dt, W, Info.MinWaterDepthCm);
}

// ---------------------------------------------------------------------------
// Delfines: acompañan la canoa y saltan
// ---------------------------------------------------------------------------

void FMarineCreatureBrain::TickDolphin(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out)
{
	using namespace MarineBrainDetail;
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Config.Species);
	const FVector BoatVel(S.PlayerVelocityCmS.X, S.PlayerVelocityCmS.Y, 0.0);
	const bool bEscort = S.bHasPlayer && S.bPlayerInBoat && BoatVel.Size() >= DolphinMinBoatSpeedCmS
		&& FVector::Dist2D(Position, S.PlayerCm) < DolphinEscortRangeCm;
	NotEscortingSeconds = bEscort ? 0.0f : NotEscortingSeconds + Dt;

	if (State == EMarineState::Jump)
	{
		Velocity.Z -= Gravity * Dt;
		Position += Velocity * Dt;
		const double Surface = W.SurfaceZ(Flat(Position));
		if (Velocity.Z < 0.0 && Position.Z < Surface - 60.0)
		{
			SetState(bEscort ? EMarineState::Accompany : EMarineState::Wander);
		}
		ClampToWater(W, true);
		return;
	}

	if (bEscort)
	{
		SetState(EMarineState::Accompany);
	}
	else if (State == EMarineState::Accompany && NotEscortingSeconds > 20.0f)
	{
		SetState(EMarineState::Wander);
	}

	if (State == EMarineState::Accompany && S.bHasPlayer)
	{
		// Junto a la amura, al ritmo de la canoa (con el rumbo del último movimiento si se para un momento).
		FVector2D Fwd = FVector2D(BoatVel.X, BoatVel.Y).GetSafeNormal();
		if (Fwd.IsNearlyZero())
		{
			Fwd = Heading;
		}
		const FVector2D Right(-Fwd.Y, Fwd.X);
		const FVector2D Goal = Flat(S.PlayerCm) + Right * (Config.EscortSide * (300.0f + Config.EscortOffsetCm)) + Fwd * 200.0f;
		const FVector GoalP(Goal.X, Goal.Y, W.SurfaceZ(Goal) - 80.0);
		FVector Desired = BoatVel + (GoalP - Position) * 1.2;
		SteerTo(ClampSize(Desired, Info.MaxSpeedCmS), 1500.0f, Dt);

		EventTimer -= Dt;
		if (EventTimer <= 0.0f && Velocity.Size2D() > 300.0 && bEscort)
		{
			Velocity.Z = 550.0;
			Out.bJumped = true;
			EventTimer = RandomRange(5.0f, 11.0f);
			SetState(EMarineState::Jump);
			Position += Velocity * Dt;
			ClampToWater(W, true);
			return;
		}
	}
	else
	{
		SteerTo(WanderVelocity(Dt, W, Info.CruiseSpeedCmS * (0.5f + 0.5f * FFaunaActivity::Level(Config.Species, S.Hours)), 0.85f), 400.0f, Dt);
	}
	Move(Dt, W, Info.MinWaterDepthCm);
}

// ---------------------------------------------------------------------------
// Tortuga marina: nada en la laguna, sube a respirar, es tímida; gancho de desove
// ---------------------------------------------------------------------------

void FMarineCreatureBrain::TickTurtle(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out)
{
	using namespace MarineBrainDetail;
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Config.Species);
	const float Cruise = Info.CruiseSpeedCmS * (0.5f + 0.5f * FFaunaActivity::Level(Config.Species, S.Hours));

	if (State == EMarineState::Nesting)
	{
		if (bNestingDone)
		{
			Velocity = FVector::ZeroVector;
			return;
		}
		FVector Desired = FlatDirection(Position, NestTarget) * Cruise;
		Desired.Z = FMath::Clamp((W.SurfaceZ(Flat(Position)) - 40.0 - Position.Z) * 0.5, -30.0, 30.0);
		SteerTo(Desired, 150.0f, Dt);
		// Al desovar sí llega hasta la orilla (el evento de mundo se encarga de la playa).
		Move(Dt, W, 0.0f);
		if (FVector::Dist2D(Position, NestTarget) < 300.0 || W.WaterDepth(Flat(Position)) < 40.0f)
		{
			bNestingDone = true;
			Velocity = FVector::ZeroVector;
			Out.bNestingArrived = true;
		}
		return;
	}

	const bool bClose = S.bHasPlayer && S.bPlayerInWater && FVector::Distance(Position, S.PlayerCm) < TurtleShyRadiusCm;
	Timer -= Dt;
	switch (State)
	{
	case EMarineState::Flee:
	{
		FVector Away = S.bHasPlayer ? FlatDirection(S.PlayerCm, Position) : GetForward();
		if (Away.IsNearlyZero())
		{
			Away = GetForward();
		}
		SteerTo(Away * Info.MaxSpeedCmS, 200.0f, Dt);
		if (StateSeconds > 4.0f && !bClose)
		{
			SetState(EMarineState::Wander);
		}
		break;
	}
	case EMarineState::Breathe:
	{
		const double Surface = W.SurfaceZ(Flat(Position));
		FVector Desired = GetForward() * (Cruise * 0.3f);
		Desired.Z = FMath::Clamp((Surface - 20.0 - Position.Z) * 0.8, -60.0, 60.0);
		SteerTo(Desired, 150.0f, Dt);
		if (Position.Z > Surface - 60.0)
		{
			EventTimer += Dt;
		}
		if (EventTimer > 4.0f || StateSeconds > 30.0f)
		{
			Timer = RandomRange(60.0f, 120.0f);
			SetState(EMarineState::Wander);
		}
		break;
	}
	case EMarineState::Wander:
	default:
		SteerTo(WanderVelocity(Dt, W, Cruise, 0.5f), 120.0f, Dt);
		if (bClose)
		{
			Out.bStartled = true;
			SetState(EMarineState::Flee);
		}
		else if (Timer <= 0.0f)
		{
			EventTimer = 0.0f;
			SetState(EMarineState::Breathe);
		}
		break;
	}
	Move(Dt, W, Info.MinWaterDepthCm);
}

// ---------------------------------------------------------------------------
// Ballena jorobada: solo de lejos
// ---------------------------------------------------------------------------

void FMarineCreatureBrain::TickWhale(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out)
{
	using namespace MarineBrainDetail;
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Config.Species);
	const double Dist = S.bHasPlayer ? FVector::Dist2D(Position, S.PlayerCm) : 1.0e12;
	const FVector Away = S.bHasPlayer ? FlatDirection(S.PlayerCm, Position) : FVector::ZeroVector;

	if (State == EMarineState::Sounding)
	{
		// Sumergida y fuera de la vista: se aleja y reaparece lejos.
		SteerTo(Away.IsNearlyZero() ? GetForward() * Info.MaxSpeedCmS : Away * Info.MaxSpeedCmS, 200.0f, Dt);
		Move(Dt, W, Info.MinWaterDepthCm);
		Position.Z = FMath::Max(W.SeabedZ(Flat(Position)) + 500.0, W.SurfaceZ(Flat(Position)) - 3000.0);
		if (StateSeconds > 25.0f)
		{
			if (Dist >= WhaleMinDistanceCm * 1.2)
			{
				bVisible = true;
				SetState(EMarineState::Travel);
			}
			else if (S.bHasPlayer)
			{
				const FVector2D Dir = Away.IsNearlyZero() ? Heading : FVector2D(Away.X, Away.Y);
				const FVector2D Far = Flat(S.PlayerCm) + Dir * (WhaleMinDistanceCm * 1.5f);
				if (W.WaterDepth(Far) >= Info.MinWaterDepthCm)
				{
					Position = FVector(Far.X, Far.Y, W.SurfaceZ(Far) - 250.0);
					Velocity = FVector(Dir.X, Dir.Y, 0.0) * Info.CruiseSpeedCmS;
					bVisible = true;
					SetState(EMarineState::Travel);
				}
				else
				{
					StateSeconds = 0.0f;
				}
			}
		}
	}
	else
	{
		FVector Desired;
		if (Dist < WhaleAvoidRadiusCm && !Away.IsNearlyZero())
		{
			// Nunca se acerca: con el jugador a la vista se aparta a toda la velocidad que da.
			Desired = Away * Info.MaxSpeedCmS;
		}
		else
		{
			if (FVector::Dist2D(Position, Config.HomeCm) > Config.HomeRadiusCm)
			{
				TravelDirection = (Flat(Config.HomeCm) - Flat(Position)).GetSafeNormal();
			}
			Desired = FVector(TravelDirection.X, TravelDirection.Y, 0.0) * Info.CruiseSpeedCmS;
		}
		Desired.Z = FMath::Clamp((W.SurfaceZ(Flat(Position)) - 250.0 - Position.Z) * 0.3, -50.0, 50.0);
		SteerTo(Desired, 60.0f, Dt);
		// Si el cambio de rumbo es lento, la parte que acerca al jugador se anula (nunca gana distancia hacia él).
		if (Dist < WhaleAvoidRadiusCm && !Away.IsNearlyZero())
		{
			const double Radial = FVector::DotProduct(Velocity, Away);
			if (Radial < 0.0)
			{
				Velocity -= Away * Radial;
			}
		}
		Move(Dt, W, Info.MinWaterDepthCm);

		EventTimer -= Dt;
		if (EventTimer <= 0.0f)
		{
			Out.bBlow = true;
			EventTimer = RandomRange(25.0f, 45.0f);
		}
	}

	if (bVisible && S.bHasPlayer && FVector::Dist2D(Position, S.PlayerCm) < WhaleMinDistanceCm)
	{
		// Demasiado cerca (el jugador va más rápido que ella): se sumerge y desaparece.
		bVisible = false;
		SetState(EMarineState::Sounding);
		Position.Z = FMath::Max(W.SeabedZ(Flat(Position)) + 500.0, W.SurfaceZ(Flat(Position)) - 3000.0);
	}
}
