#pragma once

#include "CoreMinimal.h"

struct FExplorationCatalog;

/**
 * Carga de Content/Data/exploration.json (FExplorationCatalog::Validate hace las
 * comprobaciones). Usa el módulo Json: a diferencia de ExplorationContentModel, esto
 * NO es un modelo puro y no se compila en Tools/HostTests (ver su README, «Añadir
 * código»); se valida en el editor con Tests/ExplorationContentDataSpec.cpp.
 */
namespace ExplorationContentLoader
{
	EXPLORED_API bool ParseJson(const FString& JsonText, FExplorationCatalog& OutCatalog, FString& OutError);
}
