#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ExploredSkyController.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMeshComponent;
class UVolumetricCloudComponent;

/**
 * Cielo completo del archipiélago: Sol, Luna, atmósfera, nubes volumétricas,
 * niebla, luz de cielo en tiempo real y postproceso. Lee la hora del
 * UTimeOfDaySubsystem y ajusta todo cada fotograma.
 */
UCLASS()
class EXPLORED_API AExploredSkyController : public AActor
{
	GENERATED_BODY()

public:
	AExploredSkyController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Hora que se muestra en el editor (fuera de juego). */
	UPROPERTY(EditAnywhere, Category = "Explored|Cielo", meta = (ClampMin = 0, ClampMax = 24))
	float EditorHours = 16.5f;

	UPROPERTY(EditAnywhere, Category = "Explored|Cielo")
	float EditorDay = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Cielo")
	TSoftObjectPtr<UMaterialInterface> StarMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Stars.M_Stars")));

protected:
	virtual void BeginPlay() override;

private:
	void ApplyTime(float Hours, float TotalDays);

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UDirectionalLightComponent> Moon;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UVolumetricCloudComponent> Clouds;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UPostProcessComponent> PostProcess;

	/** Esfera de estrellas (visible de noche). */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UStaticMeshComponent> StarDome;
};
