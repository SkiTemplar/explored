#include "Sky/ExploredSkyController.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "Sky/TimeOfDaySubsystem.h"

namespace
{
	constexpr float SunIlluminanceLux = 9.0f;
	constexpr float MoonIlluminanceLux = 0.35f;
}

AExploredSkyController::AExploredSkyController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetAtmosphereSunLightIndex(0);
	Sun->Intensity = SunIlluminanceLux;
	Sun->LightSourceAngle = 0.8f;
	Sun->bUseTemperature = true;
	Sun->Temperature = 5900.0f;
	Sun->SetCastShadows(true);
	Sun->bCastCloudShadows = true;
	Sun->bCastShadowsOnClouds = true;
	Sun->SetDynamicShadowDistanceMovableLight(20000.0f);
	Sun->DynamicShadowCascades = 4;

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetAtmosphereSunLight(true);
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->Intensity = MoonIlluminanceLux;
	Moon->LightColor = FColor(170, 190, 255);
	Moon->LightSourceAngle = 0.55f;
	Moon->SetCastShadows(true);
	Moon->bAffectsWorld = true;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);
	// Aire tropical algo más húmedo: más Mie y un azul algo más profundo.
	Atmosphere->MieScatteringScale = 0.006f;
	Atmosphere->MieAnisotropy = 0.82f;
	Atmosphere->RayleighScatteringScale = 0.0331f;
	Atmosphere->MultiScatteringFactor = 1.2f;

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = SLS_CapturedScene;
	SkyLight->Intensity = 1.0f;
	SkyLight->bLowerHemisphereIsBlack = false;
	SkyLight->LowerHemisphereColor = FLinearColor(0.02f, 0.05f, 0.06f);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.012f);
	Fog->SetFogHeightFalloff(0.12f);
	Fog->SetVolumetricFog(true);
	Fog->VolumetricFogScatteringDistribution = 0.75f;
	Fog->VolumetricFogExtinctionScale = 0.6f;
	Fog->SetVolumetricFogDistance(9000.0f);
	Fog->SetStartDistance(800.0f);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);
	Clouds->SetLayerBottomAltitude(2.2f);
	Clouds->SetLayerHeight(6.0f);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudMaterial(
		TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
	if (CloudMaterial.Succeeded())
	{
		Clouds->SetMaterial(CloudMaterial.Object);
	}

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;
	FPostProcessSettings& PP = PostProcess->Settings;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = -2.0f;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = 12.0f;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.6f;
	// Tono cálido, saturación algo alta y bloom suave: la isla debe invitar a explorar.
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(1.12f, 1.12f, 1.12f, 1.0f);
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(1.05f, 1.05f, 1.05f, 1.0f);
	PP.bOverride_WhiteTemp = true;
	PP.WhiteTemp = 6200.0f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.45f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.3f;
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = 0.15f;
	PP.bOverride_LumenSceneLightingQuality = true;
	PP.LumenSceneLightingQuality = 1.0f;

	StarDome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StarDome"));
	StarDome->SetupAttachment(Root);
	StarDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StarDome->SetCastShadow(false);
	StarDome->SetWorldScale3D(FVector(20000.0f));
	StarDome->bVisibleInReflectionCaptures = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/EngineMeshes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		StarDome->SetStaticMesh(Sphere.Object);
	}
}

void AExploredSkyController::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UMaterialInterface* Stars = StarMaterial.LoadSynchronous();
	StarDome->SetMaterial(0, Stars);
	StarDome->SetVisibility(Stars != nullptr);
	ApplyTime(EditorHours, EditorDay + EditorHours / 24.0f);
}

void AExploredSkyController::BeginPlay()
{
	Super::BeginPlay();
	if (UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>())
	{
		ApplyTime(Time->GetHours(), Time->GetTotalDays());
	}
}

void AExploredSkyController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>())
	{
		ApplyTime(Time->GetHours(), Time->GetTotalDays());
	}
}

void AExploredSkyController::ApplyTime(float Hours, float TotalDays)
{
	const float DayOfYear = FMath::Fmod(TotalDays, static_cast<float>(ExploredSky::DaysPerYear));
	const FVector SunDir = ExploredSky::SunDirection(Hours, DayOfYear);
	const float SunZ = static_cast<float>(SunDir.Z);
	const float Phase = ExploredSky::MoonPhase(TotalDays);
	const FVector MoonDir = ExploredSky::MoonDirection(Hours, DayOfYear, Phase);
	const float MoonZ = static_cast<float>(MoonDir.Z);

	// La luz viaja en sentido opuesto a la dirección hacia el astro.
	Sun->SetWorldRotation((-SunDir).Rotation());
	Moon->SetWorldRotation((-MoonDir).Rotation());

	// El Sol se apaga suavemente bajo el horizonte; la Luna toma el relevo.
	const float SunUp = FMath::SmoothStep(-0.08f, 0.04f, SunZ);
	const float MoonUp = FMath::SmoothStep(-0.05f, 0.08f, MoonZ);
	Sun->SetIntensity(SunIlluminanceLux * SunUp);
	Sun->SetVisibility(SunUp > 0.001f);
	// Atardecer más cálido.
	Sun->SetTemperature(FMath::Lerp(3600.0f, 5900.0f, FMath::SmoothStep(0.0f, 0.35f, SunZ)));

	const float Illumination = ExploredSky::MoonIllumination(Phase);
	const float NightFactor = 1.0f - SunUp;
	Moon->SetIntensity(MoonIlluminanceLux * MoonUp * NightFactor * FMath::Lerp(0.15f, 1.0f, Illumination));
	Moon->SetVisibility(MoonUp * NightFactor > 0.001f);

	if (StarDome && StarDome->GetStaticMesh())
	{
		StarDome->SetScalarParameterValueOnMaterials(TEXT("Night"), 1.0f - FMath::SmoothStep(-0.15f, 0.05f, SunZ));
	}

	// Noches más densas y frescas; niebla matinal suave.
	const bool bMorning = Hours > 4.5f && Hours < 9.0f;
	Fog->SetFogDensity(bMorning ? 0.02f : 0.012f);
	Fog->SetFogInscatteringColor(FMath::Lerp(FLinearColor(0.02f, 0.03f, 0.06f), FLinearColor(0.45f, 0.6f, 0.75f), SunUp));
}
