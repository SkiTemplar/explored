#pragma once

#include "CoreMinimal.h"
#include "Crafting/RecipeBookModel.h"
#include "Items/BackpackModel.h"
#include "Items/IslandConsumablesModel.h"

/**
 * Inventario de Baliza Siete (Tab, GDD v3 §9.2) y uso de la ranura rápida sin Slate: qué se puede
 * comer o beber, qué se puede soltar y qué recetas de mano (las que no piden un objeto del mundo)
 * se pueden hacer ya. SExploredInventoryPanel solo pinta y AExploredCharacter valida con esto antes
 * de llamar a UExploredSurvivorComponent.
 */

struct EXPLORED_API FBackpackMenuEntry
{
	FName ItemId;
	int32 Count = 0;
	/** Sorbos de la cantimplora; INDEX_NONE en el resto. */
	int32 Sips = INDEX_NONE;
	/** Se come o se bebe ahora (la cantimplora vacía, no). */
	bool bCanUse = false;
	/** Se puede soltar (el kit inicial no). */
	bool bCanDrop = false;
};

struct EXPLORED_API FBackpackMenuRecipe
{
	FName RecipeId;
	FName Verb;
	ECraftResult Check = ECraftResult::NotValid;

	bool CanCraft() const { return Check == ECraftResult::Ok; }
};

struct EXPLORED_API FBackpackMenuModel
{
	/** Una fila por pila, en el orden de la mochila. La carga pesada no va aquí (va en las manos). */
	static TArray<FBackpackMenuEntry> Entries(const FBackpackState& Pack, const FIslandConsumables& Consumables);

	/** Recetas de mano (lugar «cualquiera» o «suelo»), en el orden de recipes.json, con su comprobación. */
	static TArray<FBackpackMenuRecipe> HandRecipes(const FRecipeBook& Book, const FBackpackState& Pack);

	static bool CanUse(const FBackpackState& Pack, const FIslandConsumables& Consumables, FName ItemId);
	static bool CanDrop(const FBackpackState& Pack, FName ItemId);
	/** ¿Es receta de mano y se puede hacer ya con la mochila? */
	static bool CanCraft(const FRecipeBook& Book, const FBackpackState& Pack, FName RecipeId);

	/**
	 * Lo que consume el clic con la ranura rápida Selected (0-2): el objeto si se come o se bebe,
	 * NAME_None si no (manos ocupadas con carga pesada, ranura vacía o un objeto que no se consume).
	 * La cantimplora vacía sí se devuelve, para que el componente avise «está vacía».
	 */
	static FName QuickUseItem(const FBackpackState& Pack, int32 Selected, const FIslandConsumables& Consumables);
};
