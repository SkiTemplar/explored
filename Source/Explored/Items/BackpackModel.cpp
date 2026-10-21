#include "Items/BackpackModel.h"

const TCHAR* LexToString(EBackpackResult Result)
{
	switch (Result)
	{
	case EBackpackResult::Ok: return TEXT("Ok");
	case EBackpackResult::Full: return TEXT("Full");
	case EBackpackResult::HandsBusy: return TEXT("HandsBusy");
	case EBackpackResult::UnknownItem: return TEXT("UnknownItem");
	case EBackpackResult::NotEnough: return TEXT("NotEnough");
	case EBackpackResult::InvalidCount: return TEXT("InvalidCount");
	default: return TEXT("?");
	}
}

FBackpackRules FBackpackRules::Default()
{
	struct FRow
	{
		const TCHAR* Id;
		int32 Stack;
		bool bHeavy;
	};
	static const FRow Rows[] = {
		{ TEXT("cuchillo"), 1, false }, { TEXT("cantimplora"), 1, false }, { TEXT("barometro"), 1, false },
		{ TEXT("coco"), 4, false }, { TEXT("coco_vacio"), 4, false }, { TEXT("lena"), 6, false },
		{ TEXT("fibra"), 6, false }, { TEXT("cuerda"), 4, false }, { TEXT("cana_bambu"), 2, false },
		{ TEXT("cana_pescar"), 1, false }, { TEXT("tronco"), 1, true }, { TEXT("remo"), 1, false },
		{ TEXT("pez_crudo"), 3, false }, { TEXT("pez_asado"), 3, false }, { TEXT("tarro_aceite"), 2, false },
		{ TEXT("mecha"), 1, false }, { TEXT("lente"), 1, true },
	};
	FBackpackRules Rules;
	for (const FRow& Row : Rows)
	{
		FBackpackItemRule Rule;
		Rule.Id = FName(Row.Id);
		Rule.Stack = Row.Stack;
		Rule.bHeavy = Row.bHeavy;
		Rules.Items.Add(Rule.Id, Rule);
	}
	return Rules;
}

FName FBackpackModel::CanteenId()
{
	static const FName Id(TEXT("cantimplora"));
	return Id;
}

int32 FBackpackModel::Count(const FBackpackState& State, FName Id)
{
	int32 Total = (!Id.IsNone() && State.HeldHeavy == Id) ? 1 : 0;
	for (const FBackpackStack& Stack : State.Stacks)
	{
		if (Stack.Id == Id)
		{
			Total += Stack.Count;
		}
	}
	return Total;
}

int32 FBackpackModel::UsedSlots(const FBackpackState& State)
{
	return State.Stacks.Num();
}

TMap<FName, int32> FBackpackModel::ToCounts(const FBackpackState& State)
{
	TMap<FName, int32> Counts;
	for (const FBackpackStack& Stack : State.Stacks)
	{
		Counts.FindOrAdd(Stack.Id) += Stack.Count;
	}
	if (!State.HeldHeavy.IsNone())
	{
		Counts.FindOrAdd(State.HeldHeavy) += 1;
	}
	return Counts;
}

namespace BackpackModelDetail
{
	/** Añade sobre una copia ya hecha (Out); no deja nada a medias si falla porque Out se descarta. */
	EBackpackResult AddInPlace(FBackpackState& Out, const FBackpackRules& Rules, FName Id, int32 Count)
	{
		if (Count <= 0)
		{
			return EBackpackResult::InvalidCount;
		}
		const FBackpackItemRule* Rule = Rules.Find(Id);
		if (!Rule)
		{
			return EBackpackResult::UnknownItem;
		}
		if (Rule->bHeavy)
		{
			if (!Out.HeldHeavy.IsNone() || Count > 1)
			{
				return EBackpackResult::HandsBusy;
			}
			Out.HeldHeavy = Id;
			return EBackpackResult::Ok;
		}
		const int32 StackSize = FMath::Max(1, Rule->Stack);
		int32 Left = Count;
		for (FBackpackStack& Stack : Out.Stacks)
		{
			if (Stack.Id == Id && Stack.Count < StackSize)
			{
				const int32 Moved = FMath::Min(Left, StackSize - Stack.Count);
				Stack.Count += Moved;
				Left -= Moved;
				if (Left == 0)
				{
					return EBackpackResult::Ok;
				}
			}
		}
		while (Left > 0)
		{
			if (Out.Stacks.Num() >= Rules.Slots)
			{
				return EBackpackResult::Full;
			}
			const int32 Moved = FMath::Min(Left, StackSize);
			Out.Stacks.Add({ Id, Moved });
			Left -= Moved;
		}
		return EBackpackResult::Ok;
	}

	EBackpackResult RemoveInPlace(FBackpackState& Out, FName Id, int32 Count)
	{
		if (Count <= 0)
		{
			return EBackpackResult::InvalidCount;
		}
		if (FBackpackModel::Count(Out, Id) < Count)
		{
			return EBackpackResult::NotEnough;
		}
		int32 Left = Count;
		if (Out.HeldHeavy == Id)
		{
			Out.HeldHeavy = NAME_None;
			--Left;
		}
		// Se vacían primero las últimas pilas para que el orden de la mochila no salte.
		for (int32 Index = Out.Stacks.Num() - 1; Index >= 0 && Left > 0; --Index)
		{
			FBackpackStack& Stack = Out.Stacks[Index];
			if (Stack.Id != Id)
			{
				continue;
			}
			const int32 Taken = FMath::Min(Left, Stack.Count);
			Stack.Count -= Taken;
			Left -= Taken;
			if (Stack.Count == 0)
			{
				Out.Stacks.RemoveAt(Index);
			}
		}
		if (Id == FBackpackModel::CanteenId() && FBackpackModel::Count(Out, Id) == 0)
		{
			Out.CanteenSips = 0;
		}
		return EBackpackResult::Ok;
	}
}

EBackpackResult FBackpackModel::CanAdd(const FBackpackState& State, const FBackpackRules& Rules, FName Id, int32 Count)
{
	FBackpackState Scratch = State;
	return BackpackModelDetail::AddInPlace(Scratch, Rules, Id, Count);
}

EBackpackResult FBackpackModel::Add(const FBackpackState& In, const FBackpackRules& Rules, FName Id, int32 Count, FBackpackState& Out)
{
	FBackpackState Next = In;
	const EBackpackResult Result = BackpackModelDetail::AddInPlace(Next, Rules, Id, Count);
	Out = Result == EBackpackResult::Ok ? MoveTemp(Next) : In;
	return Result;
}

EBackpackResult FBackpackModel::Remove(const FBackpackState& In, FName Id, int32 Count, FBackpackState& Out)
{
	FBackpackState Next = In;
	const EBackpackResult Result = BackpackModelDetail::RemoveInPlace(Next, Id, Count);
	Out = Result == EBackpackResult::Ok ? MoveTemp(Next) : In;
	return Result;
}

EBackpackResult FBackpackModel::Exchange(const FBackpackState& In, const FBackpackRules& Rules,
	const TMap<FName, int32>& ToRemove, const TMap<FName, int32>& ToAdd, FBackpackState& Out)
{
	FBackpackState Next = In;
	for (const TPair<FName, int32>& Pair : ToRemove)
	{
		const EBackpackResult Result = BackpackModelDetail::RemoveInPlace(Next, Pair.Key, Pair.Value);
		if (Result != EBackpackResult::Ok)
		{
			Out = In;
			return Result;
		}
	}
	for (const TPair<FName, int32>& Pair : ToAdd)
	{
		const EBackpackResult Result = BackpackModelDetail::AddInPlace(Next, Rules, Pair.Key, Pair.Value);
		if (Result != EBackpackResult::Ok)
		{
			Out = In;
			return Result;
		}
	}
	Out = MoveTemp(Next);
	return EBackpackResult::Ok;
}

FBackpackState FBackpackModel::StarterKit(const FBackpackRules& Rules)
{
	FBackpackState Kit;
	for (const TCHAR* Id : { TEXT("cuchillo"), TEXT("cantimplora"), TEXT("barometro") })
	{
		BackpackModelDetail::AddInPlace(Kit, Rules, FName(Id), 1);
	}
	Kit.CanteenSips = Count(Kit, CanteenId()) > 0 ? StarterCanteenSips : 0;
	return Kit;
}

bool FBackpackModel::IsValid(const FBackpackState& State, const FBackpackRules& Rules)
{
	if (State.Stacks.Num() > Rules.Slots || State.CanteenSips < 0 || State.CanteenSips > CanteenMaxSips)
	{
		return false;
	}
	if (!State.HeldHeavy.IsNone())
	{
		const FBackpackItemRule* Rule = Rules.Find(State.HeldHeavy);
		if (!Rule || !Rule->bHeavy)
		{
			return false;
		}
	}
	for (const FBackpackStack& Stack : State.Stacks)
	{
		const FBackpackItemRule* Rule = Rules.Find(Stack.Id);
		if (!Rule || Rule->bHeavy || Stack.Count <= 0 || Stack.Count > FMath::Max(1, Rule->Stack))
		{
			return false;
		}
	}
	return true;
}
