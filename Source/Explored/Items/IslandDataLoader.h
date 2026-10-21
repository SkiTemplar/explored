#pragma once

#include "CoreMinimal.h"

#include "Cooking/CampfireModel.h"
#include "Crafting/RecipeBookModel.h"
#include "Fishing/CastFishingModel.h"
#include "Items/BackpackModel.h"
#include "Items/IslandConsumablesModel.h"

/**
 * Datos de Baliza Siete leídos de Content/Data en tiempo de ejecución (GDD v3 §5): items.json
 * (pilas y nombres), recipes.json + verbs.json, fuels.json, survival_needs.json y fish.json. Se
 * leen una vez y se cachean; si un fichero falta o está mal, se avisa en el log y se usan los
 * valores por defecto de los modelos puros (los mismos que Tools/DataCheck compara con el JSON).
 */
struct EXPLORED_API FIslandData
{
	FBackpackRules Backpack;
	FRecipeBook Recipes;
	FIslandConsumables Consumables;
	FCampfireConfig Campfire;
	FCastFishingConfig Fishing;
	/** Nombres bilingües de los objetos (id → es/en). */
	TMap<FName, TPair<FString, FString>> ItemNames;

	/** Valores por defecto de los modelos (sin leer ficheros). */
	static FIslandData Defaults();

	/** Rellena desde el texto de cada fichero; los que estén vacíos se quedan por defecto. Devuelve los errores. */
	static TArray<FString> ParseInto(FIslandData& InOut, const FString& ItemsJson, const FString& RecipesJson, const FString& VerbsJson,
		const FString& FuelsJson, const FString& NeedsJson, const FString& FishJson);
};

namespace ExploredIslandData
{
	/** Datos cacheados (se leen al primer uso). */
	EXPLORED_API const FIslandData& Get();

	/** Vuelve a leer los ficheros (tests y recarga en caliente). */
	EXPLORED_API void Reload();

	/** Nombre del objeto en el idioma actual (el id si no está en items.json). */
	EXPLORED_API FText ItemName(FName ItemId);
}
