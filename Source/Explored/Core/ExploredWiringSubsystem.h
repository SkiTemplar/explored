#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Subsystems/WorldSubsystem.h"

#include "Core/SystemLinks.h"
#include "Events/WorldEventsModel.h"
#include "Fauna/FaunaTypes.h"
#include "Save/SaveArchive.h"
#include "Survival/SurvivalModel.h"

#include "ExploredWiringSubsystem.generated.h"

class AExploredCharacter;
class AExploredFaunaManager;
class UExploredSaveSubsystem;

/**
 * Conexión entre sistemas (paquete P-WIRE). Cada sistema expone ganchos que no
 * conocen a los demás; este subsistema del mundo los enlaza sin meter lógica: las
 * reglas están en ExploredLinks (Core/SystemLinks.h, probadas en el host).
 *
 * - Guardado (ExploredWiringSave.cpp): registra las secciones de lo que vive en el
 *   personaje o en actores sueltos: «time», «inventory», «body», «fishing» y
 *   «cooking». La historia («progress» y «pages») es de UBeaconSubsystem y las
 *   pistas («tutorial») de UExploredTutorialSubsystem. Las secciones de v1 que ya
 *   no existen («boats», «world», «wiring») se conservan sin leer en las partidas
 *   antiguas (FSaveSectionRegistry guarda las desconocidas).
 * - Juego: calor de los fuegos y carga → cuerpo; muerte → modo de juego; paso de
 *   ballenas → fauna; daño de fauna → heridas y picaduras; balsa y pescado → fauna
 *   (delfines y gaviotas).
 */
UCLASS()
class EXPLORED_API UExploredWiringSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static UExploredWiringSubsystem* Get(const UObject* WorldContextObject);

	/** «Nueva partida» (AExploredPlayerController::StartNewGame): sin secciones pendientes de la anterior. */
	void BeginNewRun();

	/** Cada cuánto (s reales) se revisa la posición del jugador y los enlaces. */
	UPROPERTY(EditAnywhere, Category = "Explored|Enlaces")
	float SampleIntervalSeconds = 0.25f;

	/** Distancia (cm) a la que un fuego calienta al jugador. */
	UPROPERTY(EditAnywhere, Category = "Explored|Enlaces")
	float FireSearchRadiusCm = 1500.0f;

private:
	// --- Guardado (ExploredWiringSave.cpp) ---------------------------------------
	UExploredSaveSubsystem* FindSaveSubsystem() const;
	void RegisterSections(UExploredSaveSubsystem& Save);
	void UnregisterSections(UExploredSaveSubsystem& Save);
	void SaveInventory(FSaveArchive& Ar) const;
	void LoadInventory(const FSaveArchive& Ar);
	void SaveBody(FSaveArchive& Ar) const;
	void LoadBody(const FSaveArchive& Ar);
	void SaveFishing(FSaveArchive& Ar) const;
	void LoadFishing(const FSaveArchive& Ar);
	void SaveFires(FSaveArchive& Ar) const;
	void LoadFires(const FSaveArchive& Ar);
	void SaveClock(FSaveArchive& Ar) const;
	void LoadClock(const FSaveArchive& Ar);
	/** Aplica las secciones del personaje que llegaron antes que él. */
	void ApplyPendingPawnSections();

	// --- Enlaces ----------------------------------------------------------------
	AExploredCharacter* GetPlayerCharacter() const;
	void BindPawn(AExploredCharacter* Character);
	void UnbindPawn();
	void BindFaunaManager();
	void Sample();
	void UpdateBody(AExploredCharacter& Character);
	void UpdateFaunaAboard(AExploredCharacter& Character);

	void HandleSurvivalEvent(ESurvivalEvent Event);
	void HandleEventStarted(const FWorldEvent& Event);
	void HandleEventEnded(const FWorldEvent& Event);
	void HandleFaunaDamage(EFaunaSpecies Species, float Damage, const FVector& LocationCm);

	/** Secciones del personaje que llegaron antes de que existiera. */
	TOptional<FSaveArchive> PendingInventory;
	TOptional<FSaveArchive> PendingBody;

	TWeakObjectPtr<AExploredCharacter> BoundPawn;
	TWeakObjectPtr<AExploredFaunaManager> BoundFauna;
	FDelegateHandle BodyEventHandle;
	FDelegateHandle FaunaDamageHandle;
	FDelegateHandle EventStartedHandle;
	FDelegateHandle EventEndedHandle;

	float SampleTimer = 0.0f;
	bool bSectionsRegistered = false;
};
