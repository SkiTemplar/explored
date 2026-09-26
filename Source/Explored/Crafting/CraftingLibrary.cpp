#include "Crafting/CraftingLibrary.h"

#include "Items/ItemRegistrySubsystem.h"
#include "Misc/StringFormatArg.h"

TWeakObjectPtr<const UItemRegistrySubsystem> UCraftingLibrary::CachedRegistry;

void UCraftingLibrary::BindRegistry(const UItemRegistrySubsystem* InRegistry)
{
	CachedRegistry = InRegistry;
}

const UItemRegistrySubsystem* UCraftingLibrary::GetBoundRegistryForTests()
{
	return CachedRegistry.Get();
}

TArray<FName> UCraftingLibrary::FindActions(const FItemInstance& Left, const FItemInstance& Right)
{
	const UItemRegistrySubsystem* Registry = CachedRegistry.Get();
	if (!Registry)
	{
		return {};
	}
	return FindActionsWithData(Left, Right, Registry->GetItems(), Registry->GetTemplates());
}

bool UCraftingLibrary::Apply(const FItemInstance& Left, const FItemInstance& Right, FName VerbId, FItemInstance& OutResult, FText& OutFailReason)
{
	const UItemRegistrySubsystem* Registry = CachedRegistry.Get();
	if (!Registry)
	{
		OutFailReason = NSLOCTEXT("Explored", "Crafting_NoRegistry", "El registro de objetos no está disponible.");
		return false;
	}
	return ApplyWithData(Left, Right, VerbId, Registry->GetItems(), Registry->GetTemplates(), OutResult, OutFailReason);
}

bool UCraftingLibrary::SlotSatisfiedBy(const FCraftingSlot& Slot, const FItemInstance& Piece, const TMap<FName, FItemDefinition>& Items)
{
	if (!Piece.IsValid())
	{
		return false;
	}

	bool bRequirementsPass = true;
	if (Slot.Requirements.Num() > 0)
	{
		if (Slot.bRequireAll)
		{
			bRequirementsPass = true;
			for (const FCraftingRequirement& Req : Slot.Requirements)
			{
				if (ItemEffective::GetProperty(Piece, Req.Property, Items) < Req.MinValue)
				{
					bRequirementsPass = false;
					break;
				}
			}
		}
		else
		{
			bRequirementsPass = false;
			for (const FCraftingRequirement& Req : Slot.Requirements)
			{
				if (ItemEffective::GetProperty(Piece, Req.Property, Items) >= Req.MinValue)
				{
					bRequirementsPass = true;
					break;
				}
			}
		}
	}

	bool bTagsPass = true;
	if (Slot.RequiredTags.Num() > 0)
	{
		bTagsPass = false;
		for (const FName& Tag : Slot.RequiredTags)
		{
			if (ItemEffective::HasTag(Piece, Tag, Items))
			{
				bTagsPass = true;
				break;
			}
		}
	}

	return bRequirementsPass && bTagsPass;
}

bool UCraftingLibrary::TemplateMatches(const FCraftingTemplateDef& Template, const FItemInstance& Left, const FItemInstance& Right, const TMap<FName, FItemDefinition>& Items)
{
	if (!Left.IsValid() || !Right.IsValid())
	{
		return false;
	}

	// Simplificación deliberada: comprobamos que cada rol lo cubra AL MENOS una
	// de las dos piezas, sin exigir que sea una pieza distinta por rol. Así una
	// pieza ya compuesta (p. ej. «palo atado con liana») puede cubrir a la vez
	// el rol de Mango y el de Unión, que es como se llega a una plantilla de 3
	// materiales (cabeza + mango + atadura) combinando solo de dos en dos.
	for (const FCraftingSlot& Slot : Template.Slots)
	{
		const bool bSatisfied = SlotSatisfiedBy(Slot, Left, Items) || SlotSatisfiedBy(Slot, Right, Items);
		if (!bSatisfied)
		{
			return false;
		}
	}
	return true;
}

TArray<FName> UCraftingLibrary::FindActionsWithData(const FItemInstance& Left, const FItemInstance& Right,
	const TMap<FName, FItemDefinition>& Items, const TArray<FCraftingTemplateDef>& Templates)
{
	TArray<FName> Verbs;
	if (!Left.IsValid() || !Right.IsValid())
	{
		return Verbs;
	}

	for (const FCraftingTemplateDef& Template : Templates)
	{
		if (Verbs.Num() >= MaxActions)
		{
			break;
		}
		if (!TemplateMatches(Template, Left, Right, Items))
		{
			continue;
		}
		for (const FName& Verb : Template.Verbs)
		{
			if (Verbs.Num() >= MaxActions)
			{
				break;
			}
			Verbs.AddUnique(Verb);
		}
	}
	return Verbs;
}

FText UCraftingLibrary::BuildGeneratedName(const FCraftingTemplateDef& Template, const FItemInstance& Left, const FItemInstance& Right, const TMap<FName, FItemDefinition>& Items)
{
	if (Template.NameTemplate.IsEmpty())
	{
		if (const FItemDefinition* Def = Items.Find(Template.ResultDefinitionId))
		{
			return Def->NameEs;
		}
		return FText::GetEmpty();
	}

	TArray<FStringFormatArg> Args;
	for (const FCraftingSlot& Slot : Template.Slots)
	{
		const FItemInstance& Piece = SlotSatisfiedBy(Slot, Left, Items) ? Left : Right;
		FText LeafName;
		if (Slot.Requirements.Num() > 0)
		{
			LeafName = ItemEffective::FindDominantLeafName(Piece, Slot.Requirements[0].Property, Items);
		}
		else
		{
			LeafName = ItemEffective::GetDisplayName(Piece, Items);
		}
		Args.Add(FStringFormatArg(LeafName.ToString().ToLower()));
	}

	FString Formatted = FString::Format(*Template.NameTemplate, Args);
	if (Formatted.Len() > 0)
	{
		Formatted[0] = FChar::ToUpper(Formatted[0]);
	}
	return FText::FromString(Formatted);
}

bool UCraftingLibrary::ApplyWithData(const FItemInstance& Left, const FItemInstance& Right, FName VerbId,
	const TMap<FName, FItemDefinition>& Items, const TArray<FCraftingTemplateDef>& Templates,
	FItemInstance& OutResult, FText& OutFailReason)
{
	if (!Left.IsValid() || !Right.IsValid())
	{
		OutFailReason = NSLOCTEXT("Explored", "Crafting_NeedTwo", "Necesitas un objeto en cada mano.");
		return false;
	}

	// De entre todas las plantillas que casan, gana la más específica (más
	// slots), no la primera del catálogo: los «atado_generico»/«pegado_generico»
	// de comodín tienen un slot «Base» sin requisitos que casa con casi
	// cualquier pieza, y si ganasen por orden de aparición nunca se llegaría a
	// una plantilla de 3 roles como el hacha (Cabeza+Mango+Unión) aunque las
	// dos piezas combinadas la satisfagan de sobra.
	const FCraftingTemplateDef* BestTemplate = nullptr;
	for (const FCraftingTemplateDef& Template : Templates)
	{
		if (!Template.Verbs.Contains(VerbId))
		{
			continue;
		}
		if (!TemplateMatches(Template, Left, Right, Items))
		{
			continue;
		}
		if (!BestTemplate || Template.Slots.Num() > BestTemplate->Slots.Num())
		{
			BestTemplate = &Template;
		}
	}

	if (!BestTemplate)
	{
		OutFailReason = NSLOCTEXT("Explored", "Crafting_NoMatch", "Estos objetos no se pueden combinar así.");
		return false;
	}

	if (BestTemplate->bIsSharpen)
	{
		// Slots[0] = herramienta a afilar, Slots[1] = abrasivo (se consume, no se guarda).
		const bool bLeftIsTool = BestTemplate->Slots.Num() > 0 && SlotSatisfiedBy(BestTemplate->Slots[0], Left, Items);
		OutResult = bLeftIsTool ? Left : Right;
		OutResult.Durability = 1.0f;
		return true;
	}

	OutResult = FItemInstance();
	OutResult.DefinitionId = BestTemplate->ResultDefinitionId;
	OutResult.Quality = FMath::Clamp(FMath::Min(Left.Quality, Right.Quality), 1, 5);
	OutResult.Durability = FMath::Min(Left.Durability, Right.Durability);
	OutResult.Count = 1;
	OutResult.Components = { Left, Right };
	OutResult.GeneratedName = BuildGeneratedName(*BestTemplate, Left, Right, Items);
	return true;
}
