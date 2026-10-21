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
	/** Echar un tarro de aceite sobrante (GDD v3 §5.2: el tercer tarro arde 6 min en el refugio). */
	static FName FeedOil();
	static FName WoodItem();
	static FName OilItem();

	static FName CampfireRecipe();
	static FName GrillRecipe();
	static FName OilRecipe();

	/**
	 * Acciones válidas, en orden de preferencia (el de la tecla de interactuar sin elección). «Avivar»
	 * solo gasta leña; el aceite solo se echa con su verbo propio y solo los SpareOilJars tarros que la
	 * linterna ya no necesita, para no quemar por descuido uno del camino crítico.
	 */
	static TArray<FName> Available(const FCampfireState& State, const FBackpackState& Pack, const FRecipeBook& Recipes,
		const FCampfireConfig& Config, int32 SpareOilJars = 0);

	/** Tarros sobrantes: los de la mochila menos los que aún pide la linterna (0 si su ranura está llena). */
	static int32 SpareOilJars(int32 JarsInPack, bool bOilSlotFilled, int32 RequiredJars);
};
