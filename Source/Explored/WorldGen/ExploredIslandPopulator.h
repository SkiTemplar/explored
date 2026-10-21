#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ExploredIslandPopulator.generated.h"

class UMaterialInterface;
struct FIslandProp;
struct FIslandProps;

/**
 * Puebla la Isla de Sal en BeginPlay (solo con autoridad) a partir de Content/Data/island_props.json:
 * cada colocación se apoya en el terreno con una traza vertical y se crea con la clase registrada
 * para su tipo en ClassRegistry; los tipos sin clase aún generan un AExploredPlaceholder (primitiva
 * escalada con etiqueta) que los tramos siguientes de F3 sustituyen registrando su actor.
 *
 * Blueprint: «Populate» y «Clear» rehacen la isla; «Find Spawned» da el actor de un id estable.
 */
UCLASS()
class EXPLORED_API AExploredIslandPopulator : public AActor
{
	GENERATED_BODY()

public:
	AExploredIslandPopulator();

	/** Crea todas las colocaciones (borra antes las anteriores). Devuelve cuántas creó. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Isla")
	int32 Populate();

	/** Destruye todo lo creado por Populate. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Isla")
	void Clear();

	/** Actor creado para el id estable de island_props.json (nulo si no existe). */
	UFUNCTION(BlueprintPure, Category = "Explored|Isla")
	AActor* FindSpawned(FName PropId) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Isla")
	int32 GetSpawnedCount() const { return Spawned.Num(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Isla")
	int32 GetPlaceholderCount() const { return PlaceholderCount; }

	/** Tipo de island_props.json → clase de actor de juego. Sin entrada: AExploredPlaceholder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explored|Isla")
	TMap<FName, TSubclassOf<AActor>> ClassRegistry;

	/** Material de las primitivas de reserva (M_Toon con PaletteIndex/UsePaletteIndex). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explored|Isla")
	TSoftObjectPtr<UMaterialInterface> ToonMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explored|Isla")
	bool bPopulateOnBeginPlay = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Altura del suelo (m) en (X, Y) metros: traza contra el terreno, o el relieve analítico si no hay. */
	float TraceGroundMeters(const FVector2D& LocationMeters) const;
	AActor* SpawnProp(const FIslandProps& Props, const FIslandProp& Prop, float GroundMeters, UMaterialInterface* Material);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Spawned;

	TMap<FName, TWeakObjectPtr<AActor>> ById;
	int32 PlaceholderCount = 0;
};
