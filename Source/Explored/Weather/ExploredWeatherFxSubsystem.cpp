#include "Weather/ExploredWeatherFxSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectIterator.h"

#include "Explored.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "Weather/IslandWeatherQuery.h"

namespace WeatherFxDetail
{
	const TCHAR* const StreakMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const StreakMaterial = TEXT("/Game/Art/Materials/M_RainStreak.M_RainStreak");
	const TCHAR* const ToonMaterial = TEXT("/Game/Art/Materials/M_Toon.M_Toon");
	const FName WindSway(TEXT("WindSway"));
	const FName WindStrength(TEXT("WindStrength"));
	/** Grosor del trazo (fracción del cubo de 1 m). */
	constexpr double StreakThickness = 0.008;
	/** Opacidad de los trazos (M_RainStreak «Opacity»). */
	constexpr float StreakOpacity = 0.28f;
	/** Dirección del viento dominante (alisio del este, como las nubes). */
	const FVector2D WindDir(-0.35, -1.0);
	constexpr uint32 Seed = 0x5A1A7u;
	/** Pasadas de enlace del viento: el populador crea las palmeras en su BeginPlay. */
	constexpr int32 MaxBindPasses = 3;

	bool IsSwayingMesh(const UStaticMesh* Mesh)
	{
		const FString Name = Mesh ? Mesh->GetName() : FString();
		return Name.StartsWith(TEXT("SM_Palm")) || Name.StartsWith(TEXT("SM_Bamboo"));
	}
}

bool UExploredWeatherFxSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExploredWeatherFxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Rain = MakeUnique<FRainStreaksModel>(FRainStreaksTuning(), WeatherFxDetail::Seed);
	UMaterialInterface* Toon = LoadObject<UMaterialInterface>(nullptr, WeatherFxDetail::ToonMaterial);
	if (Toon)
	{
		FoliageWind = UMaterialInstanceDynamic::Create(Toon, this);
		FoliageWind->SetScalarParameterValue(WeatherFxDetail::WindSway, 1.0f);
	}
}

void UExploredWeatherFxSubsystem::EnsureRain()
{
	if (RainMesh || !Rain)
	{
		return;
	}
	UWorld* World = GetWorld();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, WeatherFxDetail::StreakMesh);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, WeatherFxDetail::StreakMaterial);
	if (!World || !Mesh || !Material)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Clima] Sin malla o material de lluvia (Tools/Unreal/materials_f6.py)"));
		Rain.Reset();
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	RainActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	RainMesh = NewObject<UInstancedStaticMeshComponent>(RainActor, TEXT("RainStreaks"), RF_Transient);
	RainMesh->SetStaticMesh(Mesh);
	UMaterialInstanceDynamic* Streak = UMaterialInstanceDynamic::Create(Material, RainActor);
	Streak->SetScalarParameterValue(TEXT("Opacity"), WeatherFxDetail::StreakOpacity);
	RainMesh->SetMaterial(0, Streak);
	RainMesh->SetMobility(EComponentMobility::Movable);
	RainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RainMesh->SetCastShadow(false);
	RainMesh->SetCanEverAffectNavigation(false);
	RainActor->SetRootComponent(RainMesh);
	RainMesh->RegisterComponent();
	TArray<FTransform> Hidden;
	Hidden.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), Rain->GetTuning().MaxStreaks);
	RainMesh->AddInstances(Hidden, false, true);
	RainMesh->SetVisibility(false);
}

void UExploredWeatherFxSubsystem::UpdateRain(float Intensity, float Wind)
{
	LastStreaks = 0;
	const APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!Rain || !PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
	// Bajo techo y bajo el agua no se ve llover.
	const bool bHidden = Camera.Z < -30.0 || ExploredIslandWeather::IsSheltered(this, Camera);
	const int32 Count = bHidden ? 0 : Rain->CountFor(Intensity);
	if (Count == 0)
	{
		if (RainMesh && RainMesh->IsVisible())
		{
			RainMesh->SetVisibility(false);
		}
		return;
	}
	EnsureRain();
	if (!RainMesh)
	{
		return;
	}
	const FRainStreaksFrame Frame = Rain->Evaluate(Clock, Intensity, Wind, WeatherFxDetail::WindDir, Camera);
	const FQuat Tilt = FRotationMatrix::MakeFromZ(Frame.Direction).ToQuat();
	const FVector Scale(WeatherFxDetail::StreakThickness, WeatherFxDetail::StreakThickness, Frame.LengthCm / 100.0);
	// Las gotas sobrantes quedan con escala 0 en la cámara; el búfer no se realoja cada fotograma.
	const int32 MaxStreaks = Rain->GetTuning().MaxStreaks;
	const int32 Visible = FMath::Min(Frame.Positions.Num(), MaxStreaks);
	RainTransforms.SetNumUninitialized(MaxStreaks, EAllowShrinking::No);
	for (int32 Index = 0; Index < Visible; ++Index)
	{
		RainTransforms[Index] = FTransform(Tilt, Frame.Positions[Index], Scale);
	}
	const FTransform HiddenStreak(FQuat::Identity, Camera, FVector::ZeroVector);
	for (int32 Index = Visible; Index < MaxStreaks; ++Index)
	{
		RainTransforms[Index] = HiddenStreak;
	}
	RainMesh->BatchUpdateInstancesTransforms(0, RainTransforms, true, true, true);
	RainMesh->SetVisibility(true);
	LastStreaks = Visible;
}

void UExploredWeatherFxSubsystem::BindFoliageWind()
{
	if (!FoliageWind)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const UMaterial* Base = FoliageWind->GetBaseMaterial();
	int32 Bound = 0;
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* Component = *It;
		if (!Component || Component->GetWorld() != World || !WeatherFxDetail::IsSwayingMesh(Component->GetStaticMesh()))
		{
			continue;
		}
		for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
		{
			UMaterialInterface* Current = Component->GetMaterial(Slot);
			if (Current && Current != FoliageWind && Current->GetBaseMaterial() == Base)
			{
				Component->SetMaterial(Slot, FoliageWind);
			}
		}
		++Bound;
	}
	SwayingMeshes = Bound;
	UE_LOG(LogExplored, Display, TEXT("[Clima] Viento en %d mallas de palmera y bambú"), Bound);
}

void UExploredWeatherFxSubsystem::Tick(float DeltaTime)
{
	Clock += DeltaTime;
	if (BindPasses < WeatherFxDetail::MaxBindPasses)
	{
		BindTimer -= DeltaTime;
		if (BindTimer <= 0.0f)
		{
			BindFoliageWind();
			++BindPasses;
			BindTimer = 3.0f;
		}
	}
	const UExploredWeatherSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	const float Intensity = Weather ? Weather->GetCurrent().Rain : 0.0f;
	const float Wind = Weather ? Weather->GetCurrent().Wind : 0.2f;
	if (FoliageWind)
	{
		FoliageWind->SetScalarParameterValue(WeatherFxDetail::WindStrength, Wind);
	}
	UpdateRain(Intensity, Wind);
}

TStatId UExploredWeatherFxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredWeatherFxSubsystem, STATGROUP_Tickables);
}
