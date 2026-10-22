#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "WorldGen/GrassConfigLoader.h"

#include "ExploredGrassField.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class FGrassFieldModel;
struct FGrassUpdate;

/**
 * Césped continuo alrededor de la cámara (kit K6, Content/Data/grass.json). Capa fina sobre
 * FGrassFieldModel: cada frame le pasa la cámara y aplica sus cambios a un UInstancedStaticMeshComponent
 * por malla (ISM y no HISM: las celdas entran, salen y cambian de nivel continuamente y el HISM
 * reconstruiría su árbol; el ISM recicla ranuras sin reordenar índices, se actualiza por diferencias
 * y se recorta por instancia en la GPU).
 *
 * Cada instancia lleva dos datos para el material (M_ToonCesped): el rango de adelgazamiento y la
 * variación del parche. El adelgazamiento (FGrassThinning) se escribe en MPC_Terreno, que leen
 * también M_ToonCesped y M_TerrainToon (el suelo bajo la hierba se funde con el verde de la base).
 *
 * Solo decoración: sin colisión ni respuesta a ningún canal (las trazas de interacción, el jugador y el
 * bot del playtest la atraviesan), sin navegación y sin sombras proyectadas. Si falta una malla de K6,
 * M_ToonCesped o MPC_Terreno, avisa en el log y usa SM_GrassClump_01 / M_Toon / los valores del
 * material; si falta ground_field.bin, no dibuja nada y lo avisa. Lo crea AExploredIslandPopulator en
 * su BeginPlay (no en servidor dedicado).
 *
 * Blueprint: «Get Grass Instance Count», «Get Grass Cell Count», «Get Grass Seed Ms» y «Rebuild Grass».
 */
UCLASS()
class EXPLORED_API AExploredGrassField : public AActor
{
	GENERATED_BODY()

public:
	AExploredGrassField();
	virtual ~AExploredGrassField() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Parches y matas vivos alrededor de la cámara. */
	UFUNCTION(BlueprintPure, Category = "Explored|Césped")
	int32 GetGrassInstanceCount() const;

	/** Celdas generadas. */
	UFUNCTION(BlueprintPure, Category = "Explored|Césped")
	int32 GetGrassCellCount() const;

	/** Media (ms) del tiempo de siembra por frame (modelo + escritura en los componentes) desde el último relleno completo. */
	UFUNCTION(BlueprintPure, Category = "Explored|Césped")
	float GetGrassSeedMs() const { return SeedFrames > 0 ? static_cast<float>(SeedMsSum / SeedFrames) : 0.0f; }

	/** Peor frame de siembra (ms) desde el último relleno completo. */
	UFUNCTION(BlueprintPure, Category = "Explored|Césped")
	float GetGrassSeedMaxMs() const { return static_cast<float>(SeedMsMax); }

	/** Triángulos de césped dentro del ángulo horizontal de la cámara (a la vista, sin oclusión). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Césped")
	int64 GetGrassTrianglesInView() const;

	/** Pone a cero la media y el máximo de siembra (p. ej. al empezar a caminar en las capturas). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Césped")
	void ResetSeedStats();

	/** Relee grass.json y ground_field.bin y vuelve a sembrar desde cero. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Césped")
	void RebuildGrass();

	/** Segundos entre lecturas del viento del clima (WindStrength del material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explored|Césped", meta = (ClampMin = "0.1"))
	float WindRefreshSeconds = 0.5f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool Setup();
	void CreateComponents(const FGrassAssets& Assets, const FGrassConfig& Config);
	void ApplyThinningParameters(const FGrassAssets& Assets, const FGrassThinning& Thinning);
	void ClearComponents();
	/** Reserva en cada componente las ranuras que necesita la isla (FGrassFieldModel::EstimateSlotHighWater). */
	void ReserveSlots(const FGrassConfig& Config, const TSharedPtr<const FGrassWorld>& World);
	void Apply(const FGrassUpdate& Update);
	bool CameraView(FVector2D& OutMeters, FVector2D& OutForward, float& OutHalfFovRad) const;
	void RefreshWind();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> KindComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	TUniquePtr<FGrassFieldModel> Model;
	/** Triángulos de la LOD 0 de cada malla (para el coste a la vista). */
	int32 TrisPerKind[GrassKindCount] = {};
	float WindTimer = 0.0f;
	/**
	 * Cada relleno completo tras un salto de cámara (aparición, teletransporte de capturas, bench o
	 * playtest) se anota una vez en el log; caminando no se repite hasta alejarse RelogDistanceM.
	 */
	bool bLoggedSettled = false;
	FVector2D LoggedCamera = FVector2D::ZeroVector;
	double SeedMsSum = 0.0;
	double SeedMsMax = 0.0;
	int32 SeedFrames = 0;
};
