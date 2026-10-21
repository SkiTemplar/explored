#pragma once

#include "CoreMinimal.h"

#include "WorldGen/IslandPropsModel.h"

/** Lectura de Content/Data/island_props.json (capa de motor sobre FIslandProps). */
namespace ExploredIslandProps
{
	EXPLORED_API FString IslandPropsJsonPath();

	/** Parsea y valida (FIslandProps::Validate). False con el motivo en OutError. */
	EXPLORED_API bool ParseIslandPropsJson(const FString& Json, FIslandProps& OutProps, FString& OutError);

	/** Colocaciones cargadas una vez; vacías (con aviso en el log) si el fichero falta o no es válido. */
	EXPLORED_API TSharedRef<const FIslandProps> Get();
}
