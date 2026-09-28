#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ExploredPlaytestBot.generated.h"

class AExploredCharacter;

/**
 * Bot de juego sencillo (no se incluye en Shipping). Se activa junto con
 * UExploredPlaytestAuditor («-ExploredPlaytestAudit» o «-ExploredShots=playtest»)
 * y recorre unos pocos puntos fijos de la isla Landing: en cada uno, mira hacia
 * el terreno cercano y, si UInteractionComponent le da foco (las mismas 250 cm
 * de alcance que usaría el jugador), interactúa con lo más cercano —recolectar
 * un objeto suelto vía UCarryComponent::TryPickUp si es un AExploredItemActor,
 * o el evento genérico IExploredInteractable::Interact en cualquier otro caso
 * (talar, recolectar de una celda de vegetación...)— exactamente con las
 * mismas funciones que llama AExploredCharacter::HandleInteract al pulsar la
 * tecla de interactuar.
 *
 * Registra en UExploredPlaytestAuditor, por cada punto: qué tenía enfocado, si
 * llegó a interactuar y cómo cambió el recuento de materiales de
 * UCarryComponent::CountMaterials antes/después. Pide una captura antes y
 * después de cada interacción (mismo Saved/Shots que UExploredShotSubsystem).
 *
 * Ruta corta a propósito («si da tiempo», encargo original): un par de puntos
 * de Landing, no una cobertura completa del archipiélago. IsDone() es lo que
 * consulta el auditor en solitario para no cerrar el proceso a mitad de ruta.
 */
UCLASS()
class EXPLORED_API UExploredPlaytestBot : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

	/** true cuando termina la ruta (o no tenía nada que recorrer): el auditor espera a esto en solitario. */
	bool IsDone() const { return bDone; }

private:
	struct FWaypoint
	{
		FString Name;
		FVector LocationMeters = FVector::ZeroVector;
		FRotator AimRotation = FRotator::ZeroRotator;
	};

	enum class EStepPhase : uint8
	{
		WaitFocus,
		RequestBefore,
		WaitBeforeDone,
		Interact,
		WaitEffect,
		RequestAfter,
		WaitAfterDone,
	};

	void BuildRoute();
	void BeginWaypoint(int32 Index);
	void SnapshotInventory(TMap<FName, int32>& OutCounts) const;
	FString DescribeInventoryDelta(const TMap<FName, int32>& Before, const TMap<FName, int32>& After) const;

	TArray<FWaypoint> Route;
	int32 CurrentWaypoint = INDEX_NONE;
	EStepPhase StepPhase = EStepPhase::WaitFocus;
	float Timer = 0.0f;
	float WarmupTimer = 0.0f;
	bool bActive = false;
	bool bDone = false;
	bool bWarmedUp = false;

	TWeakObjectPtr<AExploredCharacter> Character;
	FString OutputDir;
	FString FocusedActorNameAtStep;
	bool bInteractedAtStep = false;
	TMap<FName, int32> CountsBeforeStep;
};
