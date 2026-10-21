#include "WorldGen/ExploredPlaceholder.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/SoftObjectPath.h"

#include "Core/ExploredAssetFallback.h"
#include "WorldGen/IslandPropsModel.h"

namespace ExploredPlaceholderDetail
{
	const FName PaletteIndexParam(TEXT("PaletteIndex"));
	const FName UsePaletteIndexParam(TEXT("UsePaletteIndex"));
	constexpr float LabelWorldSize = 35.0f;
	constexpr float LabelGapCm = 40.0f;

	void ApplyCollision(UStaticMeshComponent& Mesh, EIslandPropCollision Collision)
	{
		switch (Collision)
		{
		case EIslandPropCollision::Solid:
			Mesh.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			break;
		case EIslandPropCollision::Query:
			// Lo ve la interacción (trazas de visibilidad) pero el personaje lo atraviesa.
			Mesh.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Mesh.SetCollisionResponseToAllChannels(ECR_Ignore);
			Mesh.SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			break;
		default:
			Mesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		}
	}
}

AExploredPlaceholder::AExploredPlaceholder()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Movable);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextBottom);
	Label->SetWorldSize(ExploredPlaceholderDetail::LabelWorldSize);
	Label->SetTextRenderColor(FColor(255, 240, 200));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetHiddenInGame(true);
	// El texto se lee de frente desde el sur (por donde se llega desde la Cala Varada) sea cual sea
	// el giro del objeto; por detrás se vería en espejo.
	Label->SetUsingAbsoluteRotation(true);
	Label->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
}

void AExploredPlaceholder::Configure(const FIslandProp& Prop, const FIslandPropType& Type, const FString& MeshPath, UMaterialInterface* ToonMaterial)
{
	using namespace ExploredPlaceholderDetail;
	PropId = Prop.Id;
	PropType = Prop.Type;

	UStaticMesh* Final = MeshPath.IsEmpty() ? nullptr : ExploredAssets::LoadMeshIfPresent(FSoftObjectPath(MeshPath));
	bFinalMesh = Final != nullptr;
	if (bFinalMesh)
	{
		Mesh->SetStaticMesh(Final);
		Mesh->SetRelativeLocation(FVector::ZeroVector);
		Mesh->SetRelativeScale3D(FVector::OneVector);
	}
	else
	{
		Mesh->SetStaticMesh(Cast<UStaticMesh>(FSoftObjectPath(IslandPropsPlacement::BasicShapePath(Type.Shape)).TryLoad()));
		Mesh->SetRelativeLocation(IslandPropsPlacement::FallbackOffset(Type));
		Mesh->SetRelativeScale3D(IslandPropsPlacement::FallbackScale(Type));
		if (ToonMaterial)
		{
			UMaterialInstanceDynamic* Tinted = UMaterialInstanceDynamic::Create(ToonMaterial, this);
			Tinted->SetScalarParameterValue(PaletteIndexParam, static_cast<float>(Type.ColorIndex));
			Tinted->SetScalarParameterValue(UsePaletteIndexParam, 1.0f);
			Mesh->SetMaterial(0, Tinted);
		}
	}
	ApplyCollision(*Mesh, Type.Collision);

#if !UE_BUILD_SHIPPING
	if (Type.bLabel && !bFinalMesh)
	{
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"), *Prop.Type.ToString(), *Prop.Id.ToString())));
		Label->SetRelativeLocation(FVector(0.0, 0.0, Type.Size.Z * 100.0 + LabelGapCm));
		Label->SetHiddenInGame(false);
	}
#endif
}
