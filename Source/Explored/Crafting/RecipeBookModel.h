#pragma once

#include "CoreMinimal.h"
#include "Items/BackpackModel.h"

/**
 * Crafteo objeto + verbo de Baliza Siete (GDD v3 §5.6): lista cerrada de 10 recetas y 6 verbos
 * (Content/Data/recipes.json y verbs.json). Arrastrar un objeto sobre otro, o sobre un objeto del
 * mundo, ofrece como mucho 3 recetas; aplicar una gasta las entradas y da la salida en una sola
 * operación (todo o nada). Modelo puro: UExploredSurvivorComponent y AExploredFire son la capa fina.
 */

/** Dónde se hace una receta (recipes.json, «place»). */
enum class ECraftPlace : uint8
{
	/** En cualquier sitio, con lo que lleva en la mochila. */
	Anywhere,
	/** Suelo despejado (la hoguera). */
	Ground,
	/** Junto a una hoguera encendida. */
	Fire,
	/** Sitio de la balsa de Playa Norte. */
	RaftSite,
	/** Linterna del faro. */
	Lantern,
};

EXPLORED_API bool CraftPlaceFromName(const FString& Name, ECraftPlace& Out);
/** Id del objeto del mundo que representa el lugar («hoguera», «sitio_balsa», «linterna»); NAME_None si no lo hay. */
EXPLORED_API FName CraftPlaceToken(ECraftPlace Place);

struct EXPLORED_API FRecipeAmount
{
	FName Item;
	int32 Count = 1;
};

struct EXPLORED_API FRecipeDef
{
	FName Id;
	FString NameEs;
	FString NameEn;
	FName Verb;
	/** Se gastan de la mochila. */
	TArray<FRecipeAmount> Inputs;
	/** Hacen falta y no se gastan (cuchillo). */
	TArray<FName> Tools;
	ECraftPlace Place = ECraftPlace::Anywhere;
	/** Lo que ya debe estar puesto en el objeto del mundo (troncos en el sitio de la balsa...). */
	TArray<FRecipeAmount> StationNeeds;
	float Seconds = 1.0f;
	int32 Steps = 1;
	/** Salida a la mochila (NAME_None si cambia el mundo). */
	FName OutputItem;
	int32 OutputCount = 0;
	/** Cambio en el mundo: «hoguera», «balsa», «linterna_montada», «final». */
	FName OutputWorld;
};

enum class ECraftResult : uint8
{
	Ok,
	/** «No sirve así»: la combinación no es ninguna receta. */
	NotValid,
	/** Falta material o herramienta. */
	MissingInputs,
	/** La receta pide otro sitio (hoguera, sitio de la balsa...). */
	WrongPlace,
	/** El objeto del mundo aún no tiene lo que pide (troncos, linterna montada). */
	StationNotReady,
	/** «No te cabe más». */
	Full,
};

EXPLORED_API const TCHAR* LexToString(ECraftResult Result);

struct EXPLORED_API FRecipeBook
{
	static constexpr int32 MaxOptions = 3;

	TArray<FRecipeDef> Recipes;
	TArray<FName> Verbs;

	const FRecipeDef* Find(FName Id) const;

	/** Las 10 recetas y los 6 verbos del GDD §5.6 (espejo de recipes.json y verbs.json). */
	static FRecipeBook Default();

	/** ¿Se puede hacer en Here una receta que pide Required? */
	static bool PlaceAllows(ECraftPlace Required, ECraftPlace Here);

	/**
	 * Recetas en las que entran A y B (B puede ser el id de un objeto del mundo: «hoguera»,
	 * «sitio_balsa», «linterna»). Si A == B la receta debe pedir al menos 2. Como mucho MaxOptions,
	 * en el orden del fichero. Vacío = «No sirve así».
	 */
	TArray<FName> FindOptions(FName A, FName B) const;

	/** Comprueba sin aplicar. Station: lo que hay puesto en el objeto del mundo (id → cantidad). */
	static ECraftResult Check(const FRecipeDef& Recipe, const FBackpackState& Pack, ECraftPlace Here, const TMap<FName, int32>& Station);

	/** Gasta las entradas y añade la salida a la mochila (si la tiene). No toca In. */
	static ECraftResult Apply(const FRecipeDef& Recipe, const FBackpackState& In, const FBackpackRules& Rules, ECraftPlace Here,
		const TMap<FName, int32>& Station, FBackpackState& Out);

	/** Coherencia: ids únicos, verbos conocidos, cantidades y tiempos positivos, una sola salida. */
	bool Validate(FString& OutError) const;
};
