#pragma once

#include "CoreMinimal.h"

#include "CarryTypes.generated.h"

UENUM(BlueprintType)
enum class EHand : uint8
{
	Left,
	Right
};

/** Los tres contenedores fijos del cuerpo, además de las manos y la mochila (GDD §4.2). */
UENUM(BlueprintType)
enum class ECarrySlot : uint8
{
	Pocket,
	Belt,
	Backpack
};
