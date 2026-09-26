#pragma once

#include "CoreMinimal.h"

#include "Carry/ExploredContainer.h"

#include "ExploredSledge.generated.h"

/**
 * Angarillas en el mundo (GDD §8.2): dos varas con travesaños que se
 * arrastran para mover troncos y piedra en cantidad (el cuello de botella de
 * la cabaña de madera, docs/balance/2026-09-26-progresion.md).
 *
 * Suelta en el suelo es un contenedor más (se puede cargar). Al engancharla,
 * el objeto y su carga pasan al FInventoryModel del jugador y el actor se
 * queda pegado detrás de él como imagen de la carga; al soltarla (a mano o al
 * empezar a nadar) recupera ambos.
 */
UCLASS()
class EXPLORED_API AExploredSledge : public AExploredContainer
{
	GENERATED_BODY()

public:
	AExploredSledge();

	/** Instancia del objeto «angarillas» con su id del modelo del jugador. */
	void SetSledgeItem(int64 InInstanceId, const FItemInstance& InInstance);
	int64 GetSledgeInstanceId() const { return SledgeInstanceId; }
	const FItemInstance& GetSledgeInstance() const { return SledgeInstance; }

	bool IsAttached() const { return bAttached; }
	/** Lo llama UCarryComponent al engancharla o soltarla. */
	void SetAttached(bool bInAttached);

	/** Distancia detrás del dueño mientras se arrastra. */
	UPROPERTY(EditAnywhere, Category = "Explored|Angarillas")
	FVector AttachOffsetCm = FVector(-160.0f, 0.0f, -70.0f);

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

protected:
	virtual FInventoryContainerSpec MakeSpec() const override { return FInventoryContainerSpec::Sledge(); }

private:
	UPROPERTY()
	FItemInstance SledgeInstance;

	UPROPERTY()
	int64 SledgeInstanceId = 0;

	UPROPERTY()
	bool bAttached = false;
};
