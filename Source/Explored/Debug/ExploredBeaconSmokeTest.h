#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ExploredBeaconSmokeTest.generated.h"

class AActor;
class APawn;
class UBeaconSubsystem;

/**
 * Prueba de humo de la historia de F3c en una partida real (-game con Map_IslaSal,
 * «-SkipMenu -ExploredBeaconSmoke»; no existe en Shipping). Con las mismas llamadas que la
 * tecla E y los comandos de consola BalizaStage, BalizaGive y BalizaTime: lee la página 1, salta
 * a la preparación, sube a la linterna, monta las 3 ranuras, guarda y carga la partida, lee la
 * página 8 (una partida del formato 1 se rechaza sin romper nada), enciende de noche y espera a las luces del Mistral y a la pantalla de epílogo.
 *
 * Escribe «[BalizaSmoke] PASO ok/FALLO» por paso y «[BalizaSmoke] RESULTADO» al final, y cierra.
 */
UCLASS()
class EXPLORED_API UExploredBeaconSmokeTest : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !bFinished; }
	/** El final deja el mundo en pausa: la prueba tiene que seguir mirando. */
	virtual bool IsTickableWhenPaused() const override { return true; }

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
	UBeaconSubsystem* GetBeacon() const;
	bool Interact(const TCHAR* PropId, bool bMovePawn = true);
	bool Idle() const;
	bool Console(const TCHAR* Command) const;
	void StartCurrent();
	void Finish(bool bOk);

	TArray<FStep> Steps;
	int32 Current = INDEX_NONE;
	bool bStarted = false;
	bool bFinished = false;
	float Elapsed = 0.0f;
	float WarmUp = 0.0f;
};
