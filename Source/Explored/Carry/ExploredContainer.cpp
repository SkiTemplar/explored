#include "Carry/ExploredContainer.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

#include "Carry/CarryComponent.h"
#include "Items/ItemRegistrySubsystem.h"

namespace ExploredContainerDetail
{
	FInventoryContainerSpec SpecForKind(EWorldContainerKind Kind)
	{
		switch (Kind)
		{
		case EWorldContainerKind::Estante: return FInventoryContainerSpec::Shelf();
		case EWorldContainerKind::Arcon: return FInventoryContainerSpec::Chest();
		case EWorldContainerKind::Cesta:
		default: return FInventoryContainerSpec::Basket();
		}
	}

	FText KindName(EWorldContainerKind Kind)
	{
		switch (Kind)
		{
		case EWorldContainerKind::Estante: return NSLOCTEXT("Explored", "Container_Shelf", "estante");
		case EWorldContainerKind::Arcon: return NSLOCTEXT("Explored", "Container_Chest", "arcón");
		case EWorldContainerKind::Cesta:
		default: return NSLOCTEXT("Explored", "Container_Basket", "cesta");
		}
	}
}

AExploredContainer::AExploredContainer()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	// Marcador hasta que haya mallas de cesta, estante y arcón (meshes_pendientes).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}

	Container.Spec = ExploredContainerDetail::SpecForKind(Kind);
}

void AExploredContainer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Container.IsEmpty())
	{
		Container.Spec = MakeSpec();
	}
	Container.Id = ContainerId;
}

FInventoryContainerSpec AExploredContainer::MakeSpec() const
{
	return ExploredContainerDetail::SpecForKind(Kind);
}

void AExploredContainer::SetKind(EWorldContainerKind InKind)
{
	Kind = InKind;
	if (Container.IsEmpty())
	{
		Container.Spec = MakeSpec();
	}
}

FTransform AExploredContainer::GetSlotTransform(int32 SlotIndex) const
{
	const int32 PerRow = FMath::Max(1, SlotsPerRow);
	const int32 Column = SlotIndex % PerRow;
	const int32 Row = (SlotIndex / PerRow) % PerRow;
	const int32 Level = SlotIndex / (PerRow * PerRow);
	const FVector Location = FirstSlotOffsetCm + FVector(Column * SlotSpacingCm.X, Row * SlotSpacingCm.Y, Level * SlotSpacingCm.Z);
	return FTransform(FRotator::ZeroRotator, Location, FVector(StoredItemScale));
}

bool AExploredContainer::RemovePayload(int64 InstanceId, FItemInstance& OutInstance)
{
	return Payloads.RemoveAndCopyValue(InstanceId, OutInstance);
}

void AExploredContainer::RefreshStoredVisuals()
{
	const UItemRegistrySubsystem* Registry = UItemRegistrySubsystem::Resolve(this);

	// Una malla por objeto guardado, en su hueco visible; se reutilizan las que ya hay.
	while (StoredMeshes.Num() < Container.Num())
	{
		UStaticMeshComponent* ItemMesh = NewObject<UStaticMeshComponent>(this);
		ItemMesh->SetMobility(EComponentMobility::Movable);
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ItemMesh->SetupAttachment(RootComponent);
		ItemMesh->RegisterComponent();
		StoredMeshes.Add(ItemMesh);
	}

	for (int32 Index = 0; Index < StoredMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* ItemMesh = StoredMeshes[Index];
		if (!ItemMesh)
		{
			continue;
		}
		if (!Container.Entries.IsValidIndex(Index))
		{
			ItemMesh->SetVisibility(false);
			continue;
		}
		const FInventoryEntry& Entry = Container.Entries[Index];
		UStaticMesh* StaticMesh = nullptr;
		FItemDefinition Definition;
		if (Registry && Registry->FindDefinition(Entry.Item.DefinitionId, Definition))
		{
			StaticMesh = Cast<UStaticMesh>(Definition.MeshPath.TryLoad());
		}
		ItemMesh->SetStaticMesh(StaticMesh);
		ItemMesh->SetRelativeTransform(GetSlotTransform(Entry.SlotIndex));
		ItemMesh->SetVisibility(StaticMesh != nullptr);
	}
}

bool AExploredContainer::FindFilledHand(const UCarryComponent& Carry, EHand& OutHand)
{
	if (!Carry.IsHandEmpty(EHand::Right))
	{
		OutHand = EHand::Right;
		return true;
	}
	if (!Carry.IsHandEmpty(EHand::Left))
	{
		OutHand = EHand::Left;
		return true;
	}
	return false;
}

void AExploredContainer::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	// Los verbos se piden al enfocar (UInteractionComponent), sin saber qué
	// lleva el jugador: se ofrecen los dos y Interact decide por las manos.
	const FText Name = ExploredContainerDetail::KindName(Kind);
	OutVerbs.Add(FText::Format(NSLOCTEXT("Explored", "Verb_StoreIn", "Guardar en {0}"), Name));
	if (!Container.IsEmpty())
	{
		OutVerbs.Add(FText::Format(NSLOCTEXT("Explored", "Verb_TakeFrom", "Sacar de {0}"), Name));
	}
}

bool AExploredContainer::CanInteract_Implementation(AActor* InInstigator) const
{
	return InInstigator && InInstigator->FindComponentByClass<UCarryComponent>() != nullptr;
}

void AExploredContainer::Interact_Implementation(AActor* InInstigator)
{
	UCarryComponent* Carry = InInstigator ? InInstigator->FindComponentByClass<UCarryComponent>() : nullptr;
	if (!Carry)
	{
		return;
	}

	FText FailReason;
	EHand Hand = EHand::Right;
	if (FindFilledHand(*Carry, Hand))
	{
		Carry->StoreInContainer(Hand, this, FailReason);
		return;
	}
	// Manos vacías: se saca lo que ocupa el hueco visible más alto (lo último colocado arriba).
	int32 LastSlot = INDEX_NONE;
	for (const FInventoryEntry& Entry : Container.Entries)
	{
		LastSlot = FMath::Max(LastSlot, Entry.SlotIndex);
	}
	if (LastSlot != INDEX_NONE)
	{
		Carry->TakeFromContainer(this, LastSlot, EHand::Right, FailReason);
	}
}
