#pragma once

#include "CoreMinimal.h"

#include "Core/ExploredRandom.h"

/**
 * Reglas puras de recolección de una especie de FVegetationScatter (árbol,
 * palmera, arbusto, hierba o roca): qué suelta cada golpe, qué suelta el
 * golpe final (al talar o agotar) y cuántos golpes hacen falta con y sin
 * herramienta. Sin UObject a propósito (ver Tools/HostTests/README.md):
 * la capa de Unreal (AExploredVegetationCell, UExploredVegetationHarvestSubsystem)
 * solo traduce esto a instancias HISM que quitar y AExploredItemActor que soltar.
 */

/** Lo que suelta un golpe: entre Min y Max unidades de un objeto de items.json. */
struct EXPLORED_API FHarvestDrop
{
	FName ItemId;
	int32 MinCount = 1;
	int32 MaxCount = 1;
};

struct EXPLORED_API FHarvestSpeciesRule
{
	/** Coincide con FScatterRule::Species (VegetationScatter.cpp). */
	FName Species;

	/** Golpes para talar/agotar la instancia a mano. */
	int32 HitsBareHands = 1;
	/** Golpes para talar/agotar la instancia con la herramienta adecuada en la mano (RequiredToolTag). */
	int32 HitsWithTool = 1;
	/** Etiqueta de items.json que debe llevar el objeto en la mano para contar como herramienta (Filo, Contundente...). NAME_None = no hace falta ninguna. */
	FName RequiredToolTag;

	/** Se sueltan en cada golpe, incluido el que tala (ramas al talar, fibra al segar). */
	TArray<FHarvestDrop> PerHitDrops;
	/** Solo al golpe final: troncos, cocos, lascas de la roca agotada. */
	TArray<FHarvestDrop> FellDrops;

	/**
	 * Horas de juego hasta que una instancia talada vuelve a ser recolectable (biblia 02 §1.2:
	 * 18 días con fruto, 24 madera sin fruto, 4 arbustos y hierba). Con perfil de tala
	 * (FFellingModel) coincide con StumpRegrowDays × 24. 0 = no rebrota.
	 */
	float RegrowHours = 0.0f;
};

struct EXPLORED_API FHarvestModel
{
	/** Reglas por defecto para las 8 especies de FVegetationScatter::DefaultRules (Palm, JungleGiant, JungleWide, Mangrove, Understory, Shrub, Grass, Rock). */
	static TArray<FHarvestSpeciesRule> DefaultRules();

	static const FHarvestSpeciesRule* FindRule(const TArray<FHarvestSpeciesRule>& Rules, FName Species);

	/** Golpes que hacen falta para esta regla según si el jugador lleva la herramienta adecuada. */
	static int32 HitsRequired(const FHarvestSpeciesRule& Rule, bool bHasRequiredTool);

	/**
	 * Aplica un golpe: NewHits es CurrentHits + 1. bOutFelled se pone a true
	 * cuando NewHits alcanza HitsRequired (la instancia se retira ese golpe).
	 * Devuelve NewHits (saturado en HitsRequired: no sigue subiendo tras talar).
	 */
	static int32 ApplyHit(const FHarvestSpeciesRule& Rule, int32 CurrentHits, bool bHasRequiredTool, bool& bOutFelled);

	/** Tira el número de unidades de cada FHarvestDrop (FExploredRandom inyectado para tests deterministas). */
	static TArray<FHarvestDrop> RollDrops(const TArray<FHarvestDrop>& Drops, FExploredRandom& Random);
};
