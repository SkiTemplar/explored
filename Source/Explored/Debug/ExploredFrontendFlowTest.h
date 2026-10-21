#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "ExploredFrontendFlowTest.generated.h"

class AExploredCharacter;
class AExploredPlayerController;

/**
 * Prueba del flujo de menús en una partida real (F5, plan.md: «menú → nueva partida → guardar →
 * menú → continuar restaura el estado»). Con «-ExploredFrontendFlow» (no existe en Shipping) y sin
 * -SkipMenu: con el menú abierto comprueba que el reloj y las barras no corren; empieza una partida,
 * cambia etapa, páginas, mochila, reloj y posición; guarda en «manual2»; vuelve al menú (recarga el
 * mapa); pulsa «Continuar» y compara. Vive en la GameInstance para sobrevivir al cambio de mapa.
 *
 * Escribe «[FrontendFlow] paso ok/FALLO» y «[FrontendFlow] RESULTADO: OK|FALLO», y cierra el juego.
 * Lo lanza Tools/frontend_flow.ps1.
 */
UCLASS()
class EXPLORED_API UExploredFrontendFlowTest : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !bFinished && !HasAnyFlags(RF_ClassDefaultObject); }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }

private:
	struct FSnapshot
	{
		int32 Stage = 0;
		uint8 Pages = 0;
		TMap<FName, int32> Items;
		int32 Day = 0;
		float Hours = 0.0f;
		FVector Location = FVector::ZeroVector;
	};

	struct FStep
	{
		FString Name;
		TFunction<bool()> Start;
		TFunction<bool()> IsDone;
		float TimeoutSeconds = 20.0f;
	};

	void BuildSteps();
	AExploredPlayerController* Controller() const;
	AExploredCharacter* Iria() const;
	bool InMode(uint8 Mode) const;
	FSnapshot Take() const;
	bool Matches(const FSnapshot& Expected, const FSnapshot& Got) const;
	void Finish(bool bOk);

	TArray<FStep> Steps;
	int32 Current = INDEX_NONE;
	float Elapsed = 0.0f;
	bool bFinished = false;
	FSnapshot Saved;
	float MenuBarsSum = -1.0f;
	float MenuHours = -1.0f;
};
