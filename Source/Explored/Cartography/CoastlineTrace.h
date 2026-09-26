#pragma once

#include "CoreMinimal.h"

class FTerrainDensity;

/**
 * Costa de referencia de una isla a partir del terreno real (puro). La usa el
 * mapa para medir la cobertura (GDD §16, «Cartógrafo»), estimar la distancia a
 * la orilla y dibujar el boceto de un mirador (GDD §5.3).
 */
struct EXPLORED_API FCoastlineTrace
{
	/**
	 * Anillo de la costa exterior (metros), un punto por rayo. Cada rayo se recorre de
	 * fuera hacia el centro y se queda con la franja de tierra más exterior que tenga
	 * anchura suficiente (así un cayo pequeño no tapa la isla ni la laguna de un atolón
	 * cuenta como costa). Los rayos sin tierra se omiten.
	 */
	static TArray<FVector2D> TraceIsland(const FTerrainDensity& Density, int32 IslandIndex, int32 NumRays = 180);

	/** Anchura mínima (m) de una franja de tierra para contar como la costa de la isla. */
	static constexpr float MinLandWidth = 50.0f;
};
