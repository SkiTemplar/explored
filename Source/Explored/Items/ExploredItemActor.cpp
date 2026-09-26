#include "Items/ExploredItemActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Items/ItemRegistrySubsystem.h"

AExploredItemActor::AExploredItemActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
}

void AExploredItemActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	RefreshFromRegistry();
}

void AExploredItemActor::InitializeFromInstance(const FItemInstance& InInstance)
{
	Instance = InInstance;
	RefreshFromRegistry();
}

void AExploredItemActor::RefreshFromRegistry()
{
	if (!Instance.IsValid())
	{
		return;
	}

	const UItemRegistrySubsystem* Registry = UItemRegistrySubsystem::Resolve(this);
	if (!Registry)
	{
		return;
	}

	FItemDefinition Definition;
	if (!Registry->FindDefinition(Instance.DefinitionId, Definition))
	{
		return;
	}

	CachedSize = Registry->GetEffectiveSize(Instance);

	if (UObject* Loaded = Definition.MeshPath.TryLoad())
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Loaded))
		{
			Mesh->SetStaticMesh(StaticMesh);
		}
	}
}

void AExploredItemActor::SetHighlighted(bool bHighlighted)
{
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(bHighlighted);
		Mesh->SetCustomDepthStencilValue(1);
	}
}

void AExploredItemActor::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	const UItemRegistrySubsystem* Registry = UItemRegistrySubsystem::Resolve(this);
	const FText Name = Registry ? Registry->GetDisplayName(Instance) : FText::GetEmpty();
	OutVerbs.Add(FText::Format(NSLOCTEXT("Explored", "Verb_PickUp", "Coger {0}"), Name));
}

bool AExploredItemActor::CanInteract_Implementation(AActor* Instigator) const
{
	return Instance.IsValid();
}

void AExploredItemActor::Interact_Implementation(AActor* Instigator)
{
	// La recogida real la hace UCarryComponent (Player/ExploredCharacter.cpp),
	// que es quien conoce las reglas de manos y tamaños; este método por
	// defecto no hace nada para no duplicar esa lógica.
}
