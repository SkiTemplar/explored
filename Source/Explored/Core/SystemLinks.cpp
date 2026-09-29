#include "Core/SystemLinks.h"

namespace SystemLinksDetail
{
	/** Margen sobre el radio nominal de una isla: los lóbulos llegan a 1,1 radios. */
	constexpr float IslandRadiusMargin = 1.1f;
	/** Radio de un islote de Los Dientes frente al radio de la isla. */
	constexpr float IsletRadiusFraction = 0.35f;

	/** Distancia (m) de un punto al borde nominal de una isla (0 dentro), incluidos sus islotes. */
	float DistanceToIsland(const FIslandDesc& Island, const FVector2D& Position)
	{
		float Best = FMath::Max(0.0f, static_cast<float>(FVector2D::Distance(Position, Island.Center)) - Island.Radius * IslandRadiusMargin);
		for (const FVector2D& Islet : Island.Islets)
		{
			const float ToIslet = static_cast<float>(FVector2D::Distance(Position, Island.Center + Islet)) - Island.Radius * IsletRadiusFraction;
			Best = FMath::Min(Best, FMath::Max(0.0f, ToIslet));
		}
		return Best;
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

	FName BoatStatId(EBoatType Type)
	{
		switch (Type)
		{
		case EBoatType::Raft: return FName(TEXT("balsa"));
		case EBoatType::Canoe: return FName(TEXT("canoa"));
		case EBoatType::Outrigger: return FName(TEXT("canoa_balancin"));
		case EBoatType::Limon: return FName(TEXT("barco_limon"));
		default: return NAME_None;
		}
	}

	FName WorldEventStatId(EWorldEventType Type)
	{
		return Type < EWorldEventType::Count ? FName(LexToString(Type)) : FName(NAME_None);
	}

	FName TechniqueStatId(EWayfindingTechnique Technique)
	{
		return Technique < EWayfindingTechnique::Count ? FName(LexToString(Technique)) : FName(NAME_None);
	}

	FName PlaceStatId(EPoiType Type)
	{
		switch (Type)
		{
		case EPoiType::StarCompass: return FName(TEXT("crater_humo"));
		case EPoiType::Waterfall: return FName(TEXT("cascada_esmeralda"));
		case EPoiType::Lighthouse: return FName(TEXT("faro_dientes"));
		case EPoiType::RadioStation: return FName(TEXT("estacion_halden"));
		case EPoiType::TideObservatory: return FName(TEXT("observatorio_mareas"));
		case EPoiType::Shipwreck: return FName(TEXT("pecio_velero"));
		default: return NAME_None;
		}
	}

	FName PlaceStatIdForRuinSite(FName SiteId)
	{
		return SiteId == FName(TEXT("ruin_smoke")) ? FName(TEXT("tubo_lava")) : FName(NAME_None);
	}

	int32 FindIslandAt(const FArchipelagoLayout& Layout, const FVector2D& PositionMeters)
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

	int32 NearestIsland(const FArchipelagoLayout& Layout, const FVector2D& PositionMeters, float& OutDistanceToCoastMeters)
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

	bool IsEventWitnessed(const FWorldEvent& Event, const FArchipelagoLayout& Layout, const FVector2D& PlayerMeters)
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

	int32 FSailingOdometer::Step(const FVector2D& PositionCm, bool bUnderSail)
	{
		if (!bUnderSail)
		{
			bHasLast = false;
			return 0;
		}
		if (bHasLast)
		{
			const double StepCm = FVector2D::Distance(PositionCm, Last);
			// Con matemáticas rápidas «NaN <= MaxStepCm» puede darse por cierto y luego FloorToInt(NaN).
			if (FMath::IsFinite(StepCm) && StepCm <= MaxStepCm)
			{
				PendingCm += StepCm;
			}
		}
		Last = PositionCm;
		bHasLast = true;
		const int32 Meters = FMath::FloorToInt(static_cast<float>(PendingCm / 100.0));
		PendingCm -= Meters * 100.0;
		return Meters;
	}

	bool FCycloneWatch::Update(bool bCycloneActive, const TArray<FPieceIntegrity>& Pieces)
	{
		if (bCycloneActive && !bActive)
		{
			bActive = true;
			StartIntegrity.Reset();
			for (const FPieceIntegrity& Piece : Pieces)
			{
				StartIntegrity.Add(Piece.Id, Piece.Integrity);
			}
			return false;
		}
		if (bCycloneActive || !bActive)
		{
			return false;
		}
		// Terminó: intacto si todas las piezas del principio siguen en pie y sin daño de temporal.
		bActive = false;
		if (StartIntegrity.Num() == 0)
		{
			return false;
		}
		for (const TPair<int32, float>& Start : StartIntegrity)
		{
			const FPieceIntegrity* Now = Pieces.FindByPredicate([&Start](const FPieceIntegrity& P) { return P.Id == Start.Key; });
			if (!Now || Start.Value - Now->Integrity > ToleranceFraction * FMath::Max(Now->MaxIntegrity, 1.0f))
			{
				StartIntegrity.Reset();
				return false;
			}
		}
		StartIntegrity.Reset();
		return true;
	}

	bool FFluteMelodyWatch::Update(bool bPerforming, float FireHeat)
	{
		const bool bNow = bPerforming && FireHeat >= MinFireHeat;
		const bool bStarted = bNow && !bWasPerformingByFire;
		bWasPerformingByFire = bNow;
		return bStarted;
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

	bool IsFacingAlong(float ViewerYawDeg, float TargetYawDeg, float ToleranceDeg)
	{
		const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(ViewerYawDeg, TargetYawDeg));
		return Delta <= ToleranceDeg;
	}

	TArray<FBuildingCost> SpentMaterials(const TMap<FName, int32>& Before, const TMap<FName, int32>& After)
	{
		TArray<FBuildingCost> Spent;
		for (const TPair<FName, int32>& Entry : Before)
		{
			const int32 Left = After.FindRef(Entry.Key);
			if (Entry.Value > Left)
			{
				Spent.Add({Entry.Key, Entry.Value - Left});
			}
		}
		return Spent;
	}

	bool PlanMaterialTakes(const TArray<FMaterialStack>& Stacks, const TArray<FBuildingCost>& Costs, TArray<FMaterialTake>& OutTakes)
	{
		OutTakes.Reset();
		TArray<int32> Order;
		for (int32 I = 0; I < Stacks.Num(); ++I)
		{
			Order.Add(I);
		}
		Order.StableSort([&Stacks](int32 A, int32 B) { return Stacks[A].Priority < Stacks[B].Priority; });

		TArray<int32> Left;
		for (const FMaterialStack& Stack : Stacks)
		{
			Left.Add(FMath::Max(0, Stack.Count));
		}
		for (const FBuildingCost& Cost : Costs)
		{
			int32 Needed = Cost.Count;
			for (const int32 Index : Order)
			{
				if (Needed <= 0)
				{
					break;
				}
				if (Stacks[Index].Item != Cost.Item || Left[Index] <= 0)
				{
					continue;
				}
				const int32 Take = FMath::Min(Needed, Left[Index]);
				Left[Index] -= Take;
				Needed -= Take;
				FMaterialTake* Existing = OutTakes.FindByPredicate([&Stacks, Index](const FMaterialTake& T) { return T.InstanceId == Stacks[Index].InstanceId; });
				if (Existing)
				{
					Existing->Count += Take;
				}
				else
				{
					OutTakes.Add({Stacks[Index].InstanceId, Take});
				}
			}
			if (Needed > 0)
			{
				OutTakes.Reset();
				return false;
			}
		}
		return true;
	}

	// --- Cooperativo: dormir en grupo (biblia 08 §5.1) ----------------------------------

	FGroupSleepDecision DecideGroupSleep(const TArray<FCoopSleeper>& Players)
	{
		FGroupSleepDecision Out;
		int32 InBed = 0;
		bool bAnyDowned = false;
		for (const FCoopSleeper& Player : Players)
		{
			// Un derribado no puede estar acostado aunque la bandera diga lo contrario.
			if (Player.bDowned)
			{
				bAnyDowned = true;
			}
			else if (Player.bInBed)
			{
				++InBed;
			}
		}
		Out.StillUp = Players.Num() - InBed;
		if (InBed == 0 && !bAnyDowned)
		{
			Out.Status = EGroupSleepStatus::NobodyInBed;
		}
		else if (bAnyDowned)
		{
			// Solo hace falta el aviso si alguien intenta dormir; sin nadie en la cama, silencio.
			Out.Status = InBed > 0 ? EGroupSleepStatus::BlockedByDowned : EGroupSleepStatus::NobodyInBed;
		}
		else
		{
			Out.Status = Out.StillUp == 0 ? EGroupSleepStatus::AllInBed : EGroupSleepStatus::WaitingForOthers;
		}
		return Out;
	}

	float HoursUntilDawn(float HoursOfDay)
	{
		const float Safe = FMath::IsFinite(HoursOfDay) ? HoursOfDay : 0.0f;
		float Hours = FMath::Fmod(GroupSleepDawnHour - Safe, 24.0f);
		if (Hours <= 0.0f)
		{
			Hours += 24.0f;
		}
		return Hours;
	}

	FGroupSleepSession::FResult FGroupSleepSession::Update(const TArray<FCoopSleeper>& Players, float HoursOfDay, float DeltaGameHours)
	{
		FResult Out;
		Out.Decision = DecideGroupSleep(Players);
		const float Delta = (FMath::IsFinite(DeltaGameHours) && DeltaGameHours > 0.0f) ? DeltaGameHours : 0.0f;

		auto Finish = [this, &Out](EGroupSleepEvent Event)
		{
			Out.Event = Event;
			Out.HoursSlept = FMath::Min(HoursSlept, TargetHours);
			Out.Recovery01 = FMath::Clamp(Out.HoursSlept / GroupSleepMaxHours, 0.0f, 1.0f);
			Out.TimeScale = 1.0f;
			bActive = false;
			HoursSlept = 0.0f;
			TargetHours = 0.0f;
		};

		if (Out.Decision.Status != EGroupSleepStatus::AllInBed)
		{
			bCompletedLatch = false;
			if (bActive)
			{
				Finish(EGroupSleepEvent::Interrupted);
			}
			return Out;
		}

		if (bCompletedLatch)
		{
			return Out;
		}

		if (!bActive)
		{
			bActive = true;
			HoursSlept = 0.0f;
			TargetHours = FMath::Min(GroupSleepMaxHours, HoursUntilDawn(HoursOfDay));
			Out.Event = EGroupSleepEvent::Started;
			Out.TimeScale = GroupSleepTimeScale;
			return Out;
		}

		HoursSlept += Delta;
		if (HoursSlept >= TargetHours)
		{
			Finish(EGroupSleepEvent::Completed);
			bCompletedLatch = true;
			return Out;
		}
		Out.TimeScale = GroupSleepTimeScale;
		return Out;
	}

	// --- Cooperativo: derribado y reanimación (biblia 08 §5.2) --------------------------

	int32 RevivesUsedOn(const FCoopDownState& State, int32 Day)
	{
		return State.RevivesDay == Day ? State.RevivesToday : 0;
	}

	ECoopHealthZero OnHealthZero(FCoopDownState& State, const FSurvivalModeSettings& Mode, int32 PlayersConnected, int32 Day)
	{
		// Si estaba derribado y ya no toca (el compañero se ha ido o cambió el modo), se
		// cierra el derribado: si no, TickGroupDowned lo mataría otra vez tras reaparecer.
		const auto EndDowned = [&State]()
		{
			State.bDowned = false;
			State.SecondsLeft = 0.0f;
			State.ReviveProgressSeconds = 0.0f;
		};
		if (PlayersConnected <= 1)
		{
			EndDowned();
			return ECoopHealthZero::Dead;
		}
		if (Mode.HasPermadeath())
		{
			EndDowned();
			return ECoopHealthZero::Spectator;
		}
		if (State.bDowned)
		{
			// Seguir a cero de salud estando en el suelo no reinicia la cuenta atrás.
			return ECoopHealthZero::Downed;
		}
		State.bDowned = true;
		State.ReviveProgressSeconds = 0.0f;
		State.SecondsLeft = RevivesUsedOn(State, Day) >= MaxRevivesPerDay ? DownedSecondsAfterCap : DownedSeconds;
		return ECoopHealthZero::Downed;
	}

	bool IsReviveMedicine(FName ItemId)
	{
		static const FName Medicines[] = { FName(TEXT("botiquin")), FName(TEXT("vendaje_tela")), FName(TEXT("gel_aloe")) };
		for (const FName& Medicine : Medicines)
		{
			if (ItemId == Medicine)
			{
				return true;
			}
		}
		return false;
	}

	bool AdvanceRevive(FCoopDownState& State, float DeltaSeconds, bool bMedicineInHand)
	{
		if (!State.bDowned)
		{
			return false;
		}
		if (FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.0f)
		{
			State.ReviveProgressSeconds += DeltaSeconds;
		}
		const float Required = bMedicineInHand ? ReviveSecondsWithMedicine : ReviveSeconds;
		return State.ReviveProgressSeconds >= Required;
	}

	void CancelRevive(FCoopDownState& State)
	{
		State.ReviveProgressSeconds = 0.0f;
	}

	void FinishRevive(FCoopDownState& State, FSurvivalState& Body, int32 Day)
	{
		if (!State.bDowned)
		{
			return;
		}
		const int32 Used = RevivesUsedOn(State, Day);
		State.bDowned = false;
		State.SecondsLeft = 0.0f;
		State.ReviveProgressSeconds = 0.0f;
		State.RevivesDay = Day;
		State.RevivesToday = Used + 1;
		Body.Health = RevivedHealth;
		Body.Morale = FMath::Clamp(Body.Morale + RevivedMoraleDelta, 0.0f, 100.0f);
	}

	void TickGroupDowned(TArray<FCoopDownState>& Players, float DeltaSeconds, TArray<int32>& OutDied)
	{
		OutDied.Reset();
		const float Delta = (FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.0f) ? DeltaSeconds : 0.0f;
		bool bAnyExpired = false;
		for (FCoopDownState& Player : Players)
		{
			if (!Player.bDowned)
			{
				continue;
			}
			Player.SecondsLeft = FMath::Max(0.0f, Player.SecondsLeft - Delta);
			bAnyExpired |= Player.SecondsLeft <= 0.0f;
		}
		if (!bAnyExpired)
		{
			return;
		}
		int32 Standing = 0;
		for (const FCoopDownState& Player : Players)
		{
			Standing += Player.bDowned ? 0 : 1;
		}
		for (int32 Index = 0; Index < Players.Num(); ++Index)
		{
			FCoopDownState& Player = Players[Index];
			if (Player.bDowned && (Player.SecondsLeft <= 0.0f || Standing == 0))
			{
				Player.bDowned = false;
				Player.SecondsLeft = 0.0f;
				Player.ReviveProgressSeconds = 0.0f;
				OutDied.Add(Index);
			}
		}
	}

	// --- Cooperativo: escalado y reparto (biblia 08 §5.6, §5.7) -------------------------

	int32 CoopPlayersClamped(int32 Players)
	{
		return FMath::Clamp(Players, 1, 4);
	}

	float CoopAbundanceScale(int32 Players)
	{
		return 1.0f + 0.25f * static_cast<float>(CoopPlayersClamped(Players) - 1);
	}

	int32 ScaleFiniteVein(int32 BaseUnits, int32 Players)
	{
		// En enteros: Base × (4 + N − 1) / 4, sin el error de coma flotante del 1,25 × Base.
		const int64 Base = FMath::Max(0, BaseUnits);
		return static_cast<int32>(FMath::Min<int64>(Base * (3 + CoopPlayersClamped(Players)) / 4, MAX_int32));
	}

	int32 PirateRaidersForPlayers(int32 Players, int32 BaseRaiders)
	{
		// Base × (1 + 0,4·(N−1)) = Base × (6 + 4N) / 10, redondeado en enteros: 5 × 1,4
		// en float da 6,9999 y un truncado daría 6 asaltantes en vez de 7.
		const int64 Base = FMath::Max(0, BaseRaiders);
		const int64 Tenths = Base * (6 + 4 * CoopPlayersClamped(Players));
		return static_cast<int32>(FMath::Min<int64>((Tenths + 5) / 10, MAX_int32));
	}

	int32 PirateCategoryBonus(int32 Players)
	{
		return (CoopPlayersClamped(Players) - 1) / 2;
	}

	bool ParseCoopScope(const FString& Text, ECoopScope& OutScope)
	{
		if (Text.Equals(TEXT("actor"), ESearchCase::IgnoreCase))
		{
			OutScope = ECoopScope::Actor;
			return true;
		}
		if (Text.Equals(TEXT("world"), ESearchCase::IgnoreCase))
		{
			OutScope = ECoopScope::World;
			return true;
		}
		if (Text.Equals(TEXT("witness"), ESearchCase::IgnoreCase))
		{
			OutScope = ECoopScope::Witness;
			return true;
		}
		return false;
	}

	TArray<int32> AchievementRecipients(ECoopScope Scope, int32 ActorId, const FVector& EventCm, const TArray<FCoopPlayerSpot>& Players)
	{
		TArray<int32> Out;
		const bool bEventFinite = FMath::IsFinite(EventCm.X) && FMath::IsFinite(EventCm.Y) && FMath::IsFinite(EventCm.Z);
		for (const FCoopPlayerSpot& Player : Players)
		{
			if (!Player.bConnected || Player.PlayerId == INDEX_NONE || Out.Contains(Player.PlayerId))
			{
				continue;
			}
			bool bGets = Player.PlayerId == ActorId;
			if (Scope == ECoopScope::World)
			{
				bGets = true;
			}
			else if (Scope == ECoopScope::Witness && !bGets && bEventFinite)
			{
				const double DistSq = FVector::DistSquared(Player.PositionCm, EventCm);
				// Una posición NaN da una distancia NaN: se descarta de forma explícita.
				bGets = FMath::IsFinite(DistSq) && DistSq < CoopWitnessRadiusCm * CoopWitnessRadiusCm;
			}
			if (bGets)
			{
				Out.Add(Player.PlayerId);
			}
		}
		return Out;
	}
}
