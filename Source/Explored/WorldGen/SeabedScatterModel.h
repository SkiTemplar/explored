#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"

/**
 * Zona del fondo tal como la expone (o la expondrá) `worldgen/realismo-terreno`: arena,
 * arrecife o roca. Deliberadamente solo estas tres: es el contrato mínimo que esa rama
 * promete, así que `FSeabedScatterModel` no depende de su implementación ni de
 * `FTerrainDensity`. El «borde del arrecife» que pide el diseño (abanicos y esponjas
 * grandes) no es una zona propia: es la franja más profunda de `Reef` (ver
 * `FSeabedScatterRule::MinDepthM`/`MaxDepthM` en `DefaultRules()`).
 */
enum class ESeabedZone : uint8
{
	Sand,
	Rock,
	Reef,
};

/**
 * Lo que sabe el fondo en un punto (X, Y) del mundo, en metros. Lo da quien hornea
 * (`FSeabedScatterModel::FSampleColumn`); este fichero no sabe de dónde sale.
 */
struct EXPLORED_API FSeabedColumn
{
	/** Metros bajo el nivel del mar. 0 o menos = fuera del agua (nunca debe llegar sembrado). */
	float DepthM = 0.0f;
	/** Pendiente del fondo en grados: 0 = plano, 90 = pared vertical. */
	float SlopeDeg = 0.0f;
	ESeabedZone Zone = ESeabedZone::Sand;
};

/** Categoría de decorado (docs/diseno §fondo marino): agrupa las reglas por lo que pide cada zona. */
enum class ESeabedCategory : uint8
{
	ReefCoral,   // Arrecife somero (1-4 m): coral denso y variado.
	ReefEdge,    // Borde del arrecife (más profundo, más pendiente): abanicos y esponjas grandes.
	SandMeadow,  // Arena: praderas marinas.
	SandShell,   // Arena: conchas sueltas.
	RockKelp,    // Roca: algas.
	RockUrchin,  // Roca: erizos y estrellas de mar.
	RockFormation, // Roca: bloques y cantos submarinos sueltos (estructura, no organismo).
};

/**
 * Una regla de siembra: una especie, su categoría, el rango de profundidad/pendiente/zona
 * donde aparece y cómo de densa y agrupada sale. Mismo patrón que `FBeachDebrisRule`:
 * `Meshes` empieza vacío y lo rellena el horneado (`ResolveSeabedMeshes` en
 * `WorldGenCommandlet.cpp`) contra `Content/Data/seabed_scatter.json`; `Generate()` no
 * necesita que estén resueltas para poder probarse en el host.
 */
struct EXPLORED_API FSeabedScatterRule
{
	FName Species;
	ESeabedCategory Category = ESeabedCategory::ReefCoral;
	TArray<FSoftObjectPath> Meshes;
	/** Peso por arquetipo de isla (0 o ausente = no aparece en esa isla). */
	TMap<EIslandArchetype, float> IslandWeight;
	float MinScale = 0.7f;
	float MaxScale = 1.3f;
	/** Paso de la rejilla en metros: junto a Chance, gobierna la densidad máxima (≈ 1/Spacing² por m²). */
	float Spacing = 1.5f;
	/** 0 = uniforme (praderas, conchas); > 0 = agrupado en manchas de ruido (coral, algas). */
	float ClusterThreshold = 0.0f;
	float MinDepthM = 1.0f;
	float MaxDepthM = 40.0f;
	float MinSlopeDeg = 0.0f;
	float MaxSlopeDeg = 90.0f;
	ESeabedZone Zone = ESeabedZone::Reef;
	/**
	 * Sin colisión en el coral pequeño y decorativo (petición del director: nadar entre
	 * coral no debe engancharse ni gastar físicas). Las piezas grandes que puedan estorbar
	 * la navegación (roca submarina) sí la llevan; ver ResolveSeabedMeshes.
	 */
	bool bCollisionEnabled = false;
	float CullStartDistanceM = 0.0f;
	float CullEndDistanceM = 40.0f;
};

/** Instancia resultante (transformación en centímetros, espacio de mundo, como FScatterInstance). */
struct EXPLORED_API FSeabedScatterInstance
{
	FName Species;
	ESeabedCategory Category = ESeabedCategory::ReefCoral;
	int32 MeshIndex = 0;
	FTransform Transform;
};

/**
 * Siembra determinista del fondo marino (arrecife, praderas, roca): corales, abanicos y
 * esponjas, praderas y conchas, algas y erizos. Modelo puro, como `FVegetationScatter` y
 * `FBeachDebrisModel`: solo depende de `CoreMinimal.h` y de `EIslandArchetype` (adelantada).
 *
 * Decoupling deliberado de la rama `worldgen/realismo-terreno`: en vez de recibir
 * `FTerrainDensity`, recibe un `FSampleColumn` que da profundidad, pendiente y zona por
 * punto. El día que esa rama aterrice, quien hornea (`WorldGenCommandlet.cpp`) envuelve su
 * consulta real en ese callback; este fichero no cambia.
 */
struct EXPLORED_API FSeabedScatterModel
{
	using FSampleColumn = TFunctionRef<FSeabedColumn(float X, float Y)>;

	/**
	 * Bajo esta profundidad, zona de rompiente: oleaje constante que no deja crecer nada
	 * (coral, praderas o lo que sea). Se aplica a todas las reglas, no es parte de ninguna.
	 */
	static constexpr float SurfBreakDepthM = 0.6f;

	/** Reglas por defecto del archipiélago (una por especie del diseño; ver docs/diseno). */
	static TArray<FSeabedScatterRule> DefaultRules();

	/**
	 * Siembra una celda rectangular del fondo marino.
	 * @param CellBoundsM Rectángulo a sembrar, en metros de mundo.
	 * @param SampleColumn Profundidad/pendiente/zona en (X, Y); la resuelve quien llama.
	 * @param AvoidPoints Canales de navegación de las balsas (y cualquier otro punto a
	 *        proteger), en metros; ninguna instancia cae a menos de AvoidRadiusM de ninguno.
	 * @param InstanceBudget Tope de instancias de esta llamada. La rejilla se recorre en
	 *        orden fijo (reglas, luego Y, luego X), así que truncar en el mismo N con el
	 *        mismo Seed da siempre el mismo resultado: sigue siendo determinista.
	 */
	static TArray<FSeabedScatterInstance> Generate(
		const FBox2D& CellBoundsM,
		FSampleColumn SampleColumn,
		const TArray<FSeabedScatterRule>& Rules,
		uint32 Seed,
		EIslandArchetype Archetype,
		const TArray<FVector2D>& AvoidPoints,
		float AvoidRadiusM = 8.0f,
		int32 InstanceBudget = 2000);
};
