#include "UI/ExploredMenuCamera.h"

#include "Camera/CameraComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"

#include "WorldGen/IslandLayout.h"
#include "WorldGen/IslandLayoutLoader.h"
#include "WorldGen/IslandPropsLoader.h"
#include "WorldGen/IslandPropsModel.h"

namespace ExploredMenuCameraDetail
{
	/** Órbita lenta alrededor del faro (GDD v3 §9.2: la isla al atardecer con la baliza apagada). */
	constexpr float LighthouseOrbitRadiusCm = 15000.0f;
	constexpr float LighthouseOrbitHeightCm = 3600.0f;
	constexpr float LighthouseLookHeightCm = 1400.0f;
	constexpr float LighthouseOrbitDegreesPerSecond = 1.2f;
	const FName TowerProp(TEXT("faro_torre"));
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
	using namespace ExploredMenuCameraDetail;
	if (const FIslandProp* Tower = ExploredIslandProps::Get()->FindProp(TowerProp))
	{
		const FVector Base = IslandPropsPlacement::WorldLocation(*Tower, 0.0f);
		OrbitCenter = Base + FVector(0.0, 0.0, LighthouseLookHeightCm);
		OrbitRadius = LighthouseOrbitRadiusCm;
		OrbitHeight = LighthouseOrbitHeightCm;
		OrbitDegreesPerSecond = LighthouseOrbitDegreesPerSecond;
		// Empieza con el sol de la tarde detrás del faro (oeste, -Y en island.json).
		OrbitAngleDeg = 70.0f;
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
