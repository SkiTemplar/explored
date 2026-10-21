#include "Cooking/FireActionsModel.h"

FName FFireActionsModel::Collect() { return FName(TEXT("recoger")); }
FName FFireActionsModel::Light() { return FName(TEXT("encender")); }
FName FFireActionsModel::Cook() { return FName(TEXT("cocinar")); }
FName FFireActionsModel::Boil() { return FName(TEXT("hervir")); }
FName FFireActionsModel::Feed() { return FName(TEXT("avivar")); }
FName FFireActionsModel::FeedOil() { return FName(TEXT("echar_aceite")); }
FName FFireActionsModel::WoodItem() { return FName(TEXT("lena")); }
FName FFireActionsModel::OilItem() { return FName(TEXT("tarro_aceite")); }

int32 FFireActionsModel::SpareOilJars(int32 JarsInPack, bool bOilSlotFilled, int32 RequiredJars)
{
	const int32 Needed = bOilSlotFilled ? 0 : FMath::Max(RequiredJars, 0);
	return FMath::Max(JarsInPack - Needed, 0);
}

FName FFireActionsModel::CampfireRecipe() { return FName(TEXT("hoguera")); }
FName FFireActionsModel::GrillRecipe() { return FName(TEXT("pez_asado")); }
FName FFireActionsModel::OilRecipe() { return FName(TEXT("tarro_aceite")); }

TArray<FName> FFireActionsModel::Available(const FCampfireState& State, const FBackpackState& Pack, const FRecipeBook& Recipes,
	const FCampfireConfig& Config, int32 SpareOilJars)
{
	TArray<FName> Actions;
	const TMap<FName, int32> NoStation;
	if (State.Job.IsDone())
	{
		Actions.Add(Collect());
	}
	if (!State.bLit)
	{
		const FRecipeDef* Recipe = Recipes.Find(CampfireRecipe());
		if (Recipe && FRecipeBook::Check(*Recipe, Pack, ECraftPlace::Ground, NoStation) == ECraftResult::Ok)
		{
			Actions.Add(Light());
		}
		return Actions;
	}
	if (!State.Job.IsActive())
	{
		const FName JobActions[] = { Cook(), Boil() };
		const FName JobRecipes[] = { GrillRecipe(), OilRecipe() };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(JobActions); ++Index)
		{
			const FRecipeDef* Recipe = Recipes.Find(JobRecipes[Index]);
			if (Recipe && FRecipeBook::Check(*Recipe, Pack, ECraftPlace::Fire, NoStation) == ECraftResult::Ok)
			{
				Actions.Add(JobActions[Index]);
			}
		}
	}
	if (Config.FuelSeconds.Contains(WoodItem()) && FBackpackModel::Count(Pack, WoodItem()) > 0)
	{
		Actions.Add(Feed());
	}
	if (Config.FuelSeconds.Contains(OilItem()) && SpareOilJars > 0 && FBackpackModel::Count(Pack, OilItem()) > 0)
	{
		Actions.Add(FeedOil());
	}
	return Actions;
}
