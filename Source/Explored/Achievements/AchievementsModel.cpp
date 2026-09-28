#include "Achievements/AchievementsModel.h"

namespace AchievementsDetail
{
	/** Tope del progreso de un logro sin cumplir: la barra solo se llena al desbloquearse. */
	constexpr float MaxProgressWhilePending = 0.99f;

	bool CompareValues(double Measure, EAchievementCompareOp Op, double Value)
	{
		switch (Op)
		{
		case EAchievementCompareOp::GreaterEqual: return Measure >= Value;
		case EAchievementCompareOp::Greater: return Measure > Value;
		case EAchievementCompareOp::LessEqual: return Measure <= Value;
		case EAchievementCompareOp::Less: return Measure < Value;
		case EAchievementCompareOp::Equal: return FMath::IsNearlyEqual(Measure, Value, 1e-6);
		default: return false;
		}
	}

	bool NumbersEqual(const TMap<FName, double>& A, const TMap<FName, double>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (const TPair<FName, double>& Pair : A)
		{
			const double* Other = B.Find(Pair.Key);
			if (!Other || *Other != Pair.Value)
			{
				return false;
			}
		}
		return true;
	}

	bool SetsEqual(const TMap<FName, TArray<FName>>& A, const TMap<FName, TArray<FName>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (const TPair<FName, TArray<FName>>& Pair : A)
		{
			const TArray<FName>* Other = B.Find(Pair.Key);
			if (!Other || *Other != Pair.Value)
			{
				return false;
			}
		}
		return true;
	}
}

const TCHAR* LexToString(EAchievementStatKind Kind)
{
	switch (Kind)
	{
	case EAchievementStatKind::Counter: return TEXT("counter");
	case EAchievementStatKind::Maximum: return TEXT("max");
	case EAchievementStatKind::Set: return TEXT("set");
	case EAchievementStatKind::Flag: return TEXT("flag");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EAchievementStatScope Scope)
{
	switch (Scope)
	{
	case EAchievementStatScope::Profile: return TEXT("profile");
	case EAchievementStatScope::Run: return TEXT("run");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EAchievementPhase Phase)
{
	switch (Phase)
	{
	case EAchievementPhase::EarlyAccess: return TEXT("AA");
	case EAchievementPhase::Phase2: return TEXT("F2");
	case EAchievementPhase::Phase3: return TEXT("F3");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EAchievementRarity Rarity)
{
	switch (Rarity)
	{
	case EAchievementRarity::Common: return TEXT("comun");
	case EAchievementRarity::Uncommon: return TEXT("infrecuente");
	case EAchievementRarity::Rare: return TEXT("raro");
	case EAchievementRarity::VeryRare: return TEXT("muy_raro");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EAchievementCoopScope Scope)
{
	switch (Scope)
	{
	case EAchievementCoopScope::Actor: return TEXT("actor");
	case EAchievementCoopScope::World: return TEXT("world");
	case EAchievementCoopScope::Witness: return TEXT("witness");
	default: return TEXT("unknown");
	}
}

bool ParseAchievementPhase(const FString& Text, EAchievementPhase& OutPhase)
{
	// Sensible a mayúsculas, igual que el resto del JSON: «aa» es un error de datos.
	if (Text.Equals(TEXT("AA"), ESearchCase::CaseSensitive)) { OutPhase = EAchievementPhase::EarlyAccess; return true; }
	if (Text.Equals(TEXT("F2"), ESearchCase::CaseSensitive)) { OutPhase = EAchievementPhase::Phase2; return true; }
	if (Text.Equals(TEXT("F3"), ESearchCase::CaseSensitive)) { OutPhase = EAchievementPhase::Phase3; return true; }
	return false;
}

bool ParseAchievementRarity(const FString& Text, EAchievementRarity& OutRarity)
{
	if (Text.Equals(TEXT("comun"), ESearchCase::CaseSensitive)) { OutRarity = EAchievementRarity::Common; return true; }
	if (Text.Equals(TEXT("infrecuente"), ESearchCase::CaseSensitive)) { OutRarity = EAchievementRarity::Uncommon; return true; }
	if (Text.Equals(TEXT("raro"), ESearchCase::CaseSensitive)) { OutRarity = EAchievementRarity::Rare; return true; }
	if (Text.Equals(TEXT("muy_raro"), ESearchCase::CaseSensitive)) { OutRarity = EAchievementRarity::VeryRare; return true; }
	return false;
}

bool ParseAchievementCoopScope(const FString& Text, EAchievementCoopScope& OutScope)
{
	if (Text.Equals(TEXT("actor"), ESearchCase::CaseSensitive)) { OutScope = EAchievementCoopScope::Actor; return true; }
	if (Text.Equals(TEXT("world"), ESearchCase::CaseSensitive)) { OutScope = EAchievementCoopScope::World; return true; }
	if (Text.Equals(TEXT("witness"), ESearchCase::CaseSensitive)) { OutScope = EAchievementCoopScope::Witness; return true; }
	return false;
}

bool ParseAchievementCompareOp(const FString& Text, EAchievementCompareOp& OutOp)
{
	if (Text == TEXT(">=")) { OutOp = EAchievementCompareOp::GreaterEqual; return true; }
	if (Text == TEXT(">")) { OutOp = EAchievementCompareOp::Greater; return true; }
	if (Text == TEXT("<=")) { OutOp = EAchievementCompareOp::LessEqual; return true; }
	if (Text == TEXT("<")) { OutOp = EAchievementCompareOp::Less; return true; }
	if (Text == TEXT("==")) { OutOp = EAchievementCompareOp::Equal; return true; }
	return false;
}

// ---------------------------------------------------------------------------
// Condiciones
// ---------------------------------------------------------------------------

FAchievementCondition FAchievementCondition::Compare(FName InStat, EAchievementCompareOp InOp, double InValue)
{
	FAchievementCondition C;
	C.Kind = EAchievementConditionKind::Compare;
	C.Stat = InStat;
	C.Op = InOp;
	C.Value = InValue;
	return C;
}

FAchievementCondition FAchievementCondition::Contains(FName InStat, FName InItem)
{
	FAchievementCondition C;
	C.Kind = EAchievementConditionKind::Contains;
	C.Stat = InStat;
	C.Item = InItem;
	return C;
}

FAchievementCondition FAchievementCondition::Flag(FName InStat)
{
	FAchievementCondition C;
	C.Kind = EAchievementConditionKind::Flag;
	C.Stat = InStat;
	return C;
}

FAchievementCondition FAchievementCondition::All(TArray<FAchievementCondition> InChildren)
{
	FAchievementCondition C;
	C.Kind = EAchievementConditionKind::All;
	C.Children = MoveTemp(InChildren);
	return C;
}

FAchievementCondition FAchievementCondition::Any(TArray<FAchievementCondition> InChildren)
{
	FAchievementCondition C;
	C.Kind = EAchievementConditionKind::Any;
	C.Children = MoveTemp(InChildren);
	return C;
}

FAchievementCondition FAchievementCondition::Not(FAchievementCondition InChild)
{
	FAchievementCondition C;
	C.Kind = EAchievementConditionKind::Not;
	C.Children.Add(MoveTemp(InChild));
	return C;
}

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------

bool FAchievementStatValues::operator==(const FAchievementStatValues& Other) const
{
	return AchievementsDetail::NumbersEqual(Numbers, Other.Numbers)
		&& AchievementsDetail::SetsEqual(Sets, Other.Sets)
		&& Flags == Other.Flags;
}

bool FAchievementsState::operator==(const FAchievementsState& Other) const
{
	return Profile == Other.Profile && Run == Other.Run && RunMode == Other.RunMode && Unlocked == Other.Unlocked;
}

// ---------------------------------------------------------------------------
// Configuración
// ---------------------------------------------------------------------------

bool FAchievementsModel::Configure(const TArray<FAchievementStatDef>& InStats, const TArray<FAchievementDef>& InAchievements, FString& OutError)
{
	Stats.Reset();
	StatIndex.Reset();
	Achievements.Reset();
	AchievementIndex.Reset();
	Dependents.Reset();

	auto Fail = [this, &OutError](const FString& Message)
	{
		OutError = Message;
		Stats.Reset();
		StatIndex.Reset();
		Achievements.Reset();
		AchievementIndex.Reset();
		Dependents.Reset();
		return false;
	};

	for (const FAchievementStatDef& Def : InStats)
	{
		if (Def.Id.IsNone())
		{
			return Fail(TEXT("Estadística sin id"));
		}
		if (StatIndex.Contains(Def.Id))
		{
			return Fail(FString::Printf(TEXT("Estadística repetida: %s"), *Def.Id.ToString()));
		}
		StatIndex.Add(Def.Id, Stats.Add(Def));
	}

	for (const FAchievementDef& Def : InAchievements)
	{
		if (Def.Id.IsNone())
		{
			return Fail(TEXT("Logro sin id"));
		}
		if (AchievementIndex.Contains(Def.Id))
		{
			return Fail(FString::Printf(TEXT("Logro repetido: %s"), *Def.Id.ToString()));
		}
		FString ConditionError;
		if (!ValidateCondition(Def.Condition, Def.Id.ToString(), ConditionError))
		{
			return Fail(ConditionError);
		}
		const int32 Index = Achievements.Add(Def);
		AchievementIndex.Add(Def.Id, Index);
		CollectStats(Def.Condition, Index);
	}

	OutError.Reset();
	return true;
}

bool FAchievementsModel::ValidateCondition(const FAchievementCondition& Condition, const FString& Where, FString& OutError) const
{
	const FAchievementStatDef* Def = nullptr;
	switch (Condition.Kind)
	{
	case EAchievementConditionKind::Compare:
	case EAchievementConditionKind::Contains:
	case EAchievementConditionKind::Flag:
		Def = FindStat(Condition.Stat);
		if (!Def)
		{
			OutError = FString::Printf(TEXT("%s: estadística desconocida «%s»"), *Where, *Condition.Stat.ToString());
			return false;
		}
		break;
	default:
		break;
	}

	switch (Condition.Kind)
	{
	case EAchievementConditionKind::Compare:
		if (Def->Kind == EAchievementStatKind::Flag)
		{
			OutError = FString::Printf(TEXT("%s: «%s» es una marca; usa una condición de marca"), *Where, *Condition.Stat.ToString());
			return false;
		}
		if (!FMath::IsFinite(Condition.Value))
		{
			OutError = FString::Printf(TEXT("%s: valor no finito"), *Where);
			return false;
		}
		return true;
	case EAchievementConditionKind::Contains:
		if (Def->Kind != EAchievementStatKind::Set)
		{
			OutError = FString::Printf(TEXT("%s: «%s» no es un conjunto"), *Where, *Condition.Stat.ToString());
			return false;
		}
		if (Condition.Item.IsNone())
		{
			OutError = FString::Printf(TEXT("%s: falta el id que debe contener «%s»"), *Where, *Condition.Stat.ToString());
			return false;
		}
		return true;
	case EAchievementConditionKind::Flag:
		if (Def->Kind != EAchievementStatKind::Flag)
		{
			OutError = FString::Printf(TEXT("%s: «%s» no es una marca"), *Where, *Condition.Stat.ToString());
			return false;
		}
		return true;
	case EAchievementConditionKind::All:
	case EAchievementConditionKind::Any:
		if (Condition.Children.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s: composición sin condiciones"), *Where);
			return false;
		}
		break;
	case EAchievementConditionKind::Not:
		if (Condition.Children.Num() != 1)
		{
			OutError = FString::Printf(TEXT("%s: «not» necesita exactamente una condición"), *Where);
			return false;
		}
		break;
	default:
		OutError = FString::Printf(TEXT("%s: tipo de condición desconocido"), *Where);
		return false;
	}

	for (const FAchievementCondition& Child : Condition.Children)
	{
		if (!ValidateCondition(Child, Where, OutError))
		{
			return false;
		}
	}
	return true;
}

void FAchievementsModel::CollectStats(const FAchievementCondition& Condition, int32 InAchievementIndex)
{
	if (!Condition.Stat.IsNone())
	{
		Dependents.FindOrAdd(Condition.Stat).AddUnique(InAchievementIndex);
	}
	for (const FAchievementCondition& Child : Condition.Children)
	{
		CollectStats(Child, InAchievementIndex);
	}
}

void FAchievementsModel::BeginRun(FName Mode)
{
	State.Run = FAchievementStatValues();
	State.RunMode = Mode;
}

// ---------------------------------------------------------------------------
// Eventos
// ---------------------------------------------------------------------------

TArray<FName> FAchievementsModel::Report(FName Stat, double Amount, FName Item)
{
	TArray<FName> NewlyUnlocked;
	const FAchievementStatDef* Def = FindStat(Stat);
	if (!Def)
	{
		return NewlyUnlocked;
	}

	FAchievementStatValues& Values = ValuesFor(*Def);
	switch (Def->Kind)
	{
	case EAchievementStatKind::Counter:
	{
		if (!FMath::IsFinite(Amount) || Amount <= 0.0)
		{
			return NewlyUnlocked;
		}
		Values.Numbers.FindOrAdd(Stat, 0.0) += Amount;
		break;
	}
	case EAchievementStatKind::Maximum:
	{
		if (!FMath::IsFinite(Amount))
		{
			return NewlyUnlocked;
		}
		double& Current = Values.Numbers.FindOrAdd(Stat, 0.0);
		if (Amount <= Current)
		{
			return NewlyUnlocked;
		}
		Current = Amount;
		break;
	}
	case EAchievementStatKind::Set:
	{
		if (Item.IsNone())
		{
			return NewlyUnlocked;
		}
		TArray<FName>& Items = Values.Sets.FindOrAdd(Stat);
		if (Items.Contains(Item))
		{
			return NewlyUnlocked;
		}
		Items.Add(Item);
		break;
	}
	case EAchievementStatKind::Flag:
	{
		if (Values.Flags.Contains(Stat))
		{
			return NewlyUnlocked;
		}
		Values.Flags.Add(Stat);
		break;
	}
	default:
		return NewlyUnlocked;
	}

	if (const TArray<int32>* Affected = Dependents.Find(Stat))
	{
		for (const int32 Index : *Affected)
		{
			TryUnlock(Index, NewlyUnlocked);
		}
	}
	return NewlyUnlocked;
}

TArray<FName> FAchievementsModel::Evaluate()
{
	TArray<FName> NewlyUnlocked;
	for (int32 Index = 0; Index < Achievements.Num(); ++Index)
	{
		TryUnlock(Index, NewlyUnlocked);
	}
	return NewlyUnlocked;
}

void FAchievementsModel::TryUnlock(int32 InAchievementIndex, TArray<FName>& OutUnlocked)
{
	const FAchievementDef& Def = Achievements[InAchievementIndex];
	if (State.Unlocked.Contains(Def.Id) || !IsReleased(Def) || !IsAvailableInCurrentMode(Def) || !Holds(Def.Condition))
	{
		return;
	}
	State.Unlocked.Add(Def.Id);
	OutUnlocked.Add(Def.Id);
}

bool FAchievementsModel::ReachesPlayer(EAchievementCoopScope Scope, bool bIsActor, float DistanceMeters)
{
	switch (Scope)
	{
	case EAchievementCoopScope::Actor:
		return bIsActor;
	case EAchievementCoopScope::World:
		return true;
	case EAchievementCoopScope::Witness:
		// Quien hace la acción siempre la presencia; el resto, dentro del radio. Una distancia
		// corrupta (NaN, infinita o negativa) nunca cuenta como testigo.
		return bIsActor || (FMath::IsFinite(DistanceMeters) && DistanceMeters >= 0.0f && DistanceMeters <= WitnessRadiusMeters);
	default:
		return false;
	}
}

bool FAchievementsModel::IsAvailableInCurrentMode(const FAchievementDef& Def) const
{
	if (Def.Modes.IsEmpty())
	{
		return true;
	}
	return !State.RunMode.IsNone() && Def.Modes.Contains(State.RunMode);
}

// ---------------------------------------------------------------------------
// Evaluación y progreso
// ---------------------------------------------------------------------------

bool FAchievementsModel::Holds(const FAchievementCondition& Condition) const
{
	switch (Condition.Kind)
	{
	case EAchievementConditionKind::Compare:
		return AchievementsDetail::CompareValues(MeasureOf(Condition), Condition.Op, Condition.Value);
	case EAchievementConditionKind::Contains:
		return SetContains(Condition.Stat, Condition.Item);
	case EAchievementConditionKind::Flag:
		return HasFlag(Condition.Stat);
	case EAchievementConditionKind::All:
		for (const FAchievementCondition& Child : Condition.Children)
		{
			if (!Holds(Child))
			{
				return false;
			}
		}
		return true;
	case EAchievementConditionKind::Any:
		for (const FAchievementCondition& Child : Condition.Children)
		{
			if (Holds(Child))
			{
				return true;
			}
		}
		return false;
	case EAchievementConditionKind::Not:
		return Condition.Children.Num() == 1 && !Holds(Condition.Children[0]);
	default:
		return false;
	}
}

double FAchievementsModel::MeasureOf(const FAchievementCondition& Condition) const
{
	const FAchievementStatDef* Def = FindStat(Condition.Stat);
	if (!Def)
	{
		return 0.0;
	}
	switch (Def->Kind)
	{
	case EAchievementStatKind::Set: return static_cast<double>(GetSetSize(Condition.Stat));
	case EAchievementStatKind::Flag: return HasFlag(Condition.Stat) ? 1.0 : 0.0;
	default: return GetNumber(Condition.Stat);
	}
}

/**
 * Reglas del progreso:
 * - «>=» y «>» avanzan en proporción a la meta (sin llegar a 1 hasta cumplirse);
 *   el resto de comparaciones, conjuntos y marcas valen 0 o 1.
 * - «all» es la media de sus hijas, salvo las «not», que no son avance sino
 *   restricciones: si alguna se ha roto, el progreso es 0 (en esta partida ya
 *   no se puede conseguir; es el caso de «Sin mapa»).
 * - «any» toma la hija más avanzada.
 */
float FAchievementsModel::Progress(const FAchievementCondition& Condition) const
{
	const bool bHolds = Holds(Condition);
	if (bHolds)
	{
		return 1.0f;
	}

	switch (Condition.Kind)
	{
	case EAchievementConditionKind::Compare:
	{
		const bool bRising = Condition.Op == EAchievementCompareOp::GreaterEqual || Condition.Op == EAchievementCompareOp::Greater;
		if (!bRising || Condition.Value <= 0.0)
		{
			return 0.0f;
		}
		const double Ratio = FMath::Clamp(MeasureOf(Condition) / Condition.Value, 0.0, 1.0);
		return FMath::Min(static_cast<float>(Ratio), AchievementsDetail::MaxProgressWhilePending);
	}
	case EAchievementConditionKind::All:
	{
		float Sum = 0.0f;
		int32 Count = 0;
		for (const FAchievementCondition& Child : Condition.Children)
		{
			if (Child.Kind == EAchievementConditionKind::Not)
			{
				if (!Holds(Child))
				{
					return 0.0f;
				}
				continue;
			}
			Sum += Progress(Child);
			++Count;
		}
		const float Mean = Count > 0 ? Sum / static_cast<float>(Count) : 0.0f;
		return FMath::Min(Mean, AchievementsDetail::MaxProgressWhilePending);
	}
	case EAchievementConditionKind::Any:
	{
		float Best = 0.0f;
		for (const FAchievementCondition& Child : Condition.Children)
		{
			Best = FMath::Max(Best, Progress(Child));
		}
		return FMath::Min(Best, AchievementsDetail::MaxProgressWhilePending);
	}
	default:
		return 0.0f;
	}
}

float FAchievementsModel::GetProgress(FName AchievementId) const
{
	const FAchievementDef* Def = FindAchievement(AchievementId);
	if (!Def)
	{
		return 0.0f;
	}
	if (IsUnlocked(AchievementId))
	{
		return 1.0f;
	}
	return Progress(Def->Condition);
}

// ---------------------------------------------------------------------------
// Consultas
// ---------------------------------------------------------------------------

const FAchievementStatDef* FAchievementsModel::FindStat(FName Stat) const
{
	const int32* Index = StatIndex.Find(Stat);
	return Index ? &Stats[*Index] : nullptr;
}

const FAchievementDef* FAchievementsModel::FindAchievement(FName AchievementId) const
{
	const int32* Index = AchievementIndex.Find(AchievementId);
	return Index ? &Achievements[*Index] : nullptr;
}

const FAchievementStatValues& FAchievementsModel::ValuesFor(const FAchievementStatDef& Def) const
{
	return Def.Scope == EAchievementStatScope::Run ? State.Run : State.Profile;
}

FAchievementStatValues& FAchievementsModel::ValuesFor(const FAchievementStatDef& Def)
{
	return Def.Scope == EAchievementStatScope::Run ? State.Run : State.Profile;
}

double FAchievementsModel::GetNumber(FName Stat) const
{
	const FAchievementStatDef* Def = FindStat(Stat);
	if (!Def)
	{
		return 0.0;
	}
	const double* Value = ValuesFor(*Def).Numbers.Find(Stat);
	return Value ? *Value : 0.0;
}

int32 FAchievementsModel::GetSetSize(FName Stat) const
{
	const FAchievementStatDef* Def = FindStat(Stat);
	if (!Def)
	{
		return 0;
	}
	const TArray<FName>* Items = ValuesFor(*Def).Sets.Find(Stat);
	return Items ? Items->Num() : 0;
}

bool FAchievementsModel::SetContains(FName Stat, FName Item) const
{
	const FAchievementStatDef* Def = FindStat(Stat);
	if (!Def)
	{
		return false;
	}
	const TArray<FName>* Items = ValuesFor(*Def).Sets.Find(Stat);
	return Items && Items->Contains(Item);
}

bool FAchievementsModel::HasFlag(FName Stat) const
{
	const FAchievementStatDef* Def = FindStat(Stat);
	return Def && ValuesFor(*Def).Flags.Contains(Stat);
}
