#pragma once

#include "CoreMinimal.h"

class AActor;
struct FIslandDecor;

/**
 * Carga Content/Data/island_decor.json y crea, sobre «Owner», un componente de instancias
 * jerárquicas (HISM) por malla, sin colisión ni navegación: decoración pura (pasada visual de F4).
 */
namespace ExploredIslandDecor
{
	/** Lee y valida el JSON; false con el motivo en OutError si falta o no es válido. */
	EXPLORED_API bool Load(const FString& JsonPath, FIslandDecor& Out, FString& OutError);

	/** Ruta por defecto (Content/Data/island_decor.json). */
	EXPLORED_API FString DefaultPath();

	/** Crea las instancias; devuelve cuántas se añadieron (0 si no hay datos o mallas). */
	EXPLORED_API int32 Build(AActor& Owner);
}
