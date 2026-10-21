#include "Narrative/LighthouseLamp.h"

#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/SoftObjectPath.h"

#include "Core/ExploredAssetFallback.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/BeaconSubsystem.h"
#include "Narrative/FinaleSequenceModel.h"
#include "WorldGen/IslandPropsModel.h"

#define LOCTEXT_NAMESPACE "ExploredBeacon"

namespace LighthouseLampDetail
{
	constexpr float IdleTickSeconds = 0.5f;
	const TCHAR* const LensMesh = TEXT("/Game/Art/K3_Objetos/SM_Lente_01.SM_Lente_01");
	const TCHAR* const OilMesh = TEXT("/Game/Art/K3_Objetos/SM_TarroAceite_01.SM_TarroAceite_01");
	const TCHAR* const WickMesh = TEXT("/Game/Art/K3_Objetos/SM_Mecha_01.SM_Mecha_01");
	const TCHAR* const CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	/** Celdas de la paleta (Tools/Style/paleta.json): cristal, latón y madera clara. */
	constexpr int32 GlassColor = 11;
	constexpr int32 BrassColor = 10;
	constexpr int32 WickColor = 5;
	const FLinearColor BeamColor(1.0f, 0.88f, 0.62f);
	const TCHAR* const BeamMaterialPath = TEXT("/Game/Art/Materials/M_BeaconBeam.M_BeaconBeam");
	const TCHAR* const LensSoundPath = TEXT("/Game/Generated/Audio/Efectos/sfx_lens_rotation_loop.sfx_lens_rotation_loop");
	constexpr float GlowLitCandelas = 2000.0f;
	/** Emisivo del cristal de la linterna con la llama a plena fuerza (M_Toon, GlowStrength). */
	constexpr float GlassGlowLit = 3.0f;
	const FName GlowCellParam(TEXT("GlowCell"));
	const FName GlowStrengthParam(TEXT("GlowStrength"));
	constexpr int32 ConeSides = 18;

	/**
	 * Ranuras de SM_FaroLinterna_01 en su espacio local (cm), de Art/Export/K2_Estructuras/manifest.json
	 * («ensamblaje» -> «ranuras_m»), con la Y de Blender cambiada de signo para Unreal. Las piezas
	 * tienen el pivote en la base: la lente se baja media altura para que quede centrada en su aro.
	 */
	const FVector LensSlotCm(0.0, 22.0, 147.0 - 26.0);
	const FVector OilSlotLeftCm(-50.0, -8.0, 105.0);
	const FVector OilSlotRightCm(50.0, -8.0, 105.0);
	const FVector WickSlotCm(0.0, -18.0, 144.0);
	/** Centro del aro de la lente: de ahí sale el haz y ahí brilla la luz. */
	const FVector LensCenterCm(0.0, 22.0, 147.0);
}

ALighthouseLamp::ALighthouseLamp()
{
	// Apagada solo mira el progreso dos veces por segundo; encendida gira el haz cada fotograma.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = LighthouseLampDetail::IdleTickSeconds;

	Beam = CreateDefaultSubobject<USpotLightComponent>(TEXT("Beam"));
	Beam->SetupAttachment(Root);
	Beam->SetMobility(EComponentMobility::Movable);
	Beam->SetIntensityUnits(ELightUnits::Candelas);
	Beam->SetIntensity(400000.0f);
	Beam->SetAttenuationRadius(250000.0f);
	Beam->SetInnerConeAngle(2.0f);
	Beam->SetOuterConeAngle(5.0f);
	Beam->SetLightColor(LighthouseLampDetail::BeamColor);
	Beam->SetCastShadows(false);
	Beam->SetVisibility(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(2000.0f);
	Glow->SetAttenuationRadius(4000.0f);
	Glow->SetLightColor(LighthouseLampDetail::BeamColor);
	Glow->SetCastShadows(false);
	Glow->SetVisibility(false);
}

void ALighthouseLamp::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	using namespace LighthouseLampDetail;
	LampHeightCm = Type.Size.Z * 100.0f;
	LampRadiusCm = FMath::Max(Type.Size.X, Type.Size.Y) * 50.0f;

	if (UsesFinalMesh())
	{
		// Malla definitiva: cada pieza en su ranura de la linterna (mallas a escala real).
		LensPart = MakePart(TEXT("LensPart"), LensMesh, ExploredAssets::FallbackSpherePath, LensSlotCm, FVector(0.6), GlassColor);
		OilParts.Reset();
		OilParts.Add(MakePart(TEXT("OilPart0"), OilMesh, CylinderPath, OilSlotLeftCm, FVector(0.18, 0.18, 0.25), BrassColor));
		OilParts.Add(MakePart(TEXT("OilPart1"), OilMesh, CylinderPath, OilSlotRightCm, FVector(0.18, 0.18, 0.25), BrassColor));
		WickPart = MakePart(TEXT("WickPart"), WickMesh, CylinderPath, WickSlotCm, FVector(0.05, 0.05, 0.35), WickColor);
		Beam->SetRelativeLocation(LensCenterCm);
		Glow->SetRelativeLocation(LensCenterCm);
		// El cristal de la linterna (celda «cristal» de la paleta) brilla con la llama: sin esto, la luz
		// queda dentro de una caja opaca y desde fuera no se ve que la baliza prende.
		if (UMaterialInterface* LanternMaterial = Mesh ? Mesh->GetMaterial(0) : nullptr)
		{
			GlassMaterial = UMaterialInstanceDynamic::Create(LanternMaterial, this);
			GlassMaterial->SetScalarParameterValue(GlowCellParam, static_cast<float>(GlassColor));
			GlassMaterial->SetScalarParameterValue(GlowStrengthParam, 0.0f);
			Mesh->SetMaterial(0, GlassMaterial);
		}
	}
	else
	{
		// Las piezas van por fuera de la primitiva (que es maciza): la lente encima, en su aro, y
		// los tarros y la mecha en la repisa del lado sur, por donde se sube.
		LensPart = MakePart(TEXT("LensPart"), LensMesh, ExploredAssets::FallbackSpherePath,
			FVector(0.0, 0.0, LampHeightCm + 30.0), FVector(0.6), GlassColor);
		OilParts.Reset();
		for (int32 Jar = 0; Jar < FBeaconProgressModel::RequiredOilJars; ++Jar)
		{
			const double Side = Jar == 0 ? -25.0 : 25.0;
			OilParts.Add(MakePart(Jar == 0 ? TEXT("OilPart0") : TEXT("OilPart1"), OilMesh, CylinderPath,
				FVector(-LampRadiusCm - 15.0, Side, LampHeightCm * 0.45), FVector(0.18, 0.18, 0.25), BrassColor));
		}
		WickPart = MakePart(TEXT("WickPart"), WickMesh, CylinderPath,
			FVector(-LampRadiusCm - 15.0, 0.0, LampHeightCm * 0.45 + 30.0), FVector(0.05, 0.05, 0.35), WickColor);
		Beam->SetRelativeLocation(FVector(0.0, 0.0, LampHeightCm * 0.7));
		Glow->SetRelativeLocation(FVector(0.0, 0.0, LampHeightCm + 60.0));
	}
	// Ligeramente hacia abajo para que el haz barra el mar cercano además del horizonte.
	Beam->SetRelativeRotation(FRotator(-2.0, 0.0, 0.0));
	BuildBeamCone();
	RefreshFromProgress();
}

UStaticMeshComponent* ALighthouseLamp::MakePart(const TCHAR* Name, const TCHAR* MeshPath, const TCHAR* FallbackPath,
	const FVector& LocationCm, const FVector& FallbackScale, int32 ColorIndex)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, Name);
	Part->SetupAttachment(Root);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UStaticMesh* Final = ExploredAssets::LoadMeshIfPresent(FSoftObjectPath(MeshPath));
	if (Final)
	{
		Part->SetStaticMesh(Final);
	}
	else
	{
		Part->SetStaticMesh(ExploredAssets::LoadMeshOrFallback(FSoftObjectPath(FallbackPath), FallbackPath));
		Part->SetRelativeScale3D(FallbackScale);
		if (UMaterialInterface* Toon = Cast<UMaterialInterface>(FSoftObjectPath(ToonMaterialPath()).TryLoad()))
		{
			UMaterialInstanceDynamic* Tinted = UMaterialInstanceDynamic::Create(Toon, this);
			Tinted->SetScalarParameterValue(TEXT("PaletteIndex"), static_cast<float>(ColorIndex));
			Tinted->SetScalarParameterValue(TEXT("UsePaletteIndex"), 1.0f);
			Part->SetMaterial(0, Tinted);
		}
	}
	Part->SetRelativeLocation(LocationCm);
	Part->SetVisibility(false);
	Part->RegisterComponent();
	return Part;
}

void ALighthouseLamp::BuildBeamCone()
{
	using namespace LighthouseLampDetail;
	if (BeamCone)
	{
		return;
	}
	BeamCone = NewObject<UProceduralMeshComponent>(this, TEXT("BeamCone"));
	BeamCone->SetupAttachment(Beam);
	BeamCone->SetMobility(EComponentMobility::Movable);
	BeamCone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamCone->SetCastShadow(false);
	// Cono abierto a lo largo de +X (el eje del foco): punta en la lente, boca a BeamConeLengthM.
	const float Length = BeamConeLengthM * 100.0f;
	const float Radius = BeamConeRadiusM * 100.0f;
	const float Slope = Radius / FMath::Max(Length, 1.0f);
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	for (int32 Side = 0; Side <= ConeSides; ++Side)
	{
		const float Angle = UE_TWO_PI * static_cast<float>(Side) / ConeSides;
		const FVector Radial(0.0, FMath::Cos(Angle), FMath::Sin(Angle));
		const FVector Normal = (Radial - FVector(Slope, 0.0, 0.0)).GetSafeNormal();
		Vertices.Add(FVector::ZeroVector);
		Vertices.Add(FVector(Length, 0.0, 0.0) + Radial * Radius);
		Normals.Add(Normal);
		Normals.Add(Normal);
		UVs.Add(FVector2D(static_cast<float>(Side) / ConeSides, 0.0f));
		UVs.Add(FVector2D(static_cast<float>(Side) / ConeSides, 1.0f));
	}
	for (int32 Side = 0; Side < ConeSides; ++Side)
	{
		const int32 Tip = Side * 2;
		Triangles.Append({ Tip, Tip + 3, Tip + 1 });
	}
	BeamCone->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, {}, {}, false);
	if (UMaterialInterface* Material = Cast<UMaterialInterface>(FSoftObjectPath(BeamMaterialPath).TryLoad()))
	{
		BeamMaterial = UMaterialInstanceDynamic::Create(Material, this);
		BeamMaterial->SetScalarParameterValue(TEXT("BeamLength"), Length);
		BeamCone->SetMaterial(0, BeamMaterial);
	}
	BeamCone->SetVisibility(false);
	BeamCone->RegisterComponent();
}

void ALighthouseLamp::StartLensSound()
{
	if (LensSound)
	{
		return;
	}
	const FSoftObjectPath Path(LighthouseLampDetail::LensSoundPath);
	USoundBase* Sound = FPackageName::DoesPackageExist(Path.GetLongPackageName()) ? Cast<USoundBase>(Path.TryLoad()) : nullptr;
	if (Sound)
	{
		LensSound = UGameplayStatics::SpawnSoundAttached(Sound, Glow, NAME_None, FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset, false, 0.9f);
	}
}

void ALighthouseLamp::UpdateFlame(float Now)
{
	using namespace LighthouseLampDetail;
	FFinaleTiming Timing;
	Timing.IgniteSeconds = LightSeconds;
	float Strength = 0.0f;
	if (bShowingLit)
	{
		Strength = 1.0f;
	}
	else if (IgniteStartedAt >= 0.0f)
	{
		// La mecha prende: la llama crece durante los 3 s de la acción.
		Strength = FFinaleSequenceModel::FlameStrength(Timing, Now - IgniteStartedAt);
		if (Now - IgniteStartedAt > LightSeconds + 1.0f)
		{
			// La acción se interrumpió sin encender: la llama no se queda prendida y el
			// tick vuelve al intervalo de reposo.
			IgniteStartedAt = -1.0f;
			Strength = 0.0f;
			SetActorTickInterval(LighthouseLampDetail::IdleTickSeconds);
		}
	}
	Glow->SetVisibility(Strength > 0.0f);
	Glow->SetIntensity(GlowLitCandelas * Strength * FFinaleSequenceModel::Flicker(Now));
	if (GlassMaterial)
	{
		GlassMaterial->SetScalarParameterValue(GlowStrengthParam, GlassGlowLit * Strength * FFinaleSequenceModel::Flicker(Now));
	}
	if (!BeamCone)
	{
		return;
	}
	const float Grow = bShowingLit && LitAt >= 0.0f ? FFinaleSequenceModel::BeamGrow(Timing, Now - LitAt) : 0.0f;
	BeamCone->SetVisibility(Grow > 0.0f);
	// Crece desde la lente: el cono conserva su apertura mientras se alarga.
	BeamCone->SetRelativeScale3D(FVector(FMath::Max(Grow, 0.01f)));
	if (BeamMaterial)
	{
		BeamMaterial->SetVectorParameterValue(TEXT("BeamOrigin"), FLinearColor(Beam->GetComponentLocation()));
		BeamMaterial->SetScalarParameterValue(TEXT("BeamIntensity"), 0.6f * Grow);
	}
}

FVector ALighthouseLamp::GetBeamDirection() const
{
	return Beam ? Beam->GetForwardVector() : GetActorForwardVector();
}

void ALighthouseLamp::DebugIgnite()
{
	UWorld* World = GetWorld();
	if (!World || bShowingLit)
	{
		return;
	}
	IgniteStartedAt = World->GetTimeSeconds();
	SetActorTickInterval(0.0f);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (UBeaconSubsystem* Subsystem = UBeaconSubsystem::Get(this))
		{
			Subsystem->TryLight();
		}
		RefreshFromProgress();
	}), FMath::Max(LightSeconds, 0.1f), false);
}

void ALighthouseLamp::BeginPlay()
{
	Super::BeginPlay();
	RefreshFromProgress();
}

void ALighthouseLamp::RefreshFromProgress()
{
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	if (!Beacon)
	{
		return;
	}
	const FBeaconProgressState& Progress = Beacon->GetRunState().Progress;
	if (LensPart)
	{
		LensPart->SetVisibility(FBeaconProgressModel::IsSlotFilled(Progress, ELanternSlot::Lens));
	}
	for (UStaticMeshComponent* Jar : OilParts)
	{
		if (Jar)
		{
			Jar->SetVisibility(FBeaconProgressModel::IsSlotFilled(Progress, ELanternSlot::Oil));
		}
	}
	if (WickPart)
	{
		WickPart->SetVisibility(FBeaconProgressModel::IsSlotFilled(Progress, ELanternSlot::Wick));
	}
	if (bShowingLit != Progress.bBeaconLit)
	{
		bShowingLit = Progress.bBeaconLit;
		IgniteStartedAt = -1.0f;
		LitAt = bShowingLit && GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f;
		Beam->SetVisibility(bShowingLit);
		Glow->SetVisibility(bShowingLit);
		SetActorTickInterval(bShowingLit ? 0.0f : LighthouseLampDetail::IdleTickSeconds);
		if (bShowingLit)
		{
			StartLensSound();
		}
		else if (LensSound)
		{
			LensSound->Stop();
			LensSound = nullptr;
		}
		if (!bShowingLit && BeamCone)
		{
			BeamCone->SetVisibility(false);
		}
	}
}

void ALighthouseLamp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshFromProgress();
	if (bShowingLit)
	{
		Beam->AddRelativeRotation(FRotator(0.0, BeamDegreesPerSecond * DeltaSeconds, 0.0));
	}
	if (bShowingLit || IgniteStartedAt >= 0.0f)
	{
		UpdateFlame(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	}
}

ELanternSlot ALighthouseLamp::NextSlot() const
{
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	if (!Beacon)
	{
		return ELanternSlot::Count;
	}
	const FBeaconProgressState& Progress = Beacon->GetRunState().Progress;
	for (const ELanternSlot Slot : { ELanternSlot::Lens, ELanternSlot::Oil, ELanternSlot::Wick })
	{
		if (!FBeaconProgressModel::IsSlotFilled(Progress, Slot))
		{
			return Slot;
		}
	}
	return ELanternSlot::Count;
}

void ALighthouseLamp::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	if (!Beacon)
	{
		return;
	}
	switch (NextSlot())
	{
	case ELanternSlot::Lens:
		OutVerbs.Add(LOCTEXT("LampLens", "Poner la lente"));
		return;
	case ELanternSlot::Oil:
		OutVerbs.Add(LOCTEXT("LampOil", "Verter los tarros"));
		return;
	case ELanternSlot::Wick:
		OutVerbs.Add(LOCTEXT("LampWick", "Apretar la mecha"));
		return;
	default:
		break;
	}
	OutVerbs.Add(Beacon->IsBeaconLit() ? LOCTEXT("LampLit", "La baliza está encendida") : LOCTEXT("LampLight", "Encender la baliza"));
}

bool ALighthouseLamp::CanInteract_Implementation(AActor* InInstigator) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	return Survivor && Beacon && !Survivor->IsBusy() && !Beacon->IsBeaconLit();
}

void ALighthouseLamp::Interact_Implementation(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	if (!Survivor || !Beacon || Beacon->IsBeaconLit())
	{
		return;
	}
	const ELanternSlot Slot = NextSlot();
	if (Slot != ELanternSlot::Count)
	{
		Beacon->TryFillSlot(Slot, *Survivor);
		RefreshFromProgress();
		return;
	}
	const TWeakObjectPtr<ALighthouseLamp> WeakThis(this);
	// La llama prende durante la acción (UpdateFlame): el tick pasa a cada fotograma.
	IgniteStartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SetActorTickInterval(0.0f);
	// Mientras dura, el superviviente está ocupado y CanInteract no deja empezar otra vez.
	Survivor->StartTimedAction(LightSeconds, [WeakThis]()
	{
		ALighthouseLamp* Lamp = WeakThis.Get();
		if (!Lamp)
		{
			return;
		}
		if (UBeaconSubsystem* Subsystem = UBeaconSubsystem::Get(Lamp))
		{
			Subsystem->TryLight();
		}
		Lamp->RefreshFromProgress();
		if (!Lamp->IsLit())
		{
			// No prendió: la llama se apaga y el tick vuelve al intervalo de reposo.
			Lamp->IgniteStartedAt = -1.0f;
			Lamp->Glow->SetVisibility(false);
			Lamp->SetActorTickInterval(LighthouseLampDetail::IdleTickSeconds);
		}
	});
}

#undef LOCTEXT_NAMESPACE
