#include "Items/ItemTypes.h"

const TCHAR* LexToString(EItemSize Size)
{
	switch (Size)
	{
	case EItemSize::Pequeno: return TEXT("Pequeño");
	case EItemSize::Mediano: return TEXT("Mediano");
	case EItemSize::Grande: return TEXT("Grande");
	case EItemSize::DosManos: return TEXT("DosManos");
	default: return TEXT("Desconocido");
	}
}

namespace ItemEffective
{
	float GetProperty(const FItemInstance& Instance, FName PropertyName, const TMap<FName, FItemDefinition>& Items)
	{
		float Best = 0.0f;
		if (const FItemDefinition* Def = Items.Find(Instance.DefinitionId))
		{
			Best = Def->GetProperty(PropertyName);
		}
		for (const FItemInstance& Component : Instance.Components)
		{
			Best = FMath::Max(Best, GetProperty(Component, PropertyName, Items));
		}
		return Best;
	}

	float GetWeightKg(const FItemInstance& Instance, const TMap<FName, FItemDefinition>& Items)
	{
		if (Instance.Components.Num() > 0)
		{
			float Total = 0.0f;
			for (const FItemInstance& Component : Instance.Components)
			{
				Total += GetWeightKg(Component, Items);
			}
			return Total;
		}
		if (const FItemDefinition* Def = Items.Find(Instance.DefinitionId))
		{
			return Def->WeightKg * FMath::Max(1, Instance.Count);
		}
		return 0.0f;
	}

	float GetVolumeLiters(const FItemInstance& Instance, const TMap<FName, FItemDefinition>& Items)
	{
		if (Instance.Components.Num() > 0)
		{
			float Total = 0.0f;
			for (const FItemInstance& Component : Instance.Components)
			{
				Total += GetVolumeLiters(Component, Items);
			}
			return Total;
		}
		if (const FItemDefinition* Def = Items.Find(Instance.DefinitionId))
		{
			return Def->VolumeLiters * FMath::Max(1, Instance.Count);
		}
		return 0.0f;
	}

	EItemSize GetSize(const FItemInstance& Instance, const TMap<FName, FItemDefinition>& Items)
	{
		if (const FItemDefinition* Def = Items.Find(Instance.DefinitionId))
		{
			return Def->Size;
		}
		return EItemSize::Pequeno;
	}

	bool HasTag(const FItemInstance& Instance, FName Tag, const TMap<FName, FItemDefinition>& Items)
	{
		if (const FItemDefinition* Def = Items.Find(Instance.DefinitionId))
		{
			return Def->HasTag(Tag);
		}
		return false;
	}

	FText GetDisplayName(const FItemInstance& Instance, const TMap<FName, FItemDefinition>& Items)
	{
		if (!Instance.GeneratedName.IsEmpty())
		{
			return Instance.GeneratedName;
		}
		if (const FItemDefinition* Def = Items.Find(Instance.DefinitionId))
		{
			return Def->NameEs;
		}
		return FText::GetEmpty();
	}

	FText FindDominantLeafName(const FItemInstance& Instance, FName PropertyName, const TMap<FName, FItemDefinition>& Items)
	{
		if (Instance.Components.Num() == 0)
		{
			if (const FItemDefinition* Def = Items.Find(Instance.DefinitionId))
			{
				return Def->NameEs;
			}
			return FText::GetEmpty();
		}

		const FItemInstance* Best = &Instance.Components[0];
		float BestValue = GetProperty(*Best, PropertyName, Items);
		for (int32 Index = 1; Index < Instance.Components.Num(); ++Index)
		{
			const float Value = GetProperty(Instance.Components[Index], PropertyName, Items);
			if (Value > BestValue)
			{
				BestValue = Value;
				Best = &Instance.Components[Index];
			}
		}
		return FindDominantLeafName(*Best, PropertyName, Items);
	}
}
