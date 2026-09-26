#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Fauna/FaunaAnimation.h"
#include "Fauna/MarineCreatureBrain.h"

#include "ExploredMarineCreature.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * Criatura marina suelta (raya, medusa, tiburones, delfín, tortuga, ballena).
 * Sin esqueleto ni física: la mueve AExploredFaunaManager de forma cinemática
 * con su FMarineCreatureBrain, y la animación la hace el material leyendo los
 * datos de primitiva 0–3 (FFaunaAnimParams; ver docs/tecnico/fauna.md).
 */
UCLASS(NotPlaceable)
class EXPLORED_API AExploredMarineCreature : public AActor
{
	GENERATED_BODY()

public:
	AExploredMarineCreature();

	/** Prepara el cerebro y la malla. Llamar justo después de crearla. */
	void InitCreature(const FMarineBrainConfig& Config, const FFaunaWorldQuery& World, UStaticMesh* Mesh);

	/** Avanza el cerebro y copia posición, rumbo, visibilidad y animación al actor. */
	FMarineBrainEvents StepCreature(float DeltaSeconds, const FFaunaStimuli& Stimuli, const FFaunaWorldQuery& World);

	const FMarineCreatureBrain& GetBrain() const { return Brain; }
	FMarineCreatureBrain& GetBrainMutable() { return Brain; }

	/** Celda de población de la que salió (para retirarla al alejarse). */
	FIntPoint SpawnCell = FIntPoint::ZeroValue;

private:
	void ApplyToActor(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Explored|Fauna")
	TObjectPtr<UStaticMeshComponent> Body;

	FMarineCreatureBrain Brain;
	FFaunaAnimState AnimState;
};
