#include "Boats/BoatModel.h"

#include "Ocean/OceanCurrents.h"
#include "Ocean/OceanWaves.h"
#include "Weather/WeatherModel.h"

const TCHAR* LexToString(EBoatType Type)
{
	switch (Type)
	{
	case EBoatType::Raft: return TEXT("Raft");
	case EBoatType::Canoe: return TEXT("Canoe");
	case EBoatType::Outrigger: return TEXT("Outrigger");
	case EBoatType::Limon: return TEXT("Limon");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(EBoatCondition Condition)
{
	switch (Condition)
	{
	case EBoatCondition::Afloat: return TEXT("Afloat");
	case EBoatCondition::Swamped: return TEXT("Swamped");
	case EBoatCondition::Capsized: return TEXT("Capsized");
	case EBoatCondition::Wrecked: return TEXT("Wrecked");
	default: return TEXT("Unknown");
	}
}

namespace BoatModelDetail
{
	/** Tabla de embarcaciones. Medidas de Tools/Blender/props/boats.py; el «Limón» es una canoa doble sin malla aún. */
	FBoatDefinition MakeDefinition(EBoatType Type)
	{
		FBoatDefinition D;
		D.Type = Type;
		switch (Type)
		{
		case EBoatType::Raft:
			// Ocho troncos de 20 cm × 2,2 m atados en 1,6 m de ancho: muy estable, lenta y sin quilla.
			D.MeshName = TEXT("SM_Raft");
			D.LengthCm = 220.0f;
			D.BeamCm = 160.0f;
			D.HullDepthCm = 22.0f;
			D.HullMassKg = 160.0f;
			D.WaterplaneCoefficient = 0.9f;
			D.MaxCargoKg = 120.0f;
			D.MaxPaddleSpeedCmS = 100.0f;
			D.PaddleThrustN = 30.0f;
			D.StrokeDurationS = 0.9f;
			D.StrokeLeverCm = 70.0f;
			D.CoastTimeConstantS = 10.0f;
			D.LateralResistance = 3.0f;
			D.YawTimeConstantS = 1.2f;
			D.RudderTurnRateDegS = 8.0f;
			D.WindageAreaM2 = 0.8f;
			D.MetacentricHeightCm = 200.0f;
			D.RollPeriodS = 1.6f;
			D.RollDamping = 0.18f;
			D.WaveRollResponse = 1.0f;
			D.CapsizeRollDeg = 20.0f;
			D.MaxLeakKgS = 0.0f;
			D.bSelfDraining = true;
			D.bShallowWaterOnly = true;
			D.MaxSafeDepthCm = 1200.0f;
			D.ImpactDamageScale = 0.5f;
			break;
		case EBoatType::Canoe:
			// Tronco excavado de 4 m y 68 cm de manga: rápida remando pero celosa.
			D.MeshName = TEXT("SM_Canoe");
			D.LengthCm = 400.0f;
			D.BeamCm = 68.0f;
			D.HullDepthCm = 50.0f;
			D.HullMassKg = 90.0f;
			D.WaterplaneCoefficient = 0.62f;
			D.MaxCargoKg = 150.0f;
			D.MaxPaddleSpeedCmS = 250.0f;
			D.PaddleThrustN = 55.0f;
			D.StrokeDurationS = 0.7f;
			D.StrokeLeverCm = 55.0f;
			D.CoastTimeConstantS = 12.0f;
			D.LateralResistance = 35.0f;
			D.YawTimeConstantS = 2.0f;
			D.RudderTurnRateDegS = 18.0f;
			D.WindageAreaM2 = 0.5f;
			D.MetacentricHeightCm = 15.0f;
			D.RollPeriodS = 1.8f;
			D.RollDamping = 0.12f;
			D.WaveRollResponse = 1.0f;
			D.CapsizeRollDeg = 28.0f;
			D.MaxLeakKgS = 3.0f;
			D.ImpactDamageScale = 1.0f;
			break;
		case EBoatType::Outrigger:
			// La misma canoa con flotador a 1,2 m, mástil de 2,5 m y vela: el mar abierto.
			D.MeshName = TEXT("SM_Canoe_Outrigger");
			D.LengthCm = 400.0f;
			D.BeamCm = 160.0f;
			D.HullDepthCm = 50.0f;
			D.HullMassKg = 140.0f;
			D.WaterplaneCoefficient = 0.33f;
			D.MaxCargoKg = 250.0f;
			D.MaxPaddleSpeedCmS = 220.0f;
			D.PaddleThrustN = 55.0f;
			D.StrokeDurationS = 0.7f;
			D.StrokeLeverCm = 55.0f;
			D.CoastTimeConstantS = 14.0f;
			D.LateralResistance = 45.0f;
			D.YawTimeConstantS = 2.5f;
			D.RudderTurnRateDegS = 16.0f;
			D.SailAreaM2 = 4.5f;
			D.SailCenterOfEffortCm = 130.0f;
			D.WindageAreaM2 = 0.7f;
			D.MetacentricHeightCm = 90.0f;
			D.RollPeriodS = 2.6f;
			D.RollDamping = 0.3f;
			D.WaveRollResponse = 0.45f;
			D.CapsizeRollDeg = 55.0f;
			D.MaxLeakKgS = 3.0f;
			D.ImpactDamageScale = 1.0f;
			break;
		case EBoatType::Limon:
		default:
			// Canoa doble de 6,5 m con chapa y tubos del Albatros, amarilla: el viaje final.
			D.Type = EBoatType::Limon;
			D.MeshName = TEXT("");
			D.LengthCm = 650.0f;
			D.BeamCm = 280.0f;
			D.HullDepthCm = 70.0f;
			D.HullMassKg = 420.0f;
			D.WaterplaneCoefficient = 0.28f;
			D.MaxCargoKg = 500.0f;
			D.MaxPaddleSpeedCmS = 150.0f;
			D.PaddleThrustN = 80.0f;
			D.StrokeDurationS = 1.0f;
			D.StrokeLeverCm = 100.0f;
			D.CoastTimeConstantS = 18.0f;
			D.LateralResistance = 70.0f;
			D.YawTimeConstantS = 3.0f;
			D.RudderTurnRateDegS = 12.0f;
			D.SailAreaM2 = 12.0f;
			D.SailCenterOfEffortCm = 200.0f;
			D.WindageAreaM2 = 1.5f;
			D.MetacentricHeightCm = 160.0f;
			D.RollPeriodS = 3.4f;
			D.RollDamping = 0.3f;
			D.WaveRollResponse = 0.4f;
			D.CapsizeRollDeg = 65.0f;
			D.MaxLeakKgS = 2.0f;
			D.ImpactDamageScale = 0.6f;
			break;
		}
		return D;
	}

	struct FDefinitionTable
	{
		FBoatDefinition Entries[static_cast<int32>(EBoatType::Count)];

		FDefinitionTable()
		{
			for (int32 I = 0; I < static_cast<int32>(EBoatType::Count); ++I)
			{
				Entries[I] = MakeDefinition(static_cast<EBoatType>(I));
			}
		}
	};

	float Wrap180(float Deg)
	{
		float R = FMath::Fmod(Deg + 180.0f, 360.0f);
		if (R < 0.0f)
		{
			R += 360.0f;
		}
		return R - 180.0f;
	}

	float Wrap360(float Deg)
	{
		float R = FMath::Fmod(Deg, 360.0f);
		if (R < 0.0f)
		{
			R += 360.0f;
		}
		return R;
	}

	/** Dirección de proa (X norte, Y este) para una guiñada. */
	FVector2D ForwardOf(float YawDeg)
	{
		const double R = FMath::DegreesToRadians(static_cast<double>(YawDeg));
		return FVector2D(FMath::Cos(R), FMath::Sin(R));
	}

	/** Dirección de estribor (derecha mirando a proa). */
	FVector2D StarboardOf(float YawDeg)
	{
		const double R = FMath::DegreesToRadians(static_cast<double>(YawDeg));
		return FVector2D(-FMath::Sin(R), FMath::Cos(R));
	}

	/** Ángulo (0–180°) entre la dirección DESDE la que llega un viento y la proa. */
	float AngleFromBowDeg(const FVector2D& Forward, const FVector2D& AirVelocity)
	{
		const FVector2D From = (-AirVelocity).GetSafeNormal();
		if (From.IsNearlyZero())
		{
			return 0.0f;
		}
		const double Cos = FMath::Clamp(FVector2D::DotProduct(From, Forward), -1.0, 1.0);
		return static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(Cos)));
	}

	float WaterHeightAt(const FBoatEnvironment& Env, const FVector2D& P, float Time)
	{
		const float Wave = Env.Waves ? Env.Waves->HeightAt(P, Time) : 0.0f;
		return Wave + Env.TideOffsetCm;
	}

	/** Coeficientes de resistencia longitudinal (cuadrático N/(m/s)² y lineal N/(m/s)) que dan la velocidad de remo pedida. */
	void SurgeDrag(const FBoatDefinition& D, float& OutQuadratic, float& OutLinear)
	{
		const float NominalMass = D.HullMassKg + FBoatModel::CrewMassKg;
		const float V = FMath::Max(D.MaxPaddleSpeedCmS / 100.0f, 0.1f);
		OutLinear = NominalMass / FMath::Max(D.CoastTimeConstantS, 0.5f);
		OutQuadratic = FMath::Max((D.PaddleThrustN - OutLinear * V) / (V * V), 0.1f * D.PaddleThrustN / (V * V));
	}

	/** Oscilador amortiguado semi-implícito hacia un objetivo (estable para Omega·H < 2). */
	void Oscillate(float& Value, float& Rate, float Target, float Omega, float Damping, float H)
	{
		const float Accel = Omega * Omega * (Target - Value) - 2.0f * Damping * Omega * Rate;
		Rate += Accel * H;
		Value += Rate * H;
	}

	/** Relaja una velocidad bajo resistencia cuadrática + lineal de forma implícita (estable con cualquier paso). */
	float DragRelax(float Velocity, float Force, float Mass, float Quadratic, float Linear, float H)
	{
		const float Pushed = Velocity + Force * H / Mass;
		return Pushed / (1.0f + (Quadratic * FMath::Abs(Pushed) + Linear) * H / Mass);
	}
}

// ----------------------------------------------------------------------------- viento y navegación

float FBoatWind::SpeedMS(float Wind01)
{
	return 1.5f + 25.0f * FMath::Clamp(Wind01, 0.0f, 1.0f);
}

float FBoatWind::PrevailingFromDeg(float TotalDays)
{
	// Alisio del ESE en la seca, del este con las primeras lluvias, del ONO en el
	// monzón y del ENE en la temporada de ciclones; el último día de cada estación
	// rola hacia el de la siguiente.
	static constexpr float SeasonFromDeg[static_cast<int32>(ESeason::Count)] = {110.0f, 90.0f, 290.0f, 60.0f};
	const ESeason Season = FWeatherModel::SeasonForDay(TotalDays);
	const int32 Index = static_cast<int32>(Season);
	const int32 Next = (Index + 1) % static_cast<int32>(ESeason::Count);
	const float DayInSeason = FMath::Fmod(FMath::Max(TotalDays, 0.0f), static_cast<float>(FWeatherModel::DaysPerSeason));
	const float Blend = FMath::Clamp(DayInSeason - (FWeatherModel::DaysPerSeason - 1.0f), 0.0f, 1.0f);
	const float Base = SeasonFromDeg[Index] + BoatModelDetail::Wrap180(SeasonFromDeg[Next] - SeasonFromDeg[Index]) * Blend;
	// Giro lento a lo largo del día (brisa de tierra y mar) sin azar: mismo día, mismo viento.
	const float Veer = 15.0f * FMath::Sin(UE_TWO_PI * TotalDays / 1.7f) + 6.0f * FMath::Sin(UE_TWO_PI * TotalDays / 0.37f);
	return BoatModelDetail::Wrap360(Base + Veer);
}

FVector2D FBoatWind::VelocityCmS(float Wind01, float FromDeg)
{
	// Sopla HACIA el rumbo opuesto al de procedencia.
	return -BoatModelDetail::ForwardOf(FromDeg) * static_cast<double>(SpeedMS(Wind01) * 100.0f);
}

float FBoatNavigation::BearingDeg(const FVector2D& From, const FVector2D& To)
{
	const FVector2D D = To - From;
	if (D.IsNearlyZero())
	{
		return 0.0f;
	}
	return BoatModelDetail::Wrap360(static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X))));
}

float FBoatNavigation::HeadingErrorDeg(float HeadingDeg, float TargetBearingDeg)
{
	const float E = BoatModelDetail::Wrap180(TargetBearingDeg - HeadingDeg);
	return E <= -180.0f ? 180.0f : E;
}

FNightNavigationReading FBoatNavigation::Evaluate(const FBoatState& State, const FStarPath& Path, float ToleranceDeg)
{
	FNightNavigationReading R;
	R.HeadingErrorDeg = HeadingErrorDeg(State.YawDeg, Path.StarBearingDeg);

	// Con poca arrancada el rumbo sobre el fondo no significa nada: se usa la proa.
	const bool bMoving = State.VelocityCmS.Size() > 20.0;
	const float CourseDeg = bMoving
		? BoatModelDetail::Wrap360(static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(State.VelocityCmS.Y, State.VelocityCmS.X))))
		: State.YawDeg;
	R.CourseErrorDeg = HeadingErrorDeg(CourseDeg, Path.StarBearingDeg);

	const FVector2D Position(State.LocationCm.X, State.LocationCm.Y);
	const FVector2D Track = Path.DestinationCm - Path.OriginCm;
	const FVector2D TrackDir = Track.GetSafeNormal();
	const FVector2D FromOrigin = Position - Path.OriginCm;
	// Estribor de la derrota = derecha de su dirección (X norte, Y este).
	const FVector2D TrackStarboard(-TrackDir.Y, TrackDir.X);
	R.CrossTrackCm = static_cast<float>(FVector2D::DotProduct(FromOrigin, TrackStarboard));
	R.DistanceToGoCm = static_cast<float>((Path.DestinationCm - Position).Size());
	R.bOnCourse = FMath::Abs(R.HeadingErrorDeg) <= ToleranceDeg && FMath::Abs(R.CourseErrorDeg) <= ToleranceDeg;
	return R;
}

// ----------------------------------------------------------------------------- modelo

const FBoatDefinition& FBoatModel::Definition(EBoatType Type)
{
	static const BoatModelDetail::FDefinitionTable Table;
	const int32 Index = FMath::Clamp(static_cast<int32>(Type), 0, static_cast<int32>(EBoatType::Count) - 1);
	return Table.Entries[Index];
}

FBoatModel::FBoatModel(EBoatType InType, const FVector& InLocationCm, float InYawDeg)
{
	State.Type = InType >= EBoatType::Count ? EBoatType::Raft : InType;
	State.LocationCm = InLocationCm;
	State.YawDeg = BoatModelDetail::Wrap360(InYawDeg);
	Def = Definition(State.Type);
}

FBoatModel::FBoatModel(const FBoatDefinition& InDefinition, const FVector& InLocationCm, float InYawDeg)
	: FBoatModel(InDefinition.Type, InLocationCm, InYawDeg)
{
	SetDefinition(InDefinition);
}

void FBoatModel::SetDefinition(const FBoatDefinition& InDefinition)
{
	Def = InDefinition;
	Def.Type = State.Type;
	// Salvaguardas: una ficha degenerada (sin piezas) no debe dividir por cero en la integración.
	Def.LengthCm = FMath::Max(Def.LengthCm, 10.0f);
	Def.BeamCm = FMath::Max(Def.BeamCm, 10.0f);
	Def.HullDepthCm = FMath::Max(Def.HullDepthCm, 1.0f);
	Def.HullMassKg = FMath::Max(Def.HullMassKg, 1.0f);
	Def.WaterplaneCoefficient = FMath::Clamp(Def.WaterplaneCoefficient, 0.05f, 1.0f);
	Def.MaxCargoKg = FMath::Max(Def.MaxCargoKg, 0.0f);
	// Estabilidad: sin GM positiva el balance no tiene rigidez, y el vuelco tiene que caer entre 0° y 90°.
	Def.MetacentricHeightCm = FMath::Max(Def.MetacentricHeightCm, 1.0f);
	Def.CapsizeRollDeg = FMath::Clamp(Def.CapsizeRollDeg, 5.0f, 89.0f);
	Def.RollPeriodS = FMath::Max(Def.RollPeriodS, 0.2f);
	Def.RollDamping = FMath::Clamp(Def.RollDamping, 0.0f, 2.0f);
	State.CargoKg = FMath::Min(State.CargoKg, Def.MaxCargoKg);
	if (!Def.HasSail())
	{
		State.bSailRaised = false;
	}
}

bool FBoatModel::Moor(const FVector2D& AnchorCm, float LengthCm)
{
	// Comprobación explícita de NaN e infinitos: el editor compila con matemáticas rápidas y ahí
	// el truco de comparar en positivo (`!(x > 0)`) no descarta los NaN.
	if (!FMath::IsFinite(LengthCm) || LengthCm <= 0.0f || !FMath::IsFinite(AnchorCm.X) || !FMath::IsFinite(AnchorCm.Y)
		|| State.Condition == EBoatCondition::Wrecked)
	{
		return false;
	}
	const FVector2D Here(State.LocationCm.X, State.LocationCm.Y);
	if ((Here - AnchorCm).Size() > LengthCm + UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	State.bMoored = true;
	State.bMooringTaut = false;
	State.MooringAnchorCm = AnchorCm;
	State.MooringLengthCm = LengthCm;
	return true;
}

float FBoatModel::TotalMassKg() const
{
	const FBoatDefinition& D = GetDefinition();
	return D.HullMassKg + (State.bCrewAboard ? CrewMassKg : 0.0f) + State.CargoKg + State.WaterInHullKg;
}

float FBoatModel::EquilibriumDraftCm() const
{
	const float Area = GetDefinition().WaterplaneAreaM2();
	return TotalMassKg() / (WaterDensity * FMath::Max(Area, 0.01f)) * 100.0f;
}

float FBoatModel::SwampWaterKg() const
{
	const FBoatDefinition& D = GetDefinition();
	const float Capacity = WaterDensity * D.WaterplaneAreaM2() * D.HullDepthCm / 100.0f;
	const float Dry = D.HullMassKg + (State.bCrewAboard ? CrewMassKg : 0.0f) + State.CargoKg;
	return FMath::Max(Capacity - Dry, 1.0f);
}

bool FBoatModel::TryStroke(EBoatSide Side)
{
	if (!State.bCrewAboard || IsStroking()
		|| (State.Condition != EBoatCondition::Afloat && State.Condition != EBoatCondition::Swamped))
	{
		return false;
	}
	State.StrokeSide = Side;
	State.StrokeTimeLeftS = GetDefinition().StrokeDurationS;
	return true;
}

bool FBoatModel::SetSailRaised(bool bRaised)
{
	if (bRaised && (!GetDefinition().HasSail() || State.Condition != EBoatCondition::Afloat))
	{
		return false;
	}
	State.bSailRaised = bRaised;
	return true;
}

bool FBoatModel::TryAddCargo(float Kg)
{
	// IsFinite explícito: con NaN las dos comparaciones son falsas y se guardaría CargoKg NaN.
	if (!FMath::IsFinite(Kg) || Kg < 0.0f || State.CargoKg + Kg > GetDefinition().MaxCargoKg + UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	State.CargoKg += Kg;
	return true;
}

float FBoatModel::RemoveCargo(float Kg)
{
	const float Removed = FMath::Clamp(Kg, 0.0f, State.CargoKg);
	State.CargoKg -= Removed;
	return Removed;
}

bool FBoatModel::TryRight()
{
	if (State.Condition != EBoatCondition::Capsized)
	{
		return false;
	}
	State.RollDeg = 0.0f;
	State.RollRateDegS = 0.0f;
	const FBoatDefinition& D = GetDefinition();
	if (D.bSelfDraining)
	{
		State.WaterInHullKg = 0.0f;
		State.Condition = EBoatCondition::Afloat;
	}
	else
	{
		State.WaterInHullKg = SwampWaterKg();
		State.Condition = EBoatCondition::Swamped;
	}
	return true;
}

void FBoatModel::Repair(float Amount01)
{
	State.HullDamage01 = FMath::Clamp(State.HullDamage01 - FMath::Max(Amount01, 0.0f), 0.0f, 1.0f);
}

void FBoatModel::ApplyDamage(float Amount01)
{
	State.HullDamage01 = FMath::Clamp(State.HullDamage01 + FMath::Max(Amount01, 0.0f), 0.0f, 1.0f);
	if (State.HullDamage01 >= 1.0f)
	{
		State.Condition = EBoatCondition::Wrecked;
		State.bSailRaised = false;
		State.StrokeTimeLeftS = 0.0f;
	}
}

float FBoatModel::TrueWindAngleDeg(float YawDeg, const FVector2D& WindCmS)
{
	return BoatModelDetail::AngleFromBowDeg(BoatModelDetail::ForwardOf(YawDeg), WindCmS);
}

float FBoatModel::SailDriveCoefficient(float TrueWindAngleDeg)
{
	// Polar sencilla (ángulo del viento verdadero → coeficiente de empuje). Zona muerta
	// por debajo de 40°: la vela flamea. Máximo de través: ciñendo el viento aparente
	// ya es más fuerte, así que el coeficiente sube despacio hasta los 90°; de ahí a
	// popa la vela deja de trabajar como ala y solo frena el aire.
	static constexpr float Angles[] = {0.0f, 40.0f, 50.0f, 60.0f, 75.0f, 90.0f, 120.0f, 150.0f, 180.0f};
	static constexpr float Values[] = {0.0f, 0.0f, 0.25f, 0.5f, 0.8f, 1.15f, 1.05f, 0.9f, 0.8f};
	const float A = FMath::Clamp(FMath::Abs(TrueWindAngleDeg), 0.0f, 180.0f);
	for (int32 I = 1; I < static_cast<int32>(UE_ARRAY_COUNT(Angles)); ++I)
	{
		if (A <= Angles[I])
		{
			const float T = (A - Angles[I - 1]) / (Angles[I] - Angles[I - 1]);
			return FMath::Lerp(Values[I - 1], Values[I], T);
		}
	}
	return Values[UE_ARRAY_COUNT(Values) - 1];
}

float FBoatModel::OptimalSailTrim01(float ApparentWindAngleDeg)
{
	// Ángulo de ataque de ~25° hasta que la vela queda abierta del todo (90°).
	const float Sheet = FMath::Clamp(FMath::Abs(ApparentWindAngleDeg) - 25.0f, 5.0f, 90.0f);
	return (Sheet - 5.0f) / 85.0f;
}

float FBoatModel::SailTrimEfficiency(float SailTrim01, float ApparentWindAngleDeg)
{
	const float Sheet = 5.0f + 85.0f * FMath::Clamp(SailTrim01, 0.0f, 1.0f);
	const float Best = 5.0f + 85.0f * OptimalSailTrim01(ApparentWindAngleDeg);
	const float Error = (Sheet - Best) / 35.0f;
	return FMath::Clamp(1.0f - Error * Error, 0.0f, 1.0f);
}

float FBoatModel::TideOffsetCm(float TotalDays, float MoonPhase01)
{
	return FOceanTide::Level(TotalDays) * FOceanTide::SpringNeapFactor(MoonPhase01) * TideAmplitudeCm;
}

void FBoatModel::Step(float DeltaSeconds, const FBoatControls& Controls, const FBoatEnvironment& Environment)
{
	// IsFinite explícito: con matemáticas rápidas `!(x > 0)` deja pasar un NaN, que envenenaría
	// el acumulador de tiempo y con él todo el estado del barco.
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return;
	}
	State.PendingTimeS += FMath::Min(DeltaSeconds, MaxFrameS);
	const int32 Steps = FMath::FloorToInt((State.PendingTimeS + 1e-6f) / FixedStepS);
	const float Remainder = FMath::Max(State.PendingTimeS - Steps * FixedStepS, 0.0f);
	for (int32 I = 0; I < Steps; ++I)
	{
		// Las olas del último subpaso coinciden con las que dibuja el océano en este fotograma.
		const float WaveTime = Environment.WaveTimeSeconds - Remainder - (Steps - 1 - I) * FixedStepS;
		Substep(FixedStepS, WaveTime, Controls, Environment);
	}
	State.PendingTimeS = Remainder;
}

void FBoatModel::Substep(float H, float WaveTime, const FBoatControls& Controls, const FBoatEnvironment& Env)
{
	using namespace BoatModelDetail;
	const FBoatDefinition& D = GetDefinition();

	const FVector2D Forward = ForwardOf(State.YawDeg);
	const FVector2D Starboard = StarboardOf(State.YawDeg);
	const FVector2D Center(State.LocationCm.X, State.LocationCm.Y);
	const double HalfSpan = 0.4 * D.LengthCm;
	const double HalfBeam = 0.5 * D.BeamCm;

	// --- Flotación: agua en cinco puntos del casco.
	const float HBow = WaterHeightAt(Env, Center + Forward * HalfSpan, WaveTime);
	const float HStern = WaterHeightAt(Env, Center - Forward * HalfSpan, WaveTime);
	const float HPort = WaterHeightAt(Env, Center - Starboard * HalfBeam, WaveTime);
	const float HStarboard = WaterHeightAt(Env, Center + Starboard * HalfBeam, WaveTime);
	const float HCenter = WaterHeightAt(Env, Center, WaveTime);
	const float MeanWater = (HBow + HStern + HPort + HStarboard + 2.0f * HCenter) / 6.0f;

	const bool bHasDepth = static_cast<bool>(Env.DepthBelowSeaLevelCm);
	const float DepthHere = bHasDepth ? Env.DepthBelowSeaLevelCm(Center) : TNumericLimits<float>::Max();
	const float SeabedZ = bHasDepth ? -DepthHere : -TNumericLimits<float>::Max();

	const float Mass = TotalMassKg();
	const float Area = FMath::Max(D.WaterplaneAreaM2(), 0.01f);
	State.bGrounded = false;

	// Altura objetivo de la quilla según el estado.
	float KeelTarget = MeanWater - EquilibriumDraftCm();
	switch (State.Condition)
	{
	case EBoatCondition::Swamped:
		KeelTarget = MeanWater - D.HullDepthCm;
		break;
	case EBoatCondition::Capsized:
		// Quilla arriba: el casco volcado asoma un tercio.
		KeelTarget = MeanWater + D.HullDepthCm * 0.35f;
		break;
	default:
		break;
	}

	if (State.Condition == EBoatCondition::Wrecked)
	{
		// Se hunde a velocidad constante hasta el fondo (o muy hondo si no hay fondo conocido).
		const float Floor = bHasDepth ? SeabedZ : MeanWater - 3000.0f;
		State.LocationCm.Z = FMath::Max(State.LocationCm.Z - 40.0 * H, static_cast<double>(Floor));
		State.HeaveVelocityCmS = 0.0f;
		State.VelocityCmS = State.VelocityCmS * static_cast<double>(FMath::Max(0.0f, 1.0f - 2.0f * H));
		State.StrokeTimeLeftS = 0.0f;
		State.bSailRaised = false;
		const FVector2D Moved = Center + State.VelocityCmS * static_cast<double>(H);
		State.LocationCm.X = Moved.X;
		State.LocationCm.Y = Moved.Y;
		return;
	}

	// --- Arfada: oscilador con la rigidez real de la flotación.
	const float HeaveOmega = FMath::Sqrt(WaterDensity * Gravity * Area / FMath::Max(Mass, 1.0f));
	{
		float Z = static_cast<float>(State.LocationCm.Z);
		float Vz = State.HeaveVelocityCmS;
		const float Target = FMath::Max(KeelTarget, SeabedZ);
		Oscillate(Z, Vz, Target, HeaveOmega, 0.6f, H);
		if (Z < SeabedZ)
		{
			// Varado: descansa sobre el fondo.
			Z = SeabedZ;
			Vz = FMath::Max(Vz, 0.0f);
		}
		if (bHasDepth && Z <= SeabedZ + GroundingToleranceCm && State.Condition != EBoatCondition::Capsized)
		{
			State.bGrounded = true;
		}
		State.LocationCm.Z = Z;
		State.HeaveVelocityCmS = Vz;
	}

	// --- Cabeceo: pendiente del agua entre proa y popa.
	{
		const float Target = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(static_cast<double>(HBow - HStern), 2.0 * HalfSpan)));
		Oscillate(State.PitchDeg, State.PitchRateDegS, Target, HeaveOmega * 0.9f, 0.6f, H);
	}

	// --- Fuerzas horizontales (SI: m/s, N).
	const bool bAfloat = State.Condition == EBoatCondition::Afloat;
	const bool bCanPropel = State.bCrewAboard && (bAfloat || State.Condition == EBoatCondition::Swamped);
	const float PropulsionScale = bAfloat ? 1.0f : 0.3f;

	const FVector2D Velocity = State.VelocityCmS / 100.0;
	const FVector2D Current = Env.CurrentCmS / 100.0;
	const FVector2D Wind = Env.WindCmS / 100.0;
	const FVector2D WaterRelative = Velocity - Current;
	float Surge = static_cast<float>(FVector2D::DotProduct(WaterRelative, Forward));
	float Sway = static_cast<float>(FVector2D::DotProduct(WaterRelative, Starboard));

	const FVector2D Apparent = Wind - Velocity;
	const float ApparentSpeed = static_cast<float>(Apparent.Size());
	const float Awa = AngleFromBowDeg(Forward, Apparent);
	const float Twa = AngleFromBowDeg(Forward, Wind);
	LastApparentWindCmS = Apparent * 100.0;
	LastApparentWindAngleDeg = Awa;
	// El aire empuja hacia sotavento: +1 si sotavento es estribor.
	const float LeewardSign = FVector2D::DotProduct(Apparent, Starboard) >= 0.0 ? 1.0f : -1.0f;

	float ForceSurge = 0.0f;
	float ForceSway = 0.0f;
	float HeelMoment = 0.0f;
	float YawTorque = 0.0f;

	// Vela: polar sobre el viento verdadero, fuerza y trimado con el aparente.
	if (State.bSailRaised && bAfloat && D.HasSail())
	{
		const float Q = 0.5f * AirDensity * ApparentSpeed * ApparentSpeed * D.SailAreaM2;
		const float Trim = Controls.bAutoTrim ? OptimalSailTrim01(Awa) : Controls.SailTrim01;
		const float Efficiency = SailTrimEfficiency(Trim, Awa);
		const float Drive = Q * SailDriveCoefficient(Twa) * Efficiency;
		// Fuerza lateral: máxima ciñendo, nula de popa; con la vela flameando (< 20°) apenas hay.
		const float SideCoefficient = 1.1f * FMath::Max(0.0f, FMath::Cos(FMath::DegreesToRadians(Awa)))
			* FMath::SmoothStep(15.0f, 30.0f, Awa);
		const float Side = Q * SideCoefficient * Efficiency * LeewardSign;
		ForceSurge += Drive;
		ForceSway += Side;
		HeelMoment += Side * D.SailCenterOfEffortCm / 100.0f;
	}

	// Obra muerta y tripulante: deriva con el viento aunque no haya vela.
	{
		const FVector2D Windage = Apparent * static_cast<double>(0.5f * AirDensity * D.WindageAreaM2 * ApparentSpeed);
		const float WindSurge = static_cast<float>(FVector2D::DotProduct(Windage, Forward));
		const float WindSway = static_cast<float>(FVector2D::DotProduct(Windage, Starboard));
		ForceSurge += WindSurge;
		ForceSway += WindSway;
		HeelMoment += WindSway * (D.HullDepthCm / 100.0f + 0.4f);
	}

	// Paladas: empujan y hacen guiñar hacia la banda contraria.
	if (State.StrokeTimeLeftS > 0.0f)
	{
		if (bCanPropel && !Controls.bBailing)
		{
			const float Thrust = D.PaddleThrustN * PropulsionScale;
			ForceSurge += Thrust;
			const float Sign = State.StrokeSide == EBoatSide::Port ? 1.0f : -1.0f;
			YawTorque += Sign * Thrust * D.StrokeLeverCm / 100.0f;
		}
		State.StrokeTimeLeftS = FMath::Max(State.StrokeTimeLeftS - H, 0.0f);
	}

	float SurgeQuadratic = 0.0f;
	float SurgeLinear = 0.0f;
	SurgeDrag(D, SurgeQuadratic, SurgeLinear);
	float SwayQuadratic = SurgeQuadratic * D.LateralResistance;
	float SwayLinear = SurgeLinear * D.LateralResistance;
	const float RudderInput = State.bCrewAboard ? FMath::Clamp(Controls.Rudder, -1.0f, 1.0f) : 0.0f;
	// El timón metido frena un poco.
	SurgeQuadratic *= 1.0f + 0.15f * FMath::Abs(RudderInput);
	if (State.Condition == EBoatCondition::Swamped)
	{
		SurgeQuadratic *= 3.0f;
		SurgeLinear *= 3.0f;
	}
	else if (State.Condition == EBoatCondition::Capsized)
	{
		SurgeQuadratic *= 6.0f;
		SurgeLinear *= 6.0f;
		SwayQuadratic = FMath::Min(SwayQuadratic, SurgeQuadratic * 2.0f);
		SwayLinear = FMath::Min(SwayLinear, SurgeLinear * 2.0f);
	}

	Surge = DragRelax(Surge, ForceSurge, Mass * 1.05f, SurgeQuadratic, SurgeLinear, H);
	Sway = DragRelax(Sway, ForceSway, Mass * 1.5f, SwayQuadratic, SwayLinear, H);
	LastLeewayDeg = FMath::Abs(Surge) > 0.05f
		? static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(static_cast<double>(Sway), static_cast<double>(FMath::Abs(Surge)))))
		: 0.0f;

	FVector2D NewVelocity = Forward * static_cast<double>(Surge) + Starboard * static_cast<double>(Sway) + Current;
	const double MaxSpeed = 20.0; // m/s: salvaguarda numérica, muy por encima de cualquier barco del juego.
	if (NewVelocity.Size() > MaxSpeed)
	{
		NewVelocity = NewVelocity.GetSafeNormal() * MaxSpeed;
	}

	// --- Guiñada: paladas y timón contra la inercia del casco.
	{
		const float LengthM = D.LengthCm / 100.0f;
		const float BeamM = D.BeamCm / 100.0f;
		const float Inertia = Mass * (LengthM * LengthM + BeamM * BeamM) / 12.0f;
		const float YawDamping = Inertia / FMath::Max(D.YawTimeConstantS, 0.1f);
		const float ReferenceSpeed = FMath::Max(D.MaxPaddleSpeedCmS / 100.0f, 0.1f);
		const float RudderGain = YawDamping * FMath::DegreesToRadians(D.RudderTurnRateDegS) / (ReferenceSpeed * ReferenceSpeed);
		if (State.Condition != EBoatCondition::Capsized)
		{
			YawTorque += RudderGain * RudderInput * Surge * FMath::Abs(Surge);
		}
		float YawRate = FMath::DegreesToRadians(State.YawRateDegS);
		YawRate = DragRelax(YawRate, YawTorque, Inertia, 0.0f, YawDamping, H);
		State.YawRateDegS = FMath::RadiansToDegrees(YawRate);
		State.YawDeg = Wrap360(State.YawDeg + State.YawRateDegS * H);
	}

	// --- Balance: pendiente transversal de la ola más la escora del viento.
	{
		const float WaveRoll = -static_cast<float>(FMath::RadiansToDegrees(
			FMath::Atan2(static_cast<double>(HStarboard - HPort), 2.0 * HalfBeam))) * D.WaveRollResponse;
		float Target = WaveRoll;
		if (State.Condition == EBoatCondition::Capsized)
		{
			Target = (State.RollDeg >= 0.0f ? 180.0f : -180.0f) + WaveRoll * 0.3f;
		}
		else
		{
			const float Righting = Mass * Gravity * D.MetacentricHeightCm / 100.0f;
			const float Ratio = HeelMoment / FMath::Max(Righting, 1.0f);
			const float Heel = FMath::Abs(Ratio) < 1.0f
				? static_cast<float>(FMath::RadiansToDegrees(FMath::Asin(static_cast<double>(Ratio))))
				: (Ratio > 0.0f ? 90.0f : -90.0f);
			Target += Heel;
		}
		const float RollOmega = UE_TWO_PI / FMath::Max(D.RollPeriodS, 0.2f);
		Oscillate(State.RollDeg, State.RollRateDegS, Target, RollOmega, D.RollDamping, H);

		if (State.Condition == EBoatCondition::Afloat || State.Condition == EBoatCondition::Swamped)
		{
			MaxAbsRollDeg = FMath::Max(MaxAbsRollDeg, FMath::Abs(State.RollDeg));
			if (FMath::Abs(State.RollDeg) > D.CapsizeRollDeg)
			{
				// Vuelco: quilla arriba, lleno de agua y con la vela en el agua.
				State.Condition = EBoatCondition::Capsized;
				State.RollDeg = State.RollDeg >= 0.0f ? 180.0f : -180.0f;
				State.RollRateDegS = 0.0f;
				State.bSailRaised = false;
				State.StrokeTimeLeftS = 0.0f;
				State.WaterInHullKg = D.bSelfDraining ? 0.0f : SwampWaterKg();
				ApplyDamage(0.05f);
			}
		}
	}

	// --- Agua embarcada: olas por encima de la borda, vías de agua y achique.
	if (State.Condition == EBoatCondition::Afloat || State.Condition == EBoatCondition::Swamped)
	{
		if (D.bSelfDraining)
		{
			State.WaterInHullKg = 0.0f;
		}
		else
		{
			const float Z = static_cast<float>(State.LocationCm.Z);
			const float RollSin = FMath::Sin(FMath::DegreesToRadians(State.RollDeg));
			const float PitchSin = FMath::Sin(FMath::DegreesToRadians(State.PitchDeg));
			const float Gunwale = Z + D.HullDepthCm;
			const float Overtop =
				FMath::Max(0.0f, HPort - (Gunwale + static_cast<float>(HalfBeam) * RollSin))
				+ FMath::Max(0.0f, HStarboard - (Gunwale - static_cast<float>(HalfBeam) * RollSin))
				+ FMath::Max(0.0f, HBow - (Gunwale + static_cast<float>(HalfSpan) * PitchSin))
				+ FMath::Max(0.0f, HStern - (Gunwale - static_cast<float>(HalfSpan) * PitchSin));
			constexpr float OvertopKgPerCmS = 2.0f;
			float Water = State.WaterInHullKg;
			Water += (Overtop * OvertopKgPerCmS + State.HullDamage01 * D.MaxLeakKgS) * H;
			if (Controls.bBailing && State.bCrewAboard)
			{
				Water -= D.BailRateKgS * H;
			}
			const float Swamp = SwampWaterKg();
			State.WaterInHullKg = FMath::Clamp(Water, 0.0f, Swamp);
			if (State.Condition == EBoatCondition::Afloat && State.WaterInHullKg >= Swamp - UE_KINDA_SMALL_NUMBER)
			{
				State.Condition = EBoatCondition::Swamped;
				State.bSailRaised = false;
			}
			else if (State.Condition == EBoatCondition::Swamped && State.WaterInHullKg <= 0.5f * Swamp)
			{
				State.Condition = EBoatCondition::Afloat;
			}
		}
	}

	// --- Desplazamiento con fondo: varadas contra arrecifes y el límite de la balsa.
	FVector2D Candidate = Center + NewVelocity * (100.0 * H);

	// Amarre: el cabo no se estira. Si el paso lo sacaría del círculo, se queda en el borde
	// y pierde la velocidad que lo alejaba; la que va de lado o hacia el poste se conserva.
	// Se aplica antes de mirar el fondo para que la varada juzgue el paso que de verdad da.
	State.bMooringTaut = false;
	auto ApplyMooring = [&]()
	{
		if (!State.bMoored)
		{
			return;
		}
		const FVector2D Offset = Candidate - State.MooringAnchorCm;
		const double Distance = Offset.Size();
		if (Distance > State.MooringLengthCm && Distance > UE_KINDA_SMALL_NUMBER)
		{
			const FVector2D Out = Offset / Distance;
			Candidate = State.MooringAnchorCm + Out * static_cast<double>(State.MooringLengthCm);
			const double Radial = FVector2D::DotProduct(NewVelocity, Out);
			if (Radial > 0.0)
			{
				NewVelocity -= Out * Radial;
			}
			State.bMooringTaut = true;
		}
	};
	ApplyMooring();
	State.bAtOpenOceanLimit = false;
	if (bHasDepth && !NewVelocity.IsNearlyZero())
	{
		const FVector2D NewForward = ForwardOf(State.YawDeg);
		const float PitchSin = FMath::Sin(FMath::DegreesToRadians(State.PitchDeg));
		const float Keel = static_cast<float>(State.LocationCm.Z);
		// Cuánto se mete la quilla en el fondo (proa, centro y popa) más allá de la tolerancia.
		auto Penetration = [&](const FVector2D& At) -> float
		{
			float Sum = 0.0f;
			for (int32 I = -1; I <= 1; ++I)
			{
				const double Offset = 0.45 * D.LengthCm * I;
				const FVector2D P = At + NewForward * Offset;
				const float KeelAt = Keel + static_cast<float>(Offset) * PitchSin;
				Sum += FMath::Max(0.0f, -(KeelAt + Env.DepthBelowSeaLevelCm(P)) - GroundingToleranceCm);
			}
			return Sum;
		};

		bool bBlocked = false;
		// Solo se bloquea lo que mete más la quilla en el fondo: siempre se puede salir hacia aguas hondas.
		if (Penetration(Candidate) > Penetration(Center) + 0.01f)
		{
			// Choca contra un fondo que sube: se detiene y, si iba rápido, el casco sufre.
			bBlocked = true;
			State.bGrounded = true;
			const float ImpactCmS = static_cast<float>(NewVelocity.Size() * 100.0);
			if (ImpactCmS > SafeImpactSpeedCmS)
			{
				ApplyDamage((ImpactCmS - SafeImpactSpeedCmS) / 100.0f * 0.12f * D.ImpactDamageScale);
				// Registro para el astillero: dónde golpea (marco del casco) y a qué velocidad.
				const FVector2D Dir = NewVelocity.GetSafeNormal();
				State.LastImpactDirection = FVector2D(FVector2D::DotProduct(Dir, NewForward), FVector2D::DotProduct(Dir, StarboardOf(State.YawDeg)));
				State.LastImpactSpeedCmS = ImpactCmS;
				++State.ImpactCount;
			}
		}
		if (!bBlocked && D.bShallowWaterOnly)
		{
			const float DepthNext = Env.DepthBelowSeaLevelCm(Candidate);
			if (DepthNext > D.MaxSafeDepthCm && DepthNext > DepthHere)
			{
				bBlocked = true;
				State.bAtOpenOceanLimit = true;
			}
		}
		if (bBlocked)
		{
			NewVelocity = FVector2D::ZeroVector;
			Candidate = Center;
		}
	}

	// Varado sin agua suficiente para flotar: el roce con el fondo lo frena.
	if (bHasDepth && KeelTarget < SeabedZ - GroundingToleranceCm && !NewVelocity.IsNearlyZero())
	{
		const float Support = FMath::Clamp((SeabedZ - KeelTarget) / FMath::Max(EquilibriumDraftCm(), 1.0f), 0.0f, 1.0f);
		const double Speed = NewVelocity.Size();
		const double Slowed = FMath::Max(0.0, Speed - 0.6 * Gravity * Support * H);
		NewVelocity = NewVelocity * (Slowed / Speed);
		Candidate = Center + NewVelocity * (100.0 * H);
		// Roce: carga sobre el fondo × distancia arrastrada (desgaste de Archard en las uniones).
		State.GroundScrapeWorkNm += static_cast<double>(Mass) * Gravity * Support * Slowed * H;
	}

	// El roce solo acorta el paso hacia Center, pero se reaplica por si Center ya estaba en el borde.
	ApplyMooring();

	State.VelocityCmS = NewVelocity * 100.0;
	State.LocationCm.X = Candidate.X;
	State.LocationCm.Y = Candidate.Y;
}

FBoatSaveData FBoatModel::ToSaveData() const
{
	FBoatSaveData Data;
	Data.Type = State.Type;
	Data.Condition = State.Condition;
	Data.LocationCm = State.LocationCm;
	Data.YawDeg = State.YawDeg;
	Data.HullDamage01 = State.HullDamage01;
	Data.CargoKg = State.CargoKg;
	Data.WaterInHullKg = State.WaterInHullKg;
	Data.bSailRaised = State.bSailRaised;
	Data.bMoored = State.bMoored;
	Data.MooringAnchorCm = State.MooringAnchorCm;
	Data.MooringLengthCm = State.MooringLengthCm;
	return Data;
}

FBoatModel FBoatModel::FromSaveData(const FBoatSaveData& Data, const FBoatDefinition* CustomDefinition)
{
	// El guardado admite NaN e infinitos: lo no finito vuelve a su valor por defecto.
	const auto Finite = [](float V, float Default) { return FMath::IsFinite(V) ? V : Default; };
	const bool bLocationValid = FMath::IsFinite(Data.LocationCm.X) && FMath::IsFinite(Data.LocationCm.Y) && FMath::IsFinite(Data.LocationCm.Z);
	FBoatModel Model(Data.Type, bLocationValid ? Data.LocationCm : FVector::ZeroVector, Finite(Data.YawDeg, 0.0f));
	if (CustomDefinition)
	{
		Model.SetDefinition(*CustomDefinition);
	}
	const FBoatDefinition& D = Model.GetDefinition();
	Model.State.Condition = Data.Condition;
	Model.State.HullDamage01 = FMath::Clamp(Finite(Data.HullDamage01, 0.0f), 0.0f, 1.0f);
	Model.State.CargoKg = FMath::Clamp(Finite(Data.CargoKg, 0.0f), 0.0f, D.MaxCargoKg);
	Model.State.WaterInHullKg = FMath::Clamp(Finite(Data.WaterInHullKg, 0.0f), 0.0f, Model.SwampWaterKg());
	Model.State.bSailRaised = Data.bSailRaised && D.HasSail() && Data.Condition == EBoatCondition::Afloat;
	if (Model.State.HullDamage01 >= 1.0f)
	{
		Model.State.Condition = EBoatCondition::Wrecked;
	}
	if (Model.State.Condition == EBoatCondition::Capsized)
	{
		Model.State.RollDeg = 180.0f;
	}
	// Se restaura sin Moor (el barco pudo guardarse con el cabo tenso), pero con sus mismas
	// defensas: un cabo o un poste no finitos, un poste fuera del alcance del cabo (el primer
	// paso llevaría el barco hasta él) o un barco destrozado no quedan amarrados. IsFinite
	// explícito: con matemáticas rápidas la comparación en positivo no descarta los NaN.
	// Con la posición ya saneada: una posición no finita vuelve al origen y el poste queda lejos.
	const FVector2D Here(Model.State.LocationCm.X, Model.State.LocationCm.Y);
	if (Data.bMoored && FMath::IsFinite(Data.MooringLengthCm) && Data.MooringLengthCm > 0.0f
		&& FMath::IsFinite(Data.MooringAnchorCm.X) && FMath::IsFinite(Data.MooringAnchorCm.Y)
		&& (Here - Data.MooringAnchorCm).Size() <= Data.MooringLengthCm + MooringLoadToleranceCm
		&& Model.State.Condition != EBoatCondition::Wrecked)
	{
		Model.State.bMoored = true;
		Model.State.MooringAnchorCm = Data.MooringAnchorCm;
		Model.State.MooringLengthCm = Data.MooringLengthCm;
	}
	return Model;
}
