#include "Mining/TerrainRuntimeMesher.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "WorldGen/TerrainDensity.h"
#include "WorldGen/TerrainRemeshModel.h"

/** Resultado de una tarea de remallado: la sección lista para el componente. */
struct FTerrainRemeshOutput
{
	FIntVector Chunk = FIntVector::ZeroValue;
	FProcMeshSection Section;
	/** Campo base calculado por la tarea, para la caché (solo en chunks editados). */
	TSharedPtr<const FDensityGrid> NewBase;
	int32 NumTriangles = 0;
	double TaskMs = 0.0;
};

namespace TerrainRuntimeMesherDetail
{
	/** Tope de la caché del campo base (156 KB por chunk). */
	constexpr int32 MaxCachedBases = 96;
	constexpr int32 MaxStartsPerTick = 6;
	constexpr int32 MaxJobsInFlight = 12;

	/** Convierte la malla del modelo a la sección del componente, con los canales que usa M_Terrain. */
	void ToSection(const FTerrainMeshData& Mesh, const FVector& OriginMeters, FProcMeshSection& Out)
	{
		const int32 Count = Mesh.Positions.Num();
		Out.ProcVertexBuffer.SetNum(Count);
		Out.SectionLocalBox = FBox(ForceInit);
		for (int32 I = 0; I < Count; ++I)
		{
			FProcMeshVertex& V = Out.ProcVertexBuffer[I];
			V.Position = FVector(Mesh.Positions[I]);
			V.Normal = FVector(Mesh.Normals[I]);
			// Tangente: el eje X del mundo sobre la superficie (UV0 va en X/Y del mundo).
			const FVector Tangent = (FVector::XAxisVector - V.Normal * V.Normal.X).GetSafeNormal(UE_SMALL_NUMBER, FVector::YAxisVector);
			V.Tangent = FProcMeshTangent(Tangent, false);
			const FLinearColor Color = Mesh.Colors.IsValidIndex(I) ? Mesh.Colors[I] : FLinearColor::White;
			// Mismo paso a sRGB que el constructor de mallas estáticas del horneado.
			V.Color = Color.ToFColor(true);
			const FVector WorldMeters = OriginMeters + V.Position / 100.0;
			V.UV0 = FVector2D(WorldMeters.X, WorldMeters.Y) / 4.0;
			const FVector4f L = Mesh.Layers.IsValidIndex(I) ? Mesh.Layers[I] : FVector4f(0.0f, 0.0f, Color.A, 0.5f);
			V.UV1 = FVector2D(L.X, L.Y);
			V.UV2 = FVector2D(L.Z, L.W);
			V.UV3 = FVector2D::ZeroVector;
			Out.SectionLocalBox += V.Position;
		}
		Out.ProcIndexBuffer = Mesh.Indices;
		Out.bEnableCollision = true;
		Out.bSectionVisible = true;
	}

	TSharedPtr<FTerrainRemeshOutput> RunRemesh(const TSharedRef<const FTerrainDensity>& Density, const FTerrainEditSettings& Edit,
		const FIntVector& Chunk, const FTerrainGridWindow& Window, TSharedPtr<const FDensityGrid> CachedBase, const TArray<FTerrainGridDelta>& Deltas, bool bKeepBase)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TerrainRemeshTask);
		const double Start = FPlatformTime::Seconds();
		TSharedPtr<FTerrainRemeshOutput> Out = MakeShared<FTerrainRemeshOutput>();
		Out->Chunk = Chunk;
		if (!CachedBase)
		{
			TSharedRef<FDensityGrid> Built = MakeShared<FDensityGrid>();
			FTerrainRemeshModel::BuildBaseGrid(*Density, Edit, Window, *Built);
			CachedBase = Built;
			if (bKeepBase)
			{
				Out->NewBase = CachedBase;
			}
		}
		FDensityGrid Grid = *CachedBase;
		FTerrainRemeshModel::ApplyDeltas(Deltas, Grid);
		const FVector Origin = FTerrainRemeshModel::EditChunkOrigin(Chunk, Edit);
		const FTerrainMeshData Mesh = FTerrainRemeshModel::BuildMesh(Grid, Origin, &Density.Get());
		Out->NumTriangles = Mesh.NumTriangles();
		if (Out->NumTriangles > 0)
		{
			ToSection(Mesh, Origin, Out->Section);
		}
		Out->TaskMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		return Out;
	}

	double ElapsedMs(double StartSeconds)
	{
		return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	}
}

const FName UTerrainRuntimeMesher::TerrainTag(TEXT("ExploredTerrain"));

UTerrainRuntimeMesher::UTerrainRuntimeMesher() = default;

void UTerrainRuntimeMesher::BeginDestroy()
{
	// Las tareas solo comparten punteros compartidos: se dejan terminar solas.
	Surveys.Reset();
	Jobs.Reset();
	Ready.Reset();
	Super::BeginDestroy();
}

void UTerrainRuntimeMesher::Initialize(UWorld& InWorld, TSharedRef<const FTerrainDensity> InDensity,
	const FTerrainEditSettings& InEdit, const FTerrainChunkSettings& InRender)
{
	World = &InWorld;
	Density = InDensity;
	EditSettings = InEdit;
	RenderSettings = InRender;
	EditPerRender = FMath::Max(1, FTerrainRemeshModel::EditChunksPerRenderChunk(InRender, InEdit));
	SkirtSamples = FMath::Max(0, FMath::RoundToInt32(InRender.VoxelSize / FMath::Max(0.01f, InEdit.CellSize)));
	Replacement = FTerrainReplacementModel(EditPerRender);
	// El material sale del primer chunk horneado que se registre (SetMaterialFromBaked).
	for (ULevel* Level : InWorld.GetLevels())
	{
		RegisterBakedActorsInLevel(Level);
	}
}

void UTerrainRuntimeMesher::RequestReplacement(const TArray<FIntVector>& RenderChunks)
{
	using namespace TerrainRuntimeMesherDetail;
	for (const FIntVector& RenderChunk : RenderChunks)
	{
		if (!Replacement.Request(RenderChunk))
		{
			continue;
		}
		const TSharedRef<const FTerrainDensity> DensityRef = Density.ToSharedRef();
		const FTerrainChunkSettings Render = RenderSettings;
		const FTerrainEditSettings Edit = EditSettings;
		FSurveyJob& Job = Surveys.AddDefaulted_GetRef();
		Job.RenderChunk = RenderChunk;
		Job.Task = UE::Tasks::Launch(UE_SOURCE_LOCATION,
			[DensityRef, Render, Edit, RenderChunk]()
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(TerrainSurveyTask);
				TArray<FIntVector> Out;
				FTerrainRemeshModel::SurfaceEditChunks(*DensityRef, Render, Edit, RenderChunk, Out);
				return Out;
			},
			UE::Tasks::ETaskPriority::BackgroundNormal);
	}
}

void UTerrainRuntimeMesher::MarkDirty(const TArray<FIntVector>& EditChunks)
{
	Queue.MarkDirty(EditChunks);
}

bool UTerrainRuntimeMesher::IsIdle() const
{
	return Surveys.IsEmpty() && Jobs.IsEmpty() && Ready.IsEmpty() && Queue.IsIdle();
}

UProceduralMeshComponent* UTerrainRuntimeMesher::FindChunkComponent(const FIntVector& EditChunk) const
{
	const TObjectPtr<UProceduralMeshComponent>* Found = ChunkComponents.Find(EditChunk);
	return Found ? Found->Get() : nullptr;
}

void UTerrainRuntimeMesher::Tick(const FTerrainEditModel& Model, const FVector& ViewerMeters, double BudgetMs)
{
	using namespace TerrainRuntimeMesherDetail;
	TRACE_CPUPROFILER_EVENT_SCOPE(TerrainRuntimeMesherTick);
	const double Start = FPlatformTime::Seconds();
	CollectSurveys(Model, false);
	CollectJobs(false);
	ApplyReady(Start, FMath::Max(0.0, BudgetMs));
	if (ElapsedMs(Start) < BudgetMs)
	{
		StartJobs(Model, ViewerMeters, MaxStartsPerTick, MaxJobsInFlight);
	}
	UpdateCollisionSwaps(false);
	Stats.GameThreadMsLastTick = static_cast<float>(ElapsedMs(Start));
	Stats.GameThreadMsMax = FMath::Max(Stats.GameThreadMsMax, Stats.GameThreadMsLastTick);
}

bool UTerrainRuntimeMesher::Flush(const FTerrainEditModel& Model, double TimeoutSeconds)
{
	const double Deadline = FPlatformTime::Seconds() + FMath::Max(0.0, TimeoutSeconds);
	while (!IsIdle())
	{
		CollectSurveys(Model, true);
		Queue.ClearThrottle();
		StartJobs(Model, FVector::ZeroVector, MAX_int32, MAX_int32);
		CollectJobs(true);
		ApplyReady(FPlatformTime::Seconds(), -1.0);
		if (FPlatformTime::Seconds() > Deadline)
		{
			return false;
		}
	}
	UpdateCollisionSwaps(!bAsyncCooking);
	return true;
}

void UTerrainRuntimeMesher::CollectSurveys(const FTerrainEditModel& Model, bool bWait)
{
	for (int32 I = 0; I < Surveys.Num();)
	{
		FSurveyJob& Job = Surveys[I];
		if (bWait)
		{
			Job.Task.Wait();
		}
		if (!Job.Task.IsCompleted())
		{
			++I;
			continue;
		}
		TArray<FIntVector> Required = Job.Task.GetResult();
		FTerrainRemeshModel::EditedEditChunks(Model, EditPerRender, Job.RenderChunk, Required);
		TArray<FIntVector> Completed;
		Replacement.SetRequired(Job.RenderChunk, Required, Completed);
		for (const FIntVector& Chunk : Required)
		{
			if (!Replacement.IsMeshed(Chunk))
			{
				Queue.MarkDirty(Chunk);
			}
		}
		for (const FIntVector& RenderChunk : Completed)
		{
			OnRenderChunkReplaced(RenderChunk);
		}
		Surveys.RemoveAtSwap(I);
	}
}

void UTerrainRuntimeMesher::CollectJobs(bool bWait)
{
	for (int32 I = 0; I < Jobs.Num();)
	{
		if (bWait)
		{
			Jobs[I].Task.Wait();
		}
		if (!Jobs[I].Task.IsCompleted())
		{
			++I;
			continue;
		}
		Ready.Add(Jobs[I].Task.GetResult());
		// Orden estable: los resultados se vuelcan en el orden en que se lanzaron.
		Jobs.RemoveAt(I);
	}
}

void UTerrainRuntimeMesher::ApplyReady(double StartSeconds, double BudgetMs)
{
	int32 Applied = 0;
	for (; Applied < Ready.Num(); ++Applied)
	{
		if (BudgetMs >= 0.0 && Applied > 0 && TerrainRuntimeMesherDetail::ElapsedMs(StartSeconds) >= BudgetMs)
		{
			break;
		}
		if (Ready[Applied])
		{
			ApplyResult(*Ready[Applied]);
		}
	}
	Ready.RemoveAt(0, Applied);
}

void UTerrainRuntimeMesher::ApplyResult(FTerrainRemeshOutput& Result)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TerrainApplyChunk);
	const double Start = FPlatformTime::Seconds();
	UProceduralMeshComponent* Component = FindChunkComponent(Result.Chunk);
	if (Result.NumTriangles > 0)
	{
		Component = Component ? Component : FindOrCreateComponent(Result.Chunk);
	}
	if (Component)
	{
		if (bAsyncCooking)
		{
			PendingCookSetups.Add(Result.Chunk, Component->GetBodySetup());
		}
		if (Result.NumTriangles > 0)
		{
			Component->SetProcMeshSection(0, Result.Section);
		}
		else
		{
			Component->ClearAllMeshSections();
		}
	}
	if (Result.NewBase)
	{
		CacheBase(Result.Chunk, Result.NewBase);
	}
	Queue.MarkFinished(Result.Chunk);
	TArray<FIntVector> Completed;
	Replacement.OnEditChunkMeshed(Result.Chunk, Completed);
	for (const FIntVector& RenderChunk : Completed)
	{
		OnRenderChunkReplaced(RenderChunk);
	}

	++Stats.TasksCompleted;
	++Stats.ChunksApplied;
	Stats.TaskMsTotal += Result.TaskMs;
	Stats.TaskMsAverage = static_cast<float>(Stats.TaskMsTotal / Stats.TasksCompleted);
	Stats.TaskMsMax = FMath::Max(Stats.TaskMsMax, static_cast<float>(Result.TaskMs));
	Stats.ApplyMsMax = FMath::Max(Stats.ApplyMsMax, static_cast<float>(TerrainRuntimeMesherDetail::ElapsedMs(Start)));
}

void UTerrainRuntimeMesher::StartJobs(const FTerrainEditModel& Model, const FVector& ViewerMeters, int32 MaxStarts, int32 MaxInFlight)
{
	const double Half = 0.5 * EditSettings.ChunkSizeMeters();
	TArray<FIntVector> Starts;
	Queue.SelectStarts(FPlatformTime::Seconds(), MaxInFlight, MaxStarts,
		[&](const FIntVector& Chunk)
		{
			const FVector Center = FTerrainRemeshModel::EditChunkOrigin(Chunk, EditSettings) + FVector(Half);
			return FVector::DistSquared(Center, ViewerMeters);
		},
		Starts);
	for (const FIntVector& Chunk : Starts)
	{
		StartJob(Model, Chunk);
	}
}

void UTerrainRuntimeMesher::StartJob(const FTerrainEditModel& Model, const FIntVector& Chunk)
{
	const FTerrainGridWindow Window = FTerrainRemeshModel::ChunkWindow(Chunk, EditSettings, EditPerRender, SkirtSamples);
	TArray<FTerrainGridDelta> Deltas;
	FTerrainRemeshModel::GatherDeltas(Model, Window, Deltas);
	TSharedPtr<const FDensityGrid> Cached;
	if (FBaseCacheEntry* Entry = BaseCache.Find(Chunk))
	{
		Entry->LastUse = ++BaseCacheClock;
		Cached = Entry->Grid;
	}
	// Solo se cachea el campo base de los chunks editados: son los que se vuelven a golpear.
	const bool bKeepBase = !Deltas.IsEmpty();
	const TSharedRef<const FTerrainDensity> DensityRef = Density.ToSharedRef();
	const FTerrainEditSettings Edit = EditSettings;
	FRemeshJob& Job = Jobs.AddDefaulted_GetRef();
	Job.Chunk = Chunk;
	Job.Task = UE::Tasks::Launch(UE_SOURCE_LOCATION,
		[DensityRef, Edit, Chunk, Window, Cached, Deltas = MoveTemp(Deltas), bKeepBase]()
		{
			return TerrainRuntimeMesherDetail::RunRemesh(DensityRef, Edit, Chunk, Window, Cached, Deltas, bKeepBase);
		},
		UE::Tasks::ETaskPriority::BackgroundNormal);
}

void UTerrainRuntimeMesher::CacheBase(const FIntVector& Chunk, TSharedPtr<const FDensityGrid> Grid)
{
	FBaseCacheEntry Entry;
	Entry.Grid = MoveTemp(Grid);
	Entry.LastUse = ++BaseCacheClock;
	BaseCache.Add(Chunk, MoveTemp(Entry));
	if (BaseCache.Num() <= TerrainRuntimeMesherDetail::MaxCachedBases)
	{
		return;
	}
	const FIntVector* Oldest = nullptr;
	uint64 OldestUse = MAX_uint64;
	for (const auto& Pair : BaseCache)
	{
		if (Pair.Value.LastUse < OldestUse)
		{
			OldestUse = Pair.Value.LastUse;
			Oldest = &Pair.Key;
		}
	}
	if (Oldest)
	{
		BaseCache.Remove(FIntVector(*Oldest));
	}
}
