#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Crafting/RecipeBookModel.h"
#include "Items/BackpackModel.h"

#include "ExploredSurvivorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIslandMessage, const FText&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBackpackChanged);

/**
 * Mochila, kit inicial, consumo y crafteo de Iria (GDD v3 §5.3 y §5.6): capa fina sobre
 * FBackpackModel, FIslandConsumables y FRecipeBook. Las acciones con duración (recoger 1 s,
 * trenzar 4 s...) van de una en una con un temporizador; mientras dura, IsBusy es true.
 * Los mensajes de una línea («No te cabe más», «No sirve así») salen por OnMessage y al log.
 *
 * Blueprint: «Get Item Count», «Try Add Item», «Consume Item», «Get Craft Options», «Craft»,
 * «Drop Heavy» y los eventos «On Message» y «On Backpack Changed».
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UExploredSurvivorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExploredSurvivorComponent();

	static UExploredSurvivorComponent* FindOn(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Explored|Mochila")
	int32 GetItemCount(FName ItemId) const { return FBackpackModel::Count(Backpack, ItemId); }

	UFUNCTION(BlueprintPure, Category = "Explored|Mochila")
	int32 GetCanteenSips() const { return Backpack.CanteenSips; }

	UFUNCTION(BlueprintPure, Category = "Explored|Mochila")
	FName GetHeldHeavy() const { return Backpack.HeldHeavy; }

	UFUNCTION(BlueprintPure, Category = "Explored|Mochila")
	int32 GetUsedSlots() const { return FBackpackModel::UsedSlots(Backpack); }

	UFUNCTION(BlueprintPure, Category = "Explored|Mochila")
	bool IsBusy() const { return bBusy; }

	/** Mete objetos (todo o nada). Si no caben, avisa «No te cabe más» y devuelve false. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mochila")
	bool TryAddItem(FName ItemId, int32 Count = 1);

	/** Come o bebe una unidad (la cantimplora, un sorbo). Lo que no se consume: «No sirve así». */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mochila")
	bool ConsumeItem(FName ItemId);

	/** Recetas que ofrece arrastrar A sobre B (como mucho 3; vacío = «No sirve así»). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Crafteo")
	TArray<FName> GetCraftOptions(FName ItemA, FName ItemB) const;

	/**
	 * Hace una receta que no necesita un objeto del mundo (cuerda, mecha, caña, remo; la hoguera
	 * la enciende en el suelo delante de Iria). Tarda lo que diga recipes.json.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|Crafteo")
	bool Craft(FName RecipeId);

	/** Suelta la carga pesada (tronco, lente) a los pies. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mochila")
	bool DropHeavy();

	/** Suelta a los pies la pila entera de un objeto (no del kit); se puede volver a coger. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mochila")
	bool DropItem(FName ItemId);

	/** Tecla G con las manos libres: suelta la última pila que no sea del kit (FBackpackModel::DropCandidate). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mochila")
	bool DropLastLoose();

	UFUNCTION(BlueprintCallable, Category = "Explored|Mochila")
	void ShowMessage(const FText& Message);

	/** Acción con duración; OnDone se llama al terminar si no se cancela. false si ya hay otra. */
	bool StartTimedAction(float Seconds, TFunction<void()> OnDone);

	/** Avance 0-1 de la acción con duración en curso (barra de acción del HUD); 0 sin acción. */
	UFUNCTION(BlueprintPure, Category = "Explored|Mochila")
	float GetActionProgress01() const;

	/** Ranura rápida elegida con las teclas 1-3 (GDD §9.1): 0-2. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mochila")
	void SelectQuickSlot(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Explored|Mochila")
	int32 GetSelectedQuickSlot() const { return SelectedQuickSlot; }
	void CancelTimedAction();

	const FBackpackState& GetBackpack() const { return Backpack; }
	/** Sustituye la mochila (resultado de un modelo puro) y avisa a la interfaz. */
	void ApplyBackpack(const FBackpackState& NewState);
	/** Mochila de una partida cargada (se valida con las reglas; si no vale, se queda la actual). */
	void RestoreBackpack(const FBackpackState& Saved);

	/** Avisa con el texto que corresponde a un resultado de la mochila o del crafteo. */
	void ReportResult(EBackpackResult Result);
	void ReportCraft(ECraftResult Result);

	UPROPERTY(BlueprintAssignable, Category = "Explored|Mochila")
	FOnIslandMessage OnMessage;

	UPROPERTY(BlueprintAssignable, Category = "Explored|Mochila")
	FOnBackpackChanged OnBackpackChanged;

	/** Distancia delante de Iria a la que se enciende una hoguera en el suelo (cm). */
	UPROPERTY(EditAnywhere, Category = "Explored|Crafteo")
	float CampfireDistanceCm = 150.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void FinishCraft(FName RecipeId);
	bool FindCampfireSpot(FVector& OutLocation) const;
	/** Punto del suelo delante de Iria donde cae lo que suelta. */
	FVector DropLocation() const;
	void ApplyBodyEffect(float Thirst, float Hunger, float Energy) const;

	FBackpackState Backpack;
	FTimerHandle ActionTimer;
	TFunction<void()> PendingAction;
	bool bBusy = false;
	float ActionTotalSeconds = 0.0f;
	int32 SelectedQuickSlot = 0;
};
