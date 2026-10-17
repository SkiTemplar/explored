#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Interaction/ExploredInteractable.h"
#include "Narrative/ExploredProgress.h"

#include "ExploredVoyageActors.generated.h"

class UStaticMeshComponent;

/**
 * Pieza del Albatros esparcida por el archipiélago (GDD §3, §8.10). UExploredVoyageSubsystem
 * la crea en el punto de interés de cada pecio; al interactuar se rescata la pieza para el
 * barco «Limón». La malla sigue en su sitio: el pecio forma parte del paisaje.
 */
UCLASS()
class EXPLORED_API AExploredSalvageSite : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredSalvageSite();

	/** Antes de FinishSpawning: qué pieza guarda. */
	void Configure(EShipPart InPart);

	EShipPart GetPart() const { return Part; }

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Viaje")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	bool IsRecovered() const;

	EShipPart Part = EShipPart::Fuselage;
};

/**
 * Grada del barco «Limón» en la playa de llegada (GDD §6.3). Muestra el armazón hasta que
 * se construye y el barco terminado después; con el barco hecho y de noche, interactuar
 * zarpa y abre el final.
 */
UCLASS()
class EXPLORED_API AExploredShipyard : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredShipyard();

	/** Cambia la malla según el progreso (armazón o barco terminado). */
	void RefreshMesh();

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Viaje")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	bool bShowingShip = false;
	bool bHasMesh = false;
};
