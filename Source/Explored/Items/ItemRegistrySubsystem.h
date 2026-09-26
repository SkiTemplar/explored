#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Crafting/CraftingTypes.h"
#include "Items/ItemTypes.h"

#include "ItemRegistrySubsystem.generated.h"

/**
 * Carga en tiempo de ejecución Content/Data/items.json, templates.json y
 * verbs.json (FJsonSerializer; no UDataAsset, GDD §12.1). Es el único punto de
 * verdad para definiciones de objetos, plantillas de fabricación y verbos.
 *
 * El parseo vive en funciones estáticas (Parse*Json) para que los Automation
 * Spec puedan validar el JSON del proyecto sin necesitar un GameInstance.
 */
UCLASS()
class EXPLORED_API UItemRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Vuelve a leer los tres JSON de Content/Data. Devuelve false si alguno no es válido. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Objetos")
	bool ReloadFromDisk();

	UFUNCTION(BlueprintPure, Category = "Explored|Objetos")
	bool FindDefinition(FName Id, FItemDefinition& OutDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Objetos")
	TArray<FName> GetAllItemIds() const;

	UFUNCTION(BlueprintPure, Category = "Explored|Objetos")
	FText GetDisplayName(const FItemInstance& Instance) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Objetos")
	float GetEffectiveProperty(const FItemInstance& Instance, FName PropertyName) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Objetos")
	float GetEffectiveWeightKg(const FItemInstance& Instance) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Objetos")
	float GetEffectiveVolumeLiters(const FItemInstance& Instance) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Objetos")
	EItemSize GetEffectiveSize(const FItemInstance& Instance) const;

	const TMap<FName, FItemDefinition>& GetItems() const { return Items; }
	const TArray<FCraftingTemplateDef>& GetTemplates() const { return Templates; }
	const TArray<FCraftingVerbDef>& GetVerbs() const { return Verbs; }

	/** Deja el registro con estos datos ya cargados; lo usan los tests que no quieren tocar disco. */
	void SetLoadedDataForTests(TMap<FName, FItemDefinition> InItems, TArray<FCraftingTemplateDef> InTemplates, TArray<FCraftingVerbDef> InVerbs);

	static bool ParseItemsJson(const FString& JsonText, TMap<FName, FItemDefinition>& OutItems, FString& OutError);
	static bool ParseTemplatesJson(const FString& JsonText, TArray<FCraftingTemplateDef>& OutTemplates, FString& OutError);
	static bool ParseVerbsJson(const FString& JsonText, TArray<FCraftingVerbDef>& OutVerbs, FString& OutError);

	/** Ruta absoluta de Content/Data/<FileName> para el proyecto en ejecución. */
	static FString GetDataFilePath(const FString& FileName);

	/**
	 * Punto único para encontrar el registro desde cualquier UObject con
	 * mundo: intenta GameInstance->GetSubsystem primero (partida real) y, si
	 * no hay GameInstance (Automation Spec con un UWorld temporal), recurre
	 * al registro que UCraftingLibrary tenga enlazado. Lo usan
	 * AExploredItemActor, UCarryComponent, AExploredCharacter y AExploredHUD
	 * para no repetir la misma cadena de comprobaciones cuatro veces.
	 */
	static const UItemRegistrySubsystem* Resolve(const UObject* WorldContextObject);

private:
	TMap<FName, FItemDefinition> Items;
	TArray<FCraftingTemplateDef> Templates;
	TArray<FCraftingVerbDef> Verbs;
};
