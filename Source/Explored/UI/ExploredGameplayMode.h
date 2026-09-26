#pragma once

#include "CoreMinimal.h"

#include "ExploredGameplayMode.generated.h"

/** Modos de «Nueva partida» (GDD §7). El modo «Personalizado» queda fuera del alcance del frontend M8. */
UENUM(BlueprintType)
enum class EExploredGameplayMode : uint8
{
	Explorer,
	Survivor,
	Castaway
};
