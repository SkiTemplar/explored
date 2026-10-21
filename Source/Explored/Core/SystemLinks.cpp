#include "Core/SystemLinks.h"

namespace SystemLinksDetail
{
	/** Distancia (m) de un punto a la costa nominal de una tierra (0 dentro). */
	float DistanceToIsland(const FIslandDesc& Island, const FVector2D& Position)
	{
		return Island.DistanceToCoast(Position);
	}

	float Ramp(float Value, float From, float To)
	{
		return FMath::Clamp((Value - From) / (To - From), 0.0f, 1.0f);
	}
}

namespace ExploredLinks
{
	using namespace SystemLinksDetail;

	float CombineFireHeat(const TArray<float>& Heats)
	{
		float Cold = 1.0f;
		for (const float Heat : Heats)
		{
			Cold *= 1.0f - FMath::Clamp(Heat, 0.0f, 1.0f);
		}
		return FMath::Clamp(1.0f - Cold, 0.0f, 1.0f);
	}

	void ApplySurvivalLinks(FSurvivalInputs& InOut, const FSurvivalLinkInputs& Links)
	{
		InOut.FireHeat = FMath::Max(InOut.FireHeat, FMath::Clamp(Links.FireHeat, 0.0f, 1.0f));
		InOut.bSheltered = InOut.bSheltered || Links.bBuildingShelter;
		// Max(0, NaN) devuelve NaN: una carga no finita cuenta como nada.
		InOut.CarriedWeightRatio = FMath::IsFinite(Links.CarriedWeightRatio) ? FMath::Max(0.0f, Links.CarriedWeightRatio) : 0.0f;
		InOut.bPlayingMusic = Links.bPlayingMusic;
		InOut.bHasHat = InOut.bHasHat || Links.bHasHat;
	}

	ERespawnDecision DecideRespawn(const FSurvivalModeSettings& Mode, bool bHasRespawnPoint)
	{
		if (Mode.HasPermadeath())
		{
			return ERespawnDecision::GameOver;
		}
		return bHasRespawnPoint ? ERespawnDecision::AtRespawnPoint : ERespawnDecision::AtStart;
	}

	int32 NearestPoint(const TArray<FVector>& Points, const FVector& From)
	{
		int32 Best = INDEX_NONE;
		double BestDistSq = 0.0;
		for (int32 I = 0; I < Points.Num(); ++I)
		{
			const double DistSq = FVector::DistSquared(Points[I], From);
			if (Best == INDEX_NONE || DistSq < BestDistSq)
			{
				Best = I;
				BestDistSq = DistSq;
			}
		}
		return Best;
	}

	FSurvivalState MakeRespawnState(const FSurvivalState& Dead)
	{
		FSurvivalState Out = Dead;
		Out.Health = 50.0f;
		Out.Hunger = FMath::Clamp(Dead.Hunger, 30.0f, 100.0f);
		Out.Thirst = FMath::Clamp(Dead.Thirst, 30.0f, 100.0f);
		Out.Energy = 60.0f;
		Out.Rest = FMath::Clamp(Dead.Rest, 40.0f, 100.0f);
		Out.Morale = FMath::Clamp(Dead.Morale - 15.0f, 20.0f, 100.0f);
		Out.BodyTemperature = 37.0f;
		Out.Wetness = 0.0f;
		Out.Wounds.Reset();
		for (float& Hours : Out.ConditionTime)
		{
			Hours = 0.0f;
		}
		return Out;
	}

	float BodyDanger01(const FSurvivalState& State)
	{
		const float Cold = Ramp(35.5f - State.BodyTemperature, 0.0f, 2.5f);
		const float Heat = Ramp(State.BodyTemperature - 38.5f, 0.0f, 2.5f);
		const float Hurt = Ramp(35.0f - State.Health, 0.0f, 30.0f);
		float Bleeding = 0.0f;
		for (const FWound& Wound : State.Wounds)
		{
			Bleeding = FMath::Max(Bleeding, Wound.Bleeding);
		}
		return FMath::Clamp(FMath::Max(FMath::Max(Cold, Heat), FMath::Max(Hurt, Bleeding * 0.7f)), 0.0f, 1.0f);
	}

	float StormDanger01(EWeatherState Weather, bool bSheltered)
	{
		float Danger = 0.0f;
		switch (Weather)
		{
		case EWeatherState::Thunderstorm: Danger = 0.25f; break;
		case EWeatherState::Gale: Danger = 0.5f; break;
		case EWeatherState::Cyclone: Danger = 0.9f; break;
		default: break;
		}
		return bSheltered ? Danger * 0.4f : Danger;
	}

	float PredatorThreat01(EFaunaSpecies Species, EMarineState State, float DistanceCm)
	{
		float StateThreat = 0.0f;
		float RangeCm = 3000.0f;
		switch (Species)
		{
		case EFaunaSpecies::ReefShark:
			RangeCm = 2500.0f;
			switch (State)
			{
			case EMarineState::Curious: StateThreat = 0.35f; break;
			case EMarineState::Circle: StateThreat = 0.6f; break;
			case EMarineState::Attack: StateThreat = 1.0f; break;
			default: break;
			}
			break;
		case EFaunaSpecies::TigerShark:
			RangeCm = 4000.0f;
			switch (State)
			{
			case EMarineState::Wander: StateThreat = 0.3f; break;
			case EMarineState::Stalk: StateThreat = 0.8f; break;
			case EMarineState::Attack: StateThreat = 1.0f; break;
			default: break;
			}
			break;
		case EFaunaSpecies::Jellyfish:
			RangeCm = 500.0f;
			StateThreat = 0.25f;
			break;
		default:
			return 0.0f;
		}
		const float Proximity = 1.0f - FMath::Clamp(DistanceCm / RangeCm, 0.0f, 1.0f);
		return FMath::Clamp(StateThreat * Proximity, 0.0f, 1.0f);
	}

	FFaunaHarm HarmFromFauna(EFaunaSpecies Species, float Damage)
	{
		FFaunaHarm Harm;
		switch (Species)
		{
		case EFaunaSpecies::Jellyfish:
			Harm.bSting = true;
			Harm.Sting = EStingKind::Jellyfish;
			break;
		case EFaunaSpecies::Stingray:
			Harm.bSting = true;
			Harm.Sting = EStingKind::Ray;
			break;
		default:
			// Mordisco: un corte más hondo cuanto más daño (40 puntos = el más hondo).
			Harm.CutDepth = Damage > 0.0f ? FMath::Clamp(Damage / 40.0f, 0.1f, 1.0f) : 0.0f;
			break;
		}
		return Harm;
	}

	int32 FindIslandAt(const FIslandLayout& Layout, const FVector2D& PositionMeters)
	{
		int32 Best = INDEX_NONE;
		double BestDist = 0.0;
		for (int32 I = 0; I < Layout.Islands.Num(); ++I)
		{
			const FIslandDesc& Island = Layout.Islands[I];
			if (DistanceToIsland(Island, PositionMeters) > 0.0f)
			{
				continue;
			}
			const double Dist = FVector2D::Distance(PositionMeters, Island.Center);
			if (Best == INDEX_NONE || Dist < BestDist)
			{
				Best = I;
				BestDist = Dist;
			}
		}
		return Best;
	}

	int32 NearestIsland(const FIslandLayout& Layout, const FVector2D& PositionMeters, float& OutDistanceToCoastMeters)
	{
		int32 Best = INDEX_NONE;
		OutDistanceToCoastMeters = TNumericLimits<float>::Max();
		for (int32 I = 0; I < Layout.Islands.Num(); ++I)
		{
			const float Dist = DistanceToIsland(Layout.Islands[I], PositionMeters);
			if (Best == INDEX_NONE || Dist < OutDistanceToCoastMeters)
			{
				Best = I;
				OutDistanceToCoastMeters = Dist;
			}
		}
		return Best;
	}

	bool IsEventWitnessed(const FWorldEvent& Event, const FIslandLayout& Layout, const FVector2D& PlayerMeters)
	{
		if (Event.Island == EIslandArchetype::Count)
		{
			return true;
		}
		const FIslandDesc* Island = Layout.FindIsland(Event.Island);
		return Island && DistanceToIsland(*Island, PlayerMeters) <= EventWitnessRangeMeters;
	}

	int32 DaysSurvived(float TotalDays, float RunStartDays)
	{
		// FloorToInt de un valor no finito o fuera de int32 es UB (llega del reloj guardado).
		const float Days = TotalDays - RunStartDays;
		if (!FMath::IsFinite(Days) || Days <= 0.0f)
		{
			return 0;
		}
		if (Days >= 2147483648.0f)
		{
			return TNumericLimits<int32>::Max();
		}
		return FMath::FloorToInt(Days);
	}

	float BoatCycloneDamagePerHour(int32 CycloneCategory, bool bGrounded)
	{
		if (CycloneCategory <= 0)
		{
			return 0.0f;
		}
		const float PerHour = 0.02f * static_cast<float>(FMath::Clamp(CycloneCategory, 1, 3) * FMath::Clamp(CycloneCategory, 1, 3));
		// Varada en la playa sufre la mitad que a flote.
		return bGrounded ? PerHour * 0.5f : PerHour;
	}
}
