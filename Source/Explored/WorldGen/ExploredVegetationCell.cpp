#include "WorldGen/ExploredVegetationCell.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"

#include "Core/ExploredWiringSubsystem.h"

AExploredVegetationCell::AExploredVegetationCell()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);
}

namespace
{
	/**
	 * Distancia a la que se deja de evaluar el World Position Offset (balanceo del
	 * viento de M_Leaf / M_Grass, ver Tools/Unreal/build_materials.py::build_foliage).
	 * Por debajo de esta distancia el balanceo no se aprecia y solo cuesta vértices;
	 * en mallas sin WPO (M_Bark) el ajuste no tiene efecto alguno.
	 */
	constexpr float WindWPODisableDistanceCm = 3000.0f;
}

UHierarchicalInstancedStaticMeshComponent* AExploredVegetationCell::GetOrCreateComponent(UStaticMesh* Mesh,
	FName Species, bool bCollision, float CullDistanceMeters, bool bCastShadow)
{
	for (const auto& Pair : ComponentSpecies)
	{
		if (Pair.Key && Pair.Key->GetStaticMesh() == Mesh)
		{
			return Pair.Key;
		}
	}

	const FName Name = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(),
		FName(*FString::Printf(TEXT("HISM_%s"), *Mesh->GetName())));
	UHierarchicalInstancedStaticMeshComponent* Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
	Component->SetMobility(EComponentMobility::Static);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(GetRootComponent());
	Component->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Component->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Component->SetCastShadow(bCastShadow);
	Component->bAffectDistanceFieldLighting = bCollision;
	Component->WorldPositionOffsetDisableDistance = WindWPODisableDistanceCm;
	if (CullDistanceMeters > 0.0f)
	{
		Component->SetCullDistances(static_cast<int32>(CullDistanceMeters * 80.0f), static_cast<int32>(CullDistanceMeters * 100.0f));
	}
	AddInstanceComponent(Component);
	Component->RegisterComponent();
	ComponentSpecies.Add(Component, Species);
	return Component;
}

FName AExploredVegetationCell::GetSpecies(const UHierarchicalInstancedStaticMeshComponent* Component) const
{
	for (const auto& Pair : ComponentSpecies)
	{
		if (Pair.Key == Component)
		{
			return Pair.Value;
		}
	}
	return NAME_None;
}

void AExploredVegetationCell::SetInteractionFocus(UPrimitiveComponent* Component, int32 InstanceIndex)
{
	FocusedComponent = Component;
	FocusedInstanceIndex = InstanceIndex;
}

void AExploredVegetationCell::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	UHierarchicalInstancedStaticMeshComponent* Component = Cast<UHierarchicalInstancedStaticMeshComponent>(FocusedComponent.Get());
	if (UExploredWiringSubsystem* Wiring = UExploredWiringSubsystem::Get(this))
	{
		Wiring->GetHarvestVerbs(*this, Component, FocusedInstanceIndex, OutVerbs);
	}
}

bool AExploredVegetationCell::CanInteract_Implementation(AActor* InInstigator) const
{
	UHierarchicalInstancedStaticMeshComponent* Component = Cast<UHierarchicalInstancedStaticMeshComponent>(FocusedComponent.Get());
	const UExploredWiringSubsystem* Wiring = UExploredWiringSubsystem::Get(this);
	return Wiring && Wiring->CanHarvestInstance(*this, Component, FocusedInstanceIndex);
}

void AExploredVegetationCell::Interact_Implementation(AActor* InInstigator)
{
	UHierarchicalInstancedStaticMeshComponent* Component = Cast<UHierarchicalInstancedStaticMeshComponent>(FocusedComponent.Get());
	if (UExploredWiringSubsystem* Wiring = UExploredWiringSubsystem::Get(this))
	{
		Wiring->HarvestInstance(*this, Component, FocusedInstanceIndex, InInstigator);
	}
}
