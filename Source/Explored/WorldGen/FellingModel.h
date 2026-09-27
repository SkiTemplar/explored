#pragma once

#include "CoreMinimal.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/GroundBranchModel.h"
#include "WorldGen/HarvestModel.h"

/**
 * Tala universal (GDD v2 §3.12): reglas puras de cómo cae un árbol, palmera o
 * arbusto de FVegetationScatter, qué suelta y dónde, y qué pasa con el tocón.
 * FHarvestModel sigue siendo la fuente de los golpes a mano y con filo (hierba y
 * roca no cambian); este modelo añade lo que la recolección no tenía:
 * golpes por clase de herramienta, rendimiento por categoría (troncos, ramas,
 * hojas, frutos), dirección de caída, tocón que rebrota y pala que lo arranca.
 *
 * El tiempo va en minutos de juego enteros (int64) para que el rebrote no
 * dependa del tamaño del paso ni acumule error de coma flotante.
 */

/** Clase de la herramienta en la mano; la capa de Unreal la deduce del objeto (etiquetas de items.json). */
enum class EFellingTool : uint8
{
	Hands = 0,	// a mano
	Blunt,		// «Contundente»: piedra, martillo
	Edge,		// «Filo»: hacha, cuchillo, lasca
	Shovel,		// pala (resultDefinitionId «pala»)
	Count
};

/** Categoría de lo que suelta el árbol al caer: decide dónde aparece a lo largo del tronco. */
enum class EFellingYieldKind : uint8
{
	Log,	// repartidos a lo largo del tronco caído
	Branch,	// en la copa
	Leaf,	// en la copa
	Fruit	// dispersos en el radio de la copa
};

struct EXPLORED_API FFellingYield
{
	FName ItemId;
	EFellingYieldKind Kind = EFellingYieldKind::Log;
	int32 MinCount = 1;
	int32 MaxCount = 1;
};

/** Perfil de tala de una especie leñosa (coincide con FScatterRule::Species). */
struct EXPLORED_API FFellingProfile
{
	FName Species;

	/** Golpes para tumbarlo con cada clase de herramienta (índice EFellingTool). 0 = esa herramienta no sirve. */
	int32 HitsByTool[(int32)EFellingTool::Count] = { 0, 0, 0, 0 };

	TArray<FFellingYield> Yields;

	/** Longitud del tronco al caer, en metros. 0 = no cae: se desbroza en el sitio (arbusto). */
	float HeightMeters = 0.0f;
	/** Radio de la copa, en metros: frutos al caer y ramas del suelo bajo el árbol en pie. */
	float CrownRadiusMeters = 1.0f;

	/** Días de juego desde la tala hasta que el tocón echa un brote. 0 = no rebrota. */
	int32 StumpRegrowDays = 0;
	/** Días de juego desde el brote hasta el ejemplar adulto (escala 1). */
	int32 SaplingToMatureDays = 1;
	/** Golpes de pala para arrancar el tocón (impide el rebrote). */
	int32 UprootShovelHits = 3;
	/** Lo que suelta el tocón arrancado. */
	TArray<FHarvestDrop> UprootDrops;

	/** Ramas sueltas que caben a la vez bajo un ejemplar en pie. */
	int32 GroundBranchCapacity = 0;
	/** Ramas nuevas por día de juego bajo un ejemplar en pie, en milésimas (entero: sin deriva). */
	int32 GroundBranchPerDayMilli = 0;
	/** Lo que se encuentra en el suelo bajo el ejemplar (rama_seca; hoja_palma bajo las palmeras). */
	FName GroundBranchItem;
};

/** Progreso de tala de una instancia en pie. */
struct EXPLORED_API FFellingProgress
{
	/** Trabajo acumulado, en unidades de FFellingModel::WorkToFell. */
	int32 Work = 0;
	/** Suma de las direcciones de los golpes (hacia dónde empuja cada golpe), ponderada por el trabajo de cada uno. */
	FVector2D Push = FVector2D::ZeroVector;
};

/** Una unidad suelta en el suelo al caer el árbol. */
struct EXPLORED_API FFellingDrop
{
	FName ItemId;
	EFellingYieldKind Kind = EFellingYieldKind::Log;
	/** Posición en el plano, en centímetros (unidades de Unreal). */
	FVector2D Position = FVector2D::ZeroVector;
};

enum class EStumpStage : uint8
{
	Stump,		// recién talado o sin rebrote
	Sapling,	// brote creciendo hacia adulto
	Mature,		// vuelve a ser un ejemplar talable
	Uprooted	// arrancado con pala: no vuelve
};

/** Tocón de una instancia talada; es lo que se persiste (ver docs/tecnico/tala-integracion.md). */
struct EXPLORED_API FStumpState
{
	int64 FelledAtMinute = 0;
	int32 UprootWork = 0;
	bool bUprooted = false;
};

struct EXPLORED_API FFellingModel
{
	static constexpr int64 MinutesPerDay = 1440;
	/**
	 * Trabajo para tumbar cualquier ejemplar: mcm(1..16). Cada golpe aporta
	 * WorkToFell / HitsByTool, que es exacto para 1..16 golpes, así que mezclar
	 * herramientas a media tala nunca deja el árbol «a una millonésima».
	 */
	static constexpr int32 WorkToFell = 720720;
	/** Peso de la pendiente frente al empuje de los golpes: a tan = 1 / SlopeWeight (≈ 27°) pesan igual. */
	static constexpr double SlopeWeight = 2.0;
	/** Escala del brote recién salido respecto al adulto. */
	static constexpr float SaplingStartScale = 0.15f;

	/** Perfiles por defecto de las especies leñosas de FVegetationScatter (Palm, JungleGiant, JungleWide, Mangrove, Understory, Shrub). */
	static TArray<FFellingProfile> DefaultProfiles();

	static const FFellingProfile* FindProfile(const TArray<FFellingProfile>& Profiles, FName Species);

	/** Golpes necesarios con esa herramienta; 0 si no sirve. */
	static int32 HitsRequired(const FFellingProfile& Profile, EFellingTool Tool);

	/**
	 * Aplica un golpe. HitDirection es hacia dónde va el golpe en el plano
	 * (del jugador al tronco); no hace falta normalizada. Devuelve true si el
	 * golpe tumba el ejemplar. Una herramienta que no sirve no hace nada.
	 */
	static bool ApplyHit(const FFellingProfile& Profile, FFellingProgress& Progress, EFellingTool Tool, const FVector2D& HitDirection);

	/**
	 * Dirección de caída (unitaria, en el plano). Combina el empuje medio de
	 * los golpes con la pendiente: Downhill apunta cuesta abajo y su módulo es
	 * la tangente de la pendiente (|∇h|). Si ambos se anulan (golpes opuestos
	 * en llano), cae hacia una dirección fija derivada de InstanceSeed.
	 */
	static FVector2D ResolveFallDirection(const FFellingProgress& Progress, const FVector2D& Downhill, uint32 InstanceSeed);

	/**
	 * Tira el rendimiento y coloca cada unidad: troncos repartidos a lo largo
	 * del tronco caído, ramas y hojas en la copa, frutos en el radio de la
	 * copa. Base en centímetros. Cada unidad es un FFellingDrop (Count 1).
	 */
	static TArray<FFellingDrop> ComputeFellDrops(const FFellingProfile& Profile, const FVector2D& Base, const FVector2D& FallDirection, FExploredRandom& Random);

	/** Etapa del tocón a esa hora. Una hora anterior a la tala (reloj corrupto) cuenta como tocón. */
	static EStumpStage StageAt(const FFellingProfile& Profile, const FStumpState& Stump, int64 NowMinute);

	/** Escala del ejemplar que rebrota: 0 tocón o arrancado, SaplingStartScale → 1 durante el crecimiento, 1 adulto. */
	static float GrowthScaleAt(const FFellingProfile& Profile, const FStumpState& Stump, int64 NowMinute);

	/**
	 * Golpe de herramienta al tocón. Solo la pala avanza; devuelve true el golpe
	 * que lo arranca. Un tocón ya arrancado o ya rebrotado a adulto no se arranca
	 * (el adulto se tala otra vez).
	 */
	static bool ApplyUprootHit(const FFellingProfile& Profile, FStumpState& Stump, EFellingTool Tool, int64 NowMinute);

	/** Fuente de ramas del suelo de un ejemplar en pie de este perfil (Position en centímetros). */
	static FGroundBranchSource MakeBranchSource(const FFellingProfile& Profile, const FVector2D& Position);

	/** Celda a la que pertenece una posición (suelo, no truncamiento: bien en coordenadas negativas). */
	static FIntPoint CellOf(const FVector2D& Position, double CellSize);
};
