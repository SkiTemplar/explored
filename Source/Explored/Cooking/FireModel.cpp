#include "Cooking/FireModel.h"

namespace FireModelDetail
{
	/** Humedad por hora que suma el viento por encima de lo que aguanta el nivel. */
	constexpr float WindQuench = 4.0f;
	/** Secado por hora por cada punto de calor mientras arde. */
	constexpr float DryPerHeat = 1.2f;
	/** Secado por hora sin lluvia (ventilación, sol). */
	constexpr float IdleDry = 0.25f;
	/** Humedad a la que las brasas se ahogan (menos que un fuego vivo). */
	constexpr float EmberDrownDampness = 0.5f;
	/** Humo denso que añade una unidad de combustible verde (horas). */
	constexpr float SignalHoursPerGreen = 0.5f;
	/** Calor de las brasas respecto a la escala del nivel. */
	constexpr float EmberHeat = 0.3f;
	/** Sin yesca cuesta mucho más prender. */
	constexpr float NoTinderFactor = 0.3f;
	/** Radio (m) al que se nota el calor del fuego. */
	constexpr float HeatRadiusMeters = 5.0f;

	bool IsExposed(const FFireLevelDef& Level, const FFireEnvironment& Env)
	{
		return !Env.bSheltered && !Level.bEnclosed;
	}

	/** Humedad por hora que meten la lluvia y el viento (0 bajo techo o en el horno). */
	float QuenchPerHour(const FFireLevelDef& Level, const FFireEnvironment& Env)
	{
		if (!IsExposed(Level, Env))
		{
			return 0.0f;
		}
		const float WindExcess = FMath::Max(0.0f, Env.Wind - Level.WindTolerance);
		return FMath::Max(0.0f, Env.Rain) * Level.RainQuench + WindExcess * WindQuench;
	}

	float BurnRatePerHour(const FFireLevelDef& Level, const FFireEnvironment& Env)
	{
		// El viento aviva el fuego: arde más deprisa a la intemperie.
		const float WindFactor = IsExposed(Level, Env) ? 1.0f + FMath::Clamp(Env.Wind, 0.0f, 1.0f) * 0.5f : 1.0f;
		return Level.BurnRate * WindFactor;
	}

	void RefreshOutputs(FFireState& S, const FFireLevelDef& Level)
	{
		switch (S.Status)
		{
		case EFireStatus::Burning:
			// La leña mojada humea y apenas calienta (biblia §5.3).
			S.Heat = Level.HeatScale * FMath::Max(S.FuelHeat, 0.2f) * (1.0f - 0.6f * S.Dampness);
			S.Smoke = FMath::Clamp(Level.BaseSmoke + 0.5f * S.Dampness + (S.SignalSmokeHours > 0.0f ? 0.5f : 0.0f), 0.0f, 1.0f);
			break;
		case EFireStatus::Embers:
			S.Heat = Level.HeatScale * EmberHeat;
			S.Smoke = 0.15f;
			break;
		default:
			S.Heat = 0.0f;
			S.Smoke = 0.0f;
			break;
		}
	}

	void StepFire(FFireState& S, const FFireLevelDef& Level, const FFireEnvironment& Env, float Dt, TArray<EFireEvent>& OutEvents)
	{
		const float Quench = QuenchPerHour(Level, Env);
		const bool bRaining = IsExposed(Level, Env) && Env.Rain > 0.0f;

		switch (S.Status)
		{
		case EFireStatus::Burning:
		{
			S.FuelHours = FMath::Max(0.0f, S.FuelHours - BurnRatePerHour(Level, Env) * Dt);
			S.SignalSmokeHours = FMath::Max(0.0f, S.SignalSmokeHours - Dt);
			const float Dry = S.Heat * DryPerHeat + (bRaining ? 0.0f : IdleDry);
			S.Dampness = FMath::Clamp(S.Dampness + (Quench - Dry) * Dt, 0.0f, 1.0f);
			if (S.Dampness >= 1.0f)
			{
				// Apagado por la lluvia o el viento: la leña que queda está mojada.
				S.Status = EFireStatus::Unlit;
				S.EmberHours = 0.0f;
				S.SignalSmokeHours = 0.0f;
				OutEvents.Add(EFireEvent::Extinguished);
			}
			else if (S.FuelHours <= 0.0f)
			{
				S.FuelHours = 0.0f;
				S.FuelHeat = 0.0f;
				S.SignalSmokeHours = 0.0f;
				S.Status = EFireStatus::Embers;
				S.EmberHours = Level.EmberHours;
				OutEvents.Add(EFireEvent::BurnedDown);
			}
			break;
		}
		case EFireStatus::Embers:
		{
			S.EmberHours -= Dt * (1.0f + Quench * 2.0f);
			S.Dampness = FMath::Clamp(S.Dampness + (Quench - (bRaining ? 0.0f : IdleDry)) * Dt, 0.0f, 1.0f);
			if (S.EmberHours <= 0.0f || S.Dampness >= EmberDrownDampness)
			{
				S.EmberHours = 0.0f;
				S.Status = EFireStatus::Unlit;
				OutEvents.Add(EFireEvent::WentOut);
			}
			break;
		}
		default:
		{
			// Apagado: la leña se moja con la lluvia y se seca despacio.
			const float Wet = IsExposed(Level, Env) ? FMath::Max(0.0f, Env.Rain) * 0.5f : 0.0f;
			S.Dampness = FMath::Clamp(S.Dampness + (Wet - (bRaining ? 0.0f : IdleDry * 0.4f)) * Dt, 0.0f, 1.0f);
			break;
		}
		}
		RefreshOutputs(S, Level);
	}
}

const TCHAR* LexToString(EFireLevel Level)
{
	switch (Level)
	{
	case EFireLevel::Fogata: return TEXT("fogata");
	case EFireLevel::Hoguera: return TEXT("hoguera");
	case EFireLevel::HornoArcilla: return TEXT("horno_arcilla");
	default: return TEXT("desconocido");
	}
}

const TCHAR* LexToString(EIgnitionMethod Method)
{
	switch (Method)
	{
	case EIgnitionMethod::Matches: return TEXT("cerillas");
	case EIgnitionMethod::Flint: return TEXT("pedernal");
	case EIgnitionMethod::Friction: return TEXT("friccion");
	default: return TEXT("desconocido");
	}
}

bool FFireState::operator==(const FFireState& O) const
{
	return Level == O.Level && Status == O.Status && FuelHours == O.FuelHours && FuelHeat == O.FuelHeat
		&& TinderCharges == O.TinderCharges && Dampness == O.Dampness && EmberHours == O.EmberHours
		&& SignalSmokeHours == O.SignalSmokeHours && Heat == O.Heat && Smoke == O.Smoke;
}

// --- Datos -------------------------------------------------------------------

namespace FireModelDetail
{
	void FillDefaultFireData(FFireData& D)
	{
#include "Cooking/FireData.inl"
	}
}

const FFireData& FFireData::Default()
{
	static const FFireData Data = []()
	{
		FFireData D;
		FireModelDetail::FillDefaultFireData(D);
		return D;
	}();
	return Data;
}

const FFireLevelDef& FFireData::GetLevel(EFireLevel Level) const
{
	check(Levels.Num() > 0);
	for (const FFireLevelDef& Def : Levels)
	{
		if (Def.Level == Level)
		{
			return Def;
		}
	}
	return Levels[0];
}

const FFuelDef* FFireData::FindFuel(FName ItemId) const
{
	return Fuels.FindByPredicate([ItemId](const FFuelDef& F) { return F.ItemId == ItemId; });
}

const FIgnitionDef* FFireData::FindIgnition(EIgnitionMethod Method) const
{
	return Ignitions.FindByPredicate([Method](const FIgnitionDef& I) { return I.Method == Method; });
}

const FIgnitionDef* FFireData::FindIgnitionByTool(FName ItemId) const
{
	return Ignitions.FindByPredicate([ItemId](const FIgnitionDef& I) { return I.ToolItemId == ItemId; });
}

// --- Reglas ------------------------------------------------------------------

bool FFireModel::AddFuel(FFireState& S, const FFireData& Data, FName ItemId, TArray<EFireEvent>& OutEvents)
{
	const FFuelDef* Fuel = Data.FindFuel(ItemId);
	if (!Fuel)
	{
		return false;
	}
	const FFireLevelDef& Level = Data.GetLevel(S.Level);
	// La yesca siempre cabe; la leña no puede pasar de lo que admite el hogar.
	if (!Fuel->bTinder && S.FuelHours + Fuel->BurnHours > Level.MaxFuelHours + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Total = S.FuelHours + Fuel->BurnHours;
	S.FuelHeat = Total > 0.0f ? (S.FuelHeat * S.FuelHours + Fuel->Heat * Fuel->BurnHours) / Total : 0.0f;
	S.FuelHours = Total;
	if (Fuel->bTinder)
	{
		S.TinderCharges = FMath::Min(S.TinderCharges + 1, MaxTinderCharges);
	}
	if (Fuel->bGreen)
	{
		S.SignalSmokeHours += FireModelDetail::SignalHoursPerGreen;
	}
	OutEvents.Add(EFireEvent::FuelAdded);

	if (S.Status == EFireStatus::Embers && S.Dampness < FireModelDetail::EmberDrownDampness)
	{
		S.Status = EFireStatus::Burning;
		S.EmberHours = 0.0f;
		OutEvents.Add(EFireEvent::Revived);
	}
	FireModelDetail::RefreshOutputs(S, Level);
	return true;
}

float FFireModel::IgnitionChance(const FFireState& S, const FFireData& Data, EIgnitionMethod Method, const FFireEnvironment& Env)
{
	const FIgnitionDef* Def = Data.FindIgnition(Method);
	if (!Def || S.Status == EFireStatus::Burning || S.FuelHours <= 0.0f)
	{
		return 0.0f;
	}
	if (S.Status == EFireStatus::Embers)
	{
		return 1.0f;
	}
	const FFireLevelDef& Level = Data.GetLevel(S.Level);
	const bool bExposed = FireModelDetail::IsExposed(Level, Env);
	float Chance = Def->BaseChance;
	Chance *= S.TinderCharges > 0 ? 1.0f : FireModelDetail::NoTinderFactor;
	Chance *= 1.0f - S.Dampness;
	if (bExposed)
	{
		Chance *= 1.0f - FMath::Clamp(Env.Rain, 0.0f, 1.0f) * 0.8f;
		Chance *= 1.0f - FMath::Clamp(Env.Wind - 0.3f, 0.0f, 1.0f) * 0.8f;
	}
	return FMath::Clamp(Chance, 0.0f, 1.0f);
}

FIgnitionResult FFireModel::TryIgnite(FFireState& S, const FFireData& Data, EIgnitionMethod Method, const FFireEnvironment& Env,
	float RandomRoll, TArray<EFireEvent>& OutEvents)
{
	FIgnitionResult Result;
	const FIgnitionDef* Def = Data.FindIgnition(Method);
	if (!Def || S.Status == EFireStatus::Burning || S.FuelHours <= 0.0f)
	{
		return Result;
	}
	Result.Chance = IgnitionChance(S, Data, Method, Env);
	Result.MinutesSpent = Def->Minutes;
	Result.bConsumedTool = Def->bConsumesTool;
	if (S.Status == EFireStatus::Unlit && S.TinderCharges > 0)
	{
		--S.TinderCharges;
	}
	Result.bLit = RandomRoll < Result.Chance;
	if (Result.bLit)
	{
		const bool bWasEmbers = S.Status == EFireStatus::Embers;
		S.Status = EFireStatus::Burning;
		S.EmberHours = 0.0f;
		OutEvents.Add(bWasEmbers ? EFireEvent::Revived : EFireEvent::Ignited);
	}
	else
	{
		OutEvents.Add(EFireEvent::IgnitionFailed);
	}
	FireModelDetail::RefreshOutputs(S, Data.GetLevel(S.Level));
	return Result;
}

void FFireModel::Tick(FFireState& S, const FFireData& Data, const FFireEnvironment& Env, float DeltaHours, TArray<EFireEvent>& OutEvents)
{
	if (DeltaHours <= 0.0f)
	{
		return;
	}
	const FFireLevelDef& Level = Data.GetLevel(S.Level);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(DeltaHours / MaxStepHours));
	const float Dt = DeltaHours / static_cast<float>(Steps);
	for (int32 I = 0; I < Steps; ++I)
	{
		FireModelDetail::StepFire(S, Level, Env, Dt, OutEvents);
	}
}

bool FFireModel::Upgrade(FFireState& S, EFireLevel NewLevel)
{
	if (NewLevel >= EFireLevel::Count || static_cast<uint8>(NewLevel) <= static_cast<uint8>(S.Level))
	{
		return false;
	}
	S.Level = NewLevel;
	return true;
}

float FFireModel::RemainingBurnHours(const FFireState& S, const FFireData& Data, const FFireEnvironment& Env)
{
	const float Rate = FireModelDetail::BurnRatePerHour(Data.GetLevel(S.Level), Env);
	return Rate > 0.0f ? S.FuelHours / Rate : 0.0f;
}

bool FFireModel::IsSignalFire(const FFireState& S)
{
	return S.Status == EFireStatus::Burning && S.Level == EFireLevel::Hoguera && S.SignalSmokeHours > 0.0f;
}

bool FFireModel::IsRespawnPoint(const FFireState& S)
{
	return S.Status != EFireStatus::Unlit;
}

float FFireModel::HeatAtDistance(const FFireState& S, float DistanceMeters)
{
	const float Falloff = 1.0f - FMath::Clamp(DistanceMeters / FireModelDetail::HeatRadiusMeters, 0.0f, 1.0f);
	return FMath::Clamp(S.Heat, 0.0f, 1.0f) * Falloff;
}
