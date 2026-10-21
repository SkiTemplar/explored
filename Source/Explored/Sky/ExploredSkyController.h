#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sky/CloudFieldModel.h"

#include "ExploredSkyController.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class UPostProcessComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class AExploredOcean;
struct FSkyLook;

/**
 * Cielo low poly de la Isla de Sal (GDD v3 §11): cúpula con degradado de paleta por momento
 * del día (M_SkyGradient), nubes K5 que derivan con el viento, discos de sol y luna, estrellas
 * (M_Stars), sol y luna direccionales, luz de cielo tintada, niebla lejana del color del
 * horizonte y postproceso. Lee la hora del UTimeOfDaySubsystem y el clima con SetWeather.
 */
UCLASS()
class EXPLORED_API AExploredSkyController : public AActor
{
	GENERATED_BODY()

public:
	AExploredSkyController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Nubosidad, lluvia, viento y niebla del clima actual (las aplica en el siguiente fotograma). */
	void SetWeather(const struct FWeatherSample& Weather);

	/** Hora que se muestra en el editor (fuera de juego). */
	UPROPERTY(EditAnywhere, Category = "Explored|Cielo", meta = (ClampMin = 0, ClampMax = 24))
	float EditorHours = 16.5f;

	UPROPERTY(EditAnywhere, Category = "Explored|Cielo")
	float EditorDay = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Cielo")
	TSoftObjectPtr<UMaterialInterface> StarMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Stars.M_Stars")));

	UPROPERTY(EditAnywhere, Category = "Explored|Cielo")
	TSoftObjectPtr<UMaterialInterface> SkyMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/M_SkyGradient.M_SkyGradient")));

	UPROPERTY(EditAnywhere, Category = "Explored|Cielo")
	TSoftObjectPtr<UMaterialInterface> CloudMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/M_Nube.M_Nube")));

	UPROPERTY(EditAnywhere, Category = "Explored|Cielo")
	TSoftObjectPtr<UMaterialInterface> DiscMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/M_SkyDisc.M_SkyDisc")));

	/** Número de nubes K5 del anillo de 300-900 m. */
	UPROPERTY(EditAnywhere, Category = "Explored|Cielo", meta = (ClampMin = 0, ClampMax = 64))
	int32 CloudCount = 34;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyTime(float Hours, float TotalDays, float DeltaSeconds);
	void ApplyLook(const FSkyLook& Look, float SunZ);
	void ApplyPostProcess(float GoldenHour, float SunUp);
	void SetupSkyAssets();
	void UpdateClouds(float DeltaSeconds, float CloudCoverNow);
	void PlaceDisc(UStaticMeshComponent* Disc, const FVector& Direction, float AngularDiameterDeg, bool bVisible) const;
	FVector GetViewLocation() const;
	/** Niebla y viraje de color cuando la cámara del jugador está bajo el agua. */
	void ApplyUnderwater(float DeltaSeconds);

	float CloudCover = 0.2f;
	float WeatherFog = 0.0f;
	float WeatherRain = 0.0f;
	float WeatherWind = 0.2f;
	/** Fracción sumergida actual (0 en superficie, 1 ya asentada bajo el agua); suaviza la transición. */
	float UnderwaterBlend = 0.0f;

	FCloudFieldModel CloudField;
	/** Índice de instancia de cada nube dentro del componente de su variante. */
	TArray<int32> CloudInstanceIndex;

	UPROPERTY(Transient)
	TWeakObjectPtr<AExploredOcean> Ocean;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UDirectionalLightComponent> Moon;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UPostProcessComponent> PostProcess;

	/** Esfera de estrellas (visible de noche). */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UStaticMeshComponent> StarDome;

	/** Cúpula del degradado de cielo (material de cielo, la captura la luz de cielo). */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UStaticMeshComponent> SkyDome;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UStaticMeshComponent> SunDisc;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TObjectPtr<UStaticMeshComponent> MoonDisc;

	/** Una instancia por nube, un componente por variante SM_Nube_A..E. */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Cielo")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> CloudLayers;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyMid;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CloudMid;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SunMid;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MoonMid;

	/**
	 * MPC_Terreno (Tools/Unreal/material_utils.py): el sol para el brillo de la arena de M_TerrainToon
	 * (SunDirection, SunGlint). Se carga en BeginPlay; si falta, la arena no brilla.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> TerrainParams;
};
