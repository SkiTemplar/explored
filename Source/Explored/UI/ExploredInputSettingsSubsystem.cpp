#include "UI/ExploredInputSettingsSubsystem.h"

#include "UI/SettingsLogic.h"

const FName UExploredInputSettingsSubsystem::ReservedConflictName(TEXT("Reserved"));

void UExploredInputSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadConfig();

	// M12: todas las acciones remapeables participan en la detección de
	// conflictos desde el principio, no solo las que la UI haya registrado.
	for (const ExploredSettingsLogic::FRemappableAction& Action : ExploredSettingsLogic::GetRemappableActions())
	{
		RegisterAction(Action.ActionName, FKey(Action.DefaultKey));
	}
}

void UExploredInputSettingsSubsystem::RegisterAction(FName ActionName, FKey DefaultKey)
{
	if (!Defaults.Contains(ActionName))
	{
		Defaults.Add(ActionName, DefaultKey);
	}
}

FKey UExploredInputSettingsSubsystem::FindOverride(FName ActionName) const
{
	for (const FExploredKeyBindingOverride& Override : Overrides)
	{
		if (Override.ActionName == ActionName)
		{
			return Override.Key;
		}
	}
	return EKeys::Invalid;
}

FKey UExploredInputSettingsSubsystem::GetKeyFor(FName ActionName, FKey DefaultKey) const
{
	const FKey Override = FindOverride(ActionName);
	return Override.IsValid() ? Override : DefaultKey;
}

FName UExploredInputSettingsSubsystem::GetConflictFor(FName ActionName, FKey NewKey) const
{
	TArray<ExploredSettingsLogic::FKeyBinding> Effective;
	Effective.Reserve(Defaults.Num());
	for (const TPair<FName, FKey>& Pair : Defaults)
	{
		Effective.Add({ Pair.Key, GetKeyFor(Pair.Key, Pair.Value).GetFName() });
	}

	const ExploredSettingsLogic::FRemapCheckResult Check =
		ExploredSettingsLogic::CheckRemap(Effective, ActionName, NewKey.IsValid() ? NewKey.GetFName() : NAME_None);
	switch (Check.Result)
	{
	case ExploredSettingsLogic::ERemapCheck::InUse: return Check.ConflictingAction;
	case ExploredSettingsLogic::ERemapCheck::ReservedKey:
	case ExploredSettingsLogic::ERemapCheck::InvalidKey: return ReservedConflictName;
	default: return NAME_None;
	}
}

bool UExploredInputSettingsSubsystem::SetKeyFor(FName ActionName, FKey NewKey)
{
	// Los mapeos de mando son fijos (ver AExploredCharacter): una tecla de mando
	// aquí sustituiría la de teclado de la acción.
	if (!NewKey.IsValid() || NewKey.IsGamepadKey())
	{
		return false;
	}
	if (!GetConflictFor(ActionName, NewKey).IsNone())
	{
		return false;
	}

	bool bFound = false;
	for (FExploredKeyBindingOverride& Override : Overrides)
	{
		if (Override.ActionName == ActionName)
		{
			Override.Key = NewKey;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		FExploredKeyBindingOverride NewOverride;
		NewOverride.ActionName = ActionName;
		NewOverride.Key = NewKey;
		Overrides.Add(NewOverride);
	}

	SaveOverrides();
	OnBindingsChanged.Broadcast(ActionName);
	return true;
}

void UExploredInputSettingsSubsystem::ResetKeyFor(FName ActionName)
{
	const int32 Index = Overrides.IndexOfByPredicate([ActionName](const FExploredKeyBindingOverride& Override)
	{
		return Override.ActionName == ActionName;
	});
	if (Index != INDEX_NONE)
	{
		Overrides.RemoveAt(Index);
		SaveOverrides();
		OnBindingsChanged.Broadcast(ActionName);
	}
}

void UExploredInputSettingsSubsystem::UseTransientStorageForTesting()
{
	bPersistOverrides = false;
	Overrides.Reset();
}

void UExploredInputSettingsSubsystem::SaveOverrides()
{
	if (bPersistOverrides)
	{
		SaveConfig();
	}
}
