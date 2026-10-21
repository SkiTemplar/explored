#include "UI/ExploredMenuCamera.h"

#include "Camera/CameraComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"

#include "WorldGen/IslandLayout.h"
#include "WorldGen/IslandLayoutLoader.h"

namespace ExploredMenuCameraDetail
{
	/** Órbita sobre la isla de llegada: distancia en radios de isla, altura y velocidad. */
	constexpr float LandingOrbitRadii = 1.45f;
	constexpr float LandingOrbitHeightCm = 13000.0f;
	constexpr float LandingLookHeightCm = 1500.0f;
	constexpr float LandingOrbitDegreesPerSecond = 2.2f;
}

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

void AExploredMenuCamera::BeginPlay()
{
	Super::BeginPlay();
	if (!bOrbitLandingIsland)
	{
		return;
	}
	const FIslandLayout Layout = *ExploredIsland::Get();
	if (const FIslandDesc* Landing = Layout.FindIsland(EIslandArchetype::Landing))
	{
		using namespace ExploredMenuCameraDetail;
		OrbitCenter = FVector(Landing->Center.X * 100.0, Landing->Center.Y * 100.0, LandingLookHeightCm);
		OrbitRadius = Landing->Radius * 100.0f * LandingOrbitRadii;
		OrbitHeight = LandingOrbitHeightCm;
		OrbitDegreesPerSecond = LandingOrbitDegreesPerSecond;
		// Empieza mirando la playa de llegada (la laguna queda hacia -Rotation desde el centro).
		OrbitAngleDeg = FMath::RadiansToDegrees(Landing->Rotation) + 180.0f;
	}
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
