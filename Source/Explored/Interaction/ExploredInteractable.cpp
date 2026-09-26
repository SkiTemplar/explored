#include "Interaction/ExploredInteractable.h"

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
