#include "Ocean/ExploredOcean.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr int32 RadialSegments = 256;
	constexpr int32 Rings = 150;
	constexpr float InnerRadius = 60.0f;      // cm
	constexpr float OuterRadius = 3000000.0f;  // 30 km
}

AExploredOcean::AExploredOcean()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Surface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Surface"));
	SetRootComponent(Surface);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCastShadow(false);
	Surface->bUseAsyncCooking = true;

}

void AExploredOcean::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (UMaterialInterface* Material = OceanMaterial.LoadSynchronous())
	{
		Surface->SetMaterial(0, Material);
	}
	Waves = FOceanWaves::Make(SeaState);
	BuildMesh();
}

void AExploredOcean::BeginPlay()
{
	Super::BeginPlay();
	Waves = FOceanWaves::Make(SeaState);
	if (UMaterialInterface* Base = OceanMaterial.LoadSynchronous())
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(Base, this);
		Surface->SetMaterial(0, MaterialInstance);
	}
	PushWaveParameters();
}

void AExploredOcean::BuildMesh()
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	Vertices.Reserve(1 + Rings * RadialSegments);
	Vertices.Add(FVector::ZeroVector);

	// Radios con progresión geométrica: resolución fina cerca, gruesa lejos.
	const float Growth = FMath::Pow(OuterRadius / InnerRadius, 1.0f / (Rings - 1));
	float Radius = InnerRadius;
	for (int32 R = 0; R < Rings; ++R)
	{
		for (int32 S = 0; S < RadialSegments; ++S)
		{
			const float Angle = UE_TWO_PI * S / RadialSegments;
			Vertices.Add(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f));
		}
		Radius *= Growth;
	}

	auto Ring = [](int32 R, int32 S) { return 1 + R * RadialSegments + (S % RadialSegments); };

	// Orientación de Unreal: (B - A) x (C - A) apunta hacia abajo para caras mirando arriba.
	for (int32 S = 0; S < RadialSegments; ++S)
	{
		Triangles.Append({0, Ring(0, S + 1), Ring(0, S)});
	}
	for (int32 R = 0; R + 1 < Rings; ++R)
	{
		for (int32 S = 0; S < RadialSegments; ++S)
		{
			const int32 A = Ring(R, S);
			const int32 B = Ring(R, S + 1);
			const int32 C = Ring(R + 1, S);
			const int32 D = Ring(R + 1, S + 1);
			Triangles.Append({A, B, C, B, D, C});
		}
	}

	Normals.Init(FVector::UpVector, Vertices.Num());
	Tangents.Init(FProcMeshTangent(1.0f, 0.0f, 0.0f), Vertices.Num());
	UVs.SetNumZeroed(Vertices.Num());
	Colors.Init(FColor::White, Vertices.Num());

	Surface->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
	// Límites enormes para que el desplazamiento de vértices nunca se recorte.
	Surface->SetBoundsScale(2.0f);
}

float AExploredOcean::GetWaveTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0f;
}

void AExploredOcean::PushWaveParameters()
{
	if (!MaterialInstance)
	{
		return;
	}
	for (int32 I = 0; I < FOceanWaves::NumWaves; ++I)
	{
		const FGerstnerWave& W = Waves.Waves[I];
		// Dirección XY, longitud de onda, amplitud en un vector y la inclinación aparte.
		MaterialInstance->SetVectorParameterValue(*FString::Printf(TEXT("Wave%d"), I),
			FLinearColor(W.Direction.X, W.Direction.Y, W.Wavelength, W.Amplitude));
		MaterialInstance->SetScalarParameterValue(*FString::Printf(TEXT("Steepness%d"), I), W.Steepness);
	}
	MaterialInstance->SetScalarParameterValue(TEXT("WaveCount"), FOceanWaves::NumWaves);
	MaterialInstance->SetScalarParameterValue(TEXT("SeaState"), SeaState);
}

void AExploredOcean::SetSeaState(float InSeaState)
{
	SeaState = FMath::Clamp(InSeaState, 0.0f, 1.0f);
	Waves = FOceanWaves::Make(SeaState);
	PushWaveParameters();
}

void AExploredOcean::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Sigue a la cámara en XY con ajuste a rejilla; la altura queda en el nivel del mar.
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
			const FVector Snapped(
				FMath::GridSnap(Camera.X, SnapStep),
				FMath::GridSnap(Camera.Y, SnapStep),
				0.0f);
			SetActorLocation(Snapped);
		}
	}
	if (MaterialInstance)
	{
		MaterialInstance->SetScalarParameterValue(TEXT("WaveTime"), GetWaveTime());
	}
}

float AExploredOcean::GetWaterHeightAt(const FVector& WorldLocation) const
{
	return Waves.HeightAt(FVector2D(WorldLocation.X, WorldLocation.Y), GetWaveTime());
}

FVector AExploredOcean::GetWaterNormalAt(const FVector& WorldLocation) const
{
	return Waves.NormalAt(FVector2D(WorldLocation.X, WorldLocation.Y), GetWaveTime());
}
