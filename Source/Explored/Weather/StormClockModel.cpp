#include "Weather/StormClockModel.h"

namespace StormDetail
{
	double At(int32 Day, double HourOfDay)
	{
		return FStormClockModel::ToAbsoluteHours(Day, HourOfDay);
	}

	FStormWindow Window(EStormState State, double Start, double End)
	{
		FStormWindow W;
		W.State = State;
		W.StartHour = Start;
		W.EndHour = End;
		return W;
	}

	FStormCurvePoint Point(double Hour, float Hpa)
	{
		FStormCurvePoint P;
		P.Hour = Hour;
		P.Hpa = Hpa;
		return P;
	}

	bool IsWall(EStormState State)
	{
		return State == EStormState::CycloneWall1 || State == EStormState::CycloneWall2;
	}
}

FStormClockConfig FStormClockConfig::Gdd()
{
	using namespace StormDetail;
	FStormClockConfig C;
	C.Points.Add(Point(At(1, 7.0), 1014.0f));
	C.Points.Add(Point(At(1, 15.0), 1010.0f));
	C.Points.Add(Point(At(2, 6.0), 1008.0f));
	C.Points.Add(Point(At(2, 18.0), 998.0f));
	C.Points.Add(Point(At(2, 19.0), 990.0f));
	C.Points.Add(Point(At(2, 22.0), 965.0f));
	C.Points.Add(Point(At(2, 22.0 + FStormClockConfig::EyeHours), 965.0f));
	C.Points.Add(Point(At(3, 2.0), 995.0f));
	C.Points.Add(Point(At(3, 6.0), 1012.0f));

	// Chubasco de 90 s reales = 2 h de juego (1 h = 45 s). Ciclón: muro 1 (3 h), ojo de 65 s reales
	// (EyeHours ≈ 1,44 h) y muro 2 hasta las 02:00 del día 3.
	C.Windows.Add(Window(EStormState::Squall, At(1, 15.0), At(1, 17.0)));
	C.Windows.Add(Window(EStormState::HighClouds, At(2, 6.0), At(2, 18.0)));
	C.Windows.Add(Window(EStormState::SeaClosed, At(2, 18.0), At(2, 19.0)));
	C.Windows.Add(Window(EStormState::CycloneWall1, At(2, 19.0), At(2, 22.0)));
	C.Windows.Add(Window(EStormState::CycloneEye, At(2, 22.0), At(2, 22.0 + FStormClockConfig::EyeHours)));
	C.Windows.Add(Window(EStormState::CycloneWall2, At(2, 22.0 + FStormClockConfig::EyeHours), At(3, 2.0)));
	return C;
}

bool FStormClockConfig::IsValid() const
{
	if (Points.IsEmpty())
	{
		return false;
	}
	for (int32 I = 0; I < Points.Num(); ++I)
	{
		const FStormCurvePoint& P = Points[I];
		if (!FMath::IsFinite(P.Hour) || !FMath::IsFinite(P.Hpa) || (I > 0 && !(P.Hour > Points[I - 1].Hour)))
		{
			return false;
		}
	}
	for (int32 I = 0; I < Windows.Num(); ++I)
	{
		const FStormWindow& W = Windows[I];
		const bool bKnownState = W.State != EStormState::Clear && static_cast<uint8>(W.State) < static_cast<uint8>(EStormState::Count);
		if (!bKnownState || !FMath::IsFinite(W.StartHour) || !FMath::IsFinite(W.EndHour) || !(W.EndHour > W.StartHour))
		{
			return false;
		}
		for (int32 J = 0; J < I; ++J)
		{
			if (W.StartHour < Windows[J].EndHour && Windows[J].StartHour < W.EndHour)
			{
				return false;
			}
		}
	}
	const float Rates[] = {SquallRain, CycloneRain, SquallChargesPerHour, CycloneChargesPerHour, SteadyHpaPerHour};
	for (const float Rate : Rates)
	{
		if (!FMath::IsFinite(Rate) || Rate < 0.0f)
		{
			return false;
		}
	}
	return FMath::IsFinite(RaftLastHour) && FMath::IsFinite(ForcedReturnHour) && ForcedReturnHour >= RaftLastHour;
}

FStormClockModel::FStormClockModel()
	: Config(FStormClockConfig::Gdd())
{
}

FStormClockModel::FStormClockModel(const FStormClockConfig& InConfig)
	: Config(InConfig.IsValid() ? InConfig : FStormClockConfig::Gdd())
	, bFallback(!InConfig.IsValid())
{
}

double FStormClockModel::ToAbsoluteHours(int32 Day, double HourOfDay)
{
	return static_cast<double>(Day - 1) * HoursPerDay + HourOfDay;
}

float FStormClockModel::PressureAt(double Hour) const
{
	const TArray<FStormCurvePoint>& P = Config.Points;
	if (!FMath::IsFinite(Hour) || Hour <= P[0].Hour)
	{
		return P[0].Hpa;
	}
	for (int32 I = 1; I < P.Num(); ++I)
	{
		if (Hour < P[I].Hour)
		{
			const double Alpha = (Hour - P[I - 1].Hour) / (P[I].Hour - P[I - 1].Hour);
			return static_cast<float>(P[I - 1].Hpa + Alpha * (P[I].Hpa - P[I - 1].Hpa));
		}
	}
	return P.Last().Hpa;
}

EBarometerTrend FStormClockModel::TrendAt(double Hour) const
{
	const TArray<FStormCurvePoint>& P = Config.Points;
	if (!FMath::IsFinite(Hour))
	{
		return EBarometerTrend::Steady;
	}
	for (int32 I = 1; I < P.Num(); ++I)
	{
		if (Hour >= P[I - 1].Hour && Hour < P[I].Hour)
		{
			const double Slope = (P[I].Hpa - P[I - 1].Hpa) / (P[I].Hour - P[I - 1].Hour);
			if (Slope <= -Config.SteadyHpaPerHour)
			{
				return EBarometerTrend::Falling;
			}
			return Slope >= Config.SteadyHpaPerHour ? EBarometerTrend::Rising : EBarometerTrend::Steady;
		}
	}
	return EBarometerTrend::Steady;
}

EStormState FStormClockModel::StateAt(double Hour) const
{
	if (!FMath::IsFinite(Hour))
	{
		return EStormState::Clear;
	}
	for (const FStormWindow& W : Config.Windows)
	{
		if (Hour >= W.StartHour && Hour < W.EndHour)
		{
			return W.State;
		}
	}
	return EStormState::Clear;
}

bool FStormClockModel::IsCyclone(double Hour) const
{
	const EStormState State = StateAt(Hour);
	return StormDetail::IsWall(State) || State == EStormState::CycloneEye;
}

bool FStormClockModel::ExtinguishesOutdoorFires(double Hour) const
{
	return StormDetail::IsWall(StateAt(Hour));
}

bool FStormClockModel::CanCrossByRaft(double Hour) const
{
	return FMath::IsFinite(Hour) && Hour < Config.RaftLastHour;
}

bool FStormClockModel::IsPastForcedReturn(double Hour) const
{
	return FMath::IsFinite(Hour) && Hour >= Config.ForcedReturnHour;
}

float FStormClockModel::ChargesPerHourFor(EStormState State) const
{
	if (State == EStormState::Squall)
	{
		return Config.SquallChargesPerHour;
	}
	return StormDetail::IsWall(State) ? Config.CycloneChargesPerHour : 0.0f;
}

FStormRain FStormClockModel::RainAt(double Hour) const
{
	const EStormState State = StateAt(Hour);
	FStormRain Rain;
	if (State == EStormState::Squall)
	{
		Rain.Intensity = Config.SquallRain;
	}
	else if (StormDetail::IsWall(State))
	{
		Rain.Intensity = Config.CycloneRain;
	}
	Rain.ChargesPerHour = ChargesPerHourFor(State);
	Rain.bRaining = Rain.Intensity > 0.0f;
	return Rain;
}

float FStormClockModel::BarrelChargesBetween(double FromHour, double ToHour) const
{
	if (!FMath::IsFinite(FromHour) || !FMath::IsFinite(ToHour) || !(ToHour > FromHour))
	{
		return 0.0f;
	}
	double Total = 0.0;
	for (const FStormWindow& W : Config.Windows)
	{
		const double Overlap = FMath::Min(ToHour, W.EndHour) - FMath::Max(FromHour, W.StartHour);
		if (Overlap > 0.0)
		{
			Total += Overlap * ChargesPerHourFor(W.State);
		}
	}
	return static_cast<float>(Total);
}

FStormBarrelState FStormClockModel::AddRain(const FStormBarrelState& Barrel, float Charges)
{
	if (!FMath::IsFinite(Charges) || Charges <= 0.0f)
	{
		return Barrel;
	}
	const float Current = static_cast<float>(FMath::Clamp(Barrel.Charges, 0, BarrelCapacity))
		+ (FMath::IsFinite(Barrel.Partial) ? FMath::Clamp(Barrel.Partial, 0.0f, 1.0f) : 0.0f);
	const float Total = Current + Charges;
	FStormBarrelState Next;
	if (Total >= static_cast<float>(BarrelCapacity))
	{
		Next.Charges = BarrelCapacity;
		return Next;
	}
	Next.Charges = FMath::FloorToInt32(Total);
	Next.Partial = Total - static_cast<float>(Next.Charges);
	return Next;
}

FStormBarrelState FStormClockModel::TakeCharge(const FStormBarrelState& Barrel, bool& bOutOk)
{
	bOutOk = Barrel.Charges > 0;
	if (!bOutOk)
	{
		return Barrel;
	}
	FStormBarrelState Next = Barrel;
	Next.Charges = FMath::Min(Barrel.Charges, BarrelCapacity) - 1;
	return Next;
}

bool FStormClockModel::IsWall(EStormState State)
{
	return StormDetail::IsWall(State);
}

FWeatherLook FStormClockModel::LookFor(EStormState State, float RainIntensity)
{
	FWeatherLook Look;
	const float Rain = FMath::IsFinite(RainIntensity) ? FMath::Clamp(RainIntensity, 0.0f, 1.0f) : 0.0f;
	switch (State)
	{
	case EStormState::Squall:
		Look.CloudCover = 0.85f;
		Look.Rain = Rain;
		Look.Wind = 0.45f;
		Look.SeaState = 0.35f;
		Look.Fog = 0.15f;
		Look.Temperature = 25.0f;
		break;
	case EStormState::HighClouds:
		Look.CloudCover = 0.55f;
		Look.Wind = 0.35f;
		Look.SeaState = 0.3f;
		Look.Temperature = 27.0f;
		break;
	case EStormState::SeaClosed:
		Look.CloudCover = 0.9f;
		Look.Wind = 0.7f;
		Look.SeaState = 0.7f;
		Look.Fog = 0.15f;
		Look.Temperature = 25.0f;
		break;
	case EStormState::CycloneWall1:
	case EStormState::CycloneWall2:
		Look.CloudCover = 1.0f;
		Look.Rain = FMath::Max(Rain, 0.0f);
		Look.Wind = 1.0f;
		Look.SeaState = 1.0f;
		Look.Fog = 0.6f;
		Look.Lightning = 0.08f;
		Look.Temperature = 22.0f;
		break;
	case EStormState::CycloneEye:
		// Calma y cielo estrellado; el mar de fondo sigue alto.
		Look.CloudCover = 0.05f;
		Look.Wind = 0.1f;
		Look.SeaState = 0.55f;
		Look.Temperature = 24.0f;
		break;
	default:
		break;
	}
	return Look;
}

FWeatherLook FStormClockModel::LookAt(double Hour) const
{
	FWeatherLook Look = LookFor(StateAt(Hour), RainAt(Hour).Intensity);
	Look.Pressure = PressureAt(Hour);
	return Look;
}
