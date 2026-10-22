#pragma once

#include "CoreMinimal.h"

#include "Cooking/CampfireModel.h"
#include "Crafting/RecipeBookModel.h"
#include "Fishing/CastFishingModel.h"
#include "Items/BackpackModel.h"
#include "Items/IslandConsumablesModel.h"
#include "Survival/IslandBodyModel.h"
#include "Weather/StormClockModel.h"

/**
 * Datos de Baliza Siete leídos de Content/Data en tiempo de ejecución (GDD v3 §5): items.json
 * (pilas y nombres), recipes.json + verbs.json, fuels.json, survival_needs.json (consumibles y
 * cuerpo), fish.json y storm_curve.json (barómetro, chubasco y ciclón). Se
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
	/** Barras, desgaste, desmayo y sueño (survival_needs.json). */
	FIslandBodyTuning Body;
	/** Curva del barómetro y ventanas del tiempo (storm_curve.json). */
	FStormClockConfig Storm;
	/** Nombres bilingües de los objetos (id → es/en). */
	TMap<FName, TPair<FString, FString>> ItemNames;
	/** Descripción breve del inventario (descEs, descEn). */
	TMap<FName, TPair<FString, FString>> ItemDescriptions;

	/** Valores por defecto de los modelos (sin leer ficheros). */
	static FIslandData Defaults();

	/** Rellena desde el texto de cada fichero; los que estén vacíos se quedan por defecto. Devuelve los errores. */
	static TArray<FString> ParseInto(FIslandData& InOut, const FString& ItemsJson, const FString& RecipesJson, const FString& VerbsJson,
		const FString& FuelsJson, const FString& NeedsJson, const FString& FishJson);

	/** storm_curve.json. Una curva no válida deja la del GDD y lo apunta en los errores. */
	static TArray<FString> ParseStorm(FStormClockConfig& InOut, const FString& StormJson);

	/**
	 * Lee los 7 ficheros de DataDir y los aplica sobre InOut. Un fichero que falta o no se puede leer
	 * da el error «<fichero>: no se puede leer» y deja sus valores por defecto. Devuelve los errores.
	 */
	static TArray<FString> LoadDirectory(FIslandData& InOut, const FString& DataDir);
};

namespace ExploredIslandData
{
	/** Datos cacheados (se leen al primer uso). */
	EXPLORED_API const FIslandData& Get();

	/** Vuelve a leer los ficheros (tests y recarga en caliente). */
	EXPLORED_API void Reload();

	/** Nombre del objeto en el idioma actual (el id si no está en items.json). */
	EXPLORED_API FText ItemName(FName ItemId);
	/** Descripción breve del objeto en el idioma activo; vacía si no la tiene. */
	EXPLORED_API FText ItemDescription(FName ItemId);
}
