#include "Items/ItemRegistrySubsystem.h"

#include "Crafting/CraftingLibrary.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	EItemSize ParseSize(const FString& Value)
	{
		if (Value == TEXT("Mediano")) return EItemSize::Mediano;
		if (Value == TEXT("Grande")) return EItemSize::Grande;
		if (Value == TEXT("DosManos")) return EItemSize::DosManos;
		return EItemSize::Pequeno;
	}

	bool ParseItemObject(const TSharedPtr<FJsonObject>& Obj, FItemDefinition& OutItem, FString& OutError)
	{
		if (!Obj.IsValid())
		{
			OutError = TEXT("Entrada de items.json vacía");
			return false;
		}

		FString IdString;
		if (!Obj->TryGetStringField(TEXT("id"), IdString) || IdString.IsEmpty())
		{
			OutError = TEXT("Objeto sin \"id\" en items.json");
			return false;
		}
		OutItem.Id = FName(*IdString);

		FString NameEs, NameEn, MeshPath, SizeString;
		Obj->TryGetStringField(TEXT("nameEs"), NameEs);
		Obj->TryGetStringField(TEXT("nameEn"), NameEn);
		Obj->TryGetStringField(TEXT("meshPath"), MeshPath);
		Obj->TryGetStringField(TEXT("size"), SizeString);
		OutItem.NameEs = FText::FromString(NameEs);
		OutItem.NameEn = NameEn.IsEmpty() ? OutItem.NameEs : FText::FromString(NameEn);
		OutItem.MeshPath = FSoftObjectPath(MeshPath);
		OutItem.Size = ParseSize(SizeString);

		float WeightKg = 0.1f, VolumeLiters = 0.1f, MaxDurability = 0.0f;
		float NutritionEnergy = 0.0f, NutritionProtein = 0.0f, NutritionVitamins = 0.0f;
		Obj->TryGetNumberField(TEXT("weightKg"), WeightKg);
		Obj->TryGetNumberField(TEXT("volumeLiters"), VolumeLiters);
		Obj->TryGetNumberField(TEXT("maxDurability"), MaxDurability);
		Obj->TryGetNumberField(TEXT("nutritionEnergy"), NutritionEnergy);
		Obj->TryGetNumberField(TEXT("nutritionProtein"), NutritionProtein);
		Obj->TryGetNumberField(TEXT("nutritionVitamins"), NutritionVitamins);
		OutItem.WeightKg = WeightKg;
		OutItem.VolumeLiters = VolumeLiters;
		OutItem.MaxDurability = MaxDurability;
		OutItem.NutritionEnergy = NutritionEnergy;
		OutItem.NutritionProtein = NutritionProtein;
		OutItem.NutritionVitamins = NutritionVitamins;

		const TArray<TSharedPtr<FJsonValue>>* PropsArray = nullptr;
		if (Obj->TryGetArrayField(TEXT("properties"), PropsArray) && PropsArray)
		{
			for (const TSharedPtr<FJsonValue>& Entry : *PropsArray)
			{
				const TSharedPtr<FJsonObject> PropObj = Entry->AsObject();
				if (!PropObj.IsValid())
				{
					continue;
				}
				FString PropName;
				float PropValue = 0.0f;
				if (PropObj->TryGetStringField(TEXT("name"), PropName))
				{
					PropObj->TryGetNumberField(TEXT("value"), PropValue);
					OutItem.Properties.Add(FName(*PropName), PropValue);
				}
			}
		}

		TArray<FString> TagStrings;
		if (Obj->TryGetStringArrayField(TEXT("tags"), TagStrings))
		{
			for (const FString& Tag : TagStrings)
			{
				OutItem.Tags.Add(FName(*Tag));
			}
		}

		return true;
	}

	bool ParseRequirement(const TSharedPtr<FJsonValue>& Value, FCraftingRequirement& OutReq)
	{
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			return false;
		}
		FString PropName;
		if (!Obj->TryGetStringField(TEXT("property"), PropName))
		{
			return false;
		}
		OutReq.Property = FName(*PropName);
		float MinValue = 0.0f;
		Obj->TryGetNumberField(TEXT("min"), MinValue);
		OutReq.MinValue = MinValue;
		return true;
	}

	bool ParseSlot(const TSharedPtr<FJsonValue>& Value, FCraftingSlot& OutSlot)
	{
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			return false;
		}
		FString RoleString;
		Obj->TryGetStringField(TEXT("role"), RoleString);
		OutSlot.Role = FName(*RoleString);

		bool bRequireAll = false;
		Obj->TryGetBoolField(TEXT("requireAll"), bRequireAll);
		OutSlot.bRequireAll = bRequireAll;

		const TArray<TSharedPtr<FJsonValue>>* Requirements = nullptr;
		if (Obj->TryGetArrayField(TEXT("requirements"), Requirements) && Requirements)
		{
			for (const TSharedPtr<FJsonValue>& ReqValue : *Requirements)
			{
				FCraftingRequirement Req;
				if (ParseRequirement(ReqValue, Req))
				{
					OutSlot.Requirements.Add(Req);
				}
			}
		}

		TArray<FString> TagStrings;
		if (Obj->TryGetStringArrayField(TEXT("tags"), TagStrings))
		{
			for (const FString& Tag : TagStrings)
			{
				OutSlot.RequiredTags.Add(FName(*Tag));
			}
		}
		return true;
	}

	bool ParseTemplateObject(const TSharedPtr<FJsonObject>& Obj, FCraftingTemplateDef& OutTemplate, FString& OutError)
	{
		if (!Obj.IsValid())
		{
			OutError = TEXT("Entrada de templates.json vacía");
			return false;
		}
		FString IdString, NameEs, ResultId, NameTemplate;
		if (!Obj->TryGetStringField(TEXT("id"), IdString) || IdString.IsEmpty())
		{
			OutError = TEXT("Plantilla sin \"id\" en templates.json");
			return false;
		}
		OutTemplate.Id = FName(*IdString);
		Obj->TryGetStringField(TEXT("nameEs"), NameEs);
		OutTemplate.NameEs = FText::FromString(NameEs);

		if (!Obj->TryGetStringField(TEXT("resultDefinitionId"), ResultId) || ResultId.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Plantilla \"%s\" sin resultDefinitionId"), *IdString);
			return false;
		}
		OutTemplate.ResultDefinitionId = FName(*ResultId);

		Obj->TryGetStringField(TEXT("nameTemplate"), NameTemplate);
		OutTemplate.NameTemplate = NameTemplate;

		float BaseMaxDurability = 20.0f;
		Obj->TryGetNumberField(TEXT("baseMaxDurability"), BaseMaxDurability);
		OutTemplate.BaseMaxDurability = BaseMaxDurability;

		bool bIsSharpen = false;
		Obj->TryGetBoolField(TEXT("isSharpen"), bIsSharpen);
		OutTemplate.bIsSharpen = bIsSharpen;

		TArray<FString> VerbStrings;
		Obj->TryGetStringArrayField(TEXT("verbs"), VerbStrings);
		for (const FString& Verb : VerbStrings)
		{
			OutTemplate.Verbs.Add(FName(*Verb));
		}
		if (OutTemplate.Verbs.Num() == 0)
		{
			OutError = FString::Printf(TEXT("Plantilla \"%s\" sin verbos"), *IdString);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* SlotsArray = nullptr;
		if (Obj->TryGetArrayField(TEXT("slots"), SlotsArray) && SlotsArray)
		{
			for (const TSharedPtr<FJsonValue>& SlotValue : *SlotsArray)
			{
				FCraftingSlot Slot;
				if (ParseSlot(SlotValue, Slot))
				{
					OutTemplate.Slots.Add(Slot);
				}
			}
		}
		if (OutTemplate.Slots.Num() < 1 || OutTemplate.Slots.Num() > 2)
		{
			OutError = FString::Printf(TEXT("Plantilla \"%s\" debe tener 1 o 2 slots (se combinan como máximo dos piezas)"), *IdString);
			return false;
		}
		return true;
	}
}

void UItemRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadFromDisk();
	UCraftingLibrary::BindRegistry(this);
}

FString UItemRegistrySubsystem::GetDataFilePath(const FString& FileName)
{
	return FPaths::ProjectContentDir() / TEXT("Data") / FileName;
}

const UItemRegistrySubsystem* UItemRegistrySubsystem::Resolve(const UObject* WorldContextObject)
{
	if (const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (const UItemRegistrySubsystem* Registry = GameInstance->GetSubsystem<UItemRegistrySubsystem>())
			{
				return Registry;
			}
		}
	}
	return UCraftingLibrary::GetBoundRegistryForTests();
}

bool UItemRegistrySubsystem::ReloadFromDisk()
{
	bool bOk = true;
	FString Error;

	FString ItemsJson;
	if (FFileHelper::LoadFileToString(ItemsJson, *GetDataFilePath(TEXT("items.json"))))
	{
		TMap<FName, FItemDefinition> LoadedItems;
		if (ParseItemsJson(ItemsJson, LoadedItems, Error))
		{
			Items = MoveTemp(LoadedItems);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Explored] items.json inválido: %s"), *Error);
			bOk = false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Explored] No se encontró %s"), *GetDataFilePath(TEXT("items.json")));
		bOk = false;
	}

	FString TemplatesJson;
	if (FFileHelper::LoadFileToString(TemplatesJson, *GetDataFilePath(TEXT("templates.json"))))
	{
		TArray<FCraftingTemplateDef> LoadedTemplates;
		if (ParseTemplatesJson(TemplatesJson, LoadedTemplates, Error))
		{
			Templates = MoveTemp(LoadedTemplates);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Explored] templates.json inválido: %s"), *Error);
			bOk = false;
		}
	}
	else
	{
		bOk = false;
	}

	FString VerbsJson;
	if (FFileHelper::LoadFileToString(VerbsJson, *GetDataFilePath(TEXT("verbs.json"))))
	{
		TArray<FCraftingVerbDef> LoadedVerbs;
		if (ParseVerbsJson(VerbsJson, LoadedVerbs, Error))
		{
			Verbs = MoveTemp(LoadedVerbs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Explored] verbs.json inválido: %s"), *Error);
			bOk = false;
		}
	}
	else
	{
		bOk = false;
	}

	return bOk;
}

void UItemRegistrySubsystem::SetLoadedDataForTests(TMap<FName, FItemDefinition> InItems, TArray<FCraftingTemplateDef> InTemplates, TArray<FCraftingVerbDef> InVerbs)
{
	Items = MoveTemp(InItems);
	Templates = MoveTemp(InTemplates);
	Verbs = MoveTemp(InVerbs);
	UCraftingLibrary::BindRegistry(this);
}

bool UItemRegistrySubsystem::FindDefinition(FName Id, FItemDefinition& OutDefinition) const
{
	if (const FItemDefinition* Found = Items.Find(Id))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

TArray<FName> UItemRegistrySubsystem::GetAllItemIds() const
{
	TArray<FName> Ids;
	Items.GenerateKeyArray(Ids);
	return Ids;
}

FText UItemRegistrySubsystem::GetDisplayName(const FItemInstance& Instance) const
{
	return ItemEffective::GetDisplayName(Instance, Items);
}

float UItemRegistrySubsystem::GetEffectiveProperty(const FItemInstance& Instance, FName PropertyName) const
{
	return ItemEffective::GetProperty(Instance, PropertyName, Items);
}

float UItemRegistrySubsystem::GetEffectiveWeightKg(const FItemInstance& Instance) const
{
	return ItemEffective::GetWeightKg(Instance, Items);
}

float UItemRegistrySubsystem::GetEffectiveVolumeLiters(const FItemInstance& Instance) const
{
	return ItemEffective::GetVolumeLiters(Instance, Items);
}

EItemSize UItemRegistrySubsystem::GetEffectiveSize(const FItemInstance& Instance) const
{
	return ItemEffective::GetSize(Instance, Items);
}

bool UItemRegistrySubsystem::ParseItemsJson(const FString& JsonText, TMap<FName, FItemDefinition>& OutItems, FString& OutError)
{
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	TArray<TSharedPtr<FJsonValue>> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root))
	{
		OutError = TEXT("JSON de items.json mal formado");
		return false;
	}
	OutItems.Empty(Root.Num());
	for (const TSharedPtr<FJsonValue>& Entry : Root)
	{
		FItemDefinition Item;
		if (!ParseItemObject(Entry->AsObject(), Item, OutError))
		{
			return false;
		}
		if (OutItems.Contains(Item.Id))
		{
			OutError = FString::Printf(TEXT("Id de objeto duplicado: %s"), *Item.Id.ToString());
			return false;
		}
		OutItems.Add(Item.Id, Item);
	}
	return true;
}

bool UItemRegistrySubsystem::ParseTemplatesJson(const FString& JsonText, TArray<FCraftingTemplateDef>& OutTemplates, FString& OutError)
{
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	TArray<TSharedPtr<FJsonValue>> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root))
	{
		OutError = TEXT("JSON de templates.json mal formado");
		return false;
	}
	OutTemplates.Empty(Root.Num());
	for (const TSharedPtr<FJsonValue>& Entry : Root)
	{
		FCraftingTemplateDef Template;
		if (!ParseTemplateObject(Entry->AsObject(), Template, OutError))
		{
			return false;
		}
		OutTemplates.Add(Template);
	}
	return true;
}

bool UItemRegistrySubsystem::ParseVerbsJson(const FString& JsonText, TArray<FCraftingVerbDef>& OutVerbs, FString& OutError)
{
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	TArray<TSharedPtr<FJsonValue>> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root))
	{
		OutError = TEXT("JSON de verbs.json mal formado");
		return false;
	}
	OutVerbs.Empty(Root.Num());
	for (const TSharedPtr<FJsonValue>& Entry : Root)
	{
		const TSharedPtr<FJsonObject> Obj = Entry->AsObject();
		if (!Obj.IsValid())
		{
			OutError = TEXT("Entrada de verbs.json vacía");
			return false;
		}
		FString IdString, NameEs, Description;
		if (!Obj->TryGetStringField(TEXT("id"), IdString) || IdString.IsEmpty())
		{
			OutError = TEXT("Verbo sin \"id\" en verbs.json");
			return false;
		}
		Obj->TryGetStringField(TEXT("nameEs"), NameEs);
		Obj->TryGetStringField(TEXT("description"), Description);

		FCraftingVerbDef Verb;
		Verb.Id = FName(*IdString);
		Verb.NameEs = FText::FromString(NameEs);
		Verb.Description = FText::FromString(Description);
		OutVerbs.Add(Verb);
	}
	return true;
}
