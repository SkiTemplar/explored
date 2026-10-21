#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Debug/PlaytestRunModel.h"

#include "ExploredPlaytestBot.generated.h"

class AActor;
class AExploredCharacter;
class FBalizaRunLogCapture;
class UExploredSurvivorComponent;
class UIslandBodyComponent;

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
 *
 * Partida completa de Baliza Siete («-BalizaRun[=competente]» con «-SkipMenu»; ver
 * ExploredPlaytestBotRun.cpp y Tools/playtest_v3.ps1): la ruta competente del GDD §3 con
 * teletransporte entre los puntos de island_props.json, las mismas llamadas que la tecla E y el
 * reloj acelerado solo mientras espera. Mide la duración a 1× (FPlaytestRunModel), desmayos,
 * etapas, errores de log y guardar/cargar en cada etapa, y lo deja en el informe del auditor.
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
	/** El final de Baliza Siete deja el mundo en pausa: el guion tiene que seguir mirando. */
	virtual bool IsTickableWhenPaused() const override { return bRunMode; }
	virtual void Deinitialize() override;

	/** true cuando termina la ruta (o no tenía nada que recorrer): el auditor espera a esto en solitario. */
	bool IsDone() const { return bDone; }

	/** Resultado de la partida completa (Run.bRan falso fuera de «-BalizaRun»). */
	const FPlaytestRunSummary& GetRunSummary() const { return Run; }

private:
	/** Un paso del guion de partida completa. Start una vez; IsDone cada fotograma. */
	struct FRunStep
	{
		FString Name;
		TFunction<bool()> Start;
		TFunction<bool()> IsDone;
		/** Tope en segundos de partida a 1× (el reloj de la prueba, no el real). */
		double Timeout1x = 120.0;
		/** El paso puede ir con el reloj acelerado (esperas y trayectos). */
		bool bFast = false;
	};

	// --- Partida completa (ExploredPlaytestBotRun.cpp) -----------------------------------
	void BeginRun();
	void TickRun(float DeltaTime);
	void BuildRunScript();
	void AddDayOne();
	void AddDayTwo();
	void AddCyclone();
	void AddDayThree();
	void AddStep(const FString& Name, TFunction<bool()> Start, TFunction<bool()> IsDone, double Timeout1x = 120.0, bool bFast = false);
	void AddGoTo(const FString& PropId);
	void AddGoToActor(const FString& Label, TFunction<AActor*()> Target);
	void AddUse(const FString& PropId, int32 Times = 1, bool bOptional = false);
	void AddCraft(const FString& RecipeId);
	void AddWaitUntil(int32 Day, double Hour, const FString& Why);
	void AddFish(int32 Target);
	void AddFireJob(TFunction<AActor*()> Fire, const FString& Action, const FString& Product);
	void AddRow(const FString& Label, TFunction<FVector()> Target, double ArriveMeters);
	void StartRunStep();
	void FinishRunStep(bool bOk);
	void EndRun(const FString& FailedStep);
	void Upkeep();
	void TrackStageAndSaves();
	bool CheckSaveLoad(const FString& StageName);
	AActor* Prop(const FString& PropId) const;
	AActor* NearestFire() const;
	UExploredSurvivorComponent* Survivor() const;
	/** Come o bebe como el jugador: AExploredCharacter::UseBackpackItem (botón «Usar» del inventario). */
	bool UseItem(const TCHAR* ItemId) const;
	UIslandBodyComponent* Body() const;
	int32 Count(const TCHAR* ItemId) const;
	bool Idle() const;
	bool InteractWith(AActor* Target) const;
	bool TeleportNear(AActor* Target, bool bCountTravel);
	double AbsHours() const;
	void SetClockScale(float Scale) const;
	void Note(const FString& Text) const;

	bool bRunMode = false;
	FString RouteName;
	TArray<FRunStep> RunSteps;
	int32 RunIndex = INDEX_NONE;
	double StepStart1x = 0.0;
	double TravelLeft1x = 0.0;
	float FastScale = 6.0f;
	int32 LastStageIndex = -1;
	TArray<FString> PendingSaveChecks;
	FPlaytestRunSummary Run;
	FPlaytestRunTuning RunTuning;
	TSharedPtr<FBalizaRunLogCapture> LogCapture;
	/** Marcas entre pasos (contador, hora...). */
	int32 MarkCount = 0;
	double MarkHours = 0.0;
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
