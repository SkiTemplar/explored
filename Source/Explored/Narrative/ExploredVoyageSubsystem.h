#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"

#include "Narrative/ExploredProgress.h"

#include "ExploredVoyageSubsystem.generated.h"

class AExploredShipyard;
class FTerrainDensity;

/**
 * Bucle del final del juego (GDD §0, §6.3): rescatar las cuatro piezas del Albatros,
 * construir el «Limón» en la grada de la playa y zarpar de noche guiándose por las
 * estrellas. El estado vive en FExploredProgress (UExploredWiringSubsystem lo guarda);
 * este subsistema coloca los pecios y la grada, aplica las reglas y avisa al HUD y al
 * PlayerController.
 */
UCLASS()
class EXPLORED_API UExploredVoyageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	EVoyageStage GetStage() const;
	int32 CountParts() const;
	bool HasPart(EShipPart Part) const;

	/** Rescata una pieza. Devuelve true si es nueva. */
	bool RecoverPart(EShipPart Part);
	/** Construye el barco en la grada si hay todas las piezas. */
	bool TryBuildShip();
	/** Zarpa si el barco está hecho y es de noche; si no, avisa de por qué. */
	bool TryDepart();
	bool IsStarNightNow() const;
	/** De día, junto al barco: funde a negro y adelanta el reloj a la noche (20:30). */
	void WaitForNight();

	/** Línea del objetivo actual para el HUD (vacía tras el final). */
	FText GetObjectiveText() const;

	static FText PartName(EShipPart Part);

	/**
	 * Sitio de la grada (metros) en la arena de la isla de inicio, junto a la orilla y al
	 * lado del punto de aparición; la proa mira al mar. False si no hay arena.
	 */
	static bool FindShipyardLocation(const FTerrainDensity& Density, FVector& OutMeters, FRotator& OutRotation);

	/** Punto tierra adentro de la isla de inicio (metros) donde cae el motor del Albatros en la v1. */
	static bool FindLandingInland(const FTerrainDensity& Density, FVector& OutMeters);

private:
	void SpawnSalvageSites(UWorld& World);
	void SpawnShipyard(UWorld& World);
	void Notify(const FText& Text, const FLinearColor& Color) const;
	float GetHours() const;

	/** Secuencia de salida: el Limón mar adentro con la cámara detrás; luego el final. */
	void PlayDepartureSequence();
	void FinishDepartureSequence();

	/** Texto «a 120 m, delante» hacia el pecio sin rescatar más cercano (vacío si no hay). */
	FText DescribeNearestSalvage() const;

	TWeakObjectPtr<AExploredShipyard> Shipyard;
	TArray<TWeakObjectPtr<class AExploredSalvageSite>> SalvageSites;
	FTimerHandle DepartureTimer;
	bool bDepartureRunning = false;
	/** Hora a la que se pulsó zarpar (para anotar el final aunque la secuencia cruce las 05:00). */
	float DepartureHours = 0.0f;
};
