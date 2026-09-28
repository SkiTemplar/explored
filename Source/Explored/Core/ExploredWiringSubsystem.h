#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Subsystems/WorldSubsystem.h"

#include "Core/ExploredRandom.h"
#include "Core/SystemLinks.h"
#include "Events/WorldEventsModel.h"
#include "Fauna/FaunaTypes.h"
#include "Narrative/ExploredProgress.h"
#include "Ruins/RuinsModel.h"
#include "Save/SaveArchive.h"
#include "Save/SaveWorldDeltas.h"
#include "Survival/SurvivalModel.h"
#include "UI/ExploredGameplayMode.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/FellingModel.h"
#include "WorldGen/HarvestModel.h"
#include "WorldGen/VegetationHarvestState.h"

#include "ExploredWiringSubsystem.generated.h"

class AExploredBoat;
class AExploredCharacter;
class AExploredFaunaManager;
class AExploredVegetationCell;
class UExploredSaveSubsystem;
class UHierarchicalInstancedStaticMeshComponent;

/**
 * Conexión entre sistemas (paquete P-WIRE). Cada sistema expone ganchos que no
 * conocen a los demás; este subsistema del mundo los enlaza sin meter lógica: las
 * reglas están en ExploredLinks (Core/SystemLinks.h, probadas en el host).
 *
 * - Guardado: registra las secciones de lo que vive en el personaje o en actores
 *   sueltos («inventory», «body», «cartography», «fishing», «cooking», «boats»,
 *   «time», «progress», «world» y «wiring»). Construcción, huerto, ruinas, eventos
 *   y logros registran la suya en su propio subsistema.
 * - Estadísticas de logros que dependen de dónde está el jugador: islas pisadas,
 *   lugares, profundidad de buceo, días vividos, metros a vela, eventos
 *   presenciados, técnicas, tesoros expuestos, mapa, ciclón superado intacto,
 *   melodías junto al fuego, isla oculta.
 * - Juego: calor de los fuegos, carga y música → cuerpo; muerte → modo de juego;
 *   brújula y bolsa estanca → mapa; hoguera de señal → barco del horizonte;
 *   erupción → obsidiana; paso de ballenas y desove → fauna; daño de fauna →
 *   heridas y picaduras; peligro, navegación y descubrimientos → música;
 *   miradores → bocetos del mapa; barca → delfines y pesca desde la barca;
 *   ciclón → daño de las embarcaciones.
 *
 * Sin compilar en la nube: verificar en local (Tools/build.ps1, Tools/test.ps1).
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

	// --- Recolección de vegetación y rocas (P-HARVEST) --------------------------
	// Lo llama AExploredVegetationCell::GetContextVerbs_Implementation / CanInteract_Implementation /
	// Interact_Implementation con la instancia exacta que fijó SetInteractionFocus.

	void GetHarvestVerbs(const AExploredVegetationCell& Cell, UHierarchicalInstancedStaticMeshComponent* Component, int32 InstanceIndex, TArray<FText>& OutVerbs) const;
	bool CanHarvestInstance(const AExploredVegetationCell& Cell, UHierarchicalInstancedStaticMeshComponent* Component, int32 InstanceIndex) const;
	void HarvestInstance(AExploredVegetationCell& Cell, UHierarchicalInstancedStaticMeshComponent* Component, int32 InstanceIndex, AActor* Instigator);

	/** Progreso de exploración de la partida (petroglifos, miradores, piezas del Albatros...). */
	FExploredProgress& GetProgress() { return Progress; }
	const FExploredProgress& GetProgress() const { return Progress; }

	/**
	 * Deltas del mundo procedural (sección «world»). Aún no hay recolección del
	 * scatter: cuando exista, la capa «harvested» se rellena aquí con
	 * WorldDeltas.Layer("harvested").Add(Celda, Índice).
	 */
	FSaveWorldDeltas& GetWorldDeltas() { return WorldDeltas; }

	/** Cada cuánto (s reales) se revisa la posición del jugador y los enlaces. */
	UPROPERTY(EditAnywhere, Category = "Explored|Enlaces")
	float SampleIntervalSeconds = 0.25f;

	/** Distancia (cm) a la que un fuego calienta al jugador. */
	UPROPERTY(EditAnywhere, Category = "Explored|Enlaces")
	float FireSearchRadiusCm = 1500.0f;

	/** Distancia (m) a un mirador para que se dibuje su boceto. */
	UPROPERTY(EditAnywhere, Category = "Explored|Enlaces")
	float ViewpointRadiusMeters = 20.0f;

	/** Distancia (m) al centro de la isla oculta para darla por alcanzada. */
	UPROPERTY(EditAnywhere, Category = "Explored|Enlaces")
	float HiddenIslandRadiusMeters = 400.0f;

private:
	// --- Guardado --------------------------------------------------------------
	void RegisterSections(UExploredSaveSubsystem& Save);
	void UnregisterSections(UExploredSaveSubsystem& Save);
	void SaveInventory(FSaveArchive& Ar) const;
	void LoadInventory(const FSaveArchive& Ar);
	void SaveBody(FSaveArchive& Ar) const;
	void LoadBody(const FSaveArchive& Ar);
	void SaveCartography(FSaveArchive& Ar) const;
	void LoadCartography(const FSaveArchive& Ar);
	void SaveFishing(FSaveArchive& Ar) const;
	void LoadFishing(const FSaveArchive& Ar);
	void SaveFires(FSaveArchive& Ar) const;
	void LoadFires(const FSaveArchive& Ar);
	void SaveBoats(FSaveArchive& Ar) const;
	void LoadBoats(const FSaveArchive& Ar);
	void SaveClock(FSaveArchive& Ar) const;
	void LoadClock(const FSaveArchive& Ar);
	void SaveProgress(FSaveArchive& Ar) const;
	void LoadProgress(const FSaveArchive& Ar);
	void SaveWorld(FSaveArchive& Ar) const;
	void LoadWorld(const FSaveArchive& Ar);
	void SaveWiring(FSaveArchive& Ar) const;
	void LoadWiring(const FSaveArchive& Ar);
	/** Aplica las secciones del personaje que llegaron antes que él. */
	void ApplyPendingPawnSections();

	// --- Enlaces ----------------------------------------------------------------
	AExploredCharacter* GetPlayerCharacter() const;
	void BindPawn(AExploredCharacter* Character);
	void UnbindPawn();
	void BindFaunaManager();
	void Sample(float DeltaSeconds);
	void UpdatePlace(AExploredCharacter& Character, const FVector2D& PositionMeters);
	void UpdateBody(AExploredCharacter& Character, float GameHours);
	void UpdateBoat(AExploredCharacter& Character, const FVector2D& PositionMeters);
	void UpdateDanger(AExploredCharacter& Character);
	void UpdateWorldEvents(const FVector2D& PositionMeters);
	void UpdateStorms(float GameHours);
	void SpawnItemsAround(const FVector& Center, FName ItemId, int32 Count, float RadiusCm) const;
	void NotifyDiscovery(bool bMorale = true) const;
	float GetTotalDays() const;
	/** Reloj del juego en minutos enteros (tala, tocón y rebrote: FVegetationStateModel). */
	int64 GetTotalMinutes() const;

	/**
	 * Arranque de partida nueva en Landing (GDD): un cuchillo y una cantimplora de
	 * salvamento junto al PlayerStart, con cocos, palos y piedras sueltos alrededor
	 * para poder fabricar, encender fuego y beber en los primeros minutos. Una sola
	 * vez por partida (bStarterKitSpawned, sección «wiring»): «Continuar» no repite
	 * el reparto. Sin malla de restos del avión propia (no hay Content en este
	 * cambio): el salvamento son los objetos, no un decorado.
	 */
	void SpawnLandingStarterKitIfNeeded();

	// --- Recolección de vegetación y rocas --------------------------------------
	/** Una vez por carga: oculta en todas las celdas del mapa lo que ya venía talado en la partida. */
	void ApplyVegetationDeltasIfNeeded();
	/** Oculta en esta celda las instancias que ya venían taladas en la partida (una sola vez por celda y carga). */
	void EnsureVegetationDeltasApplied(AExploredVegetationCell& Cell);
	void HideVegetationInstance(UHierarchicalInstancedStaticMeshComponent& Component, int32 InstanceIndex, FVegetationRuntimeState& OutState) const;
	/** Tocones: escala del brote y rebrote cuando toca (biblia 02 §1.2). */
	void TickVegetationRegrowth();
	/** Minutos de juego hasta que la especie vuelve a ser talable (0 = no rebrota) y, en OutSprout, hasta que asoma el brote. */
	int64 RegrowMinutesFor(FName Species, const FHarvestSpeciesRule& Rule, int64& OutSproutMinutes) const;
	/** Una instancia rebrotada sale de los deltas y del reloj de tala. */
	void ForgetFelledInstance(const FIntPoint& Cell, FName Component, int32 Index);
	/** Copia VegetationClock en WorldDeltas para el próximo guardado. */
	void SyncVegetationClock();
	/** true si Instigator lleva RequiredTag en una mano (NAME_None = no hace falta ninguna herramienta). */
	bool HasHarvestTool(const AActor* Instigator, FName RequiredTag) const;

	void HandleSurvivalEvent(ESurvivalEvent Event);
	void HandleRuinDiscovery(FName ElementId, const FRuinDiscovery& Result);
	void HandleMuseumChanged();
	void HandleEventStarted(const FWorldEvent& Event);
	void HandleEventEnded(const FWorldEvent& Event);
	void HandleFaunaDamage(EFaunaSpecies Species, float Damage, const FVector& LocationCm);
	void HandleRunStarted(EExploredGameplayMode Mode);

	UFUNCTION()
	void HandleIslandCharted(int32 IslandIndex);

	UFUNCTION()
	void HandleFirstCoastDrawn();

	UPROPERTY(Transient)
	FExploredProgress Progress;

	FSaveWorldDeltas WorldDeltas;
	FArchipelagoLayout Layout;

	// --- Recolección de vegetación y rocas --------------------------------------
	TArray<FHarvestSpeciesRule> HarvestRules = FHarvestModel::DefaultRules();
	/** Perfiles de tala: días de brote y rebrote de las especies leñosas y del arbusto. */
	TArray<FFellingProfile> FellingProfiles = FFellingModel::DefaultProfiles();
	/** Hora de tala de cada instancia que espera rebrote (sección «vegetationClock» del guardado). */
	FVegetationClock VegetationClock;
	TMap<FVegetationInstanceKey, FVegetationRuntimeState> VegetationRuntime;
	/** Celdas cuyos deltas guardados ya se aplicaron esta sesión (ver EnsureVegetationDeltasApplied). */
	TSet<FIntPoint> VegetationDeltasAppliedCells;
	/** false tras cada LoadWorld: el próximo Sample recorre todas las celdas (ApplyVegetationDeltasIfNeeded). */
	bool bVegetationDeltasApplied = false;
	/** Semilla propia para las tiradas de recolección; se realinea con WorldDeltas.Seed al cargar. */
	FExploredRandom HarvestRandom = FExploredRandom(0x9E3779B97F4A7C15ULL);

	/** Secciones del personaje que llegaron antes de que existiera. */
	TOptional<FSaveArchive> PendingInventory;
	TOptional<FSaveArchive> PendingBody;
	TOptional<FSaveArchive> PendingCartography;

	TWeakObjectPtr<AExploredCharacter> BoundPawn;
	TWeakObjectPtr<AExploredFaunaManager> BoundFauna;
	FDelegateHandle BodyEventHandle;
	FDelegateHandle FaunaDamageHandle;
	FDelegateHandle RuinDiscoveryHandle;
	FDelegateHandle MuseumChangedHandle;
	FDelegateHandle EventStartedHandle;
	FDelegateHandle EventEndedHandle;
	FDelegateHandle RunStartedHandle;

	ExploredLinks::FSailingOdometer Odometer;
	ExploredLinks::FCycloneWatch CycloneWatch;
	ExploredLinks::FFluteMelodyWatch MelodyWatch;
	/** Eventos ya presenciados en esta sesión (por id), para no informar en cada muestra. */
	TSet<uint64> WitnessedEvents;
	/** Día total en que empezó la partida (para «days_survived»); < 0 = aún no se sabe. */
	float RunStartDays = -1.0f;
	float LastSampleDays = -1.0f;
	bool bStarterKitSpawned = false;
	float MaxDiveReportedM = 0.0f;
	int32 LastDaysReported = -1;
	float SampleTimer = 0.0f;
	bool bSectionsRegistered = false;
};
