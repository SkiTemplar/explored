#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Mining/TerrainRuntimeMesher.h"
#include "WorldGen/TerrainDeltaCodecModel.h"
#include "WorldGen/TerrainEdits.h"

#include "TerrainEditSubsystem.generated.h"

class FTerrainDensity;
class ULevel;
struct FSaveWorldDeltas;

/**
 * Terreno editable en el juego (GDD v2 §3.4 y §7.3; docs/tecnico/terreno-editable.md).
 * Dueño de la capa de ediciones (`FTerrainEdits`) de esta máquina y del remallado en
 * tiempo de ejecución (`UTerrainRuntimeMesher`).
 *
 * Red (biblia 08 §2.2): solo quien tiene autoridad (servidor, anfitrión o partida sola)
 * edita, con `Dig`, `Shovel` y `PlaceSoil`; cada edición sale por `OnPatches` con el valor
 * final de las muestras cambiadas, que `UTerrainSyncComponent` encola por cliente. Un
 * cliente solo cambia su copia con `ApplyNetworkPacket` y remalla igual que el servidor
 * (que también necesita la colisión nueva para mover a los personajes).
 *
 * Unidades: metros en toda la API salvo que el nombre diga Cm.
 */
UCLASS()
class EXPLORED_API UTerrainEditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTerrainEditSubsystem* Get(const UObject* WorldContextObject);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// --- Autoridad -----------------------------------------------------------

	/** Esta máquina decide las ediciones (no es un cliente). */
	bool HasEditAuthority() const;
	/** Golpe de pico por esfera (FTerrainEdits::Dig). Sin autoridad, rechazado y sin tocar nada. */
	FTerrainDigResult Dig(const FTerrainDigHit& Hit);
	/** Pasada de pala hacia un plano (FTerrainEditModel::Shovel). */
	FTerrainEditResult Shovel(const FShovelStroke& Stroke);
	/** Echar tierra transportada (FTerrainEditModel::PlaceSoil). */
	FTerrainEditResult PlaceSoil(const FSoilPlacement& Placement);

	/** Parches de muestras de cada edición del servidor, para la cola de red de cada cliente. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTerrainPatches, const TArray<FTerrainDeltaCodecModel::FChunkPatch>& /*Patches*/);
	FOnTerrainPatches OnPatches;
	/** Estado completo de todos los chunks editados (cliente que entra tarde). */
	TArray<FTerrainDeltaCodecModel::FChunkPatch> FullStatePatches() const;

	// --- Cliente -------------------------------------------------------------

	/** Aplica un paquete del servidor. false si no decodifica, no cabe o esta máquina tiene autoridad. */
	bool ApplyNetworkPacket(const TArray<uint8>& Bytes);

	// --- Consultas -----------------------------------------------------------

	/** Densidad con ediciones (< 0 sólido). */
	float Density(const FVector& Meters) const;
	/** Estrato que se golpea en ese punto (FTerrainToolModel::ClassifyMaterial). */
	ETerrainMaterial MaterialAt(const FVector& Meters) const;
	const FTerrainEditModel& GetModel() const { return Edits.GetModel(); }
	const FTerrainDensity& GetTerrainDensity() const { return *TerrainDensity; }

	// --- Guardado (capa «terrain» de la sección «world») ----------------------

	void SaveTo(FSaveWorldDeltas& World) const;
	/** Sustituye las ediciones y remalla. false (y terreno sin ediciones) si la capa no se puede leer. */
	bool LoadFrom(const FSaveWorldDeltas& World);
	/** Terreno recién generado: sin ediciones y con los chunks horneados. */
	void ResetEdits();

	// --- Remallado -----------------------------------------------------------

	/** Termina el remallado pendiente sin presupuesto (tests y carga). */
	bool FlushRemeshing(double TimeoutSeconds = 30.0);
	/** Sustituye el chunk horneado que contiene el punto (lo usan los tests para medir antes de cavar). */
	void EnsureReplacedAt(const FVector& Meters);
	UTerrainRuntimeMesher* GetMesher() const { return Mesher; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Terreno")
	FTerrainRemeshStats GetRemeshStats() const;

private:
	/** Remallado, sustitución de chunks horneados y (con autoridad) difusión de la edición. */
	void AfterEdit(const TArray<FIntVector>& ChangedSamples, const TArray<FIntVector>& DirtyChunks, bool bBroadcast);
	void HandleLevelAdded(ULevel* Level, UWorld* InWorld);
	FVector ViewerMeters() const;
	/** Material de las mallas finas desde el ajuste de proyecto (carga asíncrona por referencia). */
	void RequestRuntimeMaterial();

	TSharedPtr<const FTerrainDensity> TerrainDensity;
	FTerrainEdits Edits;

	UPROPERTY()
	TObjectPtr<UTerrainRuntimeMesher> Mesher;

	FDelegateHandle LevelAddedHandle;
	TSharedPtr<struct FStreamableHandle> MaterialLoadHandle;
};
