#pragma once

#include "CoreMinimal.h"
#include "WorldGen/TerrainChunkBuilder.h"
#include "WorldGen/TerrainEditModel.h"

class FTerrainDensity;
struct FSaveWorldDeltas;

/**
 * Herramientas de cavar del terreno volumétrico (biblia 02 §2.2). El orden no se guarda
 * en ninguna parte; el nivel (`ToolTier`) sí es el de `mining.json/tools`.
 */
enum class ETerrainDigTool : uint8
{
	PalaTosca,
	PicoPiedra,
	PicoTallado,
	PicoObsidiana,
	PicoRescatado,
	Count,
};

struct EXPLORED_API FTerrainDigToolInfo
{
	/** Nivel de herramienta: 1 pala tosca, 2 pico de piedra, 3 pico tallado, 4 obsidiana o rescatado. */
	int32 ToolTier = 1;
	/** Radio de la esfera que vacía cada golpe (m). Depende de la herramienta, no del material. */
	float Radius = 0.4f;
	/** Duración de un golpe (s), también si rebota. */
	float SecondsPerHit = 1.2f;
};

/** Un golpe de cavar. Metros. */
struct EXPLORED_API FTerrainDigHit
{
	/** Centro de la esfera: el punto de impacto de la herramienta (biblia 02 §2.1). */
	FVector ImpactPoint = FVector::ZeroVector;
	/** Estrato golpeado. La arcilla y el azufre se cavan como `Tierra` (mining.json/strata). */
	ETerrainMaterial Material = ETerrainMaterial::Tierra;
	ETerrainDigTool Tool = ETerrainDigTool::PalaTosca;
};

struct EXPLORED_API FTerrainDigResult
{
	/** Muestras y volumen; `Edit.DirtyChunks` son los chunks de edición (8 m) que hay que remallar. */
	FTerrainEditResult Edit;
	/** Chunks de `FTerrainChunkBuilder` (64 m) cuya malla cambia, ordenados por (Z, Y, X). */
	TArray<FIntVector> RenderChunks;
	/** Lo que dura el golpe (s). */
	float Seconds = 0.0f;
	/** Tope de sólido del golpe (m³): 1 / golpes por m³ del material con esa herramienta. */
	double MaxVolume = 0.0;
};

/**
 * Capa de ediciones del terreno (GDD v2 §7.3, biblia 02 §2): los deltas dispersos por
 * chunk de `FTerrainEditModel` puestos encima de la densidad procedural, con las reglas de
 * minería de H0 alrededor. `FTerrainDensity::Density` la consulta (SetEdits) antes de
 * evaluar el ruido, así que el constructor de chunks, las normales y cualquier consulta
 * de densidad ven el hueco sin saber nada de él.
 *
 * - Picado por esfera: cada golpe efectivo vacía una esfera centrada en el punto de
 *   impacto con el radio de la herramienta (biblia 02 §2.2) hasta el sólido de un golpe,
 *   1 / golpes por m³ del material (`FTerrainEditModel::DesignHitsPerCubicMeter`, espejo
 *   de `mining.json`), con la dureza extra de un camino compactado. Una herramienta que no
 *   llega rebota: no toca nada, pero el golpe dura lo mismo (biblia 02 §2.1).
 * - Invalidación: los chunks de edición que lee alguna muestra de la esfera (borde,
 *   arista y esquina incluidos) y los chunks de render cuya rejilla o normales leen la
 *   densidad cambiada.
 * - Guardado: capa «terrain» de `FSaveWorldDeltas` con `FTerrainEditModel::ToValue`
 *   (versión 2, binaria). Nula si no hay ediciones.
 *
 * Red (biblia 08 §2.2): autoridad del servidor. El cliente pide el golpe y el servidor
 * comprueba la cadencia (`IsCadenceValid`), llama a `Dig` y replica las muestras de
 * `Edit.DirtyChunks`; el cliente las aplica a su copia y remalla. `ChunkChecksum` es la
 * comprobación de cada 30 s. El cliente nunca cava por su cuenta.
 */
class EXPLORED_API FTerrainEdits
{
public:
	/** Objetos por m³ de sólido quitado (mining.json/unitsPerM3). */
	static constexpr int32 UnitsPerCubicMeter = 6;
	/** Tolerancia de cadencia del servidor: un golpe un 15 % más rápido de lo normal se descarta (biblia 08 §1.2). */
	static constexpr float CadenceTolerance = 0.15f;

	static const FTerrainDigToolInfo& ToolInfo(ETerrainDigTool Tool);
	/** Golpes para vaciar 1 m³ de ese material con esa herramienta; 0 si no puede. */
	static float HitsPerCubicMeter(ETerrainMaterial Material, ETerrainDigTool Tool);
	/** Segundos de golpes por m³ vaciado (golpes por m³ × duración del golpe); 0 si no puede. */
	static float SecondsPerCubicMeter(ETerrainMaterial Material, ETerrainDigTool Tool);
	/** El servidor acepta el golpe si ha pasado al menos el 85 % de su duración desde el anterior. */
	static bool IsCadenceValid(ETerrainDigTool Tool, float SecondsSinceLastHit);
	/** Objeto de items.json que suelta el material al cavarlo. */
	static const TCHAR* LootItemId(ETerrainMaterial Material);
	/** Unidades enteras que da el volumen quitado; el resto fraccionario se acumula para el siguiente golpe. */
	static int32 TakeLootUnits(double VolumeRemoved, double& InOutRemainder);
	/**
	 * Chunks de render que leen la densidad dentro de la caja (metros): la rejilla del
	 * chunk (una muestra de solape por lado) más media celda de las normales de los
	 * vértices. Ordenados por (Z, Y, X) y sin repetir.
	 */
	static void RenderChunksTouchingBox(const FBox& Meters, const FTerrainChunkSettings& Render, TArray<FIntVector>& Out);

	explicit FTerrainEdits(const FTerrainEditSettings& InSettings = FTerrainEditSettings(),
		const FTerrainChunkSettings& InRender = FTerrainChunkSettings());

	// --- Consultas ---

	/** Delta de densidad en P (m); false si no hay ninguna muestra editada alrededor. */
	bool DeltaAt(const FVector& P, float& OutDelta) const { return Model.DeltaAt(P, OutDelta); }
	/** Densidad final sobre un campo base cualquiera (el procedural en el juego). */
	float Density(const FVector& P, FTerrainEditModel::FBaseDensity Base) const { return Model.Density(P, Base); }
	uint32 ChunkChecksum(const FIntVector& EditChunk) const { return Model.ChunkChecksum(EditChunk); }
	/** Chunks de render que contienen alguna edición (para cargarlos aunque su altura no los proponga). */
	TArray<FIntVector> EditedRenderChunks() const;
	bool IsEmpty() const { return Model.IsEmpty(); }

	/**
	 * Lo que un golpe de esa herramienta en ese punto puede ensuciar como mucho, antes de
	 * darlo: chunks de edición y de render. Lo que devuelve `Dig` está siempre contenido.
	 */
	void ChunksToInvalidate(const FVector& ImpactPoint, ETerrainDigTool Tool, TArray<FIntVector>& OutEditChunks,
		TArray<FIntVector>& OutRenderChunks) const;

	// --- Edición ---

	/** Un golpe sobre un campo base (sin ediciones). */
	FTerrainDigResult Dig(const FTerrainDigHit& Hit, FTerrainEditModel::FBaseDensity ProceduralBase);
	/** Un golpe sobre el terreno del juego: la base es `FTerrainDensity::ProceduralDensity`. */
	FTerrainDigResult Dig(const FTerrainDigHit& Hit, const FTerrainDensity& Terrain);

	FTerrainEditModel& GetModel() { return Model; }
	const FTerrainEditModel& GetModel() const { return Model; }
	const FTerrainChunkSettings& GetRenderSettings() const { return RenderSettings; }

	// --- Guardado ---

	/** Escribe la capa «terrain» (nula si no hay ediciones). */
	void SaveTo(FSaveWorldDeltas& World) const;
	/**
	 * Lee la capa «terrain». Sin capa queda vacío y devuelve true; con una capa ilegible,
	 * truncada o de otra rejilla queda vacío y devuelve false (el mundo arranca sin cavar).
	 */
	bool LoadFrom(const FSaveWorldDeltas& World);

	bool operator==(const FTerrainEdits& Other) const { return Model == Other.Model; }

private:
	/** Chunks de render que leen alguna muestra a menos de SampleReach del centro. */
	void RenderChunksForSphere(const FVector& Center, float SampleReach, TArray<FIntVector>& Out) const;

	FTerrainEditModel Model;
	FTerrainChunkSettings RenderSettings;
};
