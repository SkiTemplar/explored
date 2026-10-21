#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"

#include "Narrative/BeaconDirectorModel.h"

#include "BeaconSubsystem.generated.h"

class AExploredMistralLights;
class APawn;
class FSaveArchive;
class UExploredSaveSubsystem;
class UExploredSurvivorComponent;

/**
 * Historia de Baliza Siete en la partida (GDD v3 §2, §3, §5.9 y §5.10): capa fina sobre
 * FBeaconDirectorModel. Cada medio segundo mide el mundo (hora, distancias al faro, a la casa y
 * al carguero, barómetro, mochila, hogueras), aplica el paso del director y reparte el
 * resultado: objetivo del HUD, pensamientos de Iria (subtítulo de 4 s), autoguardado al
 * amanecer y el día 3 a las 20:00, y la llegada del Mistral tras encender la baliza.
 *
 * Secciones de guardado: «progress» (etapa, hitos, ranuras, pensamientos, encendido) y
 * «pages» (páginas leídas). Las páginas (AExploredNotebookPage) y la linterna
 * (ALighthouseLamp) llaman a ReadPage, TryFillSlot y TryLight.
 *
 * Blueprint: «Get Stage», «Get Objective Text», «Read Page», «Get Pages Read».
 * Consola (no Shipping): BalizaStage <etapa>, BalizaGive <id> <n>, BalizaTime <día> <hora>.
 */
UCLASS()
class EXPLORED_API UBeaconSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static UBeaconSubsystem* Get(const UObject* WorldContextObject);

	/** «Nueva partida»: estado inicial, reloj a D1 07:00 y cartel de inicio. */
	void BeginNewRun();

	const FBeaconRunState& GetRunState() const { return Run; }
	EBeaconStage GetStage() const { return Run.Progress.Stage; }

	UFUNCTION(BlueprintPure, Category = "Explored|Baliza")
	int32 GetStageIndex() const { return static_cast<int32>(Run.Progress.Stage); }

	UFUNCTION(BlueprintPure, Category = "Explored|Baliza")
	int32 GetPagesRead() const { return FNotebookModel::ReadCount(Run.Notebook); }

	UFUNCTION(BlueprintPure, Category = "Explored|Baliza")
	bool IsBeaconLit() const { return Run.Progress.bBeaconLit; }

	/** Línea del objetivo actual (objectives.json); vacía en el final. */
	UFUNCTION(BlueprintPure, Category = "Explored|Baliza")
	FText GetObjectiveText() const;

	/** Lee una página (1-8): la marca, enseña el texto y la nota de Iria. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Baliza")
	bool ReadPage(int32 Page);

	/**
	 * Pone en la ranura lo que lleva Iria (la lente en las manos, 2 tarros, la mecha) y lo
	 * gasta de la mochila. Si falta algo, avisa y devuelve false sin gastar nada.
	 */
	bool TryFillSlot(ELanternSlot Slot, UExploredSurvivorComponent& Survivor);

	/** Enciende la baliza (linterna montada) y arranca el final. */
	bool TryLight();

	/** Pensamiento con disparador externo (dormir, balsa). */
	void TriggerThought(EIriaThought Thought);

	// --- Depuración (comandos de consola y pruebas de humo) ---------------------------
	/** Pone la historia en el estado canónico de la etapa (FBeaconDirectorModel::StateForStage). */
	void DebugSetStage(EBeaconStage Stage);

	/** Segundos reales desde que llega el Mistral hasta la bocina y desde la bocina al fundido. */
	UPROPERTY(EditAnywhere, Category = "Explored|Baliza")
	float MistralApproachSeconds = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Baliza")
	float HornToFadeSeconds = 5.0f;

private:
	void RegisterSections();
	UExploredSaveSubsystem* FindSave() const;
	void SaveProgress(FSaveArchive& Ar) const;
	void LoadProgress(const FSaveArchive& Ar);
	void SavePages(FSaveArchive& Ar) const;
	void LoadPages(const FSaveArchive& Ar);

	void CacheLandmarks();
	APawn* GetPlayerPawn() const;
	FBeaconFacts MeasureFacts(const APawn& Pawn) const;
	double GetAbsHours() const;
	void Sample();
	/** Reparte un resultado del director: estado, pensamientos, autoguardado y Mistral. */
	void Apply(const FBeaconStepResult& Result);
	void ShowThought(EIriaThought Thought) const;
	void PlaySound2D(const TCHAR* Path, float Pitch = 1.0f) const;

	void StartMistral();
	/** Cancela el final en curso (temporizadores de bocina y fundido, luces del Mistral, música del final). */
	void ResetEnding();
	/** Al amanecer, si la leña quemada deja la partida sin hoguera posible, el mar deja leña en la Cala Varada. */
	void WashUpDriftwood(const APawn& Pawn);
	void SoundHorn();
	void FinishEnding();

	FBeaconRunState Run;
	FBeaconDirectorTuning Tuning;

	/** Posiciones de island_props.json (cm); cero si no existen. */
	FVector TowerLocation = FVector::ZeroVector;
	FVector HouseLocation = FVector::ZeroVector;
	FVector FreighterLocation = FVector::ZeroVector;
	bool bHasTower = false;
	bool bHasHouse = false;
	bool bHasFreighter = false;

	/** Punto desde el que se mide el primer movimiento. */
	FVector StartLocation = FVector::ZeroVector;
	bool bHasStartLocation = false;

	float SampleTimer = 0.0f;
	/** Hay partida en marcha (nueva o cargada); antes, en el menú, no se mide nada. */
	bool bRunActive = false;
	/** Iria estaba bajo techo en la última muestra (objetivo del ciclón). */
	bool bSheltered = false;
	bool bSectionsRegistered = false;
	bool bEndingStarted = false;
	/** Último día en que se miró la leña de deriva del amanecer (FSupplyCrateModel::DriftwoodAtDawn). */
	int32 LastDriftwoodDay = 0;

	UPROPERTY(Transient)
	TObjectPtr<AExploredMistralLights> Mistral;

	FTimerHandle HornTimer;
	FTimerHandle FadeTimer;
};
