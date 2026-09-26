#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Interaction/ExploredInteractable.h"
#include "Items/ItemTypes.h"

#include "ExploredItemActor.generated.h"

class UStaticMeshComponent;

/**
 * Representación física en el mundo de un FItemInstance: una malla con física
 * que aparece al soltar un objeto y se destruye al recogerlo (GDD §4.2, §12.2).
 */
UCLASS()
class EXPLORED_API AExploredItemActor : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredItemActor();

	/** Coloca la malla y el tamaño efectivo según la definición del objeto en el registro. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Objetos")
	void InitializeFromInstance(const FItemInstance& InInstance);

	UFUNCTION(BlueprintPure, Category = "Explored|Objetos")
	const FItemInstance& GetItemInstance() const { return Instance; }

	EItemSize GetEffectiveSize() const { return CachedSize; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Objetos")
	void SetHighlighted(bool bHighlighted);

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

protected:
	virtual void PostInitializeComponents() override;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Objetos")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	FItemInstance Instance;

private:
	void RefreshFromRegistry();

	EItemSize CachedSize = EItemSize::Pequeno;
};
