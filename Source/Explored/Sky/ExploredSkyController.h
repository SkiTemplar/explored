#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ExploredSkyController.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInterface;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMeshComponent;
class UVolumetricCloudComponent;
class AExploredOcean;

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

	/** Nubosidad, lluvia y niebla del clima actual (las aplica en el siguiente fotograma). */
	void SetWeather(const struct FWeatherSample& Weather);

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
	/** Niebla y viraje de color cuando la cámara del jugador está bajo el agua (§8: «vista
	 * submarina bonita»); el océano no usa el plugin Water de Epic, así que esto no es
	 * automático y hay que aplicarlo a mano sobre el postproceso y la niebla existentes. */
	void ApplyUnderwater(float DeltaSeconds);

	float CloudCover = 0.2f;
	float WeatherFog = 0.0f;
	float WeatherRain = 0.0f;
	/** Fracción sumergida actual (0 en superficie, 1 ya asentada bajo el agua); suaviza la transición. */
	float UnderwaterBlend = 0.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AExploredOcean> Ocean;

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
