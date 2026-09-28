#include "Mining/TerrainSyncComponent.h"

#include "Engine/NetConnection.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Explored.h"
#include "Mining/TerrainEditSubsystem.h"
#include "WorldGen/TerrainRemeshModel.h"

namespace TerrainSyncComponentDetail
{
	/** Cada cuánto se recalcula la distancia del cliente a los chunks en cola. */
	constexpr float DistanceRefreshSeconds = 0.5f;
	/** Tope de paquetes por tick (el presupuesto ya limita; esto evita bucles largos). */
	constexpr int32 MaxPacketsPerTick = 32;
}

UTerrainSyncComponent::UTerrainSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

bool UTerrainSyncComponent::IsServerForRemoteClient() const
{
	// En BeginPlay la conexión del cliente aún no está puesta (IsLocalController daría true):
	// se mira el UPlayer, que en un cliente remoto es su UNetConnection.
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	return PC && PC->HasAuthority() && Cast<UNetConnection>(PC->Player) != nullptr;
}

void UTerrainSyncComponent::BeginPlay()
{
	Super::BeginPlay();
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->HasAuthority())
	{
		SetComponentTickEnabled(false);
	}
}

bool UTerrainSyncComponent::TryStartSync()
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	UTerrainEditSubsystem* Terrain = UTerrainEditSubsystem::Get(this);
	if (!PC || !PC->Player || !Terrain)
	{
		return false;
	}
	if (!IsServerForRemoteClient())
	{
		// Anfitrión o partida sola: el terreno ya está aquí.
		SetComponentTickEnabled(false);
		return false;
	}
	PatchesHandle = Terrain->OnPatches.AddUObject(this, &UTerrainSyncComponent::EnqueuePatches);
	// Cliente que entra con el mundo ya cavado: todo el estado, con el crédito de ráfaga lleno.
	Queue.FillBudget();
	EnqueuePatches(Terrain->FullStatePatches());
	bSyncStarted = true;
	return true;
}

void UTerrainSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTerrainEditSubsystem* Terrain = UTerrainEditSubsystem::Get(this))
	{
		Terrain->OnPatches.Remove(PatchesHandle);
	}
	PatchesHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void UTerrainSyncComponent::EnqueuePatches(const TArray<FTerrainDeltaCodecModel::FChunkPatch>& Patches)
{
	for (const FTerrainDeltaCodecModel::FChunkPatch& Patch : Patches)
	{
		const int32 Rejected = Queue.Enqueue(Patch.Chunk, Patch.Samples, DistanceToChunk(Patch.Chunk));
		if (Rejected > 0)
		{
			// Muestras que no caben en el cable (delta de más de ±32 m): el cliente se desincroniza ahí.
			UE_LOG(LogExplored, Warning, TEXT("[Terreno] %d muestras del chunk (%d, %d, %d) no caben en el paquete de red"),
				Rejected, Patch.Chunk.X, Patch.Chunk.Y, Patch.Chunk.Z);
		}
	}
}

double UTerrainSyncComponent::DistanceToChunk(const FIntVector& Chunk) const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UTerrainEditSubsystem* Terrain = UTerrainEditSubsystem::Get(this);
	if (!Pawn || !Terrain)
	{
		// Sin personaje (aún cargando), todo es relevante pero sin prioridad.
		return FTerrainDeltaQueueModel::PriorityDistanceM;
	}
	const FTerrainEditSettings& Edit = Terrain->GetModel().GetSettings();
	const FVector Center = FTerrainRemeshModel::EditChunkOrigin(Chunk, Edit) + FVector(0.5 * Edit.ChunkSizeMeters());
	return FVector::Dist(Center, Pawn->GetActorLocation() / 100.0);
}

void UTerrainSyncComponent::RefreshDistances()
{
	const UTerrainEditSubsystem* Terrain = UTerrainEditSubsystem::Get(this);
	if (!Terrain)
	{
		return;
	}
	for (const FIntVector& Chunk : Terrain->GetModel().EditedChunks())
	{
		if (Queue.Contains(Chunk))
		{
			Queue.UpdateDistance(Chunk, DistanceToChunk(Chunk));
		}
	}
}

void UTerrainSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bSyncStarted && !TryStartSync())
	{
		return;
	}
	Queue.Accrue(DeltaTime);
	SecondsSinceDistanceRefresh += DeltaTime;
	if (SecondsSinceDistanceRefresh >= TerrainSyncComponentDetail::DistanceRefreshSeconds)
	{
		SecondsSinceDistanceRefresh = 0.0f;
		RefreshDistances();
	}
	FTerrainDeltaQueueModel::FOutgoingPacket Packet;
	for (int32 I = 0; I < TerrainSyncComponentDetail::MaxPacketsPerTick && Queue.TryPopPacket(Packet); ++I)
	{
		ClientReceiveTerrainPacket(Packet.Bytes);
	}
}

void UTerrainSyncComponent::ClientReceiveTerrainPacket_Implementation(const TArray<uint8>& Bytes)
{
	UTerrainEditSubsystem* Terrain = UTerrainEditSubsystem::Get(this);
	if (!Terrain || !Terrain->ApplyNetworkPacket(Bytes))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Terreno] Paquete de terreno del servidor descartado (%d bytes)"), Bytes.Num());
	}
}
