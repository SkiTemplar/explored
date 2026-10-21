#include "UI/BackpackMenuModel.h"

#include "UI/IslandHudModel.h"

namespace BackpackMenuDetail
{
	bool IsHandRecipe(const FRecipeDef& Recipe)
	{
		return Recipe.Place == ECraftPlace::Anywhere || Recipe.Place == ECraftPlace::Ground;
	}
}

TArray<FBackpackMenuEntry> FBackpackMenuModel::Entries(const FBackpackState& Pack, const FIslandConsumables& Consumables)
{
	TArray<FBackpackMenuEntry> Out;
	for (const FBackpackStack& Stack : Pack.Stacks)
	{
		if (Stack.Id.IsNone() || Stack.Count <= 0)
		{
			continue;
		}
		FBackpackMenuEntry Entry;
		Entry.ItemId = Stack.Id;
		Entry.Count = Stack.Count;
		Entry.Sips = Stack.Id == FBackpackModel::CanteenId() ? Pack.CanteenSips : INDEX_NONE;
		Entry.bCanUse = CanUse(Pack, Consumables, Stack.Id);
		Entry.bCanDrop = CanDrop(Pack, Stack.Id);
		Out.Add(Entry);
	}
	return Out;
}

TArray<FBackpackMenuRecipe> FBackpackMenuModel::HandRecipes(const FRecipeBook& Book, const FBackpackState& Pack)
{
	TArray<FBackpackMenuRecipe> Out;
	for (const FRecipeDef& Recipe : Book.Recipes)
	{
		if (!BackpackMenuDetail::IsHandRecipe(Recipe))
		{
			continue;
		}
		FBackpackMenuRecipe Row;
		Row.RecipeId = Recipe.Id;
		Row.Verb = Recipe.Verb;
		// El mismo lugar que usa UExploredSurvivorComponent::Craft.
		Row.Check = FRecipeBook::Check(Recipe, Pack, ECraftPlace::Anywhere, TMap<FName, int32>());
		Out.Add(Row);
	}
	return Out;
}

bool FBackpackMenuModel::CanUse(const FBackpackState& Pack, const FIslandConsumables& Consumables, FName ItemId)
{
	const FConsumableEffect* Effect = Consumables.Find(ItemId);
	if (!Effect || FBackpackModel::Count(Pack, ItemId) <= 0)
	{
		return false;
	}
	return !Effect->bSips || Pack.CanteenSips > 0;
}

bool FBackpackMenuModel::CanDrop(const FBackpackState& Pack, FName ItemId)
{
	return !ItemId.IsNone() && !FBackpackModel::IsStarterKit(ItemId) && FBackpackModel::Count(Pack, ItemId) > 0
		&& Pack.HeldHeavy != ItemId;
}

bool FBackpackMenuModel::CanCraft(const FRecipeBook& Book, const FBackpackState& Pack, FName RecipeId)
{
	const FRecipeDef* Recipe = Book.Find(RecipeId);
	return Recipe && BackpackMenuDetail::IsHandRecipe(*Recipe)
		&& FRecipeBook::Check(*Recipe, Pack, ECraftPlace::Anywhere, TMap<FName, int32>()) == ECraftResult::Ok;
}

FName FBackpackMenuModel::QuickUseItem(const FBackpackState& Pack, int32 Selected, const FIslandConsumables& Consumables)
{
	if (!Pack.HeldHeavy.IsNone())
	{
		return NAME_None;
	}
	const FName Held = FIslandHudModel::HeldItem(Pack, Selected);
	return !Held.IsNone() && Consumables.Find(Held) ? Held : NAME_None;
}
