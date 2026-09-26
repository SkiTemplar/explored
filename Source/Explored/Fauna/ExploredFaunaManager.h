#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"

#include "Fauna/FaunaAnimation.h"
#include "Fauna/FaunaGroups.h"
#include "Fauna/FaunaSpawning.h"
#include "Fauna/FaunaTypes.h"
#include "Fauna/MarineCreatureBrain.h"

#include "ExploredFaunaManager.generated.h"

class AExploredMarineCreature;
class AExploredOcean;
class FTerrainDensity;
class UInstancedStaticMeshComponent;
class UStaticMesh;
struct FFaunaQueryState;

/** Daño de la fauna al jugador: especie, puntos y dónde (mordisco, picadura). */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFaunaDamage, EFaunaSpecies /*Species*/, float /*Damage*/, const FVector& /*LocationCm*/);
/** Momentos visibles o audibles: salto de delfín, soplido de ballena, raya que se levanta… */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFaunaMoment, EFaunaSpecies /*Species*/, FName /*Moment*/, const FVector& /*LocationCm*/);
/** Una gaviota se ha llevado un pescado registrado con RegisterStealableFish (quien escucha lo retira). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFishStolen, AActor* /*Item*/);
/** La tortuga ha llegado a la playa para desovar (gancho de P-EVENTS). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTurtleNesting, const FVector& /*LocationCm*/);

/**
 * Gestor de fauna (capa fina de UE sobre los modelos puros de Fauna/). Puebla
 * por celdas alrededor del jugador según FFaunaSpawnRules, mueve bancos y
 * bandadas con un UInstancedStaticMeshComponent por grupo (datos por instancia
 * 0–3 = FFaunaAnimParams para el shader) y las criaturas sueltas como actores
 * cinemáticos; aplica el LOD de actualización de FFaunaLod.
 *
 * Entrada: ruido del jugador, sangre en el agua, estado de la canoa, pescado al
 * aire y paso de ballenas. Salida: delegados de daño y de momentos.
 */
UCLASS(NotPlaceable)
class EXPLORED_API AExploredFaunaManager : public AActor
{
	GENERATED_BODY()

public:
	AExploredFaunaManager();

	virtual void Tick(float DeltaSeconds) override;

	/** Ruido puntual del jugador (0–1): chapoteo, zambullida, remo. Decae solo. */
	void ReportPlayerNoise(float Noise01);
	/** Sangre o cebo en el agua (despiece, herida): atrae tiburones con la corriente. */
	void AddBloodInWater(const FVector& LocationCm, float Amount01);
	/** Estado de la embarcación (hasta que exista P-BOATS). */
	void SetPlayerBoatState(bool bInBoat, bool bCarriesFish);
	/** Pescado que las gaviotas pueden robar si queda al aire (no enganchado a nada). */
	void RegisterStealableFish(AActor* Item);
	/** Paso de ballenas activo (P-EVENTS). */
	void SetWhalePassage(bool bActive) { bWhalePassage = bActive; }
	/** Modo Explorador: fauna pacífica. */
	void SetPeaceful(bool bInPeaceful) { bPeaceful = bInPeaceful; }
	/** Pide a las tortugas cercanas a BeachCm que acudan a desovar (P-EVENTS, luna llena). */
	void StartTurtleNesting(const FVector& BeachCm, float RadiusCm);

	FOnFaunaDamage OnFaunaDamage;
	FOnFaunaMoment OnFaunaMoment;
	FOnFishStolen OnFishStolen;
	FOnTurtleNesting OnTurtleNesting;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FSchoolEntry
	{
		FFishSchoolModel Model;
		EFaunaSpecies Species = EFaunaSpecies::ReefFish;
		int32 ComponentIndex = INDEX_NONE;
		FIntPoint Cell = FIntPoint::ZeroValue;
		uint32 LodId = 0;
		EFaunaLodTier Tier = EFaunaLodTier::Full;
		float PendingDelta = 0.0f;
		TArray<FFaunaAnimState> Anim;
	};

	struct FFlockEntry
	{
		FBirdFlockModel Model;
		EFaunaSpecies Species = EFaunaSpecies::Gull;
		int32 ComponentIndex = INDEX_NONE;
		FIntPoint Cell = FIntPoint::ZeroValue;
		uint32 LodId = 0;
		EFaunaLodTier Tier = EFaunaLodTier::Full;
		float PendingDelta = 0.0f;
		TArray<FFaunaAnimState> Anim;
	};

	struct FCreatureEntry
	{
		TWeakObjectPtr<AExploredMarineCreature> Actor;
		uint32 LodId = 0;
		EFaunaLodTier Tier = EFaunaLodTier::Full;
		float PendingDelta = 0.0f;
	};

	void BuildWorldQuery();
	FFaunaStimuli GatherStimuli(float DeltaSeconds);
	FFaunaCellContext CellContext(const FIntPoint& Cell) const;
	void RefreshCells(const FVector& Center);
	void SpawnGroup(const FFaunaSpawn& Spawn, const FIntPoint& Cell);
	void DespawnCell(const FIntPoint& Cell);
	int32 AcquireInstancedComponent(EFaunaSpecies Species, int32 Count);
	void ReleaseInstancedComponent(int32 Index);
	void WriteInstances(UInstancedStaticMeshComponent* Component, const FBoidsModel& Boids, EFaunaSpecies Species,
		TArray<FFaunaAnimState>& Anim, float DeltaSeconds);
	UStaticMesh* LoadMesh(FName MeshName);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> GroupComponents;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UStaticMesh>> MeshCache;

	TArray<FSchoolEntry> Schools;
	TArray<FFlockEntry> Flocks;
	TArray<FCreatureEntry> Creatures;
	TArray<int32> FreeComponents;
	TSet<FIntPoint> LoadedCells;

	FFaunaWorldQuery WorldQuery;
	/** Marea y viento del instante y caché del fondo, compartidos con las lambdas de WorldQuery. */
	TSharedPtr<FFaunaQueryState> DynamicState;
	TSharedPtr<FTerrainDensity> Density;
	TArray<TWeakObjectPtr<AActor>> StealableFish;
	TArray<FBloodSource> Blood;

	TWeakObjectPtr<AExploredOcean> Ocean;

	uint64 FrameCounter = 0;
	uint32 NextLodId = 1;
	float CellRefreshTimer = 0.0f;
	float NoiseBurst = 0.0f;
	double GameSeconds = 0.0;
	bool bPlayerInBoat = false;
	bool bPlayerCarriesFish = false;
	bool bWhalePassage = false;
	bool bPeaceful = false;

	/** Celdas de población pobladas alrededor del jugador (radio en celdas). */
	UPROPERTY(EditAnywhere, Category = "Explored|Fauna")
	int32 CellRadius = 3;

	/** Tope de peces por banco (los bancos de mar abierto pueden pedir más). */
	UPROPERTY(EditAnywhere, Category = "Explored|Fauna")
	int32 MaxFishPerSchool = 80;
};

/** Crea el gestor de fauna al empezar la partida (no hace falta colocarlo en el mapa). */
UCLASS()
class EXPLORED_API UExploredFaunaSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	AExploredFaunaManager* GetManager() const { return Manager.Get(); }

private:
	TWeakObjectPtr<AExploredFaunaManager> Manager;
};
