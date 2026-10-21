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
 * Césped alrededor de la cámara (kit K6, Content/Data/grass.json). Capa fina sobre FGrassFieldModel:
 * cada frame le pasa la cámara y aplica sus cambios a un UInstancedStaticMeshComponent por malla
 * (ISM y no HISM: las celdas entran y salen continuamente y el HISM reconstruiría su árbol en cada
 * cambio; el ISM recicla ranuras sin reordenar índices y se recorta por instancia en la GPU).
 *
 * Solo decoración: sin colisión ni respuesta a ningún canal (las trazas de interacción, el jugador y el
 * bot del playtest la atraviesan), sin navegación y sin sombras en las matas pequeñas. Si falta una
 * malla de K6 o M_ToonCesped, usa SM_GrassClump_01 / M_Toon con un aviso; si falta ground_field.bin,
 * no dibuja nada y lo avisa. Lo crea AExploredIslandPopulator en su BeginPlay (no en servidor dedicado).
 *
 * Blueprint: «Get Grass Instance Count», «Get Grass Cell Count» y «Rebuild Grass».
 */
UCLASS()
class EXPLORED_API AExploredGrassField : public AActor
{
	GENERATED_BODY()

public:
	AExploredGrassField();
	virtual ~AExploredGrassField() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Matas vivas alrededor de la cámara. */
	UFUNCTION(BlueprintPure, Category = "Explored|Césped")
	int32 GetGrassInstanceCount() const;

	/** Celdas generadas. */
	UFUNCTION(BlueprintPure, Category = "Explored|Césped")
	int32 GetGrassCellCount() const;

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
	void CreateComponents(const FGrassAssets& Assets);
	void ClearComponents();
	void Apply(const FGrassUpdate& Update);
	bool CameraMeters(FVector2D& Out) const;
	void RefreshWind();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> KindComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	TUniquePtr<FGrassFieldModel> Model;
	float WindTimer = 0.0f;
	/**
	 * Cada relleno completo tras un salto de cámara (aparición, teletransporte de capturas, bench o
	 * playtest) se anota una vez en el log; caminando no se repite hasta alejarse RelogDistanceM.
	 */
	bool bLoggedSettled = false;
	FVector2D LoggedCamera = FVector2D::ZeroVector;
};
