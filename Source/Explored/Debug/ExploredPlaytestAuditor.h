#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Debug/PlaytestReportModel.h"

#include "ExploredPlaytestAuditor.generated.h"

/**
 * Auditor automático de playtest (no se incluye en Shipping). Se activa con
 * «-ExploredPlaytestAudit» o junto con «-ExploredShots=playtest» (ver
 * ExploredShotSubsystem) y, tras asentarse el mundo, recorre actores y
 * componentes buscando defectos frecuentes:
 *
 *  - mallas o instancias con material nulo, WorldGridMaterial o el material
 *    por defecto del motor (bloques blancos, palmeras a medio texturizar);
 *  - ítems o actores con física que se mueven o caen solos en los primeros
 *    segundos (el plátano que se cae del árbol);
 *  - instancias o actores flotando sobre el terreno o enterrados bajo él,
 *    con una traza vertical desde su base;
 *  - puntos de aparición (PlayerStart) con la cápsula del jugador solapada
 *    con colisión.
 *
 * Vuelca Saved/Shots/playtest_report.json (para leer desde fuera del editor)
 * y playtest_report.txt (resumen legible). La clasificación de qué es un
 * defecto y el formato del informe viven en el modelo puro
 * FPlaytestReportModel (Tools/HostTests); este subsistema solo recolecta la
 * evidencia (recorrer el mundo, lanzar trazas) que ese modelo no puede tocar.
 *
 * En solitario («-ExploredPlaytestAudit» sin capturas) escribe el informe y
 * cierra el proceso él mismo en cuanto termina. Combinado con el conjunto
 * «playtest» de capturas, es UExploredShotSubsystem quien pide el volcado
 * final (WriteReport) justo antes de cerrar, para que incluya también el
 * rendimiento de todas las vistas.
 */
UCLASS()
class EXPLORED_API UExploredPlaytestAuditor : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

	/** Añade una muestra de FPS medio/mínimo (y consulta la VRAM actual) para un punto de captura. */
	void RecordFrameSample(const FString& ShotName, float AvgFPS, float MinFPS);

	/** Escribe playtest_report.json y playtest_report.txt con lo acumulado hasta ahora. */
	void WriteReport();

private:
	enum class EPhase : uint8
	{
		Warmup,
		WaitingPhysics,
		Done,
	};

	void AuditMaterials();
	void AuditGroundClearance();
	void AuditSpawnPoints();
	void BeginPhysicsTracking();
	void SamplePhysicsDrift();

	FPlaytestReport Report;
	FString OutputDir;
	bool bActive = false;
	/** true si nadie más (ExploredShotSubsystem) va a cerrar el proceso: lo hace este subsistema. */
	bool bStandalone = false;
	EPhase Phase = EPhase::Warmup;
	float Timer = 0.0f;

	struct FTrackedPhysicsActor
	{
		TWeakObjectPtr<AActor> Actor;
		FVector StartLocationMeters = FVector::ZeroVector;
	};
	TArray<FTrackedPhysicsActor> TrackedPhysicsActors;
};
