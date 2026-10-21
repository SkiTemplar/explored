#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Weather/RainStreaksModel.h"

#include "ExploredWeatherFxSubsystem.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Efectos visibles del clima (F6, GDD v3 §11):
 * - Lluvia low poly: trazos geométricos (prisma fino con M_RainStreak) alrededor de la cámara, con la
 *   intensidad de UExploredWeatherSubsystem; no se dibujan bajo techo (AExploredShelterVolume) ni
 *   bajo el agua. Posiciones de FRainStreaksModel.
 * - Viento en palmeras y bambú: una instancia dinámica de M_Toon con WindSway = 1 compartida por
 *   todas sus mallas (decorado y props), con WindStrength = viento del clima.
 *
 * Blueprint: «Get Rain Streak Count».
 */
UCLASS()
class EXPLORED_API UExploredWeatherFxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Trazos de lluvia dibujados en el último fotograma (para el bot y las capturas). */
	UFUNCTION(BlueprintPure, Category = "Explored|Clima")
	int32 GetRainStreakCount() const { return LastStreaks; }

	/** Mallas con viento (palmeras y bambú) enlazadas a la instancia compartida. */
	int32 GetSwayingMeshCount() const { return SwayingMeshes; }

private:
	void EnsureRain();
	void UpdateRain(float Intensity, float Wind);
	void BindFoliageWind();

	TUniquePtr<FRainStreaksModel> Rain;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RainActor;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> RainMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FoliageWind;

	/** Búfer de transformaciones de las gotas, reutilizado cada fotograma (MaxStreaks entradas). */
	TArray<FTransform> RainTransforms;

	double Clock = 0.0;
	float BindTimer = 1.0f;
	int32 BindPasses = 0;
	int32 LastStreaks = 0;
	int32 SwayingMeshes = 0;
};
