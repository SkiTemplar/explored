#pragma once

#include "CoreMinimal.h"

/**
 * Elección de verbo sobre un objeto del mundo con varias acciones (GDD v3 §5.6: «una lista de máximo
 * 3 verbos válidos»). La tecla de interactuar ejecuta el verbo elegido y la tecla de combinar pasa al
 * siguiente. Modelo puro: AExploredFire (cocinar / hervir / avivar) guarda la elección y el HUD
 * pinta el orden.
 */
struct EXPLORED_API FVerbChoiceModel
{
	static constexpr int32 MaxVerbs = 3;

	/** Acción que ejecuta la tecla de interactuar: la elegida si sigue disponible; si no, la primera. */
	static FName Resolve(const TArray<FName>& Available, FName Selected);

	/** La que sigue a Current en Available (vuelve al principio); NAME_None si no hay ninguna. */
	static FName Next(const TArray<FName>& Available, FName Current);

	/** Para el aviso del HUD: primero la que ejecutaría, luego las siguientes en orden de ciclo; como mucho MaxVerbs. */
	static TArray<FName> Ordered(const TArray<FName>& Available, FName Selected);
};
