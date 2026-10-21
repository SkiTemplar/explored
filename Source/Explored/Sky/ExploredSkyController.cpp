#include "Sky/ExploredSkyController.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "Ocean/ExploredOcean.h"
#include "Sky/SkyPaletteModel.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Weather/WeatherModel.h"

namespace ExploredSkyDetail
{
	constexpr float SunIlluminanceLux = 9.0f;
	constexpr float MoonIlluminanceLux = 0.6f;
	/** Cúpula de cielo (cm de radio = 50 × escala) y distancia de los discos: fuera de la niebla. */
	constexpr float SkyDomeScale = 110000.0f;
	constexpr float DiscDistanceCm = 4000000.0f;
	constexpr float FogCutoffCm = 3400000.0f;
	constexpr float SunDiameterDeg = 7.0f;
	constexpr float MoonDiameterDeg = 5.0f;
	/** Brillo (emisivo) de la cúpula de día: el cielo algo más claro que la arena al sol. */
	constexpr float SkyLuminance = 1.8f;
	constexpr uint32 CloudSeed = 20261021u;
	/** Alisio del este: las nubes derivan hacia el oeste-suroeste. */
	const FVector2D WindDirection(-0.35, -0.94);
	const TCHAR* CloudMeshes[FCloudFieldModel::NumVariants] = {
		TEXT("/Game/Art/K5_Cielo/SM_Nube_A_01.SM_Nube_A_01"), TEXT("/Game/Art/K5_Cielo/SM_Nube_B_01.SM_Nube_B_01"),
		TEXT("/Game/Art/K5_Cielo/SM_Nube_C_01.SM_Nube_C_01"), TEXT("/Game/Art/K5_Cielo/SM_Nube_D_01.SM_Nube_D_01"),
		TEXT("/Game/Art/K5_Cielo/SM_Nube_E_01.SM_Nube_E_01"),
	};
	const TCHAR* SunDiscMesh = TEXT("/Game/Art/K5_Cielo/SM_DiscoSol_01.SM_DiscoSol_01");
	const TCHAR* MoonDiscMesh = TEXT("/Game/Art/K5_Cielo/SM_DiscoLuna_01.SM_DiscoLuna_01");

	UStaticMeshComponent* MakeSkyMesh(AActor* Owner, USceneComponent* Root, const TCHAR* Name)
	{
		UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->bVisibleInReflectionCaptures = false;
		Mesh->SetMobility(EComponentMobility::Movable);
		return Mesh;
	}

	FVector4 Vec4(float V) { return FVector4(V, V, V, 1.0f); }
}

AExploredSkyController::AExploredSkyController()
{
	using namespace ExploredSkyDetail;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->Intensity = SunIlluminanceLux;
	Sun->SetCastShadows(true);
	Sun->SetDynamicShadowDistanceMovableLight(20000.0f);
	Sun->DynamicShadowCascades = 4;
	// Penumbra ancha: sombras suaves de aspecto estilizado, no duras de mediodía.
	Sun->LightSourceAngle = 1.6f;
	Sun->ContactShadowLength = 0.03f;
	Sun->ContactShadowLengthInWS = false;

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->Intensity = MoonIlluminanceLux;
	Moon->LightColor = FSkyPaletteModel::PaletteColor(TEXT("gris_lila")).ToFColor(true);
	Moon->LightSourceAngle = 0.9f;
	Moon->SetCastShadows(true);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	// Captura en tiempo real de la cúpula (material de cielo): la luz ambiente toma el tono del degradado.
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = SLS_CapturedScene;
	SkyLight->Intensity = 1.0f;
	SkyLight->bLowerHemisphereIsBlack = false;
	// Sin oclusión por campos de distancia: dejaba manchas oscuras difusas bajo los objetos.
	SkyLight->SetCastShadows(false);
	SkyLight->LowerHemisphereColor = FSkyPaletteModel::PaletteColor(TEXT("agua_media"));

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	// Niebla del color del horizonte solo en la lejanía: el mar se funde con el cielo.
	Fog->SetFogDensity(0.0012f);
	Fog->SetFogHeightFalloff(0.2f);
	Fog->SetVolumetricFog(false);
	Fog->SetStartDistance(15000.0f);
	Fog->FogCutoffDistance = FogCutoffCm;

	SkyDome = MakeSkyMesh(this, Root, TEXT("SkyDome"));
	SkyDome->SetWorldScale3D(FVector(SkyDomeScale));
	StarDome = MakeSkyMesh(this, Root, TEXT("StarDome"));
	StarDome->SetWorldScale3D(FVector(SkyDomeScale * 0.95f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/EngineMeshes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		SkyDome->SetStaticMesh(Sphere.Object);
		StarDome->SetStaticMesh(Sphere.Object);
	}
	SunDisc = MakeSkyMesh(this, Root, TEXT("SunDisc"));
	MoonDisc = MakeSkyMesh(this, Root, TEXT("MoonDisc"));

	for (int32 V = 0; V < FCloudFieldModel::NumVariants; ++V)
	{
		UInstancedStaticMeshComponent* Layer = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Clouds%d"), V));
		Layer->SetupAttachment(Root);
		Layer->SetMobility(EComponentMobility::Movable);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->bVisibleInReflectionCaptures = false;
		CloudLayers.Add(Layer);
	}

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;
	FPostProcessSettings& PP = PostProcess->Settings;
	// Exposición casi fija por hora (rango estrecho) para que no «respire» al girar la cámara.
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = -1.2f;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = 2.0f;
	PP.bOverride_AutoExposureSpeedUp = true;
	PP.AutoExposureSpeedUp = 6.0f;
	PP.bOverride_AutoExposureSpeedDown = true;
	PP.AutoExposureSpeedDown = 3.0f;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.3f;
	// Grading contenido (GDD §11): los colores ya son los de la paleta; sin saturar ni contrastar
	// de más, sin oclusión ambiental de pantalla, reflejos de pantalla, grano ni aberración.
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = Vec4(0.88f);
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = Vec4(1.02f);
	PP.bOverride_ColorGain = true;
	PP.ColorGain = Vec4(1.0f);
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.2f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.25f;
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = 0.0f;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.0f;
	PP.bOverride_AmbientOcclusionIntensity = true;
	PP.AmbientOcclusionIntensity = 0.0f;
	PP.bOverride_ScreenSpaceReflectionIntensity = true;
	PP.ScreenSpaceReflectionIntensity = 0.0f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.0f;
}

void AExploredSkyController::SetupSkyAssets()
{
	using namespace ExploredSkyDetail;
	if (UMaterialInterface* Stars = StarMaterial.LoadSynchronous())
	{
		StarDome->SetMaterial(0, Stars);
	}
	StarDome->SetVisibility(StarDome->GetMaterial(0) != nullptr);
	if (UMaterialInterface* Sky = SkyMaterial.LoadSynchronous())
	{
		SkyMid = UMaterialInstanceDynamic::Create(Sky, this);
		SkyDome->SetMaterial(0, SkyMid);
	}
	UMaterialInterface* DiscBase = DiscMaterial.LoadSynchronous();
	auto SetupDisc = [this, DiscBase](UStaticMeshComponent* Disc, const TCHAR* MeshPath) -> UMaterialInstanceDynamic*
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
		Disc->SetStaticMesh(Mesh);
		Disc->SetVisibility(Mesh != nullptr && DiscBase != nullptr);
		if (!Mesh || !DiscBase)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(DiscBase, this);
		Disc->SetMaterial(0, Mid);
		return Mid;
	};
	SunMid = SetupDisc(SunDisc, SunDiscMesh);
	MoonMid = SetupDisc(MoonDisc, MoonDiscMesh);

	UMaterialInterface* CloudBase = CloudMaterial.LoadSynchronous();
	CloudMid = CloudBase ? UMaterialInstanceDynamic::Create(CloudBase, this) : nullptr;
	for (int32 V = 0; V < CloudLayers.Num(); ++V)
	{
		CloudLayers[V]->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CloudMeshes[V]));
		if (CloudMid)
		{
			CloudLayers[V]->SetMaterial(0, CloudMid);
		}
	}
}

void AExploredSkyController::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetupSkyAssets();
	ApplyTime(EditorHours, EditorDay + EditorHours / 24.0f, 0.0f);
}

void AExploredSkyController::BeginPlay()
{
	Super::BeginPlay();
	SetupSkyAssets();
	CloudField = FCloudFieldModel::Make(ExploredSkyDetail::CloudSeed, CloudCount);
	CloudInstanceIndex.Reset();
	for (UInstancedStaticMeshComponent* Layer : CloudLayers)
	{
		Layer->ClearInstances();
	}
	for (const FCloudInstance& Cloud : CloudField.Clouds)
	{
		CloudInstanceIndex.Add(CloudLayers[Cloud.Variant]->AddInstance(FTransform::Identity, true));
	}
	UpdateClouds(0.0f, CloudCover);
	if (UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>())
	{
		ApplyTime(Time->GetHours(), Time->GetTotalDays(), 0.0f);
	}
}

void AExploredSkyController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>())
	{
		ApplyTime(Time->GetHours(), Time->GetTotalDays(), DeltaSeconds);
	}
	UpdateClouds(DeltaSeconds, CloudCover);
	ApplyUnderwater(DeltaSeconds);
}

void AExploredSkyController::SetWeather(const FWeatherSample& Weather)
{
	CloudCover = Weather.CloudCover;
	WeatherFog = Weather.Fog;
	WeatherRain = Weather.Rain;
	WeatherWind = Weather.Wind;
}

FVector AExploredSkyController::GetViewLocation() const
{
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			return PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	return GetActorLocation();
}

void AExploredSkyController::UpdateClouds(float DeltaSeconds, float CloudCoverNow)
{
	if (CloudInstanceIndex.Num() != CloudField.Clouds.Num())
	{
		return;
	}
	// El viento del temporal empuja las nubes más deprisa.
	CloudField.Advance(ExploredSkyDetail::WindDirection, WeatherWind, DeltaSeconds);
	for (int32 I = 0; I < CloudField.Clouds.Num(); ++I)
	{
		const FCloudInstance& Cloud = CloudField.Clouds[I];
		const float Visible = FCloudFieldModel::Visibility(Cloud, CloudCoverNow);
		// Con el cielo cubierto las nubes bajan y se aplastan un poco (techo de tormenta).
		const float Low = FMath::Lerp(1.0f, 0.65f, FSkyPaletteModel::StormFromCloudCover(CloudCoverNow));
		const FVector Location(Cloud.Position.X * 100.0, Cloud.Position.Y * 100.0, Cloud.Altitude * Low * 100.0);
		const float Scale = FMath::Max(Cloud.Scale * Visible, 0.001f);
		const FTransform Transform(FRotator(0.0, Cloud.YawDeg, 0.0), Location, FVector(Scale, Scale, Scale * Low));
		CloudLayers[Cloud.Variant]->UpdateInstanceTransform(CloudInstanceIndex[I], Transform, true, false, true);
	}
	for (UInstancedStaticMeshComponent* Layer : CloudLayers)
	{
		Layer->MarkRenderStateDirty();
	}
}

void AExploredSkyController::PlaceDisc(UStaticMeshComponent* Disc, const FVector& Direction, float AngularDiameterDeg, bool bVisible) const
{
	if (!Disc || !Disc->GetStaticMesh())
	{
		return;
	}
	Disc->SetVisibility(bVisible);
	if (!bVisible)
	{
		return;
	}
	const FVector Dir = Direction.GetSafeNormal();
	const float Diameter = 2.0f * ExploredSkyDetail::DiscDistanceCm * FMath::Tan(FMath::DegreesToRadians(AngularDiameterDeg * 0.5f));
	// La Z local del disco mira a la cámara y su Y local queda hacia arriba en pantalla.
	const FRotator Facing = FRotationMatrix::MakeFromZY(-Dir, FVector::UpVector).Rotator();
	Disc->SetWorldLocationAndRotation(GetViewLocation() + Dir * ExploredSkyDetail::DiscDistanceCm, Facing);
	Disc->SetWorldScale3D(FVector(Diameter / 100.0f));
}

void AExploredSkyController::ApplyTime(float Hours, float TotalDays, float DeltaSeconds)
{
	const float DayOfYear = FMath::Fmod(TotalDays, static_cast<float>(ExploredSky::DaysPerYear));
	const FVector SunDir = ExploredSky::SunDirection(Hours, DayOfYear);
	const float SunZ = static_cast<float>(SunDir.Z);
	const float Phase = ExploredSky::MoonPhase(TotalDays);
	const FVector MoonDir = ExploredSky::MoonDirection(Hours, DayOfYear, Phase);
	const float MoonZ = static_cast<float>(MoonDir.Z);
	const float Storm = FSkyPaletteModel::StormFromCloudCover(CloudCover);
	const FSkyLook Look = FSkyPaletteModel::Evaluate(SunZ, Storm);

	// La luz viaja en sentido opuesto a la dirección hacia el astro.
	Sun->SetWorldRotation((-SunDir).Rotation());
	Moon->SetWorldRotation((-MoonDir).Rotation());

	const float SunUp = FMath::SmoothStep(-0.08f, 0.04f, SunZ);
	const float MoonUp = FMath::SmoothStep(-0.05f, 0.08f, MoonZ);
	Sun->SetIntensity(ExploredSkyDetail::SunIlluminanceLux * SunUp * Look.SunIntensity);
	Sun->SetLightColor(Look.SunColor);
	Sun->SetVisibility(SunUp > 0.001f);

	const float Illumination = ExploredSky::MoonIllumination(Phase);
	const float NightFactor = 1.0f - SunUp;
	Moon->SetIntensity(ExploredSkyDetail::MoonIlluminanceLux * MoonUp * NightFactor * FMath::Lerp(0.15f, 1.0f, Illumination) * (1.0f - 0.8f * CloudCover));
	Moon->SetVisibility(MoonUp * NightFactor > 0.001f);

	if (StarDome && StarDome->GetStaticMesh())
	{
		// Con el cielo cubierto (muros del ciclón) no se ven las estrellas; en el ojo, sí.
		const float Overcast = FMath::SmoothStep(0.3f, 0.7f, CloudCover);
		StarDome->SetScalarParameterValueOnMaterials(TEXT("Night"), (1.0f - FMath::SmoothStep(-0.16f, -0.02f, SunZ)) * (1.0f - Overcast));
	}
	PlaceDisc(SunDisc, SunDir, ExploredSkyDetail::SunDiameterDeg, SunZ > -0.06f && Storm < 0.6f);
	PlaceDisc(MoonDisc, MoonDir, ExploredSkyDetail::MoonDiameterDeg, MoonZ > -0.06f && NightFactor > 0.3f && Storm < 0.6f);
	if (SkyMid)
	{
		SkyMid->SetVectorParameterValue(TEXT("SunDirection"), FLinearColor(SunDir.X, SunDir.Y, SunDir.Z, 0.0f));
	}
	ApplyLook(Look, SunZ);
	// Hora dorada: sol al ras del horizonte y todavía visible.
	ApplyPostProcess(SunUp * (1.0f - FMath::SmoothStep(0.06f, 0.3f, SunZ)), SunUp);
}

void AExploredSkyController::ApplyLook(const FSkyLook& Look, float SunZ)
{
	if (SkyMid)
	{
		const float Lum = ExploredSkyDetail::SkyLuminance * Look.SkyBrightness;
		SkyMid->SetVectorParameterValue(TEXT("HorizonColor"), Look.Horizon * Lum);
		SkyMid->SetVectorParameterValue(TEXT("MiddleColor"), Look.Middle * Lum);
		SkyMid->SetVectorParameterValue(TEXT("ZenithColor"), Look.Zenith * Lum);
		SkyMid->SetVectorParameterValue(TEXT("GlowColor"), Look.Glow * Lum);
		SkyMid->SetScalarParameterValue(TEXT("GlowStrength"), Look.GlowStrength);
	}
	if (CloudMid)
	{
		CloudMid->SetVectorParameterValue(TEXT("Tint"), Look.CloudTint);
	}
	const float Night = FSkyPaletteModel::MomentWeights(SunZ).Night;
	if (SunMid)
	{
		SunMid->SetScalarParameterValue(TEXT("Brightness"), 6.0f);
	}
	if (MoonMid)
	{
		MoonMid->SetScalarParameterValue(TEXT("Brightness"), FMath::Lerp(1.0f, 2.0f, Night));
	}
	SkyLight->SetLightColor(FLinearColor::White);
	SkyLight->SetIntensity(FMath::Lerp(1.0f, 1.6f, Night));

	// Noches algo más densas; niebla matinal suave y el temporal que cierra la vista.
	const float StormFog = FMath::Clamp(WeatherFog * 1.6f, 0.0f, 1.0f);
	Fog->SetFogDensity(0.0012f + WeatherFog * 0.03f + WeatherRain * 0.006f);
	Fog->SetStartDistance(FMath::Lerp(15000.0f, 600.0f, StormFog));
	Fog->SetFogInscatteringColor(Look.FogColor * ExploredSkyDetail::SkyLuminance * Look.SkyBrightness * 0.6f);
}

void AExploredSkyController::ApplyPostProcess(float GoldenHour, float SunUp)
{
	FPostProcessSettings& PP = PostProcess->Settings;
	// De noche la exposición no compensa del todo: el cielo y el mar quedan azul noche, no grises.
	PP.AutoExposureBias = FMath::Lerp(-0.6f, 0.3f, SunUp);
	PP.ColorGain = FVector4(FMath::Lerp(1.0f, 1.08f, GoldenHour), FMath::Lerp(1.0f, 1.02f, GoldenHour), FMath::Lerp(1.0f, 0.94f, GoldenHour), 1.0f);
	PP.BloomIntensity = FMath::Lerp(0.2f, 0.4f, GoldenHour);
	PP.VignetteIntensity = 0.25f;
	PP.bOverride_SceneColorTint = false;
}

void AExploredSkyController::ApplyUnderwater(float DeltaSeconds)
{
	if (!Ocean.IsValid())
	{
		Ocean = Cast<AExploredOcean>(UGameplayStatics::GetActorOfClass(this, AExploredOcean::StaticClass()));
	}

	float TargetBlend = 0.0f;
	if (const AExploredOcean* OceanActor = Ocean.Get())
	{
		const FVector CameraLocation = GetViewLocation();
		const float WaterHeight = OceanActor->GetWaterHeightAt(CameraLocation);
		// Un pequeño margen bajo la superficie para no parpadear justo al cruzarla.
		TargetBlend = (CameraLocation.Z < WaterHeight - 15.0f) ? 1.0f : 0.0f;
	}
	UnderwaterBlend = FMath::FInterpTo(UnderwaterBlend, TargetBlend, DeltaSeconds, 2.5f);
	if (UnderwaterBlend <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Sin el plugin Water de Epic no hay compositing submarino: niebla densa y viraje hacia el
	// agua media de la paleta.
	FPostProcessSettings& PP = PostProcess->Settings;
	PP.bOverride_SceneColorTint = true;
	PP.SceneColorTint = FMath::Lerp(FLinearColor::White, FLinearColor(0.35f, 0.72f, 0.78f), UnderwaterBlend);
	PP.VignetteIntensity = FMath::Lerp(PP.VignetteIntensity, 0.6f, UnderwaterBlend);
	Fog->SetFogDensity(FMath::Lerp(Fog->FogDensity, 0.11f, UnderwaterBlend));
	Fog->SetFogInscatteringColor(FMath::Lerp(Fog->FogInscatteringLuminance, FSkyPaletteModel::PaletteColor(TEXT("agua_media")) * 0.4f, UnderwaterBlend));
}
