#pragma once

#include "CoreMinimal.h"

#include "CarryTypes.generated.h"

UENUM(BlueprintType)
enum class EHand : uint8
{
	Left,
	Right
};

/**
 * Contenedores del jugador además de las manos (GDD §8.2). Los valores nuevos
 * van siempre al final para no cambiar los ya serializados en Blueprint.
 */
UENUM(BlueprintType)
enum class ECarrySlot : uint8
{
	Pocket,
	Belt,
	Backpack,
	/** Bolsa estanca: bolsillo impermeable de la mochila o bolsa colgada del cinturón. */
	Pouch,
	/** Angarillas enganchadas detrás del jugador. */
	Sledge
};

/** Clase de contenedor del mundo (biblia §3.9); fija su capacidad en FInventoryContainerSpec. */
UENUM(BlueprintType)
enum class EWorldContainerKind : uint8
{
	Cesta,
	Estante,
	Arcon
};
