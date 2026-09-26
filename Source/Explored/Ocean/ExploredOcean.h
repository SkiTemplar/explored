#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ocean/OceanWaves.h"

#include "ExploredOcean.generated.h"

class UMaterialInstanceDynamic;
class UProceduralMeshComponent;

/**
 * Océano infinito: malla radial densa en el centro y gruesa en el horizonte
 * que sigue a la cámara. El material evalúa las mismas olas de Gerstner que
 * FOceanWaves, cuyos parámetros se le pasan cada fotograma.
 */
UCLASS()
class EXPLORED_API AExploredOcean : public AActor
{
	GENERATED_BODY()

public:
	AExploredOcean();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Altura del agua (cm, espacio de mundo) en una posición. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Océano")
	float GetWaterHeightAt(const FVector& WorldLocation) const;

	/** Normal de la superficie (espacio de mundo) en una posición; para inclinar la cámara al nadar. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Océano")
	FVector GetWaterNormalAt(const FVector& WorldLocation) const;

	/** Fuerza del mar: 0 calma, 1 temporal. La fija el sistema de clima. */
	void SetSeaState(float InSeaState);

	/** Fuerza del mar actual (0 calma, 1 temporal): más espuma de cresta y de orilla cuando sube. */
	UFUNCTION(BlueprintPure, Category = "Explored|Océano")
	float GetSeaState() const { return SeaState; }

protected:
	virtual void BeginPlay() override;

private:
	void BuildMesh();
	void PushWaveParameters();
	float GetWaveTime() const;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Océano")
	TObjectPtr<UProceduralMeshComponent> Surface;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

	UPROPERTY(EditAnywhere, Category = "Explored|Océano")
	TSoftObjectPtr<UMaterialInterface> OceanMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Ocean.M_Ocean")));

	UPROPERTY(EditAnywhere, Category = "Explored|Océano")
	float SeaState = 0.15f;

	/** Paso de ajuste al seguir a la cámara (cm), evita que la malla «nade». */
	UPROPERTY(EditAnywhere, Category = "Explored|Océano")
	float SnapStep = 400.0f;

	FOceanWaves Waves;
};
