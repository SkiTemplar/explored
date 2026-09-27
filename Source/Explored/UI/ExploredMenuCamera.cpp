#include "UI/ExploredMenuCamera.h"

#include "Camera/CameraComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"

AExploredMenuCamera::AExploredMenuCamera()
{
	// Justificado: es la única cámara del proyecto que necesita moverse sola
	// mientras el jugador está en el menú; el resto del juego no tiene tick.
	PrimaryActorTick.bCanEverTick = true;
	GetCameraComponent()->SetFieldOfView(65.0f);

	// Sigue GetActorLocation() cada vez que World Partition actualiza sus fuentes de streaming
	// (no hace falta que el componente tickee); con Shapes vacío usa una esfera del radio de
	// carga de cada grid centrada en la cámara, que barre la órbita entera con ella.
	StreamingSource = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(TEXT("StreamingSource"));
}

void AExploredMenuCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	OrbitAngleDeg = FMath::Fmod(OrbitAngleDeg + OrbitDegreesPerSecond * DeltaTime, 360.0f);
	const float AngleRad = FMath::DegreesToRadians(OrbitAngleDeg);
	const FVector Location = OrbitCenter + FVector(
		FMath::Cos(AngleRad) * OrbitRadius,
		FMath::Sin(AngleRad) * OrbitRadius,
		OrbitHeight);
	SetActorLocation(Location);
	SetActorRotation((OrbitCenter - Location).Rotation());
}
