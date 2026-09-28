#include "Villages/BarterModel.h"

const TCHAR* LexToString(EBarterCategory Category)
{
	switch (Category)
	{
	case EBarterCategory::None: return TEXT("none");
	case EBarterCategory::CommonFood: return TEXT("fruta_y_pescado");
	case EBarterCategory::PreparedFood: return TEXT("comida_preparada");
	case EBarterCategory::FiberCeramic: return TEXT("fibra_y_ceramica");
	case EBarterCategory::Leather: return TEXT("pieles");
	case EBarterCategory::GoodTool: return TEXT("herramientas_buenas");
	case EBarterCategory::Medicine: return TEXT("medicina");
	case EBarterCategory::WorkedMetal: return TEXT("metal_trabajado");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EBarterResult Result)
{
	switch (Result)
	{
	case EBarterResult::Accepted: return TEXT("Accepted");
	case EBarterResult::NotContacted: return TEXT("NotContacted");
	case EBarterResult::OutsideHours: return TEXT("OutsideHours");
	case EBarterResult::Hostile: return TEXT("Hostile");
	case EBarterResult::Cooldown: return TEXT("Cooldown");
	case EBarterResult::TierTooLow: return TEXT("TierTooLow");
	case EBarterResult::NotBasicWhileWary: return TEXT("NotBasicWhileWary");
	case EBarterResult::NotTradeable: return TEXT("NotTradeable");
	case EBarterResult::Invalid: return TEXT("Invalid");
	case EBarterResult::NotEnough: return TEXT("NotEnough");
	case EBarterResult::NoFreeTrade: return TEXT("NoFreeTrade");
	default: return TEXT("Unknown");
	}
}

int32 FBarterModel::CategoryValue(EBarterCategory Category)
{
	switch (Category)
	{
	case EBarterCategory::CommonFood: return 1;
	case EBarterCategory::PreparedFood: return 2;
	case EBarterCategory::FiberCeramic: return 2;
	case EBarterCategory::Leather: return 3;
	case EBarterCategory::GoodTool: return 3;
	case EBarterCategory::Medicine: return 4;
	case EBarterCategory::WorkedMetal: return 5;
	default: return 0;
	}
}

bool FBarterModel::IsBasic(EBarterCategory Category)
{
	return Category == EBarterCategory::CommonFood || Category == EBarterCategory::PreparedFood ||
		Category == EBarterCategory::FiberCeramic;
}

bool FBarterModel::IsWithinHours(float Hour)
{
	// Con NaN las dos comparaciones fallan: cerrado.
	return Hour >= OpenHour && Hour < CloseHour;
}

FBarterOutcome FBarterModel::Evaluate(const FReputationState& State, const FBarterRequest& Request)
{
	FBarterOutcome Out;
	const ESettlement S = Request.Settlement;
	if (S >= ESettlement::Count)
	{
		Out.Result = EBarterResult::Invalid;
		return Out;
	}
	if (!State.Get(S).bContacted)
	{
		Out.Result = EBarterResult::NotContacted;
		return Out;
	}
	if (!IsWithinHours(Request.Hour))
	{
		Out.Result = EBarterResult::OutsideHours;
		return Out;
	}
	const EReputationTier Tier = FReputationModel::Tier(State, S);
	if (Tier == EReputationTier::Hostile)
	{
		Out.Result = EBarterResult::Hostile;
		return Out;
	}
	if (FReputationModel::IsInCooldown(State, S, Request.Day))
	{
		Out.Result = EBarterResult::Cooldown;
		return Out;
	}

	const FBarterOffer& Wanted = Request.Wanted;
	if (Wanted.Id.IsNone() || Wanted.Value < 1 || Wanted.Value > 5 || Wanted.MinTier >= EReputationTier::Count ||
		Request.WantedCount < 1 || Request.WantedCount > MaxCountPerLine || Request.Given.Num() > MaxLines)
	{
		Out.Result = EBarterResult::Invalid;
		return Out;
	}
	if (Tier < Wanted.MinTier)
	{
		Out.Result = EBarterResult::TierTooLow;
		return Out;
	}

	if (Request.bUseFreeTrade)
	{
		if (!FReputationModel::HasFreeTrade(State, S, Request.Day))
		{
			Out.Result = EBarterResult::NoFreeTrade;
			return Out;
		}
		// El regalo del encargo es un objeto, no un lote: una unidad de lo pedido.
		if (Request.WantedCount != 1 || Request.Given.Num() != 0)
		{
			Out.Result = EBarterResult::Invalid;
			return Out;
		}
		Out.bUsedFreeTrade = true;
		Out.Result = EBarterResult::Accepted;
		return Out;
	}

	if (Request.Given.Num() == 0)
	{
		Out.Result = EBarterResult::NotEnough;
		Out.RequiredQuarters = static_cast<int64>(Wanted.Value) * Request.WantedCount * 4;
		return Out;
	}
	int64 Sum = 0;
	for (const FBarterGive& Line : Request.Given)
	{
		if (Line.Count < 1 || Line.Count > MaxCountPerLine || Line.Category >= EBarterCategory::Count)
		{
			Out.Result = EBarterResult::Invalid;
			return Out;
		}
		if (Line.Category == EBarterCategory::None)
		{
			Out.Result = EBarterResult::NotTradeable;
			return Out;
		}
		if (Tier == EReputationTier::Wary && !IsBasic(Line.Category))
		{
			Out.Result = EBarterResult::NotBasicWhileWary;
			return Out;
		}
		Sum += static_cast<int64>(CategoryValue(Line.Category)) * Line.Count;
	}
	Out.OfferedQuarters = Sum * FReputationModel::TradeRateQuarters(Tier);
	Out.RequiredQuarters = static_cast<int64>(Wanted.Value) * Request.WantedCount * 4;
	Out.Result = Out.OfferedQuarters >= Out.RequiredQuarters ? EBarterResult::Accepted : EBarterResult::NotEnough;
	return Out;
}

FBarterOutcome FBarterModel::Execute(FReputationState& State, const FBarterRequest& Request)
{
	FBarterOutcome Out = Evaluate(State, Request);
	if (!Out.IsAccepted())
	{
		return Out;
	}
	FSettlementReputation& S = State.Get(Request.Settlement);
	if (Out.bUsedFreeTrade)
	{
		S.FreeTradeDay = -1;
		return Out;
	}
	Out.ReputationGained = FReputationModel::Apply(State, Request.Settlement, EReputationAction::Trade, Request.Day).Delta;
	return Out;
}

int32 FBarterModel::AffordableCount(const FReputationState& State, const FBarterRequest& Request)
{
	FBarterRequest One = Request;
	One.WantedCount = 1;
	const FBarterOutcome Outcome = Evaluate(State, One);
	if (!Outcome.IsAccepted())
	{
		return 0;
	}
	if (Outcome.bUsedFreeTrade)
	{
		return 1;
	}
	const int64 Count = Outcome.OfferedQuarters / Outcome.RequiredQuarters;
	return static_cast<int32>(FMath::Min<int64>(Count, MaxCountPerLine));
}
