#include "UI/TutorialHintModel.h"

namespace TutorialHintDetail
{
	uint8 Bit(ETutorialHint Hint)
	{
		return static_cast<uint8>(1u << static_cast<uint32>(Hint));
	}

	FTutorialHintResult Rejected(const FTutorialHintState& State)
	{
		FTutorialHintResult Result;
		Result.State = State;
		return Result;
	}
}

bool FTutorialHintModel::IsValid(ETutorialHint Hint)
{
	return static_cast<uint8>(Hint) < static_cast<uint8>(ETutorialHint::Count);
}

bool FTutorialHintModel::WasShown(const FTutorialHintState& State, ETutorialHint Hint)
{
	return IsValid(Hint) && (State.ShownMask & TutorialHintDetail::Bit(Hint)) != 0;
}

bool FTutorialHintModel::IsQueued(const FTutorialHintState& State, ETutorialHint Hint)
{
	return State.Queue.Contains(Hint);
}

int32 FTutorialHintModel::ShownCount(const FTutorialHintState& State)
{
	int32 Count = 0;
	for (uint32 Bits = State.ShownMask; Bits != 0; Bits &= Bits - 1)
	{
		++Count;
	}
	return Count;
}

bool FTutorialHintModel::AllShown(const FTutorialHintState& State)
{
	return ShownCount(State) == HintCount;
}

FTutorialHintResult FTutorialHintModel::Request(const FTutorialHintState& State, ETutorialHint Hint)
{
	if (!IsValid(Hint) || WasShown(State, Hint) || IsQueued(State, Hint))
	{
		return TutorialHintDetail::Rejected(State);
	}
	FTutorialHintResult Result;
	Result.bOk = true;
	Result.State = State;
	Result.State.Queue.Add(Hint);
	Result.Hint = Hint;
	return Result;
}

FTutorialHintResult FTutorialHintModel::PopNext(const FTutorialHintState& State)
{
	if (State.Queue.IsEmpty())
	{
		return TutorialHintDetail::Rejected(State);
	}
	FTutorialHintResult Result;
	Result.bOk = true;
	Result.Hint = State.Queue[0];
	Result.State = State;
	Result.State.Queue.RemoveAt(0);
	Result.State.ShownMask = static_cast<uint8>(State.ShownMask | TutorialHintDetail::Bit(Result.Hint));
	return Result;
}

FName FTutorialHintModel::TextId(ETutorialHint Hint)
{
	switch (Hint)
	{
	case ETutorialHint::Move:
		return FName(TEXT("tut_mover"));
	case ETutorialHint::Pickup:
		return FName(TEXT("tut_coger"));
	case ETutorialHint::Drink:
		return FName(TEXT("tut_beber"));
	case ETutorialHint::Craft:
		return FName(TEXT("tut_craftear"));
	case ETutorialHint::Sleep:
		return FName(TEXT("tut_dormir"));
	case ETutorialHint::Notebook:
		return FName(TEXT("tut_cuaderno"));
	case ETutorialHint::Barometer:
		return FName(TEXT("tut_barometro"));
	case ETutorialHint::Paddle:
		return FName(TEXT("tut_remar"));
	default:
		return FName(NAME_None);
	}
}

FTutorialHintRecord FTutorialHintModel::ToRecord(const FTutorialHintState& State)
{
	FTutorialHintRecord Record;
	Record.ShownMask = State.ShownMask;
	for (const ETutorialHint Hint : State.Queue)
	{
		Record.Queue.Add(static_cast<uint8>(Hint));
	}
	return Record;
}

FTutorialHintResult FTutorialHintModel::FromRecord(const FTutorialHintRecord& Record)
{
	const FTutorialHintState Empty;
	FTutorialHintState State;
	State.ShownMask = Record.ShownMask;
	if (Record.Queue.Num() > HintCount)
	{
		return TutorialHintDetail::Rejected(Empty);
	}
	for (const uint8 Raw : Record.Queue)
	{
		const ETutorialHint Hint = static_cast<ETutorialHint>(Raw);
		if (Raw >= static_cast<uint8>(ETutorialHint::Count) || WasShown(State, Hint) || IsQueued(State, Hint))
		{
			return TutorialHintDetail::Rejected(Empty);
		}
		State.Queue.Add(Hint);
	}
	FTutorialHintResult Result;
	Result.bOk = true;
	Result.State = State;
	return Result;
}
