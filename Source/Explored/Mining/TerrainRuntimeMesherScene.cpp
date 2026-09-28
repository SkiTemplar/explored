// Parte de UTerrainRuntimeMesher que toca el mundo: componentes de las mallas finas,
// chunks horneados (ocultar, colisión) y el intercambio de colisión al sustituir.

#include "Mining/TerrainRuntimeMesher.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

#include "Explored.h"
#include "WorldGen/TerrainRemeshModel.h"

namespace TerrainRuntimeMesherSceneDetail
{
	/** Distancia de dibujado de las mallas finas: la de carga de World Partition del terreno (1,2 km). */
	constexpr float CullDistanceCm = 120000.0f;
	/** Si la colisión fina no se ha cocinado en este tiempo, se apaga igualmente la horneada. */
	constexpr double CollisionSwapTimeoutSeconds = 2.0;
}

AActor* UTerrainRuntimeMesher::EnsureRuntimeActor()
{
	if (RuntimeActor)
	{
		return RuntimeActor;
	}
	UWorld* W = World.Get();
	if (!W)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(W->PersistentLevel, AActor::StaticClass(), TEXT("ExploredTerrainRuntime"));
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	RuntimeActor = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!RuntimeActor)
	{
		return nullptr;
	}
	USceneComponent* Root = NewObject<USceneComponent>(RuntimeActor, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RuntimeActor->SetRootComponent(Root);
	Root->RegisterComponent();
	RuntimeActor->Tags.Add(TerrainTag);
	return RuntimeActor;
}

UProceduralMeshComponent* UTerrainRuntimeMesher::FindOrCreateComponent(const FIntVector& EditChunk)
{
	if (UProceduralMeshComponent* Existing = FindChunkComponent(EditChunk))
	{
		return Existing;
	}
	AActor* Owner = EnsureRuntimeActor();
	if (!Owner)
	{
		return nullptr;
	}
	const FName Name = MakeUniqueObjectName(Owner,
		UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("TerrainEdit_%d_%d_%d"), EditChunk.X, EditChunk.Y, EditChunk.Z));
	UProceduralMeshComponent* Component = NewObject<UProceduralMeshComponent>(Owner, Name);
	Component->bUseAsyncCooking = bAsyncCooking;
	Component->bUseComplexAsSimpleCollision = true;
	Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// La navegación de la fauna sobre terreno editado es trabajo aparte (GDD v2 §7.4).
	Component->SetCanEverAffectNavigation(false);
	Component->SetCullDistance(TerrainRuntimeMesherSceneDetail::CullDistanceCm);
	Component->SetupAttachment(Owner->GetRootComponent());
	const FVector OriginCm = FTerrainRemeshModel::EditChunkOrigin(EditChunk, EditSettings) * 100.0;
	Component->SetRelativeLocation(OriginCm);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}
	Component->RegisterComponent();
	Owner->AddInstanceComponent(Component);
	ChunkComponents.Add(EditChunk, Component);
	return Component;
}

void UTerrainRuntimeMesher::RegisterBakedActorsInLevel(ULevel* Level)
{
	if (!Level)
	{
		return;
	}
	for (AActor* Actor : Level->Actors)
	{
		AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
		if (!MeshActor || !MeshActor->ActorHasTag(TerrainTag))
		{
			continue;
		}
		UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent();
		const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
		FIntVector Coord;
		if (!Mesh || !FTerrainRemeshModel::ParseBakedChunkName(Mesh->GetName(), Coord))
		{
			continue;
		}
		BakedActors.Add(Coord, MeshActor);
		SetMaterialFromBaked(MeshComponent->GetMaterial(0));
		// Un chunk que vuelve a cargarse por streaming ya sustituido se oculta al momento.
		if (Replacement.GetState(Coord) == ETerrainReplacementState::Replaced)
		{
			SetBakedHidden(Coord, true);
			SetBakedCollision(Coord, !BakedCollisionOff.Contains(Coord));
		}
	}
}

void UTerrainRuntimeMesher::SetBakedHidden(const FIntVector& RenderChunk, bool bHidden)
{
	const TWeakObjectPtr<AActor>* Found = BakedActors.Find(RenderChunk);
	if (AActor* Actor = Found ? Found->Get() : nullptr)
	{
		Actor->SetActorHiddenInGame(bHidden);
	}
}

void UTerrainRuntimeMesher::SetBakedCollision(const FIntVector& RenderChunk, bool bEnabled)
{
	const TWeakObjectPtr<AActor>* Found = BakedActors.Find(RenderChunk);
	if (AActor* Actor = Found ? Found->Get() : nullptr)
	{
		Actor->SetActorEnableCollision(bEnabled);
	}
}

void UTerrainRuntimeMesher::OnRenderChunkReplaced(const FIntVector& RenderChunk)
{
	SetBakedHidden(RenderChunk, true);
	CollisionSwaps.Add({RenderChunk, FPlatformTime::Seconds()});
	++Stats.RenderChunksReplaced;
	UE_LOG(LogExplored, Verbose, TEXT("[Terreno] Chunk horneado (%d, %d, %d) sustituido por mallas finas"),
		RenderChunk.X, RenderChunk.Y, RenderChunk.Z);
}

void UTerrainRuntimeMesher::SetAsyncCollisionCooking(bool bAsync)
{
	bAsyncCooking = bAsync;
	for (const auto& Pair : ChunkComponents)
	{
		if (UProceduralMeshComponent* Component = Pair.Value.Get())
		{
			Component->bUseAsyncCooking = bAsync;
		}
	}
}

bool UTerrainRuntimeMesher::IsCollisionCookPending()
{
	TArray<FIntVector> Chunks;
	PendingCookSetups.GetKeys(Chunks);
	bool bPending = false;
	for (const FIntVector& Chunk : Chunks)
	{
		bPending |= IsCollisionPending(Chunk);
	}
	return bPending;
}

void UTerrainRuntimeMesher::SetMaterialFromBaked(UMaterialInterface* BakedMaterial)
{
	if (Material || !BakedMaterial)
	{
		return;
	}
	// El material de las mallas finas es el de los chunks horneados (M_Terrain): así no se
	// carga nada por ruta y las que se crearon antes de verlo lo reciben ahora.
	Material = BakedMaterial;
	for (const auto& Pair : ChunkComponents)
	{
		if (UProceduralMeshComponent* Component = Pair.Value.Get())
		{
			Component->SetMaterial(0, Material);
		}
	}
}

bool UTerrainRuntimeMesher::IsCollisionPending(const FIntVector& EditChunk)
{
	const TWeakObjectPtr<UObject>* Before = PendingCookSetups.Find(EditChunk);
	if (!Before)
	{
		return false;
	}
	UProceduralMeshComponent* Component = FindChunkComponent(EditChunk);
	if (!Component || Component->GetBodySetup() != Before->Get())
	{
		PendingCookSetups.Remove(EditChunk);
		return false;
	}
	return true;
}

void UTerrainRuntimeMesher::UpdateCollisionSwaps(bool bForce)
{
	const double Now = FPlatformTime::Seconds();
	for (int32 I = 0; I < CollisionSwaps.Num();)
	{
		const FCollisionSwap Swap = CollisionSwaps[I];
		bool bPending = false;
		if (!bForce && Now - Swap.StartSeconds < TerrainRuntimeMesherSceneDetail::CollisionSwapTimeoutSeconds)
		{
			for (const auto& Pair : ChunkComponents)
			{
				if (Replacement.RenderChunkOf(Pair.Key) == Swap.RenderChunk && IsCollisionPending(Pair.Key))
				{
					bPending = true;
					break;
				}
			}
		}
		if (bPending)
		{
			++I;
			continue;
		}
		SetBakedCollision(Swap.RenderChunk, false);
		BakedCollisionOff.Add(Swap.RenderChunk);
		CollisionSwaps.RemoveAtSwap(I);
	}
}

void UTerrainRuntimeMesher::ResetAll()
{
	for (const FIntVector& RenderChunk : Replacement.ActiveRenderChunks())
	{
		SetBakedHidden(RenderChunk, false);
		SetBakedCollision(RenderChunk, true);
	}
	for (const auto& Pair : ChunkComponents)
	{
		if (UProceduralMeshComponent* Component = Pair.Value.Get())
		{
			Component->DestroyComponent();
		}
	}
	ChunkComponents.Reset();
	Surveys.Reset();
	Jobs.Reset();
	Ready.Reset();
	Queue.Reset();
	Replacement.Reset();
	PendingCookSetups.Reset();
	CollisionSwaps.Reset();
	BakedCollisionOff.Reset();
	// El campo base no depende de las ediciones: la caché sigue valiendo.
}
