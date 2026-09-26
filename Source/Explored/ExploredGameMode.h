#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "ExploredGameMode.generated.h"

class APawn;

UCLASS()
class EXPLORED_API AExploredGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AExploredGameMode();

	/**
	 * Muerte del jugador (GDD §7, §8.6, §11). La avisa UExploredWiringSubsystem
	 * cuando el cuerpo emite ESurvivalEvent::Died. Según el modo
	 * (ExploredLinks::DecideRespawn):
	 * - Explorador y Superviviente: reaparece en la fogata encendida más cercana
	 *   (AExploredFire o pieza de construcción con respawnPoint) o, sin ninguna,
	 *   en el PlayerStart, con el cuerpo tocado (ExploredLinks::MakeRespawnState).
	 * - Náufrago: sin reaparición; tras RespawnDelaySeconds vuelve al menú
	 *   recargando el mapa (la partida guardada sigue ahí).
	 */
	void HandlePlayerDeath(APawn* Pawn);

	/** Segundos de pantalla negra (o de quietud) antes de reaparecer o volver al menú. */
	UPROPERTY(EditAnywhere, Category = "Explored|Partida")
	float RespawnDelaySeconds = 3.0f;

private:
	void RespawnPawn(TWeakObjectPtr<APawn> WeakPawn);
	void ReturnToMenu();
	/** Fuegos encendidos y fogatas de la construcción que sirven para reaparecer. */
	void GatherRespawnPoints(TArray<FVector>& OutPoints) const;

	bool bDeathInProgress = false;
};
