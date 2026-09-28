#pragma once

#include "CoreMinimal.h"
#include "Core/ExploredNoise.h"
#include "WorldGen/ArchipelagoLayout.h"

struct FKarstLayout;

/**
 * Forma de cada isla antes de erosionar, en Q: coordenadas locales normalizadas por el radio,
 * alargadas y con la costa deformada. La erosión (FIslandReliefModel) trabaja sobre una
 * rejilla en ese mismo Q, así que la base analítica y la diferencia erosionada casan punto a
 * punto. Modelo puro (solo CoreMinimal.h y otros modelos); lo usa FTerrainDensity.
 */
class EXPLORED_API FIslandShapeModel
{
public:
	/**
	 * Coordenadas Q de un punto del mundo (m) para esta isla y su ruido. Sin bFineDetail la
	 * deformación usa solo sus dos octavas bajas (misma forma a gran escala): mar adentro, las
	 * altas dentaban el talud.
	 */
	static FVector2D WarpedLocal(const FIslandDesc& Island, const FExploredNoise& N, float X, float Y, bool bFineDetail = true);

	/**
	 * Distancia normalizada a la costa (1 = costa nominal) con lóbulos, bahías y penínsulas.
	 * Sin bFineDetail omite el rizado corto de la costa: bajo el agua, ese rizado dibujaba el
	 * talud de la plataforma en dientes de sierra.
	 */
	static float CoastT(const FIslandDesc& Island, const FExploredNoise& N, const FVector2D& Q, bool bFineDetail = true);

	/**
	 * Relieve de tierra antes de erosionar (m); -1000 donde el arquetipo no tiene tierra. El
	 * macizo kárstico aquí es solo el llano entre torres: las torres se añaden después de
	 * erosionar para que la erosión térmica no las convierta en conos.
	 */
	static float BaseLand(const FIslandDesc& Island, const FExploredNoise& N, const FVector2D& Q, float T, const FKarstLayout* Karst);

	/** Valor periódico en [-1, 1] de un ruido a lo largo del perímetro (Angle en radianes). */
	static float AroundCoast(const FExploredNoise& N, float Angle, float Frequency, float Offset, int32 Octaves);
};
