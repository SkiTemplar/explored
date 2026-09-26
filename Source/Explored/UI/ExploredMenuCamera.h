#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"

#include "ExploredMenuCamera.generated.h"

/**
 * Cámara cinemática del menú principal: orbita despacio sobre el
 * archipiélago a altura constante, mirando siempre al centro (GDD §10).
 * AExploredPlayerController busca una instancia en el mundo y, si no
 * encuentra ninguna, la genera con los valores por defecto (ver
 * UI/ExploredPlayerController.cpp).
 */
UCLASS()
class EXPLORED_API AExploredMenuCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	AExploredMenuCamera();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "Explored|Cámara de menú")
	FVector OrbitCenter = FVector(0.0f, 0.0f, 4000.0f);

	/** 400 m por defecto (GDD §10: sobrevuela a 400 m de altura). */
	UPROPERTY(EditAnywhere, Category = "Explored|Cámara de menú")
	float OrbitHeight = 40000.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Cámara de menú")
	float OrbitRadius = 300000.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Cámara de menú")
	float OrbitDegreesPerSecond = 1.6f;

private:
	float OrbitAngleDeg = 0.0f;
};
