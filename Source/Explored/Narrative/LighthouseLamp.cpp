#include "Narrative/LighthouseLamp.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/SoftObjectPath.h"

#include "Core/ExploredAssetFallback.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/BeaconSubsystem.h"
#include "WorldGen/IslandPropsModel.h"

#define LOCTEXT_NAMESPACE "ExploredBeacon"

namespace LighthouseLampDetail
{
	const TCHAR* const LensMesh = TEXT("/Game/Art/K3_Objetos/SM_Lente_01.SM_Lente_01");
	const TCHAR* const OilMesh = TEXT("/Game/Art/K3_Objetos/SM_TarroAceite_01.SM_TarroAceite_01");
	const TCHAR* const WickMesh = TEXT("/Game/Art/K3_Objetos/SM_Mecha_01.SM_Mecha_01");
	const TCHAR* const CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	/** Celdas de la paleta (Tools/Style/paleta.json): cristal, latón y madera clara. */
	constexpr int32 GlassColor = 11;
	constexpr int32 BrassColor = 10;
	constexpr int32 WickColor = 5;
	const FLinearColor BeamColor(1.0f, 0.88f, 0.62f);
}

ALighthouseLamp::ALighthouseLamp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;

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
	// Ligeramente hacia abajo para que el haz barra el mar cercano además del horizonte.
	Beam->SetRelativeRotation(FRotator(-2.0, 0.0, 0.0));
	Glow->SetRelativeLocation(FVector(0.0, 0.0, LampHeightCm + 60.0));
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
		Beam->SetVisibility(bShowingLit);
		Glow->SetVisibility(bShowingLit);
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
	});
}

#undef LOCTEXT_NAMESPACE
