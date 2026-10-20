#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "ExploredGameMode.generated.h"

class AExploredFire;
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
	 * - Explorador y Superviviente: reaparece junto al último fuego encendido si
	 *   sigue ardiendo o, si no, en el PlayerStart, con el cuerpo tocado
	 *   (ExploredLinks::MakeRespawnState).
	 * - Náufrago: sin reaparición; tras RespawnDelaySeconds vuelve al menú
	 *   recargando el mapa (la partida guardada sigue ahí).
	 */
	void HandlePlayerDeath(APawn* Pawn);

	/** Lo avisa AExploredFire al prender: ese fuego pasa a ser el punto de reaparición. */
	void NotifyFireLit(AExploredFire* Fire);

	/** Segundos de pantalla negra (o de quietud) antes de reaparecer o volver al menú. */
	UPROPERTY(EditAnywhere, Category = "Explored|Partida")
	float RespawnDelaySeconds = 3.0f;

private:
	void RespawnPawn(TWeakObjectPtr<APawn> WeakPawn);
	void ReturnToMenu();
	/** El último fuego encendido si aún sirve para reaparecer (arde o tiene brasas); si no, nullptr. */
	const AExploredFire* GetRespawnFire() const;

	/** Último fuego que se encendió en la partida (no lo mantiene vivo). */
	TWeakObjectPtr<AExploredFire> LastLitFire;

	bool bDeathInProgress = false;
};
