// Mochila de Baliza Siete desde los controles del jugador (GDD v3 §5.3, §5.6 y §9.2): abrir el
// inventario, el clic de la ranura rápida y los botones Usar / Soltar / receta de SExploredInventoryPanel.
#include "Player/ExploredCharacter.h"

#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"
#include "UI/BackpackMenuModel.h"
#include "UI/ExploredPlayerController.h"

void AExploredCharacter::SetBackpackOpen(bool bOpen)
{
	if (bBackpackOpen == bOpen)
	{
		return;
	}
	bBackpackOpen = bOpen;
	if (AExploredPlayerController* PC = Cast<AExploredPlayerController>(GetController()))
	{
		PC->ApplyBackpackInputMode(bBackpackOpen);
	}
	OnBackpackToggled.Broadcast(bBackpackOpen);
}

bool AExploredCharacter::UseSelectedQuickSlot()
{
	if (!Survivor || Survivor->IsBusy())
	{
		return false;
	}
	const FName Item = FBackpackMenuModel::QuickUseItem(Survivor->GetBackpack(), Survivor->GetSelectedQuickSlot(),
		ExploredIslandData::Get().Consumables);
	return !Item.IsNone() && Survivor->ConsumeItem(Item);
}

bool AExploredCharacter::UseBackpackItem(FName ItemId)
{
	if (!Survivor || Survivor->IsBusy())
	{
		return false;
	}
	// La cantimplora vacía pasa: ConsumeItem avisa «La cantimplora está vacía».
	const bool bConsumable = ExploredIslandData::Get().Consumables.Find(ItemId) != nullptr;
	return bConsumable && Survivor->GetItemCount(ItemId) > 0 && Survivor->ConsumeItem(ItemId);
}

bool AExploredCharacter::DropBackpackItem(FName ItemId)
{
	if (!Survivor || Survivor->IsBusy() || !FBackpackMenuModel::CanDrop(Survivor->GetBackpack(), ItemId))
	{
		return false;
	}
	return Survivor->DropItem(ItemId);
}

bool AExploredCharacter::CraftFromBackpack(FName RecipeId)
{
	if (!Survivor || Survivor->IsBusy())
	{
		return false;
	}
	const FRecipeBook& Book = ExploredIslandData::Get().Recipes;
	if (!FBackpackMenuModel::CanCraft(Book, Survivor->GetBackpack(), RecipeId))
	{
		// Que el jugador sepa qué falta: Craft compara y avisa con el motivo.
		if (Book.Find(RecipeId))
		{
			Survivor->Craft(RecipeId);
		}
		return false;
	}
	return Survivor->Craft(RecipeId);
}
