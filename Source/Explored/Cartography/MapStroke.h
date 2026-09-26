#pragma once

#include "CoreMinimal.h"

/**
 * Geometría de polilíneas para los trazos del mapa (GDD §5.2, §5.6): distancias,
 * simplificación de Douglas–Peucker, suavizado y remuestreo. Funciones puras y
 * deterministas; las unidades son las del llamador (metros o coordenadas de mapa).
 */
namespace MapStroke
{
	/** Distancia de P al segmento AB (si A y B coinciden, distancia al punto). */
	EXPLORED_API double DistanceToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B);

	/** Distancia de P a la polilínea; si bClosed, incluye el segmento que cierra el anillo. */
	EXPLORED_API double DistanceToPolyline(const FVector2D& P, const TArray<FVector2D>& Points, bool bClosed = false);

	/** Longitud total de la polilínea. */
	EXPLORED_API double Length(const TArray<FVector2D>& Points, bool bClosed = false);

	/**
	 * Douglas–Peucker iterativo (pila explícita, sin recursión). Conserva los
	 * extremos; todo punto descartado queda a menos de Tolerance del resultado.
	 */
	EXPLORED_API TArray<FVector2D> Simplify(const TArray<FVector2D>& Points, double Tolerance);

	/** Douglas–Peucker para un anillo cerrado (el primer punto no se repite al final). */
	EXPLORED_API TArray<FVector2D> SimplifyClosed(const TArray<FVector2D>& Points, double Tolerance);

	/** Suavizado laplaciano: acerca cada punto interior a la media de sus vecinos. Conserva los extremos. */
	EXPLORED_API void Smooth(TArray<FVector2D>& Points, double Alpha, int32 Iterations);

	/** Remuestrea la polilínea a paso constante (el último tramo puede ser más corto). */
	EXPLORED_API TArray<FVector2D> Resample(const TArray<FVector2D>& Points, double Spacing, bool bClosed);
}
