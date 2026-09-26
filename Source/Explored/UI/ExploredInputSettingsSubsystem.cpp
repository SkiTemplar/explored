#include "UI/ExploredInputSettingsSubsystem.h"

void UExploredInputSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadConfig();
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

bool UExploredInputSettingsSubsystem::SetKeyFor(FName ActionName, FKey NewKey)
{
	if (!NewKey.IsValid())
	{
		return false;
	}

	// Comprueba conflicto contra la tecla efectiva de cualquier otra acción conocida.
	for (const TPair<FName, FKey>& Pair : Defaults)
	{
		if (Pair.Key == ActionName)
		{
			continue;
		}
		const FKey Effective = GetKeyFor(Pair.Key, Pair.Value);
		if (Effective == NewKey)
		{
			return false;
		}
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

	SaveConfig();
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
		SaveConfig();
		OnBindingsChanged.Broadcast(ActionName);
	}
}
