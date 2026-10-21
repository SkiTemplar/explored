#pragma once

#include "CoreMinimal.h"
#include "Cooking/CampfireModel.h"
#include "Crafting/RecipeBookModel.h"
#include "Items/BackpackModel.h"

/**
 * Qué se puede hacer ahora en una hoguera de Baliza Siete (GDD v3 §5.2 y §5.6), sin actor:
 * «recoger» lo cocinado, «encender» (receta hoguera), «cocinar» un pez, «hervir» aceite y «avivar»
 * con combustible. AExploredFire guarda el verbo elegido (FVerbChoiceModel) y ejecuta el resultado.
 */
struct EXPLORED_API FFireActionsModel
{
	static FName Collect();
	static FName Light();
	static FName Cook();
	static FName Boil();
	static FName Feed();

	static FName CampfireRecipe();
	static FName GrillRecipe();
	static FName OilRecipe();

	/** Acciones válidas, en orden de preferencia (el de la tecla de interactuar sin elección). */
	static TArray<FName> Available(const FCampfireState& State, const FBackpackState& Pack, const FRecipeBook& Recipes,
		const FCampfireConfig& Config);
};
