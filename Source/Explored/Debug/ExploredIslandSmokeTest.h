#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ExploredIslandSmokeTest.generated.h"

class AActor;
class APawn;

/**
 * Prueba de humo del bucle de recursos de F3b en una partida real (-game con Map_IslaSal y
 * «-ExploredIslandSmoke»; no existe en Shipping). Con las mismas llamadas que la tecla E
 * (IExploredInteractable::Interact y AExploredFire::PerformAction) recoge leña y fibra, trenza
 * una cuerda, ata la caña de pescar, pesca un pez, enciende la hoguera del faro, lo asa, lo come,
 * bebe un coco, abre la caja de suministros y llena y bebe la cantimplora en el barril.
 *
 * Escribe «[IslaSmoke] PASO ok/FALLO» por paso y «[IslaSmoke] RESULTADO» al final, y cierra el juego.
 */
UCLASS()
class EXPLORED_API UExploredIslandSmokeTest : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !bFinished; }

private:
	struct FStep
	{
		FString Name;
		TFunction<bool()> Start;
		TFunction<bool()> IsDone;
		float TimeoutSeconds = 10.0f;
	};

	void BuildSteps();
	AActor* FindProp(const TCHAR* PropId) const;
	APawn* GetPawn() const;
	int32 Count(const TCHAR* ItemId) const;
	bool Interact(const TCHAR* PropId);
	bool Idle() const;
	void Finish(bool bOk);

	TArray<FStep> Steps;
	int32 Current = INDEX_NONE;
	bool bStarted = false;
	bool bFinished = false;
	bool bAllOk = true;
	float Elapsed = 0.0f;
	float WarmUp = 0.0f;
};
