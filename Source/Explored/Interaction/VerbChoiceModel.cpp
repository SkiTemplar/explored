#include "Interaction/VerbChoiceModel.h"

FName FVerbChoiceModel::Resolve(const TArray<FName>& Available, FName Selected)
{
	if (Available.Num() == 0)
	{
		return NAME_None;
	}
	return !Selected.IsNone() && Available.Contains(Selected) ? Selected : Available[0];
}

FName FVerbChoiceModel::Next(const TArray<FName>& Available, FName Current)
{
	if (Available.Num() == 0)
	{
		return NAME_None;
	}
	const int32 Index = Available.IndexOfByKey(Current);
	return Index == INDEX_NONE ? Available[0] : Available[(Index + 1) % Available.Num()];
}

TArray<FName> FVerbChoiceModel::Ordered(const TArray<FName>& Available, FName Selected)
{
	TArray<FName> Out;
	const int32 Start = Available.IndexOfByKey(Resolve(Available, Selected));
	if (Start == INDEX_NONE)
	{
		return Out;
	}
	const int32 Total = FMath::Min(Available.Num(), MaxVerbs);
	for (int32 Offset = 0; Offset < Total; ++Offset)
	{
		Out.Add(Available[(Start + Offset) % Available.Num()]);
	}
	return Out;
}
