#pragma once

#include "CoreMinimal.h"

#include "Narrative/BeaconProgressModel.h"

struct FIslandProp;

/**
 * Caja de suministros de la base del faro (GDD v3 §5.3): entrega una sola vez
 * lo mínimo que falta del camino crítico, con tope de 1 cuerda, 3 fibras y
 * 2 peces crudos. Evita que una partida quede sin salida por recursos gastados.
 *
 * Lo que falta se calcula con los hitos de FBeaconProgressState y el
 * inventario (id de objeto del GDD §7 → cantidad):
 * - Mecha sin hacer (ni montada ni en la mochila): 3 fibras.
 * - Aceite: 2 peces crudos por tarro que falte para llegar a 2.
 * - Balsa (sin lente ni balsa): 2 cuerdas, más 1 para el remo si no lo hay.
 * - Caña de pescar: 1 cuerda si faltan más peces de los que da la caja y no hay caña.
 * Las cuerdas que la caja no cubre se piden como fibras (2 por cuerda).
 * - Leña: 3 si falta algún tarro (hay que encender una hoguera para hervir) y no se llega a 3.
 * Los troncos, la caña de bambú y los cocos no van en la caja (sobran en la isla).
 *
 * Con los recursos que quedan en la isla (sobrecarga de 3 argumentos) la caja solo entrega lo que
 * ya no se puede conseguir: fibras y leña cuentan las de la isla, y los peces no faltan mientras
 * haya caña o se pueda atar una (el pescado reaparece). Así la caja no se gasta pronto y queda como
 * red de seguridad (decisión de F3e: el auditor encontró que la leña quemada de más dejaba la
 * baliza imposible y que la caja, abierta el día 1, ya no servía después).
 */

/** Lo que se ha guardado de la caja. */
struct EXPLORED_API FSupplyCrateState
{
	bool bDelivered = false;
};

struct EXPLORED_API FSupplyCrateOpenResult
{
	/** Objetos entregados (vacío si no se entregó nada). */
	TMap<FName, int32> Items;
	FSupplyCrateState State;
	bool bDelivered = false;
};

struct EXPLORED_API FSupplyCrateModel
{
	static constexpr int32 MaxRope = 1;
	static constexpr int32 MaxFiber = 3;
	static constexpr int32 MaxRawFish = 2;
	static constexpr int32 MaxWood = 3;
	static constexpr int32 WoodPerFire = 3;
	static constexpr int32 FibersPerRope = 2;
	static constexpr int32 FibersPerWick = 3;
	static constexpr int32 FishPerOilJar = 2;

	static FName RopeId();
	static FName FiberId();
	static FName RawFishId();
	static FName WoodId();

	/** Lo mínimo que falta (ya con los topes). Las cantidades negativas del inventario cuentan como 0. */
	static TMap<FName, int32> ComputeMissing(const TMap<FName, int32>& Inventory, const FBeaconProgressState& Progress);
	/** Igual, pero descontando lo que aún se puede recoger en la isla (id de objeto → cantidad). */
	static TMap<FName, int32> ComputeMissing(const TMap<FName, int32>& Inventory, const FBeaconProgressState& Progress,
		const TMap<FName, int32>& WorldRemaining);

	/**
	 * Abre la caja. Si ya se entregó, o si no falta nada, no entrega y la caja
	 * sigue como estaba (sin nada que falte no se gasta: queda para más tarde).
	 */
	static FSupplyCrateOpenResult Open(const FSupplyCrateState& State, const TMap<FName, int32>& Inventory,
		const FBeaconProgressState& Progress);
	static FSupplyCrateOpenResult Open(const FSupplyCrateState& State, const TMap<FName, int32>& Inventory,
		const FBeaconProgressState& Progress, const TMap<FName, int32>& WorldRemaining);

	/**
	 * Leña de deriva que el mar deja en Cala Varada al amanecer (cierra el softlock de quemar la leña
	 * dos veces, docs/v3/estado-f3.md): la que falta para encender una hoguera si aún quedan tarros
	 * por hervir y no hay ningún fuego encendido; 0 en otro caso. Se repite cada amanecer.
	 */
	static int32 DriftwoodAtDawn(const TMap<FName, int32>& Inventory, const FBeaconProgressState& Progress,
		const TMap<FName, int32>& WorldRemaining, bool bAnyFireBurning);

	/**
	 * Dónde se deja la leña de deriva (cm): sobre el montón de la cala (Beach) con el suelo real
	 * en metros, ExploredSaveStates::RestLiftCm por encima para que no nazca dentro de la arena.
	 */
	static FVector DriftwoodPileLocation(const FIslandProp& Beach, float GroundMeters);
};
