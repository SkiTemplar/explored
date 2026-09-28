#pragma once

#include "CoreMinimal.h"
#include "WorldGen/TerrainEditModel.h"

/** Lo que pide el jugador al mantener E con la pala en modo camino. Metros. */
struct EXPLORED_API FShovelPathRequest
{
	/** Donde está el jugador y hacia donde quiere el camino (solo cuenta la planta). */
	FVector Start = FVector::ZeroVector;
	FVector End = FVector(4.0, 0.0, 0.0);
	/** Estrato de superficie: decide si sobra `tierra_suelta` o `arena`. */
	ETerrainMaterial Material = ETerrainMaterial::Tierra;
	int32 ToolTier = 1;
};

/** Tramo ya ajustado a las reglas de biblia 02 §3. */
struct EXPLORED_API FShovelPathPlan
{
	bool bValid = false;
	/** Extremos del eje sobre el plano objetivo (Z = altura final del camino). */
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	/** Largo en planta (≤ 4 m). */
	double Length = 0.0;
	/** Pendiente del terreno sin tocar entre los extremos y la del camino (≤ 15°). */
	double GroundSlopeDeg = 0.0;
	double PathSlopeDeg = 0.0;
	/** Golpes de pala: 6 por tramo de 4 m, en proporción, al menos 1. */
	int32 HitsRequired = 0;
	ETerrainMaterial Material = ETerrainMaterial::Tierra;
	int32 ToolTier = 1;
};

/** Un camino a medio hacer. Se guarda con el jugador mientras mantiene E. */
struct EXPLORED_API FShovelPathJob
{
	FShovelPathPlan Plan;
	int32 HitsDone = 0;
	/** Tierra cortada que aún no se ha usado para rellenar (m³). */
	double Spare = 0.0;
	bool bFinished = false;
};

struct EXPLORED_API FShovelPathHitResult
{
	FTerrainEditResult Edit;
	/** Este golpe ha terminado el camino (compactado entero). */
	bool bFinished = false;
	/** Lo que sobra al terminar va a la carga del jugador. */
	FString LootItem;
	int32 LootUnits = 0;
};

/**
 * Modo «camino» de la pala (biblia 02 §3). Una franja de **1,5 m** de ancho y hasta **4 m**
 * de largo por uso se aplana hacia un plano con pendiente de **15° como mucho** (el plano
 * gira sobre el punto medio: lo que se corta arriba rellena abajo) y, al terminar, se
 * compacta. Sobre un camino terminado caminar cuesta un **15 % menos** de resistencia.
 *
 * - **Coste:** 6 golpes de pala tosca por tramo de 4 m; un tramo más corto, en proporción.
 *   Cada golpe es una pasada de pala por toda la franja (`FTerrainEditModel::Shovel`, que
 *   limita lo que cambia cada pasada), así que un desnivel grande necesita todos los golpes.
 * - **Tierra sobrante:** lo cortado rellena primero lo bajo del mismo tramo; lo que sobra al
 *   terminar sale como `tierra_suelta` (o `arena`), 6 unidades por m³ (`mining.json`).
 * - Picar o echar tierra encima de un camino le quita la compactación a esas columnas.
 *
 * Red (biblia 08 §2.2 y §2.12): el servidor planifica y aplica; cada golpe sale por la cola
 * de terreno como una pasada de pala normal. La compactación va en el mismo chunk.
 */
class EXPLORED_API FShovelPathModel
{
public:
	using FGroundHeight = TFunctionRef<double(double X, double Y)>;

	static constexpr float StripWidth = 1.5f;
	static constexpr float MaxLength = 4.0f;
	static constexpr float MaxSlopeDeg = 15.0f;
	static constexpr int32 HitsPerFullStrip = 6;
	/** Multiplicador de la resistencia al caminar sobre un camino terminado (−15 %). */
	static constexpr float PathStaminaFactor = 0.85f;
	/** Tramo más corto que se acepta (medio paso). */
	static constexpr float MinLength = 0.25f;
	/** Separación de las pasadas de pala dentro de la franja y su borde suave. */
	static constexpr float StrokeSpacing = 1.0f;
	static constexpr float StrokeEdge = 0.5f;
	/** Una pala no arrasa un cerro: solo toca lo que queda a 1 m del plano. */
	static constexpr float VerticalReach = 1.0f;
	/** Unidades de tierra por m³ que sobra (`mining.json/unitsPerM3`). */
	static constexpr int32 UnitsPerCubicMeter = 6;

	/** Ajusta la petición: largo ≤ 4 m, pendiente ≤ 15°, golpes. bValid = false si es degenerada. */
	static FShovelPathPlan Plan(const FShovelPathRequest& Request, FGroundHeight Ground);
	/** Pasadas de pala de un golpe (todas iguales en cada golpe: el terreno converge al plano). */
	static TArray<FShovelStroke> Strokes(const FShovelPathPlan& Plan);
	/** Aplica el golpe siguiente; al llegar a HitsRequired compacta la franja. */
	static FShovelPathHitResult ApplyHit(FShovelPathJob& Job, FTerrainEditModel& Terrain, FTerrainEditModel::FBaseDensity Base);
	/** Multiplicador del coste de resistencia al caminar en (X, Y). */
	static float StaminaFactor(const FTerrainEditModel& Terrain, double X, double Y);
};
