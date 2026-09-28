#pragma once

#include "CoreMinimal.h"

#include "WorldGen/FellingModel.h"

/**
 * Caída del árbol talado (biblia 02 §1.2): hacia dónde cae y contra qué choca.
 *
 * - Dirección: la del golpe final más la pendiente (FFellingModel::ResolveFallDirection),
 *   desviada hacia donde sopla el viento hasta ±WindDeviationDeg de la especie
 *   (±20°, palmera ±15°) en proporción a la fuerza del viento (FWeatherSample::Wind, 0–1).
 * - Colisión: el tronco gira sobre la base desde la vertical hasta el suelo y
 *   barre un cuarto de círculo de radio la altura. La construcción ligera (nivel
 *   palma o bambú) que toca la aplasta (40 % de su integridad máxima) y sigue
 *   cayendo; la construcción pesada y el terreno que se alzan en su camino lo
 *   paran y queda apoyado, en diagonal.
 *
 * Todo en el plano del suelo y en centímetros. Sin UObject: la capa de Unreal
 * (UBuildingSubsystem, FTerrainDensity) solo traduce piezas y muestras de
 * terreno a FTreeFallObstacle y aplica el resultado.
 */

/** Viento del momento: hacia dónde sopla (no hace falta normalizada) y su fuerza 0–1. */
struct EXPLORED_API FTreeFallWind
{
	FVector2D Direction = FVector2D::ZeroVector;
	float Strength = 0.0f;
};

enum class ETreeFallObstacleKind : uint8
{
	/** Pieza de construcción (FBuildingPiece): se aplasta si es ligera, frena el tronco si no. */
	Building,
	/** Muestra de terreno que se alza sobre la base (loma, roca): frena el tronco, no sufre daño. */
	Terrain
};

/** Algo que puede estar en el camino del tronco, visto como un cilindro vertical. */
struct EXPLORED_API FTreeFallObstacle
{
	ETreeFallObstacleKind Kind = ETreeFallObstacleKind::Building;
	/** Centro en el plano, en centímetros. */
	FVector2D Center = FVector2D::ZeroVector;
	/** Radio de la huella en el plano, en centímetros. */
	double RadiusCm = 100.0;
	/** Altura de su parte más alta sobre la base del árbol, en centímetros. ≤ 0 = el tronco pasa por encima. */
	double TopCm = 250.0;
	/** Orden del nivel de material (building_pieces.json «tiers»: 0 palma, 1 bambú, 2 madera, 3 piedra). */
	int32 TierOrder = 0;
	/** Integridad máxima de la pieza, para el daño del aplastamiento. */
	float MaxIntegrity = 0.0f;
};

struct EXPLORED_API FTreeFallHit
{
	/** Índice en el array de obstáculos que se pasó a Resolve. */
	int32 ObstacleIndex = INDEX_NONE;
	/** Daño en puntos de integridad (0 si solo lo frena). */
	float Damage = 0.0f;
	/** Ángulo del tronco desde la vertical al tocarlo, en grados. */
	double ContactAngleDeg = 0.0;
};

struct EXPLORED_API FTreeFallResult
{
	/** Dirección de caída, unitaria. */
	FVector2D Direction = FVector2D(1.0, 0.0);
	/** Ángulo final del tronco desde la vertical, en grados: 90 = en el suelo, menos = apoyado. */
	double RestAngleDeg = 90.0;
	/** Parte de la altura que queda en horizontal (sen RestAngle): para ComputeFellDrops. */
	double ReachFraction = 1.0;
	/** Construcción ligera aplastada, en el orden en que la toca. */
	TArray<FTreeFallHit> Crushed;
	/** Lo que lo detuvo (pieza pesada o terreno); ObstacleIndex INDEX_NONE si llega al suelo. */
	FTreeFallHit StoppedBy;
};

struct EXPLORED_API FTreeFallModel
{
	/** Niveles que un tronco aplasta: palma (0) y bambú (1) (biblia 02 §1.2). */
	static constexpr int32 MaxCrushedTierOrder = 1;
	/** Daño del aplastamiento, en fracción de la integridad máxima de la pieza. */
	static constexpr float CrushDamageFraction = 0.4f;
	/** Radio del tronco que barre la caída, en centímetros. */
	static constexpr double TrunkRadiusCm = 20.0;

	/**
	 * Desvía Direction hacia donde sopla el viento: como mucho MaxDeviationDeg ×
	 * fuerza (0–1), y nunca más allá de la propia dirección del viento. Con
	 * viento nulo, de cara o de espaldas no cambia nada. Entradas no finitas
	 * se ignoran (viento cero).
	 */
	static FVector2D ApplyWind(const FVector2D& Direction, const FTreeFallWind& Wind, double MaxDeviationDeg);

	/** Dirección final de la caída: golpe final + pendiente (FFellingModel) + viento de la especie. */
	static FVector2D ResolveDirection(const FFellingProfile& Profile, const FFellingProgress& Progress,
		const FVector2D& Downhill, const FTreeFallWind& Wind, uint32 InstanceSeed);

	/**
	 * Hace caer el tronco de Profile.HeightMeters desde Base hacia Direction
	 * entre los obstáculos. Determinista: a igual ángulo de contacto gana el
	 * índice menor. Un arbusto (altura 0) no cae y no toca nada.
	 */
	static FTreeFallResult Resolve(const FFellingProfile& Profile, const FVector2D& Base, const FVector2D& Direction,
		const TArray<FTreeFallObstacle>& Obstacles);
};
