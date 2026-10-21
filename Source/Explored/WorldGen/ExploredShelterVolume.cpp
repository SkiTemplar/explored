#include "WorldGen/ExploredShelterVolume.h"

#include "Components/BoxComponent.h"

namespace ExploredShelterVolumeDetail
{
	/** Altura sobre el suelo del refugio a la que reaparece el personaje (cm). */
	constexpr double RespawnLiftCm = 100.0;
}

AExploredShelterVolume::AExploredShelterVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(GetRootComponent());
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);
	Box->SetBoxExtent(FVector(200.0, 150.0, 140.0));
	Box->SetRelativeLocation(FVector(0.0, 0.0, 140.0));
	Box->ShapeColor = FColor(80, 200, 255);
	Tags.Add(TEXT("Shelter"));
}

void AExploredShelterVolume::SetExtentMeters(const FVector& HalfExtentMeters)
{
	const FVector Clamped = HalfExtentMeters.ComponentMax(FVector(0.25));
	Box->SetBoxExtent(Clamped * 100.0);
	// El pivote del actor está en el suelo: la caja sube media altura.
	Box->SetRelativeLocation(FVector(0.0, 0.0, Clamped.Z * 100.0));
}

bool AExploredShelterVolume::IsActorSheltered(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(Actor->GetActorLocation());
	const FVector Extent = Box->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

FVector AExploredShelterVolume::GetRespawnLocation() const
{
	const FVector Center = Box->GetComponentLocation();
	return FVector(Center.X, Center.Y, Center.Z - Box->GetScaledBoxExtent().Z + ExploredShelterVolumeDetail::RespawnLiftCm);
}
