#pragma once

#include "CoreMinimal.h"

/**
 * Adelgazamiento continuo del césped con la distancia (docs/v3/investigacion-cesped-arena.md).
 *
 * f(d) es la fracción de parches que se ven a d metros (horizontales) de la cámara: 1 hasta DenseM,
 * (DenseM / d)^Exponent después y un fundido final a 0 entre EndM - EndFadeM y EndM. Es la única fuente
 * de verdad: el material del césped (M_ToonCesped) y el del terreno (M_TerrainToon) evalúan la misma
 * fórmula con los parámetros de MPC_Terreno que escribe AExploredGrassField, y FGrassFieldModel la usa
 * para decidir qué siembra.
 *
 * Cada parche lleva un rango r en [0, 1) (dato por instancia 0). El material lo encoge a 0 cuando f(d)
 * baja de r (Visibility) y agranda los que quedan (Grow) para que la cobertura no caiga de golpe. Los
 * niveles de densidad de una celda son f_k = LevelRatio^k (k = 0..LevelCount()-1): el nivel k contiene
 * los parches con r < f_k, así que cada nivel es un subconjunto del anterior y subir o bajar de nivel
 * solo añade o quita parches que el material ya tiene encogidos a 0.
 */
struct EXPLORED_API FGrassThinning
{
	static constexpr float DefaultDenseM = 25.0f;
	static constexpr float DefaultEndM = 100.0f;

	/** f = 1 hasta aquí (m). */
	float DenseM = DefaultDenseM;
	/** f = 0 desde aquí (m): más allá no hay celdas. */
	float EndM = DefaultEndM;
	/** Fundido final de f a 0 en [EndM - EndFadeM, EndM]. */
	float EndFadeM = 25.0f;
	float Exponent = 2.0f;
	/** Agrandado máximo (horizontal) de los parches que quedan. */
	float MaxGrow = 2.0f;
	/** Fracción del agrandado que se aplica también a la altura (0 = altura fija). */
	float GrowZ = 0.4f;
	/** Ancho relativo del encogido: el parche pasa de entero (f ≥ r) a 0 (f ≤ r / (1 + Band)). */
	float Band = 0.2f;
	float LevelRatio = 0.5f;
	/** El último nivel es el primero con f_k ≤ MinLevel. */
	float MinLevel = 0.015f;
	/** Adelanto (m) con el que una celda sube de nivel antes de necesitarlo. */
	float MarginM = 4.0f;
	/** Histéresis (m) extra para bajar de nivel o quitar la celda. */
	float HysteresisM = 3.0f;

	/** f(d) en [0, 1], no creciente. */
	float Fraction(double DistanceM) const;
	/** Tamaño relativo (0-1) de un parche de rango Rank a esa distancia (lo mismo que el material). */
	float Visibility(float Rank, double DistanceM) const;
	/** Rangos que el material puede enseñar (aunque sea encogidos) a esa distancia: min(f·(1 + Band), 1). */
	float Reach(double DistanceM) const;
	/** Agrandado horizontal de los parches que quedan: min(1 / sqrt(f), MaxGrow). */
	float Grow(double DistanceM) const;

	int32 LevelCount() const;
	/** f_k del nivel (0 = todos los parches). */
	float LevelFraction(int32 Level) const;
	/**
	 * Nivel que necesita una celda cuya distancia mínima a la cámara es CellDistanceM, adelantando
	 * ExtraM metros: el más ralo con f_k ≥ Reach(CellDistanceM - ExtraM). INDEX_NONE si f es 0 (sin celda).
	 */
	int32 LevelFor(double CellDistanceM, double ExtraM) const;
	/** Distancia (m) a partir de la cual una celda ya no hace falta (con su adelanto). */
	double ReachM() const { return EndM + MarginM + HysteresisM; }

	bool Validate(FString& OutError) const;
};
