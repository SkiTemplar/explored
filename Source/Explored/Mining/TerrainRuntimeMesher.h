#pragma once

#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include "UObject/Object.h"

#include "WorldGen/TerrainChunkBuilder.h"
#include "WorldGen/TerrainEditModel.h"
#include "WorldGen/TerrainRemeshQueueModel.h"
#include "WorldGen/TerrainReplacementModel.h"

#include "TerrainRuntimeMesher.generated.h"

class AActor;
class FTerrainDensity;
class ULevel;
class UMaterialInterface;
class UProceduralMeshComponent;
class UWorld;
struct FDensityGrid;
struct FTerrainRemeshOutput;

/** Coste medido del remallado en tiempo de ejecución (ms). */
USTRUCT(BlueprintType)
struct EXPLORED_API FTerrainRemeshStats
{
	GENERATED_BODY()

	/** Tareas de remallado terminadas y su coste en el hilo de fondo. */
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Terreno")
	int32 TasksCompleted = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Terreno")
	float TaskMsAverage = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Terreno")
	float TaskMsMax = 0.0f;

	/** Coste en el hilo de juego del último Tick y el máximo visto (sin contar Flush). */
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Terreno")
	float GameThreadMsLastTick = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Terreno")
	float GameThreadMsMax = 0.0f;
	/** Lo más caro que ha costado volcar UN chunk en su componente (hilo de juego). */
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Terreno")
	float ApplyMsMax = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Terreno")
	int32 ChunksApplied = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Terreno")
	int32 RenderChunksReplaced = 0;

	double TaskMsTotal = 0.0;
};

/**
 * Remallado del terreno editado en tiempo de ejecución (docs/tecnico/terreno-editable.md).
 *
 * - Un chunk horneado de 64 m (Nanite, `SM_Terrain_X_Y_Z`) que recibe su primera edición
 *   se sustituye por un `UProceduralMeshComponent` por cada chunk de edición de 8 m con
 *   superficie, a 0,25 m. El horneado sigue visible mientras se construyen y se oculta de
 *   golpe al terminar; su colisión se apaga cuando la colisión fina ya está cocinada.
 * - Cada chunk sucio se malla en una tarea de fondo (`UE::Tasks`): campo base (cacheado
 *   para los chunks editados), deltas, Surface Nets, normales, color y capas, y la sección
 *   del componente ya montada. El hilo de juego solo copia los deltas al lanzar y vuelca
 *   la sección al volver, con un presupuesto en ms por fotograma.
 * - La colisión es la malla compleja del propio componente (cocinado asíncrono de Chaos
 *   por defecto): el jugador puede entrar en el hueco en cuanto se cocina.
 *
 * Todo en el hilo de juego salvo las tareas, que solo leen el campo procedural (inmutable)
 * y copias de los deltas.
 */
UCLASS(Transient)
class EXPLORED_API UTerrainRuntimeMesher : public UObject
{
	GENERATED_BODY()

public:
	UTerrainRuntimeMesher();
	virtual void BeginDestroy() override;

	void Initialize(UWorld& InWorld, TSharedRef<const FTerrainDensity> InDensity, const FTerrainEditSettings& InEdit,
		const FTerrainChunkSettings& InRender);

	/** Sustituye estos chunks horneados (idempotente). */
	void RequestReplacement(const TArray<FIntVector>& RenderChunks);
	/** Chunks de edición que hay que volver a mallar. */
	void MarkDirty(const TArray<FIntVector>& EditChunks);

	/** Trabajo del fotograma dentro del presupuesto. Model es la copia del terreno de esta máquina. */
	void Tick(const FTerrainEditModel& Model, const FVector& ViewerMeters, double BudgetMs);
	/** Termina todo lo pendiente sin presupuesto (tests, carga de partida). false si vence el plazo. */
	bool Flush(const FTerrainEditModel& Model, double TimeoutSeconds);
	/** Deja todo horneado otra vez: borra las mallas finas y vuelve a mostrar los chunks horneados. */
	void ResetAll();

	/** Registra los chunks horneados de un nivel recién cargado (World Partition) y oculta los sustituidos. */
	void RegisterBakedActorsInLevel(ULevel* Level);

	bool IsIdle() const;
	/** Hay tareas, resultados o un cambio de colisión pendiente. */
	bool NeedsTick() const { return !IsIdle() || !CollisionSwaps.IsEmpty(); }
	UProceduralMeshComponent* FindChunkComponent(const FIntVector& EditChunk) const;
	ETerrainReplacementState GetReplacementState(const FIntVector& RenderChunk) const { return Replacement.GetState(RenderChunk); }
	const FTerrainRemeshStats& GetStats() const { return Stats; }
	void ResetStats() { Stats = FTerrainRemeshStats(); }
	/**
	 * Cocinado de colisión asíncrono (por defecto; también cambia los componentes que ya
	 * existen). Los tests lo quitan para comprobar la colisión al momento.
	 */
	void SetAsyncCollisionCooking(bool bAsync);
	/** Algún componente tiene colisión nueva sin cocinar todavía. */
	bool IsCollisionCookPending();
	int32 EditChunksPerRender() const { return EditPerRender; }
	/**
	 * Material de las mallas finas por referencia (ajuste de proyecto `UExploredTerrainSettings`).
	 * Manda sobre el de los chunks horneados y se aplica también a las mallas ya creadas.
	 */
	void SetMaterial(UMaterialInterface* InMaterial);
	UMaterialInterface* GetMaterial() const { return Material; }

	/** Etiqueta de los actores de terreno (horneados y el de las mallas finas). */
	static const FName TerrainTag;

private:
	struct FSurveyJob
	{
		FIntVector RenderChunk;
		UE::Tasks::TTask<TArray<FIntVector>> Task;
	};
	struct FRemeshJob
	{
		FIntVector Chunk;
		UE::Tasks::TTask<TSharedPtr<FTerrainRemeshOutput>> Task;
	};
	struct FCollisionSwap
	{
		FIntVector RenderChunk;
		double StartSeconds = 0.0;
	};
	struct FBaseCacheEntry
	{
		TSharedPtr<const FDensityGrid> Grid;
		uint64 LastUse = 0;
	};

	void CollectSurveys(const FTerrainEditModel& Model, bool bWait);
	void CollectJobs(bool bWait);
	/** Vuelca resultados hasta gastar el presupuesto (al menos uno); BudgetMs < 0 = todos. */
	void ApplyReady(double StartSeconds, double BudgetMs);
	void ApplyResult(FTerrainRemeshOutput& Result);
	void StartJobs(const FTerrainEditModel& Model, const FVector& ViewerMeters, int32 MaxStarts, int32 MaxInFlight);
	void StartJob(const FTerrainEditModel& Model, const FIntVector& Chunk);
	void OnRenderChunkReplaced(const FIntVector& RenderChunk);
	void UpdateCollisionSwaps(bool bForce);
	bool IsCollisionPending(const FIntVector& EditChunk);

	UProceduralMeshComponent* FindOrCreateComponent(const FIntVector& EditChunk);
	AActor* EnsureRuntimeActor();
	void SetBakedHidden(const FIntVector& RenderChunk, bool bHidden);
	void SetBakedCollision(const FIntVector& RenderChunk, bool bEnabled);
	void SetMaterialFromBaked(UMaterialInterface* BakedMaterial);
	void ApplyMaterialToComponents();
	void CacheBase(const FIntVector& Chunk, TSharedPtr<const FDensityGrid> Grid);

	TWeakObjectPtr<UWorld> World;
	TSharedPtr<const FTerrainDensity> Density;
	FTerrainEditSettings EditSettings;
	FTerrainChunkSettings RenderSettings;
	int32 EditPerRender = 8;
	/** Muestras del faldón en la cara baja de un chunk de render (una celda horneada: 2 m / 0,25 m). */
	int32 SkirtSamples = 8;

	UPROPERTY()
	TObjectPtr<AActor> RuntimeActor;
	UPROPERTY()
	TMap<FIntVector, TObjectPtr<UProceduralMeshComponent>> ChunkComponents;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;

	TMap<FIntVector, TWeakObjectPtr<AActor>> BakedActors;
	FTerrainRemeshQueueModel Queue;
	FTerrainReplacementModel Replacement;
	TArray<FSurveyJob> Surveys;
	TArray<FRemeshJob> Jobs;
	TArray<TSharedPtr<FTerrainRemeshOutput>> Ready;
	TMap<FIntVector, FBaseCacheEntry> BaseCache;
	uint64 BaseCacheClock = 0;
	/** Colisión de cada chunk antes de su último volcado: cuando cambia, ya está cocinada la nueva. */
	TMap<FIntVector, TWeakObjectPtr<UObject>> PendingCookSetups;
	TArray<FCollisionSwap> CollisionSwaps;
	/** Chunks de render cuya colisión horneada ya está apagada. */
	TSet<FIntVector> BakedCollisionOff;
	FTerrainRemeshStats Stats;
	/** Coste estimado de volcar un chunk (ms), para no empezar uno que no cabe en el presupuesto. */
	double ApplyMsEstimate = 0.3;
	bool bAsyncCooking = true;
	/** El material vino de SetMaterial: el de los horneados ya no lo sustituye. */
	bool bExplicitMaterial = false;
};
