#include "WorldGen/ExploredVegetationCell.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

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
	 * Más allá de esta distancia el balanceo no se aprecia y solo cuesta vértices;
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
