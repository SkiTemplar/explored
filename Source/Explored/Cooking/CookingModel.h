#pragma once

#include "CoreMinimal.h"

#include "Cooking/FireModel.h"
#include "Survival/SurvivalModel.h"

/** Técnicas de cocina y conservación (GDD §8.8; hornear es la cerámica y el carbón del horno, GDD §8.5). */
enum class ECookTechnique : uint8
{
	Roast,   // Asar en espeto o sobre las brasas.
	Boil,    // Hervir en recipiente estanco.
	Stew,    // Guisar en vasija.
	Smoke,   // Ahumar en el secadero junto al humo.
	Salt,    // Salar (sin fuego).
	Dry,     // Secar al sol o al aire (sin fuego).
	Bake,    // Hornear: solo en el horno de arcilla.
	Count
};

/** Estado de conservación de una comida (GDD §8.8: crudo < cocinado < ahumado o salado). */
enum class EFoodState : uint8
{
	Raw,
	Cooked,
	Smoked,
	Salted,
	Dried,
	Count
};

/** Frescura percibida. */
enum class EFreshness : uint8
{
	Fresh,
	Stale,     // Pasado: alimenta algo menos.
	Spoiled,   // Estropeado: puede intoxicar.
};

/** Estado de una cocción. */
enum class EPotStatus : uint8
{
	Empty,
	Cooking,
	Done,
	Burnt,
};

EXPLORED_API const TCHAR* LexToString(ECookTechnique Technique);
EXPLORED_API const TCHAR* LexToString(EFoodState State);

/** Técnicas que necesitan el calor del fuego. */
EXPLORED_API bool TechniqueNeedsFire(ECookTechnique Technique);

/** Un ingrediente pedido por una receta: por id exacto o por etiqueta (entonces solo crudo). */
struct EXPLORED_API FCookIngredientReq
{
	FName ItemId;
	FName Tag;
	int32 Count = 1;
};

/** Utensilio de cocina (recipes.json, «vessels»). */
struct EXPLORED_API FCookVesselDef
{
	FName Id;
	FString NameEs;
	/** Objetos de items.json que hacen de este utensilio. */
	TArray<FName> ItemIds;
	/** Pieza de building_pieces.json que hace de este utensilio (el secadero). */
	FName PieceId;
	int32 Capacity = 1;
	/** No pierde líquido: puede hervir y guisar. */
	bool bWatertight = false;
	/** Usos antes de romperse o quemarse (0 = no se gasta); la olla de coco aguanta pocos. */
	int32 MaxUses = 0;
};

/** Receta con nombre (recipes.json, «recipes»). */
struct EXPLORED_API FCookRecipeDef
{
	FName Id;
	FString NameEs;
	ECookTechnique Technique = ECookTechnique::Roast;
	/** Utensilios válidos; vacío = no hace falta ninguno. */
	TArray<FName> Vessels;
	EFireLevel MinFireLevel = EFireLevel::Fogata;
	TArray<FCookIngredientReq> Ingredients;
	FName ResultItemId;
	int32 ResultCount = 1;
	/** Minutos de juego a calor de referencia. */
	float CookMinutes = 20.0f;
	/** Minutos de más hasta quemarse; negativo = no se quema. */
	float BurnAfterMinutes = -1.0f;

	int32 NumIngredientUnits() const;
};

/** Lo que aporta una comida y cómo se conserva (recipes.json, «foods», con la nutrición de items.json). */
struct EXPLORED_API FFoodDef
{
	FName ItemId;
	EFoodState State = EFoodState::Raw;
	FName Family;
	/** Etiquetas del objeto en items.json (para casar recetas por etiqueta). */
	TArray<FName> Tags;
	FConsumable Effects;
	/** Cocinarlo quita la toxicidad (yuca, agua sin tratar); no vale para una seta venenosa. */
	bool bCookingRemovesToxicity = false;
};

/** Reglas de conservación y deterioro (recipes.json, «preservation»). */
struct EXPLORED_API FPreservationDef
{
	float StateHours[static_cast<int32>(EFoodState::Count)] = {24.0f, 72.0f, 336.0f, 480.0f, 400.0f};
	TMap<FName, float> FamilyFactor;
	/** Fracción de la vida útil a partir de la cual está pasado. */
	float StaleFraction = 0.75f;
	/** Multiplicador de lo que alimenta una comida pasada. */
	float StaleNutrition = 0.8f;
	/** Toxicidad que suma al estropearse y la que alcanza a las 2× de su vida útil. */
	float SpoiledToxicity = 0.35f;
	float RottenToxicity = 0.9f;
	float SpoiledMorale = -5.0f;
	/** Horas que dura caliente la comida recién hecha (el calor corporal que da). */
	float WarmHours = 1.0f;
};

/** Guiso improvisado: lo que sale de la vasija si no coincide ninguna receta (biblia §3.6). */
struct EXPLORED_API FImprovisedDef
{
	ECookTechnique Technique = ECookTechnique::Stew;
	FName ResultItemId;
	float CookMinutes = 45.0f;
	float BurnAfterMinutes = 30.0f;
	float Efficiency = 0.9f;
	float Warmth = 4.0f;
	float Morale = 2.0f;
};

/** Tablas de cocina. Default() se genera desde recipes.json e items.json (CookingData.inl). */
struct EXPLORED_API FCookingData
{
	TArray<FCookVesselDef> Vessels;
	TArray<FCookRecipeDef> Recipes;
	TArray<FFoodDef> Foods;
	FPreservationDef Preservation;
	FImprovisedDef Improvised;
	FName BurntItemId;

	static const FCookingData& Default();

	const FFoodDef* FindFood(FName ItemId) const;
	const FCookRecipeDef* FindRecipe(FName RecipeId) const;
	const FCookVesselDef* FindVessel(FName VesselId) const;
	/** Utensilio que representa este objeto en la mano (NAME_None si ninguno). */
	FName VesselForItem(FName ItemId) const;
};

/** Un ingrediente concreto que el jugador pone a cocinar. */
struct EXPLORED_API FCookIngredient
{
	FName ItemId;
	/** Etiquetas de items.json; si están vacías se usan las de FFoodDef. */
	TArray<FName> Tags;

	FCookIngredient() = default;
	explicit FCookIngredient(FName InItemId) : ItemId(InItemId) {}
};

/** Lo que rodea a la cocción. */
struct EXPLORED_API FCookEnvironment
{
	/** Calor del fuego (FFireState::Heat); 0 sin fuego. */
	float FireHeat = 0.0f;
	/** Humo del fuego (FFireState::Smoke). */
	float Smoke = 0.0f;
	EFireLevel FireLevel = EFireLevel::Fogata;
	float Sun = 0.0f;       // 0–1
	float Rain = 0.0f;      // 0–1
	float Wind = 0.2f;      // 0–1
	bool bSheltered = false;
};

/**
 * Una cocción en curso. Datos planos para el guardado: la receta se
 * resuelve al empezar y se guarda su id (None = guiso improvisado).
 */
struct EXPLORED_API FCookingPot
{
	EPotStatus Status = EPotStatus::Empty;
	ECookTechnique Technique = ECookTechnique::Roast;
	FName VesselId;
	FName RecipeId;
	TArray<FName> IngredientIds;
	float ProgressMinutes = 0.0f;

	bool operator==(const FCookingPot& Other) const;
	bool operator!=(const FCookingPot& Other) const { return !(*this == Other); }
};

/** Lo que sale al retirar la cocción. */
struct EXPLORED_API FCookedFood
{
	FName ItemId;
	int32 Count = 1;
	FConsumable Effects;
	EFoodState State = EFoodState::Cooked;
};

/** Frescura de una comida guardada (datos planos para el guardado). */
struct EXPLORED_API FFoodFreshness
{
	FName ItemId;
	/** Horas efectivas de deterioro (ya multiplicadas por el almacenaje). */
	float AgeHours = 0.0f;
	/** Horas reales desde que salió del fuego; negativo = no se cocinó (no da calor). */
	float HoursSinceCooked = -1.0f;

	bool operator==(const FFoodFreshness& Other) const;
};

/** Reglas de cocina, conservación y deterioro. Funciones puras. */
struct EXPLORED_API FCookingModel
{
	/** Límite de ingredientes que se intentan casar (la vasija más grande admite 4). */
	static constexpr int32 MaxIngredients = 8;
	/** Calor mínimo para que avance una técnica de fuego (las brasas de una fogata llegan). */
	static constexpr float MinCookHeat = 0.15f;
	/** Calor al que CookMinutes se cumple a ritmo 1. */
	static constexpr float ReferenceHeat = 0.8f;

	/**
	 * Receta con nombre que casa exactamente con estos ingredientes en este
	 * utensilio y nivel de fuego. Si casan varias, gana la de más ingredientes
	 * y, a igualdad, la primera del fichero (como las plantillas de fabricación).
	 */
	static const FCookRecipeDef* FindRecipe(const FCookingData& Data, ECookTechnique Technique, FName VesselId,
		const TArray<FCookIngredient>& Ingredients, EFireLevel FireLevel);

	/**
	 * Empieza una cocción. Guisar sin receta conocida da un guiso improvisado;
	 * el resto de técnicas necesitan receta. Falla si no cabe en el utensilio
	 * o si hervir/guisar no se hace en un recipiente estanco.
	 */
	static bool StartPot(FCookingPot& OutPot, const FCookingData& Data, ECookTechnique Technique, FName VesselId,
		const TArray<FCookIngredient>& Ingredients, EFireLevel FireLevel, FString& OutFailReason);

	/** Ritmo de avance (1 = minutos de receta) según la técnica y el entorno. */
	static float ProgressRate(ECookTechnique Technique, const FCookEnvironment& Env);

	/**
	 * Deja una cocción cargada de fuera (guardado) dentro de rango: estado y técnica
	 * válidos y ProgressMinutes finito y no negativo (NaN dejaba la olla atascada).
	 */
	static void SanitizePot(FCookingPot& Pot);

	/** Avanza la cocción; lo que se queda en el fuego después de hecho acaba quemándose. */
	static void TickPot(FCookingPot& Pot, const FCookingData& Data, const FCookEnvironment& Env, float DeltaMinutes);

	/** Minutos de receta (o del guiso improvisado) y de margen antes de quemarse (negativo = nunca). */
	static float CookMinutesFor(const FCookingPot& Pot, const FCookingData& Data);
	static float BurnAfterMinutesFor(const FCookingPot& Pot, const FCookingData& Data);

	/** Lo que sale si está hecho o quemado; false si aún se está haciendo o está vacío. */
	static bool Collect(const FCookingPot& Pot, const FCookingData& Data, FCookedFood& OutFood);

	/** Efectos de un guiso improvisado: suma de ingredientes con merma, calor y ánimo. */
	static FConsumable ImprovisedEffects(const FCookingData& Data, const TArray<FName>& IngredientIds);

	/** Desgaste de durabilidad (0–1) de un utensilio por cada cocción. */
	static float VesselWearPerUse(const FCookVesselDef& Vessel);

	// --- Conservación y deterioro ---------------------------------------------

	/** Horas hasta estropearse; negativo = no se estropea (agua, sal) o no es comida. */
	static float ShelfLifeHours(const FCookingData& Data, FName ItemId);

	static EFreshness GetFreshness(const FCookingData& Data, const FFoodFreshness& Food);

	/**
	 * Envejece la comida. StorageFactor multiplica el deterioro (1 al aire;
	 * más de 1 con calor húmedo; menos de 1 en vasija sellada).
	 */
	static void Age(FFoodFreshness& Food, float DeltaHours, float StorageFactor = 1.0f);

	/** Lo que aporta ahora: nutrición mermada si está pasada, toxicidad si se estropeó, calor si sigue caliente. */
	static FConsumable CurrentEffects(const FCookingData& Data, const FFoodFreshness& Food);

	/** Frescura inicial de lo que sale del fuego. */
	static FFoodFreshness FreshFromPot(const FCookedFood& Food);
};
