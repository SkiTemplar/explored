#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Debug/WalkProbeModel.h"

#include "ExploredWalkProbe.generated.h"

/**
 * Prueba funcional a pie (no se incluye en Shipping). Con «-ExploredWalkProbe» en la línea de
 * comandos del juego, espera a los shaders, pone al personaje a andar con AddMovementInput (lo
 * mismo que el teclado) por los caminos de island.json desde la Cala Varada hasta la base del faro
 * y escribe el resultado en Saved/Logs/walk_probe.json (o en «-WalkProbeOut=ruta»). Después cierra
 * el juego. Uso: UnrealEditor.exe Explored.uproject -game -ExploredWalkProbe -windowed ...
 */
UCLASS()
class EXPLORED_API UExploredWalkProbe : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

private:
	void BeginWalk();
	void Finish();

	FWalkProbeModel Probe;
	bool bActive = false;
	bool bWalking = false;
	float Timer = 0.0f;
	float LogTimer = 0.0f;
	float MaxHeightMeters = -1000.0f;
	FVector StartLocation = FVector::ZeroVector;
	FString StartZone;
	FString OutputPath;
};
