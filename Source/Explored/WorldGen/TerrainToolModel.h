#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/MiningModel.h"
#include "WorldGen/TerrainEdits.h"

/** Lo que hace un uso de herramienta sobre el terreno (GDD v2 §3.4). */
enum class ETerrainToolAction : uint8
{
	/** Pico (o pala usada como pico): picado por esfera de `FTerrainEdits::Dig`. */
	Pick,
	/** Pala: aplanar hacia el plano de los pies del jugador (`FTerrainEditModel::Shovel`). */
	ShovelFlatten,
	/** Pala: echar la tierra transportada (`FTerrainEditModel::PlaceSoil`). */
	PlaceSoil,
	Count,
};

/** Por qué el servidor descarta una petición de herramienta. */
enum class ETerrainToolVerdict : uint8
{
	Accepted,
	/** Algún valor no es finito o la acción no existe. */
	Invalid,
	/** El punto de impacto está más lejos de los ojos que el alcance (con tolerancia de red). */
	TooFar,
	/** Más rápido que la duración del golpe (−15 %, `FTerrainEdits::CadenceTolerance`). */
	TooSoon,
	/** En el punto de impacto no hay superficie de terreno (ni cerca). */
	NotSurface,
};

/** Columna del terreno donde cae el golpe, tal como la ve `FTerrainDensity`. Metros. */
struct EXPLORED_API FTerrainStrataQuery
{
	bool bHasIsland = false;
	EIslandArchetype Archetype = EIslandArchetype::Landing;
	/** Altura de la superficie sin editar en esa columna. */
	float ColumnHeight = 0.0f;
	/** Altura del punto golpeado. */
	float Z = 0.0f;
	/** Pesos de `FTerrainDensity::SurfaceLayers` en la superficie: X arena y Z roca. */
	float SandWeight = 0.0f;
	float RockWeight = 0.0f;
};

/** Petición de uso tal como la recibe el servidor. Metros y segundos. */
struct EXPLORED_API FTerrainToolRequest
{
	ETerrainToolAction Action = ETerrainToolAction::Pick;
	ETerrainDigTool Tool = ETerrainDigTool::PicoPiedra;
	FVector ImpactPoint = FVector::ZeroVector;
	/** Ojos del personaje EN EL SERVIDOR (no lo que diga el cliente). */
	FVector EyeLocation = FVector::ZeroVector;
	/** Tiempo desde el uso anterior aceptado de este jugador. */
	float SecondsSinceLastUse = 0.0f;
	/** Densidad del terreno (con ediciones) en el punto de impacto. */
	float DensityAtImpact = 0.0f;
};

/**
 * Reglas de las herramientas de terreno en el juego (pico y pala), puras: qué herramienta
 * es cada objeto, qué hace cada botón, qué estrato se golpea, cuándo acepta el servidor
 * una petición y cómo se lleva la tierra (GDD v2 §3.4, biblia 02 §2–3, biblia 08 §1.2).
 *
 * Estratos: tabla por isla de `Content/Data/mining.json/strata` para los cuatro materiales
 * que cava el modelo (arena, tierra, caliza, basalto) medida desde la superficie SIN editar
 * de la columna; una pared de roca desnuda en superficie (peso de roca alto) es roca. Las
 * vetas, la arcilla, el azufre, la obsidiana y el cristal necesitan su propia generación
 * y todavía no salen aquí (el golpe cae en el material que las rodea).
 */
class EXPLORED_API FTerrainToolModel
{
public:
	/** Largo de la traza desde la cámara (m). */
	static constexpr float ReachMeters = 3.0f;
	/** Holgura del servidor sobre el alcance: el personaje se ha movido mientras viajaba la RPC. */
	static constexpr float ServerReachTolerance = 1.5f;
	/** Tope de |densidad| en el impacto: más lejos, el cliente no está golpeando el terreno. */
	static constexpr float MaxSurfaceDistance = 0.75f;
	/** Tierra que cabe en la carga del jugador (m³). Lo que sobra se pierde. */
	static constexpr double MaxCarriedSoil = 2.0;
	/** Pala (GDD v2 §3.4): alcanza el plano en 1 m y suaviza hasta 1,75 m. */
	static constexpr float ShovelRadius = 1.0f;
	static constexpr float ShovelEdge = 0.75f;
	/** Echar tierra: esfera de 0,5 m. */
	static constexpr float SoilRadius = 0.5f;
	/** Peso de roca en superficie a partir del cual el golpe cae en roca desnuda. */
	static constexpr float ExposedRockWeight = 0.6f;
	/** Solo cuenta la roca desnuda hasta esta profundidad (más abajo manda la tabla). */
	static constexpr float ExposedRockDepth = 1.0f;
	/** Peso de arena a partir del cual la superficie es playa. */
	static constexpr float SandySurfaceWeight = 0.5f;

	/** Herramienta de un objeto de items.json o de mining.json/tools; false si no cava. */
	static bool ToolFromItem(const FName& ItemId, ETerrainDigTool& OutTool);
	/** Pico: pica con los dos botones. Pala: principal aplana, secundario echa tierra. */
	static ETerrainToolAction ActionFor(ETerrainDigTool Tool, bool bSecondary);
	static bool IsShovel(ETerrainDigTool Tool) { return Tool == ETerrainDigTool::PalaTosca; }

	/** Estrato (como material del modelo) del punto golpeado. */
	static ETerrainMaterial ClassifyMaterial(const FTerrainStrataQuery& Query);

	/** Duración de un uso (s): la del golpe de la herramienta o la de una pasada de pala. */
	static float SecondsPerUse(ETerrainToolAction Action, ETerrainDigTool Tool);
	/** Validación del servidor, en este orden: valores, alcance, cadencia y superficie. */
	static ETerrainToolVerdict Validate(const FTerrainToolRequest& Request);

	/** Pasada de pala hacia el plano horizontal de los pies (FeetZ), centrada en el impacto. */
	static FShovelStroke MakeShovelStroke(const FVector& ImpactPoint, double FeetZ, ETerrainMaterial Material,
		int32 ToolTier, double CarriedSoil);
	/** Echar tierra en el punto de impacto con lo que se lleva. */
	static FSoilPlacement MakeSoilPlacement(const FVector& ImpactPoint, double CarriedSoil);

	/** Solo la arena y la tierra se pueden llevar y volver a echar. */
	static bool IsLooseSoil(ETerrainMaterial Material) { return Material == ETerrainMaterial::Arena || Material == ETerrainMaterial::Tierra; }
	/**
	 * Tierra que lleva el jugador tras una edición: suma lo arrancado si es tierra suelta,
	 * resta lo echado, y se queda en [0, MaxCarriedSoil]. Entradas no finitas no cambian nada.
	 */
	static double UpdateCarriedSoil(double Carried, ETerrainMaterial Material, double VolumeRemoved, double VolumeAdded);

	/**
	 * Lo que oye y ve el jugador en cuanto pulsa, antes de que responda el servidor (biblia
	 * 08: el cliente predice solo lo audiovisual). Rebote si la herramienta no llega al
	 * material; al echar tierra sin llevar nada, fallo; si no, golpe.
	 */
	static EMineHitCue PredictCue(ETerrainToolAction Action, ETerrainDigTool Tool, ETerrainMaterial Material, double CarriedSoil);
	/** Lo que ha pasado de verdad según el resultado del servidor. */
	static EMineHitCue CueFromResult(const FTerrainEditResult& Result);
};
