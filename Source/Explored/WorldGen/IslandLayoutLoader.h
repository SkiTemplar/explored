#pragma once

#include "CoreMinimal.h"
#include "WorldGen/IslandLayout.h"

/** Carga de `Content/Data/island.json` (lado motor; FIslandLayout no lee ficheros). */
namespace ExploredIsland
{
	/** Ruta absoluta de island.json. */
	EXPLORED_API FString IslandJsonPath();

	/** Interpreta el JSON de island.json. False y OutError si falta un campo o no valida. */
	EXPLORED_API bool ParseIslandJson(const FString& Json, FIslandLayout& OutLayout, FString& OutError);

	/**
	 * Disposición de la isla, cargada una vez por proceso. Si island.json falta o no es
	 * válido, avisa en el registro y devuelve FIslandLayout::Provisional(): nunca falla.
	 */
	EXPLORED_API TSharedRef<const FIslandLayout> Get();
}
