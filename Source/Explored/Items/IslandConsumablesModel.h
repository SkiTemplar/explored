#pragma once

#include "CoreMinimal.h"
#include "Items/BackpackModel.h"

/**
 * Lo que da cada objeto al consumirlo (GDD v3 §5.1 y §7; Content/Data/survival_needs.json,
 * «consumables»). Coco: +25 sed y +5 hambre, deja un coco vacío. Cantimplora: un sorbo de +20.
 * Pez crudo: +20 hambre y -10 energía. Pez asado: +45 hambre.
 */
struct EXPLORED_API FConsumableEffect
{
	FName Item;
	float Thirst = 0.0f;
	float Hunger = 0.0f;
	float Energy = 0.0f;
	/** Objeto que queda al consumirlo (coco_vacio); NAME_None si no deja nada. */
	FName Leaves;
	/** true si se bebe a sorbos de un recipiente que no se gasta (cantimplora). */
	bool bSips = false;
};

enum class EConsumeResult : uint8
{
	Ok,
	/** El objeto no se come ni se bebe: «No sirve así». */
	NotConsumable,
	/** No lo lleva. */
	Missing,
	/** Cantimplora vacía. */
	Empty,
};

struct EXPLORED_API FConsumeOutcome
{
	EConsumeResult Result = EConsumeResult::NotConsumable;
	FConsumableEffect Effect;
	/** Lo que deja (coco vacío) no cabía: hay que soltarlo en el suelo. */
	FName Dropped;
};

struct EXPLORED_API FIslandConsumables
{
	static constexpr float CoconutThirst = 25.0f;
	static constexpr float CoconutHunger = 5.0f;
	static constexpr float CanteenSipThirst = 20.0f;
	static constexpr float RawFishHunger = 20.0f;
	static constexpr float RawFishEnergy = -10.0f;
	static constexpr float GrilledFishHunger = 45.0f;

	TArray<FConsumableEffect> Effects;

	const FConsumableEffect* Find(FName Item) const;

	static FIslandConsumables Default();

	/**
	 * Consume una unidad (o un sorbo). No toca In; Out es la mochila resultante. Si lo que deja
	 * no cabe, el consumo se hace igual y Dropped dice qué soltar.
	 */
	static FConsumeOutcome Consume(const FIslandConsumables& Table, const FBackpackState& In, const FBackpackRules& Rules,
		FName Item, FBackpackState& Out);
};
