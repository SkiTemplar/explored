#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "WorldGen/TerrainDeltaCodecModel.h"
#include "WorldGen/TerrainDeltaQueueModel.h"

#include "TerrainSyncComponent.generated.h"

/**
 * Réplica del terreno editado servidor → cliente (biblia 08 §2.2), en el PlayerController.
 *
 * En el servidor, para cada cliente remoto: escucha las ediciones de
 * `UTerrainEditSubsystem::OnPatches`, las encola en su `FTerrainDeltaQueueModel` (fusiona
 * ediciones del mismo chunk, 8 KB/s sostenidos y 16 KB/s de pico, prioridad a lo cercano)
 * y manda cada paquete por una RPC fiable al dueño. Al conectarse, encola el estado
 * completo de todos los chunks editados. El cliente aplica el paquete a su copia y remalla.
 * El anfitrión y la partida sola no mandan nada: ya tienen el terreno.
 */
UCLASS(ClassGroup = (Explored))
class EXPLORED_API UTerrainSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerrainSyncComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Paquetes pendientes para este cliente (0 en el cliente y en el anfitrión). */
	int32 NumQueuedChunks() const { return Queue.Num(); }

protected:
	UFUNCTION(Client, Reliable)
	void ClientReceiveTerrainPacket(const TArray<uint8>& Bytes);

private:
	/** Este componente está en el servidor y su PlayerController es de un cliente remoto. */
	bool IsServerForRemoteClient() const;
	/** Empieza a replicar en cuanto el PlayerController tiene su conexión; apaga el tick si es local. */
	bool TryStartSync();
	void EnqueuePatches(const TArray<FTerrainDeltaCodecModel::FChunkPatch>& Patches);
	/** Distancia (m) del personaje del cliente al centro del chunk de edición. */
	double DistanceToChunk(const FIntVector& Chunk) const;
	void RefreshDistances();

	FTerrainDeltaQueueModel Queue;
	FDelegateHandle PatchesHandle;
	float SecondsSinceDistanceRefresh = 0.0f;
	bool bSyncStarted = false;
};
