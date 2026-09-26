#include "Ruins/MuseumDisplayComponent.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Ruins/MuseumModel.h"
#include "Ruins/RuinsSubsystem.h"

UMuseumDisplayComponent::UMuseumDisplayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

URuinsSubsystem* UMuseumDisplayComponent::GetRuinsSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<URuinsSubsystem>() : nullptr;
}

void UMuseumDisplayComponent::BeginPlay()
{
	Super::BeginPlay();
	if (DisplayKey == INDEX_NONE && GetOwner())
	{
		// Sin clave de la construcción: el nombre del actor (nunca -1).
		DisplayKey = static_cast<int32>(GetTypeHash(GetOwner()->GetPathName()) & 0x7FFFFFFFu);
	}
	if (URuinsSubsystem* Ruins = GetRuinsSubsystem())
	{
		// Si el mueble ya venía del guardado, AddDisplay dice DuplicateDisplay y se respeta lo expuesto.
		Ruins->RegisterDisplay(DisplayKey, DisplayId);
		// AddUObject guarda un puntero débil: si el componente muere, el delegado no lo llama.
		MuseumChangedHandle = Ruins->OnMuseumChanged.AddUObject(this, &UMuseumDisplayComponent::RefreshSlots);
	}
	RefreshSlots();
}

void UMuseumDisplayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Quitar el actor del mundo (streaming, fin de partida) no desmonta el mueble del museo:
	// eso lo hace la construcción con URuinsSubsystem::UnregisterDisplay al derribarlo.
	if (URuinsSubsystem* Ruins = GetRuinsSubsystem())
	{
		Ruins->OnMuseumChanged.Remove(MuseumChangedHandle);
	}
	MuseumChangedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UMuseumDisplayComponent::PlaceInFirstFreeSlot(FName ArtifactId)
{
	URuinsSubsystem* Ruins = GetRuinsSubsystem();
	const FDisplayDef* Def = Ruins ? Ruins->GetMuseum().GetCatalog().FindDisplay(DisplayId) : nullptr;
	if (!Def)
	{
		return false;
	}
	for (int32 Slot = 0; Slot < Def->Slots.Num(); ++Slot)
	{
		if (Ruins->GetMuseum().CanPlace(ArtifactId, DisplayKey, Slot) == EMuseumResult::Ok)
		{
			return Ruins->PlaceArtifact(ArtifactId, DisplayKey, Slot) == EMuseumResult::Ok;
		}
	}
	return false;
}

FName UMuseumDisplayComponent::RemoveFromSlot(int32 Slot)
{
	FName Removed;
	if (URuinsSubsystem* Ruins = GetRuinsSubsystem())
	{
		Ruins->RemoveArtifact(DisplayKey, Slot, Removed);
	}
	return Removed;
}

void UMuseumDisplayComponent::RefreshSlots()
{
	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	const URuinsSubsystem* Ruins = GetRuinsSubsystem();
	if (!Root || !Ruins)
	{
		return;
	}
	const FMuseumModel& Museum = Ruins->GetMuseum();
	const FDisplayDef* Def = Museum.GetCatalog().FindDisplay(DisplayId);
	const FDisplayState* State = Museum.FindDisplay(DisplayKey);
	const int32 NumSlots = Def ? Def->Slots.Num() : 0;

	// Un componente de malla por hueco, creado la primera vez y reutilizado después.
	while (SlotMeshes.Num() < NumSlots)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetRelativeLocation(Def->Slots[SlotMeshes.Num()].Offset);
		Mesh->RegisterComponent();
		SlotMeshes.Add(Mesh);
	}

	for (int32 Slot = 0; Slot < SlotMeshes.Num(); ++Slot)
	{
		UStaticMeshComponent* Mesh = SlotMeshes[Slot];
		if (!Mesh)
		{
			continue;
		}
		const FName ArtifactId = State && State->Slots.IsValidIndex(Slot) ? State->Slots[Slot] : FName();
		const FArtifactDef* Artifact = Museum.GetCatalog().FindArtifact(ArtifactId);
		UStaticMesh* Asset = nullptr;
		if (Artifact && !Artifact->Mesh.IsNone())
		{
			const FString Name = Artifact->Mesh.ToString();
			const FString Path = FString::Printf(TEXT("%s/%s.%s"), *MeshFolder, *Name, *Name);
			Asset = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
		}
		Mesh->SetStaticMesh(Asset);
		Mesh->SetVisibility(Asset != nullptr);
	}
}
