#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ExploredSaveSubsystem.generated.h"

/**
 * Interfaz mínima de guardado que consume el frontend (menú «Continuar» y
 * pausa «Guardar partida»). El guardado real (semilla, deltas del mundo,
 * jugador y progreso — GDD §10 y módulo `Save`) lo implementa otro equipo:
 * esta clase solo expone la superficie que la UI necesita hoy y debe
 * ampliarse, no sustituirse, cuando llegue esa implementación.
 *
 * Contrato para el equipo de guardado:
 * - HasSaveGame() / GetSlotNames(): hoy escanean Saved/SaveGames/*.sav por
 *   nombre de archivo. Sustituir por metadatos reales (miniatura, fecha,
 *   día de juego) sin cambiar la firma ni el nombre de la clase, para que
 *   AExploredPlayerController siga compilando sin tocarlo.
 * - RequestSave(FName Slot): hoy es un stub que dispara OnSaveCompleted en
 *   el siguiente tick (simula la escritura para que la UI pueda mostrar el
 *   indicador «Guardando…»). Sustituir el cuerpo por la escritura real del
 *   USaveGame; mantener el delegado para que el indicador siga funcionando.
 */
UCLASS()
class EXPLORED_API UExploredSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Explored|Guardado")
	bool HasSaveGame() const;

	UFUNCTION(BlueprintCallable, Category = "Explored|Guardado")
	TArray<FString> GetSlotNames() const;

	/** Lanza un guardado (stub). Ver comentario de clase. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Guardado")
	void RequestSave(FName Slot = TEXT("Auto"));

	DECLARE_MULTICAST_DELEGATE(FOnSaveCompleted);
	FOnSaveCompleted OnSaveCompleted;

private:
	FString GetSaveDirectory() const;
};
