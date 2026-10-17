#include "Interaction/ExploredInteractable.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

// Los valores por defecto se usan si un implementador de Blueprint u otra
// clase C++ no sobrescribe el evento. En C++ se sobrescriben *_Implementation
// directamente (ver AExploredItemActor).

void IExploredInteractable::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
}

bool IExploredInteractable::CanInteract_Implementation(AActor* Instigator) const
{
	return true;
}

void IExploredInteractable::Interact_Implementation(AActor* Instigator)
{
}

void ExploredInteraction::AddFocusBox(AActor& Owner, const UStaticMeshComponent& Mesh)
{
	const FBoxSphereBounds Bounds = Mesh.CalcBounds(Mesh.GetComponentTransform());
	if (Bounds.BoxExtent.IsNearlyZero())
	{
		return;
	}
	UBoxComponent* Box = NewObject<UBoxComponent>(&Owner, TEXT("InteractionFocus"));
	Box->SetupAttachment(Owner.GetRootComponent());
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Box->SetGenerateOverlapEvents(false);
	Box->RegisterComponent();
	// Límites en mundo, alineados a ejes: la caja se coloca en mundo sin rotación.
	Box->SetWorldLocationAndRotation(Bounds.Origin, FRotator::ZeroRotator);
	Box->SetBoxExtent(Bounds.BoxExtent);
}

