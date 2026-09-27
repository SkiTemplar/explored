#pragma once

#include "CoreMinimal.h"
#include "Save/SaveValue.h"
#include "WorldGen/SurfaceNets.h"

/**
 * Material del terreno a efectos de excavación (GDD v2 §3.4). La dureza y el nivel
 * mínimo de herramienta siguen `docs/balance/2026-09-27-mineria.md`; la arena es algo
 * más blanda que la tierra.
 */
enum class ETerrainMaterial : uint8
{
	Arena,
	Tierra,
	Caliza,
	Basalto,
	Obsidiana,
	Count,
};

struct EXPLORED_API FTerrainMaterialInfo
{
	/** Dureza relativa (tierra = 1). Divide lo que arranca cada golpe. */
	float Hardness = 1.0f;
	/** Nivel mínimo de herramienta: 1 pala tosca, 2 pico de piedra, 3 pico tallado, 4 obsidiana o rescatado. */
	int32 MinToolTier = 1;
};

/**
 * Rejilla de la capa de ediciones. Independiente de la del terreno horneado (2 m): una
 * galería o un peldaño necesitan más resolución. 32 celdas × 0,25 m = chunk de 8 m, que
 * encaja 8 veces en el chunk de 64 m de `FTerrainChunkSettings`.
 */
struct EXPLORED_API FTerrainEditSettings
{
	float CellSize = 0.25f;
	int32 CellsPerChunk = 32;

	float ChunkSizeMeters() const { return CellSize * CellsPerChunk; }
};

/** Qué ha hecho una edición. Los volúmenes están en m³ de sólido. */
struct EXPLORED_API FTerrainEditResult
{
	/** Chunks de edición que hay que remallar, ordenados por (Z, Y, X) y sin repetir. */
	TArray<FIntVector> DirtyChunks;
	/** Sólido quitado (tierra o roca que pasa al inventario). */
	double VolumeRemoved = 0.0;
	/** Sólido añadido (tierra que sale del inventario). */
	double VolumeAdded = 0.0;
	int32 SamplesChanged = 0;
	/** La herramienta no puede con el material: no se ha tocado nada. */
	bool bRejected = false;

	bool Changed() const { return SamplesChanged > 0; }
};

/** Un golpe de pico. Metros. */
struct EXPLORED_API FPickaxeHit
{
	FVector ImpactPoint = FVector::ZeroVector;
	/** Dirección del golpe, hacia dentro de la pared. */
	FVector Direction = FVector(0.0, 0.0, -1.0);
	ETerrainMaterial Material = ETerrainMaterial::Tierra;
	int32 ToolTier = 2;
	/** Semilla de la forma irregular del hueco (p. ej. contador de golpes del jugador). */
	uint32 Seed = 0;
};

/** Una pasada de pala: aplana hacia un plano con borde suave, compacta y marca camino. */
struct EXPLORED_API FShovelStroke
{
	/** Punto del plano objetivo (normalmente, bajo los pies del jugador). */
	FVector Center = FVector::ZeroVector;
	/** Normal del plano (hacia el aire). Se admite inclinada para rampas. */
	FVector PlaneNormal = FVector(0.0, 0.0, 1.0);
	/** Radio en el que se alcanza el plano; después, transición suave hasta Radius + EdgeWidth. */
	float Radius = 1.0f;
	float EdgeWidth = 0.75f;
	/** Solo se tocan muestras a menos de esta distancia del plano (una pala no arrasa un cerro). */
	float VerticalReach = 1.0f;
	ETerrainMaterial Material = ETerrainMaterial::Tierra;
	int32 ToolTier = 1;
	/** Tierra que lleva el jugador para rellenar huecos (m³). */
	double SoilBudget = 0.0;
	bool bMarkPath = true;
};

/** Echar tierra transportada: rellena una esfera hasta agotar lo que se lleva. */
struct EXPLORED_API FSoilPlacement
{
	FVector Center = FVector::ZeroVector;
	float Radius = 0.5f;
	double SoilBudget = 0.0;
};

/** Escalera picada en una ladera o en una mina. Se ajusta con FTerrainEditModel::SnapStairs. */
struct EXPLORED_API FStairCarve
{
	/** Pie del primer peldaño. */
	FVector Start = FVector::ZeroVector;
	/** Dirección de avance (solo cuenta la componente horizontal). */
	FVector Direction = FVector(1.0, 0.0, 0.0);
	/** Contrahuella: positiva sube, negativa baja (mina). */
	float StepRise = 0.3f;
	/** Huella. */
	float StepRun = 0.3f;
	int32 NumSteps = 6;
	float Width = 1.0f;
	/** Altura libre sobre cada huella (en ladera empinada o bajo tierra, la escalera es un túnel). */
	float Headroom = 2.2f;
	ETerrainMaterial Material = ETerrainMaterial::Tierra;
	int32 ToolTier = 2;
	/** Sólido máximo que arranca esta llamada (m³); 0 = sin tope. Tallar cuesta golpes. */
	double MaxVolume = 0.0;
};

/**
 * Terreno editable: deltas dispersos de densidad por chunk sobre el campo procedural
 * puro (`FTerrainDensity`). El campo base se pasa como función, así que el modelo se
 * prueba con campos analíticos y en el juego se engancha a `FTerrainDensity::Density`.
 *
 * - Las muestras están en la rejilla global `Global * CellSize` (metros). Cada chunk
 *   guarda `delta` (densidad final − base) en milímetros enteros: sin deriva al
 *   guardar ni al repetir la misma edición.
 * - Convenio de densidad de `FTerrainDensity`: < 0 sólido, > 0 aire, ~distancia en m.
 * - Volumen de sólido de una muestra: `Occupancy(d) = clamp(0.5 − d / CellSize, 0, 1)`
 *   por `CellSize³`. Es la cuenta con la que se conserva la tierra al cavar y rellenar.
 * - Antes de editar, la densidad de una muestra se lleva a la banda [−2·Celda, +Celda]
 *   (no cambia su volumen): la roca profunda no exige cien golpes por ser «muy sólida».
 * - Un chunk C lee las muestras globales [C·N − 1, C·N + N] en cada eje (convenio de
 *   `FTerrainChunkBuilder` y `FSurfaceNets`); toda edición marca sucios todos los chunks
 *   que leen alguna muestra cambiada.
 */
class EXPLORED_API FTerrainEditModel
{
public:
	using FBaseDensity = TFunctionRef<float(const FVector&)>;

	// --- Números de diseño (GDD v2 §3.4) ---

	/** Radio nominal del hueco de un golpe de pico y su irregularidad (±20 %). */
	static constexpr float PickaxeRadius = 0.5f;
	static constexpr float PickaxeIrregularity = 0.2f;
	/** El centro del pincel entra en la pared desde el punto de impacto. */
	static constexpr float PickaxeBite = 0.15f;
	/** Densidad que suma el centro del pincel en tierra (dureza 1) con la herramienta mínima. */
	static constexpr float PickaxeStrength = 1.1f;
	/** Cada nivel de herramienta por encima del mínimo multiplica lo que arranca por esto. */
	static constexpr float TierBonus = 1.5f;
	/**
	 * Suelo de golpes por m³: en blando (o con herramienta muy superior) el límite es el
	 * tamaño del hueco de un golpe (≈0,17 m³), no la dureza. Para mover tierra, la pala.
	 */
	static constexpr float MinHitsPerCubicMeter = 6.0f;
	static constexpr float SecondsPerPickaxeHit = 0.9f;
	static constexpr float SecondsPerShovelStroke = 1.2f;
	/** Cambio máximo de densidad por pasada de pala (m). */
	static constexpr float ShovelMaxChange = 0.25f;
	/** Compactación (0–100) que suma cada pasada de pala; ≥ PathThreshold es camino. */
	static constexpr int32 CompactionPerStroke = 34;
	static constexpr int32 PathThreshold = 50;
	/** Dureza extra de un camino totalmente compactado (+50 %). */
	static constexpr float CompactedHardnessBonus = 0.5f;
	/** Rejilla de las escaleras: origen y huella en múltiplos de 30 cm, contrahuella de 15 en 15 cm. */
	static constexpr float StairGrid = 0.3f;

	static const FTerrainMaterialInfo& MaterialInfo(ETerrainMaterial Material);
	/** Golpes por m³ de diseño: 6 × dureza con la herramienta mínima, ÷ 1,5 por nivel extra, nunca menos de 6; 0 si no puede. */
	static float DesignHitsPerCubicMeter(ETerrainMaterial Material, int32 ToolTier);
	/** Multiplicador de lo que arranca un golpe (0 si la herramienta no llega al material). */
	static float ToolFactor(ETerrainMaterial Material, int32 ToolTier);
	static float Occupancy(float Density, float CellSize);
	/** Escalera ajustada a la rejilla de diseño; devuelve false si la dirección es degenerada. */
	static bool SnapStairs(const FStairCarve& In, FStairCarve& Out);

	explicit FTerrainEditModel(const FTerrainEditSettings& InSettings = FTerrainEditSettings());

	// --- Herramientas ---

	FTerrainEditResult Pickaxe(const FPickaxeHit& Hit, FBaseDensity Base);
	FTerrainEditResult Shovel(const FShovelStroke& Stroke, FBaseDensity Base);
	FTerrainEditResult PlaceSoil(const FSoilPlacement& Placement, FBaseDensity Base);
	/** Talla la escalera tal cual (llamar antes a SnapStairs para ajustarla a la rejilla). */
	FTerrainEditResult CarveStairs(const FStairCarve& Stairs, FBaseDensity Base);

	// --- Consultas ---

	const FTerrainEditSettings& GetSettings() const { return Settings; }
	FVector SamplePosition(const FIntVector& Global) const;
	FIntVector ChunkOfSample(const FIntVector& Global) const;
	/** Delta guardado en una muestra (metros); 0 si no se ha tocado. */
	float SampleDelta(const FIntVector& Global) const;
	float SampleDensity(const FIntVector& Global, FBaseDensity Base) const;
	/** Densidad en cualquier punto: base + delta interpolado trilinealmente. */
	float Density(const FVector& P, FBaseDensity Base) const;
	/** Sólido (m³) de las muestras de la rejilla dentro de la caja. */
	double SolidVolume(const FBox& Box, FBaseDensity Base) const;
	/** Rejilla lista para `FSurfaceNets::Polygonize` del chunk de edición (N + 2 muestras por eje). */
	void BuildChunkGrid(const FIntVector& Chunk, FBaseDensity Base, FDensityGrid& Out) const;
	/** Chunks de edición que leen la muestra (1, 2, 4 u 8). */
	void ChunksReadingSample(const FIntVector& Global, TArray<FIntVector>& Out) const;

	/** Compactación 0–100 de la columna que contiene (X, Y). */
	int32 Compaction(double X, double Y) const;
	bool IsPath(double X, double Y) const { return Compaction(X, Y) >= PathThreshold; }
	/** Dureza de un material en un punto, con el extra de un camino compactado. */
	float HardnessAt(ETerrainMaterial Material, const FVector& P) const;

	TArray<FIntVector> EditedChunks() const;
	int32 NumEditedSamples() const;
	int32 NumPathColumns() const { return PathColumns.Num(); }
	bool IsEmpty() const { return Chunks.Num() == 0 && PathColumns.Num() == 0; }
	void Reset();

	// --- Guardado ---

	/**
	 * {"v":1,"cell":0.25,"n":32,"chunks":[[CX,CY,CZ,[Inicio,Cuenta,d…,Inicio,Cuenta,d…]],…],
	 *  "paths":[X,Y,C,…]}. Chunks, tramos y columnas en orden determinista.
	 */
	FSaveValue ToValue() const;
	/** Sustituye el contenido; devuelve false (y queda vacío) si el valor no es válido o es de otra rejilla. */
	bool FromValue(const FSaveValue& Value);

	bool operator==(const FTerrainEditModel& Other) const;
	bool operator!=(const FTerrainEditModel& Other) const { return !(*this == Other); }

	/** Delta máximo admitido al cargar (mm): acota ficheros manipulados. */
	static constexpr int32 MaxDeltaMm = 1000000;

private:
	struct FProposal
	{
		FIntVector Global;
		float Base = 0.0f;
		/** Densidad actual real. */
		float Old = 0.0f;
		/** Densidad propuesta (dentro de la banda). */
		float New = 0.0f;
	};

	float BandMin() const { return -2.0f * Settings.CellSize; }
	float BandMax() const { return Settings.CellSize; }
	float Band(float D) const { return FMath::Clamp(D, BandMin(), BandMax()); }

	/** Recorre las muestras de la rejilla dentro de la caja, en orden (Z, Y, X). */
	void ForEachSampleInBox(const FBox& Box, TFunctionRef<void(const FIntVector&, const FVector&)> Visit) const;
	/** Sólido que ganaría (> 0) o perdería (< 0) la propuesta escalada por S. */
	double ProposalVolume(const TArray<FProposal>& Proposals, float S) const;
	/** Mayor escala en [0, 1] cuyo |volumen| no pasa de Limit (bisección). */
	float ScaleToVolume(const TArray<FProposal>& Proposals, double Limit) const;
	void Commit(const TArray<FProposal>& Proposals, float Scale, FTerrainEditResult& Result);
	void SetDeltaMm(const FIntVector& Global, int32 DeltaMm);
	int32 GetDeltaMm(const FIntVector& Global) const;
	FIntPoint ColumnOf(double X, double Y) const;
	void AddCompaction(const FVector& Center, float Radius, int32 Amount);
	void ClearCompaction(const TArray<FProposal>& Proposals);

	FTerrainEditSettings Settings;
	/** Chunk → (índice local X + N·(Y + N·Z) → delta en mm). */
	TMap<FIntVector, TMap<int32, int32>> Chunks;
	/** Columna (X, Y) de la rejilla → compactación 0–100. */
	TMap<FIntPoint, int32> PathColumns;
};
