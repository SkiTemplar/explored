#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Interaction/ExploredInteractable.h"
#include "WorldGen/ExploredPlaceholder.h"

#include "ExploredBeaconActors.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Página del cuaderno de Roig en su lugar de la isla (GDD v3 §2.1; island_props.json, tipo
 * «pagina» con «page» 1-8). «Leer la página» la marca en UBeaconSubsystem, enseña el texto y la
 * nota de Iria; se puede releer. Malla: primitiva Plane con papel (GDD §11.1, no es malla única).
 */
UCLASS()
class EXPLORED_API AExploredNotebookPage : public AExploredPlaceholder, public IExploredInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Explored|Baliza")
	int32 GetPage() const { return Page; }

	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

protected:
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

private:
	int32 Page = 0;
};

/**
 * Torre del faro (SM_FaroTorre_01). En el blockout no hay escalera interior: «Subir a la
 * linterna» lleva a Iria a la galería, junto a la linterna, y «Bajar del faro» la devuelve a la
 * puerta. Cuando exista la torre con escalera (F4), este atajo se retira.
 */
UCLASS()
class EXPLORED_API AExploredLighthouseTower : public AExploredPlaceholder, public IExploredInteractable
{
	GENERATED_BODY()

public:
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	/**
	 * Punto de la galería (cm, a ras de su suelo): siempre el lado sur, donde están las ranuras de
	 * la linterna y, al lado, la página 8 (island_props.json).
	 */
	FVector GalleryPoint() const;
	/** Punto de la puerta (cm, a ras del suelo) del lado de FromLocation. */
	FVector DoorPoint(const FVector& FromLocation) const;
	bool IsUp(const AActor& Actor) const;

protected:
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

private:
	float HeightCm = 1200.0f;
	float RadiusCm = 200.0f;
};

/**
 * Luces del correo «Mistral» (GDD v3 §2: sin malla, solo luces y bocina): luz de tope blanca y
 * luces de costado roja y verde que se acercan desde el horizonte hacia la costa.
 */
UCLASS()
class EXPLORED_API AExploredMistralLights : public AActor
{
	GENERATED_BODY()

public:
	AExploredMistralLights();

	/** Empieza el acercamiento de From a To (cm) en Seconds segundos. */
	void Approach(const FVector& From, const FVector& To, float Seconds);

	virtual void Tick(float DeltaSeconds) override;

private:
	UPointLightComponent* MakeLight(const TCHAR* Name, const FLinearColor& Color, const FVector& Offset, float Intensity);
	UStaticMeshComponent* MakeBulb(const TCHAR* Name, UPointLightComponent& Light, float SizeMeters);

	UPROPERTY(VisibleAnywhere, Category = "Explored|Baliza")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Baliza")
	TObjectPtr<UPointLightComponent> MastLight;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Baliza")
	TObjectPtr<UPointLightComponent> PortLight;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Baliza")
	TObjectPtr<UPointLightComponent> StarboardLight;

	FVector Start = FVector::ZeroVector;
	FVector Target = FVector::ZeroVector;
	float Duration = 1.0f;
	float Elapsed = 0.0f;
};
