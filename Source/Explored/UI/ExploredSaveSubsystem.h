#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Save/SaveFormat.h"
#include "Save/SaveSlots.h"
#include "Save/SaveWorldDeltas.h"

#include "ExploredSaveSubsystem.generated.h"

class FSubsystemCollectionBase;
class UWorld;

/**
 * Guardado de partida (GDD §15 y §17.2): conecta el núcleo puro de `Save/`
 * con el disco y con los sistemas del juego. Ver docs/tecnico/guardado.md.
 *
 * - Formato: texto JSON legible con cabecera (versión, semilla, tiempo jugado,
 *   fecha, ranura), secciones por sistema y suma de control.
 * - Ranuras: «manual1»…«manual3» y «auto», cada una con su copia «.bak», en
 *   Saved/SaveGames/. Escritura atómica: temporal y renombrado.
 * - Cada sistema registra su sección con RegisterSection(nombre, Save, Load).
 *   La sección «player» (posición y rotación del personaje) la registra este
 *   subsistema.
 *
 * Mantiene la superficie que ya usaba la UI (HasSaveGame, GetSlotNames,
 * RequestSave y OnSaveCompleted) con la misma firma.
 */
UCLASS()
class EXPLORED_API UExploredSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Hay al menos una ranura que se puede cargar (principal o copia). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Guardado")
	bool HasSaveGame() const;

	/** Ranuras cargables en orden de «Continuar» (la más reciente primero). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Guardado")
	TArray<FString> GetSlotNames() const;

	/**
	 * Guarda en la ranura indicada («Auto», «Manual1»…«Manual3» o «1»…«3»; un
	 * nombre desconocido va a «auto»). Dispara OnSaveCompleted si sale bien y
	 * OnSaveFailed si no.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|Guardado")
	void RequestSave(FName Slot = TEXT("Auto"));

	/** Carga la ranura de «Continuar». Devuelve false si no hay ninguna legible. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Guardado")
	bool LoadContinueGame();

	/** Guarda en una ranura canónica («auto», «manual1»…). */
	bool SaveToSlot(const FString& SlotId, FString& OutError);

	/** Carga una ranura y reparte sus secciones; cae a la copia si la principal está dañada. */
	ESaveLoadResult LoadFromSlot(const FString& SlotId, FString& OutError);

	/** Autoguardado por dormir u hoguera (con intervalo mínimo entre hogueras). */
	void RequestAutosave(ESaveTrigger Trigger);

	/** Estado de todas las ranuras con fichero, en orden de «Continuar». */
	TArray<FSaveSlotInfo> ListSlots() const;
	FString GetContinueSlotId() const;

	/**
	 * Registra la sección de un sistema. Save recibe un archivo vacío que el
	 * sistema rellena; Load recibe el de la partida (vacío si no lo trae) y debe
	 * partir del estado por defecto. Los callbacks que capturen UObjects deben
	 * usar punteros débiles; retira la sección al destruir el sistema.
	 */
	bool RegisterSection(const FString& Name, FSaveSectionRegistry::FSaveFunc Save, FSaveSectionRegistry::FLoadFunc Load);
	bool UnregisterSection(const FString& Name);

	/** Migraciones del formato (versión origen → función). */
	FSaveMigrations& GetMigrations() { return Migrations; }

	void SetWorldSeed(int64 InSeed) { WorldSeed = InSeed; }
	int64 GetWorldSeed() const { return WorldSeed; }
	double GetPlayTimeSeconds() const;

	/** Saved/SaveGames/. */
	FString GetSaveDirectory() const;

	DECLARE_MULTICAST_DELEGATE(FOnSaveCompleted);
	FOnSaveCompleted OnSaveCompleted;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSaveFailed, const FString& /*Error*/);
	FOnSaveFailed OnSaveFailed;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLoadCompleted, const FString& /*SlotId*/);
	FOnLoadCompleted OnLoadCompleted;

private:
	void RegisterPlayerSection();
	void SavePlayer(FSaveArchive& Ar);
	void LoadPlayer(const FSaveArchive& Ar);
	/** Coloca al personaje en el estado pendiente; si aún no existe, espera al siguiente mapa. */
	void ApplyPendingPlayer();
	void HandlePostLoadMap(UWorld* LoadedWorld);
	FString GetGameVersion() const;

	FSaveSectionRegistry Sections;
	FSaveMigrations Migrations;

	/** Último estado del jugador leído o escrito (los valores del cuerpo aún no tienen componente). */
	FSavePlayerState PlayerState;
	bool bPlayerPending = false;

	int64 WorldSeed = 0;
	double PlayTimeAtLoad = 0.0;
	double SessionStartSeconds = 0.0;
	double LastAutosaveSeconds = -1.0;

	FDelegateHandle PostLoadMapHandle;
};
