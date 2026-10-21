#include "Items/IslandConsumablesModel.h"

const FConsumableEffect* FIslandConsumables::Find(FName Item) const
{
	return Effects.FindByPredicate([Item](const FConsumableEffect& Effect) { return Effect.Item == Item; });
}

FIslandConsumables FIslandConsumables::Default()
{
	FIslandConsumables Table;
	FConsumableEffect Coconut;
	Coconut.Item = TEXT("coco");
	Coconut.Thirst = CoconutThirst;
	Coconut.Hunger = CoconutHunger;
	Coconut.Leaves = TEXT("coco_vacio");
	Table.Effects.Add(Coconut);

	FConsumableEffect Canteen;
	Canteen.Item = FBackpackModel::CanteenId();
	Canteen.Thirst = CanteenSipThirst;
	Canteen.bSips = true;
	Table.Effects.Add(Canteen);

	FConsumableEffect RawFish;
	RawFish.Item = TEXT("pez_crudo");
	RawFish.Hunger = RawFishHunger;
	RawFish.Energy = RawFishEnergy;
	Table.Effects.Add(RawFish);

	FConsumableEffect GrilledFish;
	GrilledFish.Item = TEXT("pez_asado");
	GrilledFish.Hunger = GrilledFishHunger;
	Table.Effects.Add(GrilledFish);
	return Table;
}

FConsumeOutcome FIslandConsumables::Consume(const FIslandConsumables& Table, const FBackpackState& In, const FBackpackRules& Rules,
	FName Item, FBackpackState& Out)
{
	FConsumeOutcome Outcome;
	Out = In;
	const FConsumableEffect* Effect = Table.Find(Item);
	if (!Effect)
	{
		Outcome.Result = EConsumeResult::NotConsumable;
		return Outcome;
	}
	Outcome.Effect = *Effect;
	if (FBackpackModel::Count(In, Item) <= 0)
	{
		Outcome.Result = EConsumeResult::Missing;
		return Outcome;
	}
	if (Effect->bSips)
	{
		if (In.CanteenSips <= 0)
		{
			Outcome.Result = EConsumeResult::Empty;
			return Outcome;
		}
		Out.CanteenSips = In.CanteenSips - 1;
		Outcome.Result = EConsumeResult::Ok;
		return Outcome;
	}
	FBackpackState Eaten;
	FBackpackModel::Remove(In, Item, 1, Eaten);
	if (!Effect->Leaves.IsNone() && FBackpackModel::Add(Eaten, Rules, Effect->Leaves, 1, Eaten) != EBackpackResult::Ok)
	{
		Outcome.Dropped = Effect->Leaves;
	}
	Out = MoveTemp(Eaten);
	Outcome.Result = EConsumeResult::Ok;
	return Outcome;
}
