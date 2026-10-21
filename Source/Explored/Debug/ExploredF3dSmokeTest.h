#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ExploredF3dSmokeTest.generated.h"

class APawn;

/**
 * Prueba de humo de F3d en una partida real (-game con Map_IslaSal, «-SkipMenu -ExploredF3dSmoke»;
 * no existe en Shipping): reloj de 24 min, HUD (barras, ranuras, reloj, barómetro, aviso), cuerpo
 * con una barra baja (velocidad -20 %), chubasco, balsa (atar, remar, encallar, bajar), mar
 * cerrado, cruce forzoso de las 18:45, ciclón (muro, fuego apagado, olas, niebla, barril), ojo,
 * reloj acelerado, desmayo y dormir. Usa los comandos BalizaTime, BalizaTimeScale, BalizaBody,
 * BalizaRaft, BalizaSleep y las mismas llamadas que la tecla E.
 *
 * Capturas: Saved/Shots/f3d-hud.png y Saved/Shots/f3d-ciclon.png. Escribe «[F3dSmoke] PASO ok» o
 * «FALLO» por paso y «[F3dSmoke] RESULTADO» al final, y cierra.
 */
UCLASS()
class EXPLORED_API UExploredF3dSmokeTest : public UTickableWorldSubsystem
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
	void AddClimateSteps();
	void AddRaftSteps();
	void AddBodySteps();
	APawn* GetPawn() const;
	bool Console(const FString& Command) const;
	double AbsHours() const;
	bool Shot(const FString& Name);
	void Face(const FVector& Target) const;
	void StartCurrent();
	void Finish(bool bOk);

	TArray<FStep> Steps;
	int32 Current = INDEX_NONE;
	bool bStarted = false;
	bool bFinished = false;
	float Elapsed = 0.0f;
	float WarmUp = 0.0f;
	/** Valores medidos entre pasos. */
	double MarkHours = 0.0;
	float MarkThirst = 0.0f;
	FVector MarkLocation = FVector::ZeroVector;
	int32 MarkCount = 0;
};
