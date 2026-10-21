#pragma once

#include "CoreMinimal.h"

/**
 * Mochila de Iria (GDD v3 §5.3 y §7): 8 huecos, pilas por objeto y una carga pesada en las manos
 * (tronco o lente: se llevan de uno en uno, fuera de los huecos). Modelo puro e inmutable: cada
 * operación devuelve un estado nuevo y no toca el de entrada. Lo usa UExploredSurvivorComponent.
 */

/** Regla de un objeto (items.json: «stack» y «heavy»). */
struct EXPLORED_API FBackpackItemRule
{
	FName Id;
	int32 Stack = 1;
	/** Pesado: va en las manos, no en un hueco, y solo cabe uno. */
	bool bHeavy = false;
};

/** Huecos y reglas de los 17 objetos. */
struct EXPLORED_API FBackpackRules
{
	static constexpr int32 DefaultSlots = 8;

	int32 Slots = DefaultSlots;
	TMap<FName, FBackpackItemRule> Items;

	const FBackpackItemRule* Find(FName Id) const { return Items.Find(Id); }

	/** Los 17 objetos del GDD §7 con sus pilas (espejo de Content/Data/items.json). */
	static FBackpackRules Default();
};

/** Una pila: ocupa un hueco. */
struct EXPLORED_API FBackpackStack
{
	FName Id;
	int32 Count = 0;

	bool operator==(const FBackpackStack& Other) const { return Id == Other.Id && Count == Other.Count; }
};

struct EXPLORED_API FBackpackState
{
	TArray<FBackpackStack> Stacks;
	/** Carga pesada en las manos (NAME_None = manos libres). */
	FName HeldHeavy;
	/** Sorbos que quedan en la cantimplora (0-4). */
	int32 CanteenSips = 0;

	bool operator==(const FBackpackState& Other) const
	{
		return Stacks == Other.Stacks && HeldHeavy == Other.HeldHeavy && CanteenSips == Other.CanteenSips;
	}
};

enum class EBackpackResult : uint8
{
	Ok,
	/** «No te cabe más»: sin huecos o con la pila llena. */
	Full,
	/** Ya lleva algo pesado en las manos. */
	HandsBusy,
	/** El id no está en items.json. */
	UnknownItem,
	/** No hay tantas unidades. */
	NotEnough,
	/** Cantidad nula o negativa. */
	InvalidCount,
};

EXPLORED_API const TCHAR* LexToString(EBackpackResult Result);

struct EXPLORED_API FBackpackModel
{
	static constexpr int32 CanteenMaxSips = 4;
	static constexpr int32 StarterCanteenSips = 2;

	static FName CanteenId();

	/** Unidades de un objeto (incluida la carga de las manos). */
	static int32 Count(const FBackpackState& State, FName Id);
	static int32 UsedSlots(const FBackpackState& State);
	/** Todo lo que lleva, por id (para FSupplyCrateModel y el guardado). */
	static TMap<FName, int32> ToCounts(const FBackpackState& State);

	/** ¿Cabe Count de Id entero? (Todo o nada: el GDD no recoge a medias.) */
	static EBackpackResult CanAdd(const FBackpackState& State, const FBackpackRules& Rules, FName Id, int32 Count);
	static EBackpackResult Add(const FBackpackState& In, const FBackpackRules& Rules, FName Id, int32 Count, FBackpackState& Out);
	static EBackpackResult Remove(const FBackpackState& In, FName Id, int32 Count, FBackpackState& Out);

	/**
	 * Quita y añade en una sola operación (recetas, caja de suministros): o se aplica todo o nada.
	 * Lo que se quita libera huecos antes de añadir.
	 */
	static EBackpackResult Exchange(const FBackpackState& In, const FBackpackRules& Rules,
		const TMap<FName, int32>& ToRemove, const TMap<FName, int32>& ToAdd, FBackpackState& Out);

	/** Kit inicial (GDD §5.3): cuchillo, cantimplora con 2 sorbos y barómetro. */
	static FBackpackState StarterKit(const FBackpackRules& Rules);

	/** Estado coherente con las reglas (para cargar partidas): ids conocidos, pilas y huecos dentro de los límites. */
	static bool IsValid(const FBackpackState& State, const FBackpackRules& Rules);
};
