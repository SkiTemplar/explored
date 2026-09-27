#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"

#include "ExploredMenuCamera.generated.h"

class UWorldPartitionStreamingSourceComponent;

/**
 * Cámara cinemática del menú principal: orbita despacio sobre el
 * archipiélago a altura constante, mirando siempre al centro (GDD §10).
 * AExploredPlayerController busca una instancia en el mundo y, si no
 * encuentra ninguna, la genera con los valores por defecto (ver
 * UI/ExploredPlayerController.cpp).
 *
 * World Partition transmite en función de la posición del pawn poseído, no
 * de la cámara activa: mientras el jugador está en el menú puede no haber
 * pawn (o uno quieto en el origen), así que sin una fuente de streaming
 * propia el terreno bajo la órbita (radio de 3 km, toda la anchura del
 * archipiélago) no se cargaría y se verían huecos. StreamingSource sigue la
 * ubicación de este actor automáticamente (ver Tick) y la registra en
 * UWorldPartitionSubsystem mientras el componente está activo.
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

	UPROPERTY(VisibleAnywhere, Category = "Explored|Cámara de menú")
	TObjectPtr<UWorldPartitionStreamingSourceComponent> StreamingSource;
};
