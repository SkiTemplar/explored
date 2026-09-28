#include "Mining/TerrainEditSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

#include "Explored.h"
#include "Save/SaveWorldDeltas.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"
#include "WorldGen/TerrainNetSyncModel.h"
#include "WorldGen/TerrainToolModel.h"

namespace TerrainEditSubsystemDetail
{
	TAutoConsoleVariable<float> CVarRemeshBudgetMs(TEXT("explored.Terrain.RemeshBudgetMs"), 1.5f,
		TEXT("Presupuesto del remallado del terreno en el hilo de juego por fotograma (ms)."));
}

UTerrainEditSubsystem* UTerrainEditSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UTerrainEditSubsystem>() : nullptr;
}

bool UTerrainEditSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UTerrainEditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TerrainDensity = MakeShared<FTerrainDensity>(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
	Mesher = NewObject<UTerrainRuntimeMesher>(this);
	if (UWorld* World = GetWorld())
	{
		Mesher->Initialize(*World, TerrainDensity.ToSharedRef(), Edits.GetModel().GetSettings(), Edits.GetRenderSettings());
	}
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UTerrainEditSubsystem::HandleLevelAdded);
}

void UTerrainEditSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (ULevel* Level : InWorld.GetLevels())
	{
		Mesher->RegisterBakedActorsInLevel(Level);
	}
}

void UTerrainEditSubsystem::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	OnPatches.Clear();
	Super::Deinitialize();
}

TStatId UTerrainEditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTerrainEditSubsystem, STATGROUP_Tickables);
}

void UTerrainEditSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Mesher && Mesher->NeedsTick())
	{
		Mesher->Tick(Edits.GetModel(), ViewerMeters(), TerrainEditSubsystemDetail::CVarRemeshBudgetMs.GetValueOnGameThread());
	}
}

void UTerrainEditSubsystem::HandleLevelAdded(ULevel* Level, UWorld* InWorld)
{
	if (InWorld == GetWorld() && Mesher)
	{
		Mesher->RegisterBakedActorsInLevel(Level);
	}
}

FVector UTerrainEditSubsystem::ViewerMeters() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return FVector::ZeroVector;
	}
	FVector Location;
	FRotator Rotation;
	PC->GetPlayerViewPoint(Location, Rotation);
	return Location / 100.0;
}

bool UTerrainEditSubsystem::HasEditAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

FTerrainDigResult UTerrainEditSubsystem::Dig(const FTerrainDigHit& Hit)
{
	if (!HasEditAuthority())
	{
		FTerrainDigResult Rejected;
		Rejected.Edit.bRejected = true;
		return Rejected;
	}
	FTerrainDigResult Result = Edits.Dig(Hit, *TerrainDensity);
	AfterEdit(Result.Edit.ChangedSamples, Result.Edit.DirtyChunks, true);
	return Result;
}

FTerrainEditResult UTerrainEditSubsystem::Shovel(const FShovelStroke& Stroke)
{
	FTerrainEditResult Result;
	if (!HasEditAuthority())
	{
		Result.bRejected = true;
		return Result;
	}
	const FTerrainDensity& Base = *TerrainDensity;
	Result = Edits.GetModel().Shovel(Stroke, [&Base](const FVector& P) { return Base.ProceduralDensity(P); });
	AfterEdit(Result.ChangedSamples, Result.DirtyChunks, true);
	return Result;
}

FTerrainEditResult UTerrainEditSubsystem::PlaceSoil(const FSoilPlacement& Placement)
{
	FTerrainEditResult Result;
	if (!HasEditAuthority())
	{
		Result.bRejected = true;
		return Result;
	}
	const FTerrainDensity& Base = *TerrainDensity;
	Result = Edits.GetModel().PlaceSoil(Placement, [&Base](const FVector& P) { return Base.ProceduralDensity(P); });
	AfterEdit(Result.ChangedSamples, Result.DirtyChunks, true);
	return Result;
}

void UTerrainEditSubsystem::AfterEdit(const TArray<FIntVector>& ChangedSamples, const TArray<FIntVector>& DirtyChunks, bool bBroadcast)
{
	if (ChangedSamples.IsEmpty() || !Mesher)
	{
		return;
	}
	const FTerrainEditModel& Model = Edits.GetModel();
	TArray<FIntVector> RenderChunks;
	FTerrainEdits::RenderChunksTouchingBox(FTerrainNetSyncModel::SamplesBounds(Model, ChangedSamples), Edits.GetRenderSettings(), RenderChunks);
	Mesher->RequestReplacement(RenderChunks);
	Mesher->MarkDirty(DirtyChunks);
	if (bBroadcast && OnPatches.IsBound())
	{
		OnPatches.Broadcast(FTerrainNetSyncModel::PatchesForSamples(Model, ChangedSamples));
	}
}

TArray<FTerrainDeltaCodecModel::FChunkPatch> UTerrainEditSubsystem::FullStatePatches() const
{
	TArray<FTerrainDeltaCodecModel::FChunkPatch> Patches;
	for (const FIntVector& Chunk : Edits.GetModel().EditedChunks())
	{
		Patches.Add(FTerrainNetSyncModel::FullChunkPatch(Edits.GetModel(), Chunk));
	}
	return Patches;
}

bool UTerrainEditSubsystem::ApplyNetworkPacket(const TArray<uint8>& Bytes)
{
	FTerrainDeltaCodecModel::FPacket Packet;
	if (HasEditAuthority() || !FTerrainDeltaCodecModel::Decode(Bytes, Packet))
	{
		return false;
	}
	TArray<FIntVector> Dirty;
	TArray<FIntVector> Changed;
	if (!FTerrainNetSyncModel::ApplyPacket(Edits.GetModel(), Packet, Dirty, &Changed))
	{
		return false;
	}
	AfterEdit(Changed, Dirty, false);
	return true;
}

float UTerrainEditSubsystem::Density(const FVector& Meters) const
{
	const FTerrainDensity& Base = *TerrainDensity;
	return Edits.Density(Meters, [&Base](const FVector& P) { return Base.ProceduralDensity(P); });
}

ETerrainMaterial UTerrainEditSubsystem::MaterialAt(const FVector& Meters) const
{
	const FTerrainDensity& Base = *TerrainDensity;
	const FTerrainColumn Column = Base.SampleColumn(static_cast<float>(Meters.X), static_cast<float>(Meters.Y));
	const FVector Surface(Meters.X, Meters.Y, Column.Height);
	const FVector4f Layers = Base.SurfaceLayers(Surface, Base.Normal(Surface));
	FTerrainStrataQuery Query;
	Query.bHasIsland = Base.GetLayout().Islands.IsValidIndex(Column.IslandIndex);
	Query.Archetype = Query.bHasIsland ? Base.GetLayout().Islands[Column.IslandIndex].Archetype : EIslandArchetype::Landing;
	Query.ColumnHeight = Column.Height;
	Query.Z = static_cast<float>(Meters.Z);
	Query.SandWeight = Layers.X;
	Query.RockWeight = Layers.Z;
	return FTerrainToolModel::ClassifyMaterial(Query);
}

void UTerrainEditSubsystem::SaveTo(FSaveWorldDeltas& World) const
{
	Edits.SaveTo(World);
}

bool UTerrainEditSubsystem::LoadFrom(const FSaveWorldDeltas& World)
{
	const bool bLoaded = Edits.LoadFrom(World);
	if (!bLoaded)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Terreno] La capa «terrain» del guardado no se puede leer: el terreno arranca sin cavar"));
	}
	if (Mesher)
	{
		Mesher->ResetAll();
		Mesher->RequestReplacement(Edits.EditedRenderChunks());
	}
	return bLoaded;
}

void UTerrainEditSubsystem::ResetEdits()
{
	Edits.GetModel().Reset();
	if (Mesher)
	{
		Mesher->ResetAll();
	}
}

bool UTerrainEditSubsystem::FlushRemeshing(double TimeoutSeconds)
{
	return Mesher && Mesher->Flush(Edits.GetModel(), TimeoutSeconds);
}

void UTerrainEditSubsystem::EnsureReplacedAt(const FVector& Meters)
{
	if (!Mesher)
	{
		return;
	}
	const double Size = Edits.GetRenderSettings().ChunkSizeMeters();
	if (!FMath::IsFinite(Meters.X) || !FMath::IsFinite(Meters.Y) || !FMath::IsFinite(Meters.Z) || Size <= 0.0)
	{
		return;
	}
	const FIntVector RenderChunk(FMath::FloorToInt32(Meters.X / Size), FMath::FloorToInt32(Meters.Y / Size),
		FMath::FloorToInt32(Meters.Z / Size));
	Mesher->RequestReplacement({RenderChunk});
}

FTerrainRemeshStats UTerrainEditSubsystem::GetRemeshStats() const
{
	return Mesher ? Mesher->GetStats() : FTerrainRemeshStats();
}
