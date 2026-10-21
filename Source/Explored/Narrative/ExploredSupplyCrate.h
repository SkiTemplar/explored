#pragma once

#include "CoreMinimal.h"

#include "Interaction/ExploredInteractable.h"
#include "Narrative/SupplyCrateModel.h"
#include "WorldGen/ExploredPlaceholder.h"

#include "ExploredSupplyCrate.generated.h"

/**
 * Caja de suministros de la base del faro (GDD v3 §5.3): al abrirla entrega una sola vez lo
 * mínimo que falte del camino crítico (FSupplyCrateModel; tope 1 cuerda, 3 fibras, 2 peces).
 * Si no cabe en la mochila no se entrega y queda para más tarde. El progreso de la baliza aún no
 * está cableado (F3c): mientras tanto se calcula con el progreso inicial y la mochila.
 */
UCLASS()
class EXPLORED_API AExploredSupplyCrate : public AExploredPlaceholder, public IExploredInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Explored|Caja")
	bool IsDelivered() const { return State.bDelivered; }

	void RefreshFromIslandState();

	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

protected:
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

private:
	FSupplyCrateState State;
};
