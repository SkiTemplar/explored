#include "Interaction/InteractionComponent.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/ExploredInteractable.h"
#include "Items/ExploredItemActor.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// La detección de foco no necesita 60 Hz: 20 Hz es imperceptible aquí y
	// evita convertir esta traza en un coste por fotograma (ver hooks.md).
	SetComponentTickInterval(0.05f);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const AActor* Owner = GetOwner())
	{
		CameraComponent = Owner->FindComponentByClass<UCameraComponent>();
	}
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateFocus();
}

void UInteractionComponent::UpdateFocus()
{
	AActor* Owner = GetOwner();
	if (!Owner || !CameraComponent)
	{
		return;
	}

	const FVector Start = CameraComponent->GetComponentLocation();
	const FVector End = Start + CameraComponent->GetForwardVector() * TraceDistanceCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredInteractionFocus));
	Params.AddIgnoredActor(Owner);

	FHitResult Hit;
	AActor* NewFocus = nullptr;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			if (HitActor->GetClass()->ImplementsInterface(UExploredInteractable::StaticClass())
				&& IExploredInteractable::Execute_CanInteract(HitActor, Owner))
			{
				NewFocus = HitActor;
			}
		}
	}

	// L5: si el actor enfocado se destruye, el puntero débil pasa a null y
	// NewFocus (null) coincidía con él, así que nunca se avisaba de la pérdida
	// de foco. bHasFocus recuerda que el último aviso fue «hay foco».
	const bool bLostStaleFocus = !NewFocus && bHasFocus && !FocusedActor.IsValid();
	if (NewFocus == FocusedActor.Get() && !bLostStaleFocus)
	{
		return;
	}

	if (AExploredItemActor* PreviousItem = Cast<AExploredItemActor>(FocusedActor.Get()))
	{
		PreviousItem->SetHighlighted(false);
	}

	FocusedActor = NewFocus;
	bHasFocus = NewFocus != nullptr;
	CurrentVerbs.Reset();

	if (NewFocus)
	{
		if (AExploredItemActor* NewItem = Cast<AExploredItemActor>(NewFocus))
		{
			NewItem->SetHighlighted(true);
		}
		IExploredInteractable::Execute_GetContextVerbs(NewFocus, CurrentVerbs);
		if (CurrentVerbs.Num() > 3)
		{
			CurrentVerbs.SetNum(3);
		}
	}

	OnFocusChanged.Broadcast(NewFocus);
}

bool UInteractionComponent::InteractWithFocus()
{
	AActor* Focus = FocusedActor.Get();
	if (!Focus)
	{
		return false;
	}
	IExploredInteractable::Execute_Interact(Focus, GetOwner());
	return true;
}
