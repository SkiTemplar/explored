#include "Villages/ReputationModel.h"

namespace ReputationDetail
{
	constexpr int32 NumSettlements = static_cast<int32>(ESettlement::Count);

	int32 Clamp(int64 Value)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Value, FReputationModel::MinReputation, FReputationModel::MaxReputation));
	}

	/** Suma Delta con recorte y abre o alarga el enfriamiento si el resultado queda por debajo de 20. */
	FReputationChange Shift(FSettlementReputation& S, int32 Delta, int32 Day)
	{
		FReputationChange Change;
		Change.Before = S.Reputation;
		S.Reputation = Clamp(static_cast<int64>(S.Reputation) + Delta);
		Change.After = S.Reputation;
		Change.Delta = Change.After - Change.Before;
		// Toda acción que baja y deja al pueblo en Hostil cuenta como ofensa grave, aunque el
		// número ya estuviera en 0 y no pueda bajar más: si no, pegar a un aldeano con la
		// reputación en el suelo saldría gratis.
		if (Delta < 0 && S.Reputation < FReputationModel::HostileBelow)
		{
			const int32 Until = static_cast<int32>(FMath::Min<int64>(
				static_cast<int64>(Day) + FReputationModel::HostileCooldownDays, TNumericLimits<int32>::Max()));
			if (Until > S.TradeCooldownUntilDay)
			{
				S.TradeCooldownUntilDay = Until;
				Change.bCooldownStarted = true;
			}
		}
		return Change;
	}
}

const TCHAR* LexToString(ESettlement Settlement)
{
	switch (Settlement)
	{
	case ESettlement::WhiteSands: return TEXT("whitesands");
	case ESettlement::Mesa: return TEXT("mesa");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EReputationTier Tier)
{
	switch (Tier)
	{
	case EReputationTier::Hostile: return TEXT("hostil");
	case EReputationTier::Wary: return TEXT("cauta");
	case EReputationTier::Neutral: return TEXT("neutral");
	case EReputationTier::Good: return TEXT("buena");
	case EReputationTier::High: return TEXT("alta");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EReputationAction Action)
{
	switch (Action)
	{
	case EReputationAction::Trade: return TEXT("trueque");
	case EReputationAction::ReturnRitual: return TEXT("devolver_ritual");
	case EReputationAction::BoardRequest: return TEXT("encargo_tablon");
	case EReputationAction::RespectfulDay: return TEXT("jornada_respetuosa");
	case EReputationAction::DefendRaid: return TEXT("defender_asalto");
	case EReputationAction::HuntNearby: return TEXT("cazar_cerca");
	case EReputationAction::MineOrFellNearMarae: return TEXT("minar_talar_marae");
	case EReputationAction::Loot: return TEXT("saquear");
	case EReputationAction::StrikeVillager: return TEXT("golpear_aldeano");
	default: return TEXT("unknown");
	}
}

bool FReputationState::operator==(const FReputationState& Other) const
{
	for (int32 I = 0; I < ReputationDetail::NumSettlements; ++I)
	{
		if (!(Settlements[I] == Other.Settlements[I]))
		{
			return false;
		}
	}
	return ReturnedRituals == Other.ReturnedRituals;
}

int32 FReputationModel::TierMin(EReputationTier Tier)
{
	switch (Tier)
	{
	case EReputationTier::Hostile: return 0;
	case EReputationTier::Wary: return 20;
	case EReputationTier::Neutral: return 40;
	case EReputationTier::Good: return 70;
	case EReputationTier::High: return 90;
	default: return MaxReputation + 1;
	}
}

EReputationTier FReputationModel::TierOf(int32 Reputation)
{
	if (Reputation >= 90)
	{
		return EReputationTier::High;
	}
	if (Reputation >= 70)
	{
		return EReputationTier::Good;
	}
	if (Reputation >= 40)
	{
		return EReputationTier::Neutral;
	}
	if (Reputation >= 20)
	{
		return EReputationTier::Wary;
	}
	return EReputationTier::Hostile;
}

int32 FReputationModel::TradeRateQuarters(EReputationTier Tier)
{
	switch (Tier)
	{
	case EReputationTier::Wary: return 3;
	case EReputationTier::Neutral: return 4;
	case EReputationTier::Good: return 5;
	case EReputationTier::High: return 6;
	default: return 0;
	}
}

int32 FReputationModel::ActionDelta(EReputationAction Action)
{
	switch (Action)
	{
	case EReputationAction::Trade: return 3;
	case EReputationAction::ReturnRitual: return 5;
	case EReputationAction::BoardRequest: return 4;
	case EReputationAction::RespectfulDay: return 2;
	case EReputationAction::DefendRaid: return 10;
	case EReputationAction::HuntNearby: return -15;
	case EReputationAction::MineOrFellNearMarae: return -10;
	case EReputationAction::Loot: return -20;
	case EReputationAction::StrikeVillager: return -25;
	default: return 0;
	}
}

bool FReputationModel::Contact(FReputationState& State, ESettlement Settlement)
{
	if (Settlement >= ESettlement::Count)
	{
		return false;
	}
	FSettlementReputation& S = State.Get(Settlement);
	if (S.bContacted)
	{
		return false;
	}
	S.bContacted = true;
	S.Reputation = FirstContactReputation;
	return true;
}

FReputationChange FReputationModel::Apply(FReputationState& State, ESettlement Settlement, EReputationAction Action, int32 Day)
{
	FReputationChange Ignored;
	Ignored.bIgnored = true;
	if (Settlement >= ESettlement::Count || Action >= EReputationAction::Count || Action == EReputationAction::ReturnRitual)
	{
		return Ignored;
	}
	const bool bFirst = Contact(State, Settlement);
	FSettlementReputation& S = State.Get(Settlement);
	Ignored.Before = Ignored.After = S.Reputation;
	Ignored.bFirstContact = bFirst;

	switch (Action)
	{
	case EReputationAction::Trade:
		if (S.LastTradeReputationDay == Day)
		{
			return Ignored;
		}
		S.LastTradeReputationDay = Day;
		break;
	case EReputationAction::RespectfulDay:
		if (S.LastRespectfulDay == Day)
		{
			return Ignored;
		}
		S.LastRespectfulDay = Day;
		break;
	case EReputationAction::DefendRaid:
		if (S.Reputation < DefendRaidMinReputation)
		{
			return Ignored;
		}
		break;
	case EReputationAction::BoardRequest:
		S.FreeTradeDay = Day;
		break;
	default:
		break;
	}

	FReputationChange Change = ReputationDetail::Shift(S, ActionDelta(Action), Day);
	Change.bFirstContact = bFirst;
	return Change;
}

bool FReputationModel::IsRitualProvenance(EArtifactProvenance Provenance)
{
	return Provenance == EArtifactProvenance::Marae || Provenance == EArtifactProvenance::RitualCave;
}

FReputationChange FReputationModel::ReturnRitualObject(FReputationState& State, ESettlement Settlement, FName ArtifactId,
	EArtifactProvenance Provenance, int32 Day)
{
	FReputationChange Ignored;
	Ignored.bIgnored = true;
	if (Settlement >= ESettlement::Count || ArtifactId.IsNone() || !IsRitualProvenance(Provenance) ||
		State.ReturnedRituals.Contains(ArtifactId))
	{
		if (Settlement < ESettlement::Count)
		{
			Ignored.Before = Ignored.After = State.Get(Settlement).Reputation;
		}
		return Ignored;
	}
	const bool bFirst = Contact(State, Settlement);
	State.ReturnedRituals.Add(ArtifactId);
	FReputationChange Change = ReputationDetail::Shift(State.Get(Settlement), ActionDelta(EReputationAction::ReturnRitual), Day);
	Change.bFirstContact = bFirst;
	return Change;
}

FVillagerStrikeResult FReputationModel::StrikeVillager(FReputationState& State, ESettlement Settlement, int32 Day)
{
	FVillagerStrikeResult Result;
	Result.Change = Apply(State, Settlement, EReputationAction::StrikeVillager, Day);
	return Result;
}

EReputationTier FReputationModel::Tier(const FReputationState& State, ESettlement Settlement)
{
	if (Settlement >= ESettlement::Count || !State.Get(Settlement).bContacted)
	{
		return EReputationTier::Neutral;
	}
	return TierOf(State.Get(Settlement).Reputation);
}

bool FReputationModel::IsInCooldown(const FReputationState& State, ESettlement Settlement, int32 Day)
{
	return Settlement < ESettlement::Count && Day < State.Get(Settlement).TradeCooldownUntilDay;
}

int32 FReputationModel::CooldownDaysLeft(const FReputationState& State, ESettlement Settlement, int32 Day)
{
	if (!IsInCooldown(State, Settlement, Day))
	{
		return 0;
	}
	return static_cast<int32>(FMath::Min<int64>(
		static_cast<int64>(State.Get(Settlement).TradeCooldownUntilDay) - Day, TNumericLimits<int32>::Max()));
}

bool FReputationModel::IsTradeOpenByReputation(const FReputationState& State, ESettlement Settlement, int32 Day)
{
	if (Settlement >= ESettlement::Count || !State.Get(Settlement).bContacted)
	{
		return false;
	}
	return Tier(State, Settlement) != EReputationTier::Hostile && !IsInCooldown(State, Settlement, Day);
}

bool FReputationModel::HasFreeTrade(const FReputationState& State, ESettlement Settlement, int32 Day)
{
	return Settlement < ESettlement::Count && State.Get(Settlement).bContacted && State.Get(Settlement).FreeTradeDay == Day;
}

bool FReputationModel::CanTeachWayfinding(const FReputationState& State, ESettlement Settlement)
{
	return Settlement < ESettlement::Count && State.Get(Settlement).bContacted &&
		State.Get(Settlement).Reputation >= TierMin(EReputationTier::Good);
}

bool FReputationModel::GrantsHighFavors(const FReputationState& State, ESettlement Settlement)
{
	return Settlement < ESettlement::Count && State.Get(Settlement).bContacted &&
		State.Get(Settlement).Reputation >= TierMin(EReputationTier::High);
}

bool FReputationModel::IsHostileCombatant(const FReputationState& /*State*/, ESettlement /*Settlement*/)
{
	return false;
}

void FReputationModel::Save(FSaveArchive& Ar, const FReputationState& State)
{
	for (int32 I = 0; I < ReputationDetail::NumSettlements; ++I)
	{
		const FSettlementReputation& S = State.Settlements[I];
		FSaveArchive Entry;
		Entry.Write(TEXT("contacted"), S.bContacted);
		Entry.Write(TEXT("reputation"), S.Reputation);
		Entry.Write(TEXT("tradeCooldownUntilDay"), S.TradeCooldownUntilDay);
		Entry.Write(TEXT("lastTradeReputationDay"), S.LastTradeReputationDay);
		Entry.Write(TEXT("lastRespectfulDay"), S.LastRespectfulDay);
		Entry.Write(TEXT("freeTradeDay"), S.FreeTradeDay);
		Ar.Write(LexToString(static_cast<ESettlement>(I)), Entry);
	}
	Ar.Write(TEXT("returnedRituals"), State.ReturnedRituals);
}

void FReputationModel::Load(const FSaveArchive& Ar, FReputationState& OutState)
{
	OutState = FReputationState();
	for (int32 I = 0; I < ReputationDetail::NumSettlements; ++I)
	{
		FSaveArchive Entry;
		if (!Ar.Read(LexToString(static_cast<ESettlement>(I)), Entry))
		{
			continue;
		}
		FSettlementReputation S;
		Entry.Read(TEXT("contacted"), S.bContacted);
		if (!S.bContacted)
		{
			continue;
		}
		// Sin número legible, el contacto vuelve al valor del primer contacto en vez de a 0 (Hostil).
		int64 Reputation = FirstContactReputation;
		Entry.Read(TEXT("reputation"), Reputation);
		S.Reputation = ReputationDetail::Clamp(Reputation);
		Entry.Read(TEXT("tradeCooldownUntilDay"), S.TradeCooldownUntilDay);
		Entry.Read(TEXT("lastTradeReputationDay"), S.LastTradeReputationDay);
		Entry.Read(TEXT("lastRespectfulDay"), S.LastRespectfulDay);
		Entry.Read(TEXT("freeTradeDay"), S.FreeTradeDay);
		OutState.Settlements[I] = S;
	}
	if (const FSaveValue* List = Ar.FindValue(TEXT("returnedRituals")); List && List->IsArray())
	{
		for (int32 I = 0; I < List->Num(); ++I)
		{
			FName Id;
			if (TSaveTraits<FName>::FromValue(List->At(I), Id) && !Id.IsNone())
			{
				OutState.ReturnedRituals.AddUnique(Id);
			}
		}
	}
}
