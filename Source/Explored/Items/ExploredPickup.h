#pragma once

#include "CoreMinimal.h"

#include "Interaction/ExploredInteractable.h"
#include "WorldGen/ExploredPlaceholder.h"

#include "ExploredPickup.generated.h"

/**
 * Recurso que se recoge (GDD v3 §5.3): montón de leña, fibra, caña de bambú, tronco o la lente.
 * Id estable de island_props.json, objeto de items.json y cantidad; la interacción dura 1 s y,
 * si cabe en la mochila, el id pasa al conjunto de recogidos (no reaparece). Los troncos y la
 * lente van a las manos (carga pesada). Sobre AExploredPlaceholder: primitiva hasta que exista
 * la malla /Game/Art/K3_Objetos/SM_<Nombre>_01.
 *
 * Blueprint: «Get Item Id», «Get Count» y la interacción genérica (tecla E).
 */
UCLASS()
class EXPLORED_API AExploredPickup : public AExploredPlaceholder, public IExploredInteractable
{
	GENERATED_BODY()

public:
	/** Segundos que dura recoger (GDD §5.3). */
	static constexpr float PickupSeconds = 1.0f;

	/** Objeto suelto creado en partida (no tiene id de island_props.json). */
	void ConfigureLoose(FName InItemId, int32 InCount);

	/** Se destruye si su id ya está recogido (tras cargar una partida). */
	void RefreshFromIslandState();

	UFUNCTION(BlueprintPure, Category = "Explored|Recolección")
	FName GetItemId() const { return ItemId; }

	UFUNCTION(BlueprintPure, Category = "Explored|Recolección")
	int32 GetCount() const { return Count; }

	/** Se puede coger: no está recogido (oculto) ni destruyéndose. */
	bool IsAvailable() const { return !bTaken && !IsActorBeingDestroyed(); }

	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	/** Tipo de island_props.json → objeto de items.json (cuando no coinciden). */
	static FName ItemForPropType(FName PropTypeName);

protected:
	virtual void BeginPlay() override;
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

private:
	void Collect(AActor* InInstigator);
	/** Oculta (recogido) o vuelve a mostrar un recurso colocado. */
	void SetTaken(bool bInTaken);

	UPROPERTY(VisibleAnywhere, Category = "Explored|Recolección")
	FName ItemId;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Recolección")
	int32 Count = 1;

	bool bTaken = false;

	/** Soltado en partida: al cogerlo se borra de la lista de soltados en vez de marcarse recogido. */
	bool bLoose = false;
};
