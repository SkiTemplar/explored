#include "Raiders/PirateThreatModel.h"

#include "Core/ExploredRandom.h"

namespace PirateThreatDetail
{
	/** Corrientes del PCG por uso: el intervalo y el grupo de un mismo asalto no comparten tiradas. */
	constexpr uint64 IntervalStream = 0x52414944494E54ULL;  // «RAIDINT»
	constexpr uint64 PartyStream = 0x5241494450525459ULL;   // «RAIDPRTY»

	FExploredRandom MakeRng(uint64 Seed, int32 Serial, uint64 Stream)
	{
		return FExploredRandom(Seed ^ (static_cast<uint64>(static_cast<uint32>(Serial)) * 0x9E3779B97F4A7C15ULL), Stream);
	}

	int32 ClampThreat(int64 Value)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Value, FPirateThreatModel::MinThreat, FPirateThreatModel::MaxThreat));
	}

	int32 ClampPlayers(int32 PlayerCount)
	{
		return FMath::Clamp(PlayerCount, 1, FPirateThreatModel::MaxPlayers);
	}

	int32 AddDays(int32 Day, int64 Days)
	{
		return static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Day) + Days, TNumericLimits<int32>::Max()));
	}
}

const TCHAR* LexToString(EPirateType Type)
{
	switch (Type)
	{
	case EPirateType::Raider: return TEXT("saqueador");
	case EPirateType::Archer: return TEXT("arquero");
	case EPirateType::Firestarter: return TEXT("incendiario");
	case EPirateType::Captain: return TEXT("capitan");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EPirateThreatAction Action)
{
	switch (Action)
	{
	case EPirateThreatAction::LootOrBurnCamp: return TEXT("saquear_campamento");
	case EPirateThreatAction::DefeatShipCrew: return TEXT("derrotar_tripulacion");
	case EPirateThreatAction::KillInProvokedRaid: return TEXT("matar_en_asalto_provocado");
	case EPirateThreatAction::RepelUnprovokedRaid: return TEXT("repeler_asalto");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(ERaidCategory Category)
{
	switch (Category)
	{
	case ERaidCategory::None: return TEXT("none");
	case ERaidCategory::Low: return TEXT("low");
	case ERaidCategory::Medium: return TEXT("medium");
	case ERaidCategory::High: return TEXT("high");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(ERaidDayEvent Event)
{
	switch (Event)
	{
	case ERaidDayEvent::Nothing: return TEXT("Nothing");
	case ERaidDayEvent::Scheduled: return TEXT("Scheduled");
	case ERaidDayEvent::Warning: return TEXT("Warning");
	case ERaidDayEvent::Postponed: return TEXT("Postponed");
	case ERaidDayEvent::Raid: return TEXT("Raid");
	default: return TEXT("Unknown");
	}
}

int32 FRaidParty::Num(EPirateType Type) const
{
	switch (Type)
	{
	case EPirateType::Raider: return Raiders;
	case EPirateType::Archer: return Archers;
	case EPirateType::Firestarter: return Firestarters;
	case EPirateType::Captain: return Captains;
	default: return 0;
	}
}

FPirateStats FPirateThreatModel::Stats(EPirateType Type)
{
	FPirateStats S;
	switch (Type)
	{
	case EPirateType::Raider: S.Health = 40.0f; S.Damage = 9.0f; S.RangeM = 1.5f; break;
	case EPirateType::Archer: S.Health = 30.0f; S.Damage = 9.0f; S.RangeM = 15.0f; break;
	case EPirateType::Firestarter: S.Health = 35.0f; S.Damage = 0.0f; S.RangeM = 12.0f; break;
	case EPirateType::Captain: S.Health = 90.0f; S.Damage = 15.0f; S.RangeM = 1.5f; S.bStuns = true; break;
	default: break;
	}
	return S;
}

int32 FPirateThreatModel::ActionDelta(EPirateThreatAction Action)
{
	switch (Action)
	{
	case EPirateThreatAction::LootOrBurnCamp: return 8;
	case EPirateThreatAction::DefeatShipCrew: return 15;
	case EPirateThreatAction::KillInProvokedRaid: return 4;
	default: return 0;
	}
}

bool FPirateThreatModel::IsAggression(EPirateThreatAction Action)
{
	return Action == EPirateThreatAction::LootOrBurnCamp || Action == EPirateThreatAction::DefeatShipCrew ||
		Action == EPirateThreatAction::KillInProvokedRaid;
}

ERaidCategory FPirateThreatModel::CategoryOf(int32 Threat)
{
	if (Threat >= 75)
	{
		return ERaidCategory::High;
	}
	if (Threat >= 50)
	{
		return ERaidCategory::Medium;
	}
	if (Threat >= 25)
	{
		return ERaidCategory::Low;
	}
	return ERaidCategory::None;
}

void FPirateThreatModel::IntervalDays(ERaidCategory Category, int32& OutMin, int32& OutMax)
{
	switch (Category)
	{
	case ERaidCategory::Low: OutMin = 6; OutMax = 9; return;
	case ERaidCategory::Medium: OutMin = 4; OutMax = 6; return;
	case ERaidCategory::High: OutMin = 2; OutMax = 4; return;
	default: OutMin = 0; OutMax = 0; return;
	}
}

ERaidCategory FPirateThreatModel::EffectiveCategory(ERaidCategory Base, bool bNearestVillageHostile, int32 PlayerCount)
{
	if (Base == ERaidCategory::None || Base >= ERaidCategory::Count)
	{
		return ERaidCategory::None;
	}
	int32 Level = static_cast<int32>(Base);
	Level += bNearestVillageHostile ? 1 : 0;
	Level += (PirateThreatDetail::ClampPlayers(PlayerCount) - 1) / 2;
	return static_cast<ERaidCategory>(FMath::Min(Level, static_cast<int32>(ERaidCategory::High)));
}

int32 FPirateThreatModel::ScaledPartySize(int32 Base, int32 PlayerCount)
{
	const int64 Tenths = 10 + 4 * (PirateThreatDetail::ClampPlayers(PlayerCount) - 1);
	return static_cast<int32>((static_cast<int64>(FMath::Max(0, Base)) * Tenths + 5) / 10);
}

FRaidParty FPirateThreatModel::MakeParty(ERaidCategory Category, int32 PlayerCount, uint64 Seed, int32 Serial)
{
	FRaidParty Party;
	FExploredRandom Rng = PirateThreatDetail::MakeRng(Seed, Serial, PirateThreatDetail::PartyStream);
	switch (Category)
	{
	case ERaidCategory::Low:
		Party.Raiders = 1;
		Party.Archers = 1;
		break;
	case ERaidCategory::Medium:
		Party.Raiders = Rng.Chance(MediumExtraRaiderChance) ? 2 : 1;
		Party.Archers = 1;
		Party.Firestarters = 1;
		break;
	case ERaidCategory::High:
		Party.Captains = 1;
		Party.Raiders = 2;
		Party.Archers = 1;
		Party.Firestarters = 1;
		if (Rng.RangeInt(5, 6) == 6)
		{
			++Party.Raiders;
		}
		break;
	default:
		return Party;
	}
	const int32 Target = ScaledPartySize(Party.Total(), PlayerCount);
	for (int32 Extra = 0; Party.Total() < Target; ++Extra)
	{
		switch (Extra % 3)
		{
		case 0: ++Party.Raiders; break;
		case 1: ++Party.Archers; break;
		default: ++Party.Firestarters; break;
		}
	}
	return Party;
}

int32 FPirateThreatModel::Apply(FPirateThreatState& State, EPirateThreatAction Action, int32 Day)
{
	if (Action >= EPirateThreatAction::Count)
	{
		return 0;
	}
	AdvanceCalm(State, Day);
	const int32 Before = State.Threat;
	State.Threat = PirateThreatDetail::ClampThreat(static_cast<int64>(State.Threat) + ActionDelta(Action));
	if (IsAggression(Action) && Day > State.CalmSinceDay)
	{
		State.CalmSinceDay = Day;
	}
	return State.Threat - Before;
}

void FPirateThreatModel::AdvanceCalm(FPirateThreatState& State, int32 Day)
{
	const int64 Elapsed = static_cast<int64>(Day) - State.CalmSinceDay;
	if (Elapsed < CalmDays)
	{
		return;
	}
	const int64 Steps = Elapsed / CalmDays;
	State.Threat = PirateThreatDetail::ClampThreat(static_cast<int64>(State.Threat) - Steps * CalmDecay);
	State.CalmSinceDay = PirateThreatDetail::AddDays(State.CalmSinceDay, Steps * CalmDays);
}

FRaidDayResult FPirateThreatModel::EvaluateDay(FPirateThreatState& State, int32 Day, const FRaidConditions& Conditions, uint64 Seed)
{
	FRaidDayResult R;
	if (Day <= State.LastEvaluatedDay)
	{
		R.BaseCategory = CategoryOf(State.Threat);
		R.NextRaidDay = State.NextRaidDay;
		return R;
	}
	State.LastEvaluatedDay = Day;
	AdvanceCalm(State, Day);
	R.BaseCategory = CategoryOf(State.Threat);

	if (!Conditions.bBaseMarked || R.BaseCategory == ERaidCategory::None)
	{
		State.NextRaidDay = -1;
		return R;
	}

	auto Schedule = [&State, Seed, &R](int32 FromDay)
	{
		int32 Min = 0, Max = 0;
		IntervalDays(R.BaseCategory, Min, Max);
		FExploredRandom Rng = PirateThreatDetail::MakeRng(Seed, State.RaidSerial, PirateThreatDetail::IntervalStream);
		State.NextRaidDay = PirateThreatDetail::AddDays(FromDay, Rng.RangeInt(Min, Max));
	};

	if (State.NextRaidDay < 0)
	{
		Schedule(Day);
		R.Event = ERaidDayEvent::Scheduled;
		R.NextRaidDay = State.NextRaidDay;
		return R;
	}

	if (Day >= State.NextRaidDay)
	{
		if (!Conditions.bResourcesExposed && !Conditions.bNearestVillageHostile)
		{
			// Sin nada que llevarse ni pueblo desprotegido: se reintenta mañana sin gastar el ciclo.
			State.NextRaidDay = PirateThreatDetail::AddDays(Day, 1);
			R.Event = ERaidDayEvent::Postponed;
			R.bSmokeWarning = true;
			R.NextRaidDay = State.NextRaidDay;
			return R;
		}
		R.Event = ERaidDayEvent::Raid;
		R.Category = EffectiveCategory(R.BaseCategory, Conditions.bNearestVillageHostile, Conditions.PlayerCount);
		R.Party = MakeParty(R.Category, Conditions.PlayerCount, Seed, State.RaidSerial);
		R.bDawnDrum = R.Category == ERaidCategory::High;
		State.RaidSerial = State.RaidSerial < TNumericLimits<int32>::Max() ? State.RaidSerial + 1 : 0;
		Schedule(Day);
		R.NextRaidDay = State.NextRaidDay;
		return R;
	}

	if (Day == State.NextRaidDay - 1)
	{
		R.Event = ERaidDayEvent::Warning;
		R.bSmokeWarning = true;
	}
	R.NextRaidDay = State.NextRaidDay;
	return R;
}

void FPirateThreatModel::Save(FSaveArchive& Ar, const FPirateThreatState& State)
{
	Ar.Write(TEXT("threat"), State.Threat);
	Ar.Write(TEXT("calmSinceDay"), State.CalmSinceDay);
	Ar.Write(TEXT("nextRaidDay"), State.NextRaidDay);
	Ar.Write(TEXT("raidSerial"), State.RaidSerial);
	Ar.Write(TEXT("lastEvaluatedDay"), State.LastEvaluatedDay);
}

void FPirateThreatModel::Load(const FSaveArchive& Ar, FPirateThreatState& OutState)
{
	OutState = FPirateThreatState();
	int64 Threat = 0;
	Ar.Read(TEXT("threat"), Threat);
	OutState.Threat = PirateThreatDetail::ClampThreat(Threat);
	Ar.Read(TEXT("calmSinceDay"), OutState.CalmSinceDay);
	Ar.Read(TEXT("nextRaidDay"), OutState.NextRaidDay);
	Ar.Read(TEXT("raidSerial"), OutState.RaidSerial);
	Ar.Read(TEXT("lastEvaluatedDay"), OutState.LastEvaluatedDay);
	// Un asalto se programa como mucho 9 días (el intervalo más largo) después del último día
	// evaluado y los aplazamientos lo mueven de uno en uno: más allá es un dato roto que dejaría
	// la isla sin asaltos para siempre. Se borra y el programador vuelve a tirar.
	int32 MaxMin = 0, MaxInterval = 0;
	IntervalDays(ERaidCategory::Low, MaxMin, MaxInterval);
	if (OutState.NextRaidDay < -1 || (OutState.NextRaidDay >= 0 &&
		(OutState.LastEvaluatedDay < 0 || static_cast<int64>(OutState.NextRaidDay) > static_cast<int64>(OutState.LastEvaluatedDay) + MaxInterval)))
	{
		OutState.NextRaidDay = -1;
	}
	if (OutState.RaidSerial < 0)
	{
		OutState.RaidSerial = 0;
	}
	if (OutState.LastEvaluatedDay < -1)
	{
		OutState.LastEvaluatedDay = -1;
	}
}
