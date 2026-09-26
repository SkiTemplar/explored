#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Interaction/ExploredInteractable.h"
#include "Ruins/RuinsModel.h"

#include "ExploredRuinElement.generated.h"

class UStaticMeshComponent;

/**
 * Elemento descubrible de una ruina (GDD §6.1): petroglifo, estatua, altar,
 * brújula estelar, canoa doble fosilizada o cueva ritual. URuinsSubsystem crea
 * uno por elemento del modelo que no tenga ya un actor colocado en el mapa, en la
 * posición de FRuinElement asentada en el suelo.
 *
 * La interacción decide si cuenta como descubrimiento y avisa a
 * URuinsSubsystem::NotifyElementDiscovered:
 * - estatua: hay que mirar hacia donde mira ella (ExploredLinks::IsFacingAlong);
 * - altar: se deja una ofrenda (una unidad de lo que se lleve en una mano);
 * - el resto: basta con examinarlo.
 *
 * Mallas marcadoras de /Engine/BasicShapes hasta que existan las del pueblo navegante.
 */
UCLASS()
class EXPLORED_API AExploredRuinElement : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredRuinElement();

	/** Antes de FinishSpawning: qué elemento es. */
	void Configure(FName InElementId, ERuinElementKind InKind);

	UFUNCTION(BlueprintPure, Category = "Explored|Ruinas")
	FName GetElementId() const { return ElementId; }

	ERuinElementKind GetKind() const { return Kind; }

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	/** Tolerancia (grados) para alinearse con la estatua. */
	UPROPERTY(EditAnywhere, Category = "Explored|Ruinas")
	float StatueAlignToleranceDeg = 20.0f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Ruinas")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/**
	 * Id del elemento en FRuinsLayout (el ContentId del petroglifo o «<ruina>_<tipo>»).
	 * En un actor colocado a mano basta con esto: el tipo se lee del modelo al empezar.
	 */
	UPROPERTY(EditAnywhere, Category = "Explored|Ruinas")
	FName ElementId;

private:
	bool IsDiscovered() const;
	/** Tipo del elemento según el modelo de ruinas (para los colocados a mano). */
	void ResolveKind();
	void ApplyMarkerMesh();

	ERuinElementKind Kind = ERuinElementKind::Petroglyph;
};
