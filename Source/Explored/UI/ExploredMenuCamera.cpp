#include "UI/ExploredMenuCamera.h"

#include "Camera/CameraComponent.h"

AExploredMenuCamera::AExploredMenuCamera()
{
	// Justificado: es la única cámara del proyecto que necesita moverse sola
	// mientras el jugador está en el menú; el resto del juego no tiene tick.
	PrimaryActorTick.bCanEverTick = true;
	GetCameraComponent()->SetFieldOfView(65.0f);
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
