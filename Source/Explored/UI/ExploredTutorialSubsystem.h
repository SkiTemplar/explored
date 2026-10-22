#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "UI/TutorialHintModel.h"

#include "ExploredTutorialSubsystem.generated.h"

class AExploredCharacter;
class AExploredHUD;
class FSaveArchive;
class UExploredSaveSubsystem;

/**
 * Pistas de enseñanza en la partida (GDD v3 §4): cada medio segundo mide lo que hace Iria
 * (FTutorialFacts), pide las pistas cuyo disparador se cumple (FTutorialHintModel) y saca la
 * siguiente como aviso del HUD cuando la anterior ya se fue. Solo con el juego en marcha (sin
 * menú, pausa, escena del final ni desmayo). Cada pista sale una vez por partida.
 *
 * Sección de guardado «tutorial» (máscara de mostradas y cola). Una partida guardada sin la
 * sección es anterior a las pistas: se dan todas por vistas.
 *
 * Blueprint: «Get Hints Shown».
 */
UCLASS()
class EXPLORED_API UExploredTutorialSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static UExploredTutorialSubsystem* Get(const UObject* WorldContextObject);

	/** «Nueva partida»: ninguna pista vista. */
	void BeginNewRun();

	UFUNCTION(BlueprintPure, Category = "Explored|Pistas")
	int32 GetHintsShown() const { return FTutorialHintModel::ShownCount(State); }

	/** Segundos en pantalla de cada pista. */
	UPROPERTY(EditAnywhere, Category = "Explored|Pistas")
	float HintSeconds = 8.0f;

	/** Pausa entre una pista y la siguiente. */
	UPROPERTY(EditAnywhere, Category = "Explored|Pistas")
	float GapSeconds = 1.5f;

private:
	void RegisterSections();
	UExploredSaveSubsystem* FindSave() const;
	void SaveTutorial(FSaveArchive& Ar) const;
	void LoadTutorial(const FSaveArchive& Ar);

	/** true si el jugador está jugando de verdad (ni menú, ni pausa, ni final, ni desmayo). */
	bool IsPlaying(const AExploredCharacter* Character) const;
	FTutorialFacts MeasureFacts(const AExploredCharacter& Character) const;
	void ShowNext(AExploredHUD& HUD);
	FText HintText(ETutorialHint Hint, const AExploredHUD& HUD) const;

	FTutorialHintState State;
	FTutorialTriggerTuning Tuning;
	float PlayingSeconds = 0.0f;
	float SampleTimer = 0.0f;
	/** Tiempo hasta que se puede enseñar la siguiente pista. */
	float BusySeconds = 0.0f;
	bool bSectionsRegistered = false;
};
