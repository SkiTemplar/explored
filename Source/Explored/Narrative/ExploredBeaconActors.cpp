#include "Narrative/ExploredBeaconActors.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"

#include "Core/ExploredAssetFallback.h"
#include "Explored.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/BeaconSubsystem.h"
#include "Narrative/NotebookModel.h"
#include "WorldGen/IslandPropsModel.h"

#define LOCTEXT_NAMESPACE "ExploredBeacon"

namespace BeaconActorsDetail
{
	/** La galería rodea la linterna: el punto queda entre el borde de la linterna y el de la torre. */
	constexpr float GalleryInsetCm = 35.0f;
	constexpr float DoorOutsetCm = 120.0f;
	/** Por encima de esta altura sobre la base de la torre, Iria está arriba. */
	constexpr float UpThresholdFraction = 0.5f;
	/** Luces de banda del Mistral (roja y verde): marcas de 80 m de alcance, no focos sobre el mar. */
	constexpr float SideLightCandelas = 3000.0f;
	/** Luz blanca del mástil: la principal, pero sin bañar el mar de cian a cientos de metros. */
	constexpr float MastLightCandelas = 4000.0f;
	constexpr float MastLightRadiusCm = 4000.0f;
	constexpr float SideLightRadiusCm = 8000.0f;
	/**
	 * Casco del Mistral: la malla del carguero a media escala (20 × 4,4 m), de color hierro y hundida su
	 * calado, para que de noche se lea un barco y no solo tres puntos de luz. Sin arte nuevo.
	 */
	const TCHAR* const HullMeshPath = TEXT("/Game/Art/K2_Estructuras/SM_CargueroCasco_01.SM_CargueroCasco_01");
	const TCHAR* const HullMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	constexpr float HullScale = 0.5f;
	constexpr float HullDraftCm = 150.0f;
	/** Hierro de la paleta (#45474D) en lineal. */
	const FLinearColor HullColor = FLinearColor::FromSRGBColor(FColor(0x45, 0x47, 0x4D));
	/** Ojos de buey por banda, su altura sobre el agua y la separación a lo largo del casco (cm). */
	constexpr int32 PortholesPerSide = 5;
	constexpr float PortholeHeightCm = 280.0f;
	constexpr float PortholeSpacingCm = 260.0f;
	constexpr float PortholeOutsetCm = 230.0f;
	constexpr float PortholeSizeM = 0.35f;
	const TCHAR* const EmissiveMaterialPath = TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial");

	FVector HorizontalDirection(const FVector& From, const FVector& To)
	{
		FVector Direction = To - From;
		Direction.Z = 0.0;
		return Direction.IsNearlyZero() ? FVector(-1.0, 0.0, 0.0) : Direction.GetSafeNormal();
	}

	/** Coloca al personaje con los pies en FeetCm (la cápsula se mide desde su centro). */
	void PlaceFeetAt(AActor& Actor, const FVector& FeetCm, const FVector& LookAt)
	{
		float HalfHeight = 90.0f;
		if (const ACharacter* Character = Cast<ACharacter>(&Actor))
		{
			HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		const FVector Location = FeetCm + FVector(0.0, 0.0, HalfHeight + 5.0);
		Actor.SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		FRotator Facing = (LookAt - Location).Rotation();
		Facing.Pitch = 0.0;
		Facing.Roll = 0.0;
		if (APawn* Pawn = Cast<APawn>(&Actor))
		{
			if (AController* Controller = Pawn->GetController())
			{
				Controller->SetControlRotation(Facing);
			}
		}
		Actor.SetActorRotation(Facing);
	}
}

// ---------------------------------------------------------------------------
// AExploredNotebookPage
// ---------------------------------------------------------------------------

void AExploredNotebookPage::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	Page = Prop.Page;
	if (!FNotebookModel::IsValidPage(Page))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Baliza] La página %s no tiene número 1-8 en island_props.json"), *Prop.Id.ToString());
	}
}

void AExploredNotebookPage::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	const bool bRead = Beacon && FNotebookModel::IsRead(Beacon->GetRunState().Notebook, Page);
	OutVerbs.Add(bRead ? LOCTEXT("PageReread", "Releer la página") : LOCTEXT("PageRead", "Leer la página"));
}

bool AExploredNotebookPage::CanInteract_Implementation(AActor* InInstigator) const
{
	return FNotebookModel::IsValidPage(Page) && UBeaconSubsystem::Get(this) != nullptr;
}

void AExploredNotebookPage::Interact_Implementation(AActor* InInstigator)
{
	if (UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this))
	{
		Beacon->ReadPage(Page);
	}
}

// ---------------------------------------------------------------------------
// AExploredLighthouseTower
// ---------------------------------------------------------------------------

void AExploredLighthouseTower::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	HeightCm = Type.Size.Z * 100.0f;
	RadiusCm = FMath::Max(Type.Size.X, Type.Size.Y) * 50.0f;
}

bool AExploredLighthouseTower::IsUp(const AActor& Actor) const
{
	return Actor.GetActorLocation().Z - GetActorLocation().Z > HeightCm * BeaconActorsDetail::UpThresholdFraction;
}

FVector AExploredLighthouseTower::GalleryPoint() const
{
	// +X es el norte: la galería de trabajo mira al sur.
	return GetActorLocation() + FVector(-(RadiusCm - BeaconActorsDetail::GalleryInsetCm), 0.0, HeightCm);
}

FVector AExploredLighthouseTower::DoorPoint(const FVector& FromLocation) const
{
	const FVector Base = GetActorLocation();
	const FVector Direction = BeaconActorsDetail::HorizontalDirection(Base, FromLocation);
	return Base + Direction * (RadiusCm + BeaconActorsDetail::DoorOutsetCm);
}

void AExploredLighthouseTower::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	OutVerbs.Add(Pawn && IsUp(*Pawn) ? LOCTEXT("TowerDown", "Bajar del faro") : LOCTEXT("TowerUp", "Subir a la linterna"));
}

bool AExploredLighthouseTower::CanInteract_Implementation(AActor* InInstigator) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	return InInstigator && (!Survivor || !Survivor->IsBusy());
}

void AExploredLighthouseTower::Interact_Implementation(AActor* InInstigator)
{
	if (!InInstigator)
	{
		return;
	}
	const FVector From = InInstigator->GetActorLocation();
	const FVector Center = GetActorLocation();
	if (IsUp(*InInstigator))
	{
		BeaconActorsDetail::PlaceFeetAt(*InInstigator, DoorPoint(From), Center + BeaconActorsDetail::HorizontalDirection(Center, From) * 1000.0);
	}
	else
	{
		BeaconActorsDetail::PlaceFeetAt(*InInstigator, GalleryPoint(), Center + FVector(0.0, 0.0, HeightCm + 120.0));
	}
}

// ---------------------------------------------------------------------------
// AExploredMistralLights
// ---------------------------------------------------------------------------

AExploredMistralLights::AExploredMistralLights()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);
	// Las luces son marcas, no focos: con 30 000-60 000 cd y 600 m de alcance teñían el mar de una mancha
	// cian delante del faro. Lo que se lee de lejos es la bombilla emisiva del mástil (MakeBulb).
	MastLight = MakeLight(TEXT("MastLight"), FLinearColor(1.0f, 0.97f, 0.9f), FVector(0.0, 0.0, 1400.0), BeaconActorsDetail::MastLightCandelas,
		BeaconActorsDetail::MastLightRadiusCm);
	PortLight = MakeLight(TEXT("PortLight"), FLinearColor(1.0f, 0.12f, 0.08f), FVector(0.0, -500.0, 700.0), BeaconActorsDetail::SideLightCandelas,
		BeaconActorsDetail::SideLightRadiusCm);
	StarboardLight = MakeLight(TEXT("StarboardLight"), FLinearColor(0.1f, 1.0f, 0.25f), FVector(0.0, 500.0, 700.0),
		BeaconActorsDetail::SideLightCandelas, BeaconActorsDetail::SideLightRadiusCm);
}

UPointLightComponent* AExploredMistralLights::MakeLight(const TCHAR* Name, const FLinearColor& Color, const FVector& Offset, float Intensity,
	float AttenuationRadiusCm)
{
	UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name);
	Light->SetupAttachment(Root);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetRelativeLocation(Offset);
	Light->SetLightColor(Color);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(AttenuationRadiusCm);
	Light->SetCastShadows(false);
	return Light;
}

UStaticMeshComponent* AExploredMistralLights::MakeBulb(const TCHAR* Name, UPointLightComponent& Light, float SizeMeters)
{
	UStaticMeshComponent* Bulb = NewObject<UStaticMeshComponent>(this, Name);
	Bulb->SetupAttachment(&Light);
	Bulb->SetMobility(EComponentMobility::Movable);
	Bulb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bulb->SetCastShadow(false);
	Bulb->SetStaticMesh(ExploredAssets::LoadMeshOrFallback(FSoftObjectPath(ExploredAssets::FallbackSpherePath), ExploredAssets::FallbackSpherePath));
	Bulb->SetRelativeScale3D(FVector(SizeMeters));
	if (FPackageName::DoesPackageExist(FSoftObjectPath(BeaconActorsDetail::EmissiveMaterialPath).GetLongPackageName()))
	{
		if (UMaterialInterface* Emissive = Cast<UMaterialInterface>(FSoftObjectPath(BeaconActorsDetail::EmissiveMaterialPath).TryLoad()))
		{
			Bulb->SetMaterial(0, Emissive);
		}
	}
	Bulb->RegisterComponent();
	return Bulb;
}

void AExploredMistralLights::MakeHull()
{
	using namespace BeaconActorsDetail;
	const FSoftObjectPath HullPath(HullMeshPath);
	if (!FPackageName::DoesPackageExist(HullPath.GetLongPackageName()))
	{
		return;
	}
	UStaticMesh* HullMesh = Cast<UStaticMesh>(HullPath.TryLoad());
	if (!HullMesh)
	{
		return;
	}
	UStaticMeshComponent* Hull = NewObject<UStaticMeshComponent>(this, TEXT("Hull"));
	Hull->SetupAttachment(Root);
	Hull->SetMobility(EComponentMobility::Movable);
	Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Hull->SetCastShadow(false);
	Hull->SetStaticMesh(HullMesh);
	Hull->SetRelativeLocation(FVector(0.0, 0.0, -HullDraftCm));
	Hull->SetRelativeScale3D(FVector(HullScale));
	if (UMaterialInterface* Base = Cast<UMaterialInterface>(FSoftObjectPath(HullMaterialPath).TryLoad()))
	{
		UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(Base, this);
		Paint->SetVectorParameterValue(TEXT("Color"), HullColor);
		for (int32 Slot = 0; Slot < Hull->GetNumMaterials(); ++Slot)
		{
			Hull->SetMaterial(Slot, Paint);
		}
	}
	Hull->RegisterComponent();

	UMaterialInterface* Emissive = FPackageName::DoesPackageExist(FSoftObjectPath(EmissiveMaterialPath).GetLongPackageName())
		? Cast<UMaterialInterface>(FSoftObjectPath(EmissiveMaterialPath).TryLoad()) : nullptr;
	UStaticMesh* Sphere = ExploredAssets::LoadMeshOrFallback(FSoftObjectPath(ExploredAssets::FallbackSpherePath), ExploredAssets::FallbackSpherePath);
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		for (int32 Index = 0; Index < PortholesPerSide; ++Index)
		{
			const float AlongCm = (Index - (PortholesPerSide - 1) * 0.5f) * PortholeSpacingCm;
			UStaticMeshComponent* Porthole = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Porthole%d_%d"), Side > 0 ? 1 : 0, Index));
			Porthole->SetupAttachment(Root);
			Porthole->SetMobility(EComponentMobility::Movable);
			Porthole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Porthole->SetCastShadow(false);
			Porthole->SetStaticMesh(Sphere);
			Porthole->SetRelativeLocation(FVector(AlongCm, Side * PortholeOutsetCm, PortholeHeightCm));
			Porthole->SetRelativeScale3D(FVector(PortholeSizeM));
			if (Emissive)
			{
				Porthole->SetMaterial(0, Emissive);
			}
			Porthole->RegisterComponent();
		}
	}
}

void AExploredMistralLights::Approach(const FVector& From, const FVector& To, float Seconds)
{
	Start = From;
	Target = To;
	Duration = FMath::Max(0.1f, Seconds);
	Elapsed = 0.0f;
	SetActorLocation(From);
	SetActorRotation(BeaconActorsDetail::HorizontalDirection(From, To).Rotation());
	// Un punto visible a varios kilómetros: las luces solas no se ven si no hay nada que iluminar.
	MakeBulb(TEXT("MastBulb"), *MastLight, 3.0f);
	MakeBulb(TEXT("PortBulb"), *PortLight, 2.0f);
	MakeBulb(TEXT("StarboardBulb"), *StarboardLight, 2.0f);
	MakeHull();
}

void AExploredMistralLights::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Elapsed >= Duration)
	{
		return;
	}
	Elapsed = FMath::Min(Duration, Elapsed + DeltaSeconds);
	// Frena al acercarse, como un barco que reduce para atracar.
	const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / Duration, 2.0f);
	SetActorLocation(FMath::Lerp(Start, Target, static_cast<double>(Alpha)));
}

#undef LOCTEXT_NAMESPACE
