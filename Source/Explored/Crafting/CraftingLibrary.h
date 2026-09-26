#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "Crafting/CraftingTypes.h"
#include "Items/ItemTypes.h"

#include "CraftingLibrary.generated.h"

class UItemRegistrySubsystem;

/**
 * Fabricación por combinación de dos objetos + un verbo (biblia §2.2-§2.4).
 *
 * FindActions/Apply no reciben el registro como parámetro: la firma pública es
 * la que consume el resto del juego. UItemRegistrySubsystem se enlaza una sola
 * vez con BindRegistry al inicializarse (lo hace él mismo en Initialize). Los
 * tests de Automation usan las variantes *WithData, que son puras y no
 * dependen de ningún UObject ni de un mundo.
 */
UCLASS()
class EXPLORED_API UCraftingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxActions = 3;

	/** Lo llama UItemRegistrySubsystem al inicializarse; no hace falta invocarlo a mano en el juego. */
	static void BindRegistry(const UItemRegistrySubsystem* InRegistry);

	/**
	 * El registro enlazado, si hay alguno. Lo usa UCarryComponent como
	 * alternativa cuando no hay GameInstance (los Automation Spec que montan
	 * un UWorld temporal sin arrancar el juego entero); en partida siempre
	 * hay GameInstance y se usa ese camino primero.
	 */
	static const UItemRegistrySubsystem* GetBoundRegistryForTests();

	/** Como máximo MaxActions verbos, en el orden en que aparecen sus plantillas en templates.json. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Fabricación")
	static TArray<FName> FindActions(const FItemInstance& Left, const FItemInstance& Right);

	UFUNCTION(BlueprintCallable, Category = "Explored|Fabricación")
	static bool Apply(const FItemInstance& Left, const FItemInstance& Right, FName VerbId, FItemInstance& OutResult, FText& OutFailReason);

	/**
	 * M17: variantes con contexto de mundo. Usan el registro de la GameInstance de
	 * ese mundo (UItemRegistrySubsystem::Resolve) y solo caen al enlazado si no hay
	 * GameInstance, así que un test que rebinde el registro durante PIE no deja la
	 * fabricación del juego apuntando a su registro de prueba.
	 */
	static TArray<FName> FindActionsInWorld(const UObject* WorldContextObject, const FItemInstance& Left, const FItemInstance& Right);
	static bool ApplyInWorld(const UObject* WorldContextObject, const FItemInstance& Left, const FItemInstance& Right, FName VerbId,
		FItemInstance& OutResult, FText& OutFailReason);

	// --- Variantes puras (sin UObject) que usan los Automation Spec y a las que delegan las de arriba ---

	static TArray<FName> FindActionsWithData(const FItemInstance& Left, const FItemInstance& Right,
		const TMap<FName, FItemDefinition>& Items, const TArray<FCraftingTemplateDef>& Templates);

	static bool ApplyWithData(const FItemInstance& Left, const FItemInstance& Right, FName VerbId,
		const TMap<FName, FItemDefinition>& Items, const TArray<FCraftingTemplateDef>& Templates,
		FItemInstance& OutResult, FText& OutFailReason);

	static bool SlotSatisfiedBy(const FCraftingSlot& Slot, const FItemInstance& Piece, const TMap<FName, FItemDefinition>& Items);

	static bool TemplateMatches(const FCraftingTemplateDef& Template, const FItemInstance& Left, const FItemInstance& Right, const TMap<FName, FItemDefinition>& Items);

private:
	static FText BuildGeneratedName(const FCraftingTemplateDef& Template, const FItemInstance& Left, const FItemInstance& Right, const TMap<FName, FItemDefinition>& Items);

	static TWeakObjectPtr<const UItemRegistrySubsystem> CachedRegistry;
};
