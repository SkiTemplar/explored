#include "Fauna/FaunaGroups.h"

#include "Core/ExploredRandom.h"

namespace FaunaGroupsDetail
{
	/** Distancia por delante a la que un banco migratorio comprueba si hay tierra o bajíos. */
	constexpr float MigrationProbeCm = 4000.0f;
	/** Radio de huida ante un depredador. */
	constexpr float PredatorThreatRadiusCm = 900.0f;
	/** Velocidad del jugador (cm/s) que asusta aunque no haga ruido. */
	constexpr float StartlingPlayerSpeedCmS = 180.0f;

	FVector RandomUnit(FExploredRandom& Random)
	{
		const FVector2D D = Random.InsideUnitDisc();
		const FVector V(D.X, D.Y, Random.RangeFloat(-0.2f, 0.2f));
		return V.GetSafeNormal(UE_SMALL_NUMBER, FVector(1.0, 0.0, 0.0));
	}

	/** Punto medio de una franja (o el propio Z si la franja no es válida). */
	double MidZ(const FBoidBand& Band, double Fallback)
	{
		return Band.MinZ <= Band.MaxZ ? 0.5 * (Band.MinZ + Band.MaxZ) : Fallback;
	}
}

// ---------------------------------------------------------------------------
// Bancos de peces
// ---------------------------------------------------------------------------

FBoidBand FFishSchoolModel::WaterBand(const FVector& P, const FFaunaWorldQuery& World, float SurfaceClearanceCm, float SeabedClearanceCm)
{
	const FVector2D XY(P.X, P.Y);
	FBoidBand Band;
	Band.MinZ = World.SeabedZ(XY) + SeabedClearanceCm;
	Band.MaxZ = World.SurfaceZ(XY) - SurfaceClearanceCm;
	return Band;
}

FBoidsParams FFishSchoolModel::ParamsFor(EFishSchoolKind Kind)
{
	FBoidsParams P;
	if (Kind == EFishSchoolKind::Reef)
	{
		const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(EFaunaSpecies::ReefFish);
		P.NeighborRadiusCm = 150.0f;
		P.SeparationRadiusCm = 35.0f;
		P.CruiseSpeedCmS = Info.CruiseSpeedCmS;
		P.MaxSpeedCmS = Info.MaxSpeedCmS;
		P.MaxAccelCmS2 = 900.0f;
		P.SteeringRate = 3.0f;
		P.BandMarginCm = 40.0f;
		P.MinBandThicknessCm = 30.0f;
		P.FleeWeight = 4.0f;
	}
	else
	{
		const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(EFaunaSpecies::OpenSeaFish);
		P.NeighborRadiusCm = 450.0f;
		P.SeparationRadiusCm = 120.0f;
		P.CruiseSpeedCmS = Info.CruiseSpeedCmS;
		P.MaxSpeedCmS = Info.MaxSpeedCmS;
		P.MaxAccelCmS2 = 700.0f;
		P.SteeringRate = 1.5f;
		P.BandMarginCm = 150.0f;
		P.MinBandThicknessCm = 200.0f;
		P.TargetWeight = 0.8f;
	}
	P.VerticalAgility = 0.4f;
	return P;
}

void FFishSchoolModel::Init(const FFishSchoolConfig& InConfig, const FFaunaWorldQuery& World)
{
	using namespace FaunaGroupsDetail;
	Config = InConfig;
	Boids = FBoidsModel(ParamsFor(Config.Kind));
	State = Config.Kind == EFishSchoolKind::Reef ? EFishSchoolState::Schooling : EFishSchoolState::Migrating;
	MigrationDirection = Config.MigrationDirection.GetSafeNormal();
	if (MigrationDirection.IsNearlyZero())
	{
		MigrationDirection = FVector2D(1.0, 0.0);
	}
	CalmSeconds = 0.0f;
	StateSeconds = 0.0f;
	CalmSpread = 0.0f;
	LocalTime = 0.0;

	FExploredRandom Random(Config.Seed, 0xF15Fu);
	const float Cruise = Boids.GetParams().CruiseSpeedCmS;
	for (int32 I = 0; I < Config.Count; ++I)
	{
		const FVector2D Offset = Random.InsideUnitDisc() * (Config.HomeRadiusCm * 0.4f);
		FVector P(Config.HomeCm.X + Offset.X, Config.HomeCm.Y + Offset.Y, Config.HomeCm.Z);
		const FBoidBand Band = WaterBand(P, World, Config.SurfaceClearanceCm, Config.SeabedClearanceCm);
		if (Band.MinZ <= Band.MaxZ)
		{
			P.Z = FMath::Lerp(Band.MinZ, Band.MaxZ, static_cast<double>(Random.RangeFloat(0.3f, 0.7f)));
		}
		Boids.AddAgent(P, RandomUnit(Random) * Cruise);
	}
}

bool FFishSchoolModel::IsThreatened(const FFaunaStimuli& Stimuli) const
{
	using namespace FaunaGroupsDetail;
	const FVector Center = Boids.Centroid();
	if (Stimuli.bHasPlayer && (Stimuli.bPlayerInWater || Stimuli.bPlayerInBoat))
	{
		// Ruido y movimientos bruscos asustan desde más lejos; quien entra despacio se acerca más (biblia §4.2).
		const bool bFast = Stimuli.PlayerVelocityCmS.Size() > StartlingPlayerSpeedCmS;
		const float Radius = Config.ScatterRadiusCm * (0.35f + 0.65f * FMath::Clamp(FMath::Max(Stimuli.PlayerNoise01, bFast ? 0.8f : 0.0f), 0.0f, 1.0f));
		if (FVector::Distance(Center, Stimuli.PlayerCm) < Radius + Boids.Spread())
		{
			return true;
		}
	}
	for (const FVector& Predator : Stimuli.Predators)
	{
		if (FVector::Distance(Center, Predator) < PredatorThreatRadiusCm + Boids.Spread())
		{
			return true;
		}
	}
	return false;
}

FBoidsEnvironment FFishSchoolModel::BuildEnvironment(const FFaunaStimuli& Stimuli, const FFaunaWorldQuery& World) const
{
	using namespace FaunaGroupsDetail;
	FBoidsEnvironment Env;
	const float SurfaceClear = Config.SurfaceClearanceCm;
	const float SeabedClear = Config.SeabedClearanceCm;
	const FFaunaWorldQuery* WorldPtr = &World;
	Env.Band = [WorldPtr, SurfaceClear, SeabedClear](const FVector& P)
	{
		return WaterBand(P, *WorldPtr, SurfaceClear, SeabedClear);
	};

	if (Stimuli.bHasPlayer && (Stimuli.bPlayerInWater || Stimuli.bPlayerInBoat))
	{
		FBoidThreat Threat;
		Threat.Position = Stimuli.PlayerCm;
		Threat.RadiusCm = Config.ScatterRadiusCm * (0.6f + 0.8f * FMath::Clamp(Stimuli.PlayerNoise01, 0.0f, 1.0f));
		Env.Threats.Add(Threat);
	}
	for (const FVector& Predator : Stimuli.Predators)
	{
		FBoidThreat Threat;
		Threat.Position = Predator;
		Threat.RadiusCm = PredatorThreatRadiusCm;
		Env.Threats.Add(Threat);
	}

	const FVector Center = Boids.Centroid();
	switch (State)
	{
	case EFishSchoolState::Scattered:
		Env.CohesionScale = 0.05f;
		Env.SeparationScale = 2.0f;
		Env.AlignmentScale = 0.3f;
		break;
	case EFishSchoolState::Regrouping:
		Env.CohesionScale = 3.0f;
		Env.AlignmentScale = 1.5f;
		Env.bHasTarget = true;
		// Cada pez vuelve hacia el centro del banco aunque no vea a ningún vecino.
		Env.Target = Center;
		Env.TargetScale = 1.5f;
		break;
	case EFishSchoolState::Migrating:
	{
		Env.bHasTarget = true;
		const FVector2D Ahead = FVector2D(Center.X, Center.Y) + MigrationDirection * MigrationProbeCm;
		const FBoidBand Band = WaterBand(FVector(Ahead.X, Ahead.Y, Center.Z), World, SurfaceClear, SeabedClear);
		Env.Target = FVector(Ahead.X, Ahead.Y, MidZ(Band, Center.Z));
		break;
	}
	case EFishSchoolState::Schooling:
	default:
	{
		// Vueltas lentas alrededor de su casa en el arrecife.
		const double Angle = LocalTime * 0.08 + (Config.Seed % 628) * 0.01;
		const FVector2D Orbit = FVector2D(Config.HomeCm.X, Config.HomeCm.Y)
			+ FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Config.HomeRadiusCm * 0.5f);
		const FBoidBand Band = WaterBand(FVector(Orbit.X, Orbit.Y, Center.Z), World, SurfaceClear, SeabedClear);
		Env.bHasTarget = true;
		Env.Target = FVector(Orbit.X, Orbit.Y, MidZ(Band, Center.Z));
		break;
	}
	}
	return Env;
}

void FFishSchoolModel::Tick(float DeltaSeconds, const FFaunaStimuli& Stimuli, const FFaunaWorldQuery& World)
{
	using namespace FaunaGroupsDetail;
	if (DeltaSeconds <= 0.0f || Boids.Num() == 0)
	{
		return;
	}
	LocalTime += DeltaSeconds;
	StateSeconds += DeltaSeconds;

	const bool bThreat = IsThreatened(Stimuli);
	CalmSeconds = bThreat ? 0.0f : CalmSeconds + DeltaSeconds;

	if (Config.Kind == EFishSchoolKind::Reef)
	{
		const EFishSchoolState Previous = State;
		if (bThreat)
		{
			State = EFishSchoolState::Scattered;
		}
		else if (State == EFishSchoolState::Scattered && CalmSeconds >= Config.RegroupDelaySeconds)
		{
			State = EFishSchoolState::Regrouping;
		}
		else if (State == EFishSchoolState::Regrouping && Boids.Spread() <= GetRegroupedSpread() && StateSeconds > 1.0f)
		{
			State = EFishSchoolState::Schooling;
		}
		if (State != Previous)
		{
			StateSeconds = 0.0f;
		}
		if (State == EFishSchoolState::Schooling)
		{
			// Dispersión habitual del banco en calma (media lenta): la referencia para darlo por rehecho.
			const float Alpha = FMath::Clamp(DeltaSeconds * 0.5f, 0.0f, 1.0f);
			CalmSpread = CalmSpread <= 0.0f ? Boids.Spread() : FMath::Lerp(CalmSpread, Boids.Spread(), Alpha);
		}
	}
	else
	{
		// Si hay tierra o bajío por delante, la ruta gira 90° (siempre hacia el mismo lado para cada banco).
		const FVector Center = Boids.Centroid();
		const FVector2D Probe = FVector2D(Center.X, Center.Y) + MigrationDirection * MigrationProbeCm;
		if (World.WaterDepth(Probe) < FFaunaSpeciesInfo::Get(EFaunaSpecies::OpenSeaFish).MinWaterDepthCm)
		{
			const double Side = (Config.Seed & 1u) != 0 ? 1.0 : -1.0;
			MigrationDirection = FVector2D(-MigrationDirection.Y * Side, MigrationDirection.X * Side);
		}
	}

	// Ciclo diario: de noche los peces de arrecife van más despacio.
	FBoidsParams Params = ParamsFor(Config.Kind);
	const EFaunaSpecies Species = Config.Kind == EFishSchoolKind::Reef ? EFaunaSpecies::ReefFish : EFaunaSpecies::OpenSeaFish;
	Params.CruiseSpeedCmS *= 0.5f + 0.5f * FFaunaActivity::Level(Species, Stimuli.Hours);
	if (State == EFishSchoolState::Scattered && CalmSeconds < 1.0f)
	{
		// Arrancada de huida mientras dura la amenaza.
		Params.CruiseSpeedCmS = Params.MaxSpeedCmS * 0.7f;
	}
	else if (State == EFishSchoolState::Regrouping)
	{
		// Vuelven deprisa a juntarse.
		Params.CruiseSpeedCmS = Params.MaxSpeedCmS * 0.5f;
	}
	Boids.SetParams(Params);
	Boids.Step(DeltaSeconds, BuildEnvironment(Stimuli, World));
}

// ---------------------------------------------------------------------------
// Bandadas de aves
// ---------------------------------------------------------------------------

int32 FGullTheft::FindStealable(const TArray<FStealableItem>& Items, const FVector& CenterCm, float SearchRadiusCm,
	bool bHasPlayer, const FVector& PlayerCm, float PlayerSafeRadiusCm)
{
	int32 Best = INDEX_NONE;
	double BestDist = static_cast<double>(SearchRadiusCm);
	for (int32 I = 0; I < Items.Num(); ++I)
	{
		const FStealableItem& Item = Items[I];
		if (!Item.bIsFish || !Item.bExposed)
		{
			continue;
		}
		if (bHasPlayer && FVector::Dist2D(Item.PositionCm, PlayerCm) < PlayerSafeRadiusCm)
		{
			continue;
		}
		const double Dist = FVector::Dist2D(Item.PositionCm, CenterCm);
		if (Dist <= BestDist)
		{
			BestDist = Dist;
			Best = I;
		}
	}
	return Best;
}

FBoidBand FBirdFlockModel::AirBand(const FVector& P, const FFaunaWorldQuery& World, float MinAltitudeCm, float MaxAltitudeCm)
{
	const FVector2D XY(P.X, P.Y);
	const double Ground = FMath::Max(World.SeabedZ(XY), World.SurfaceZ(XY));
	FBoidBand Band;
	Band.MinZ = Ground + MinAltitudeCm;
	Band.MaxZ = Ground + FMath::Max(MaxAltitudeCm, MinAltitudeCm + 100.0f);
	return Band;
}

FBoidsParams FBirdFlockModel::ParamsFor(EFaunaSpecies Species)
{
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Species);
	FBoidsParams P;
	P.NeighborRadiusCm = 900.0f;
	P.SeparationRadiusCm = 250.0f;
	P.CruiseSpeedCmS = Info.CruiseSpeedCmS;
	// Siempre en vuelo: nunca por debajo de la velocidad de pérdida (GDD §10 y §12).
	P.MinSpeedCmS = Info.CruiseSpeedCmS * 0.55f;
	P.MaxSpeedCmS = Info.MaxSpeedCmS;
	P.MaxAccelCmS2 = 900.0f;
	P.SteeringRate = 1.5f;
	P.VerticalAgility = 0.4f;
	P.BandMarginCm = 250.0f;
	P.MinBandThicknessCm = 50.0f;
	P.LookAheadSeconds = 0.0f;
	P.TargetWeight = 0.7f;
	P.FleeWeight = 2.5f;
	return P;
}

void FBirdFlockModel::Init(const FBirdFlockConfig& InConfig, const FFaunaWorldQuery& World)
{
	using namespace FaunaGroupsDetail;
	Config = InConfig;
	Boids = FBoidsModel(ParamsFor(Config.Species));
	State = EBirdFlockState::Foraging;
	LocalTime = 0.0;
	FExploredRandom Random(Config.Seed, 0xB1Du);
	const float Cruise = Boids.GetParams().CruiseSpeedCmS;
	for (int32 I = 0; I < Config.Count; ++I)
	{
		const FVector2D Offset = Random.InsideUnitDisc() * 600.0f;
		FVector P(Config.HomeCm.X + Offset.X, Config.HomeCm.Y + Offset.Y, 0.0);
		const FBoidBand Band = AirBand(P, World, Config.MinAltitudeCm, Config.MaxAltitudeCm);
		P.Z = FMath::Lerp(Band.MinZ, Band.MaxZ, static_cast<double>(Random.RangeFloat(0.2f, 0.5f)));
		FVector Dir = RandomUnit(Random);
		Dir.Z = 0.0;
		Boids.AddAgent(P, Dir.GetSafeNormal(UE_SMALL_NUMBER, FVector(1.0, 0.0, 0.0)) * Cruise);
	}
}

FBirdFlockEvents FBirdFlockModel::Tick(float DeltaSeconds, const FFaunaStimuli& Stimuli, const FFaunaWorldQuery& World)
{
	using namespace FaunaGroupsDetail;
	FBirdFlockEvents Events;
	if (DeltaSeconds <= 0.0f || Boids.Num() == 0)
	{
		return Events;
	}
	LocalTime += DeltaSeconds;

	const FVector Center = Boids.Centroid();
	const FVector2D Center2D(Center.X, Center.Y);
	const float MinAlt = Config.MinAltitudeCm;
	const float MaxAlt = Config.MaxAltitudeCm;
	const FFaunaWorldQuery* WorldPtr = &World;

	FBoidsEnvironment Env;
	Env.Band = [WorldPtr, MinAlt, MaxAlt](const FVector& P)
	{
		return AirBand(P, *WorldPtr, MinAlt, MaxAlt);
	};

	auto AltitudeAt = [&World, MinAlt, MaxAlt](const FVector2D& XY, float Fraction)
	{
		const FBoidBand Band = AirBand(FVector(XY.X, XY.Y, 0.0), World, MinAlt, MaxAlt);
		return FMath::Lerp(Band.MinZ, Band.MaxZ, static_cast<double>(Fraction));
	};

	const float Daylight = FFaunaActivity::Daylight(Stimuli.Hours);
	const bool bDusk = FFaunaActivity::IsDusk(Stimuli.Hours);
	int32 StealIndex = INDEX_NONE;
	if (!bDusk && Daylight > 0.2f)
	{
		StealIndex = FGullTheft::FindStealable(Stimuli.Stealables, Center, Config.StealSearchRadiusCm,
			Stimuli.bHasPlayer, Stimuli.PlayerCm, Config.PlayerSafeRadiusCm);
	}
	const bool bBoatWithFish = Stimuli.bHasPlayer && Stimuli.bPlayerInBoat && Stimuli.bPlayerCarriesFish
		&& FVector::Dist2D(Stimuli.PlayerCm, Center) < Config.FollowBoatRadiusCm;

	Env.bHasTarget = true;
	if (bDusk || Daylight <= 0.2f)
	{
		// Aves al atardecer (GDD §6.2): rumbo a la isla más cercana; de noche, vueltas altas sobre ella.
		const FVector2D Land = World.NearestLandTo(Center2D);
		const double Distance = FVector2D::Distance(Land, Center2D);
		const bool bOverLand = Distance < Config.HomeRadiusCm * 0.5f;
		State = bOverLand ? EBirdFlockState::CirclingLand : EBirdFlockState::ReturningToLand;
		FVector2D Goal = Land;
		if (bOverLand)
		{
			const double Angle = LocalTime * 0.05;
			Goal = Land + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Config.HomeRadiusCm * 0.3f);
		}
		Env.Target = FVector(Goal.X, Goal.Y, AltitudeAt(Goal, 0.5f));
		Env.TargetScale = 1.5f;
	}
	else if (StealIndex != INDEX_NONE)
	{
		State = EBirdFlockState::Stealing;
		const FVector Item = Stimuli.Stealables[StealIndex].PositionCm;
		Env.Target = FVector(Item.X, Item.Y, AltitudeAt(FVector2D(Item.X, Item.Y), 0.0f));
		Env.TargetScale = 2.0f;
		Env.CohesionScale = 0.3f;
	}
	else if (bBoatWithFish)
	{
		State = EBirdFlockState::FollowingBoat;
		const FVector2D Boat(Stimuli.PlayerCm.X, Stimuli.PlayerCm.Y);
		Env.Target = FVector(Boat.X, Boat.Y, AltitudeAt(Boat, 0.25f));
	}
	else
	{
		State = EBirdFlockState::Foraging;
		const double Angle = LocalTime * 0.03 + (Config.Seed % 628) * 0.01;
		const FVector2D Goal = FVector2D(Config.HomeCm.X, Config.HomeCm.Y)
			+ FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Config.HomeRadiusCm * 0.6f);
		Env.Target = FVector(Goal.X, Goal.Y, AltitudeAt(Goal, 0.35f));
	}

	if (Stimuli.bHasPlayer)
	{
		// A distancia del jugador; si le siguen porque lleva pescado se acercan más.
		FBoidThreat Threat;
		Threat.Position = Stimuli.PlayerCm;
		Threat.RadiusCm = State == EBirdFlockState::FollowingBoat ? Config.PlayerFleeRadiusCm * 0.5f : Config.PlayerFleeRadiusCm;
		Env.Threats.Add(Threat);
		if (State == EBirdFlockState::Foraging && FVector::Distance(Center, Stimuli.PlayerCm) < Config.PlayerFleeRadiusCm)
		{
			State = EBirdFlockState::Fleeing;
		}
	}

	Boids.Step(DeltaSeconds, Env);

	if (State == EBirdFlockState::Stealing)
	{
		const FVector Item = Stimuli.Stealables[StealIndex].PositionCm;
		for (const FBoidAgent& Agent : Boids.GetAgents())
		{
			if (FVector::Dist2D(Agent.Position, Item) <= Config.GrabRadiusCm)
			{
				Events.StolenItemIndex = StealIndex;
				break;
			}
		}
	}
	return Events;
}
