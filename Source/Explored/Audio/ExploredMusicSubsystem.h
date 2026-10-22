#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Audio/SoundtrackModel.h"

#include "ExploredMusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Música de Baliza Siete (GDD v3 §10). Reúne el contexto (menú, créditos, final, hora y
 * reloj de tormenta), se lo pasa a FSoundtrackModel y ejecuta sus órdenes con un
 * UAudioComponent por pieza (fundidos nativos FadeIn/FadeOut). Las piezas llevan la
 * SoundClass SC_Music (Tools/Unreal/import_audio.py): el volumen de Música de los ajustes
 * se aplica por la clase.
 *
 * Avisan desde fuera: UBeaconSubsystem (SetFinale al encender la baliza) y
 * AExploredPlayerController (SetCredits con los créditos en pantalla).
 * Las piezas se leen de Content/Data/soundtrack.json.
 *
 * Blueprint: «Set Credits», «Set Finale», «Get Music State Name».
 */
UCLASS()
class EXPLORED_API UExploredMusicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	/** La música sigue sonando (y fundiéndose) con el juego en pausa. */
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Créditos en pantalla. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Música")
	void SetCredits(bool bInCredits) { bCredits = bInCredits; }

	/** La baliza encendida: tema de resolución hasta el epílogo. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Música")
	void SetFinale(bool bActive) { bFinale = bActive; }

	/**
	 * Baja la música a Gain (0-1) durante HoldSeconds reales y luego la devuelve a su volumen: deja sitio
	 * a un sonido que tiene que oírse por encima (la bocina del Mistral) sin subirlo hasta recortar.
	 */
	void Duck(float Gain, float HoldSeconds);

	/** Estado musical actual («day», «eye»...), para depuración y el bot. */
	UFUNCTION(BlueprintPure, Category = "Explored|Música")
	FString GetMusicStateName() const { return Model ? LexToString(Model->GetState()) : TEXT("silence"); }

	ESoundtrackState GetState() const { return Model ? Model->GetState() : ESoundtrackState::Silence; }

	/** Carga el catálogo desde el texto de soundtrack.json. */
	static bool ParseSoundtrack(const FString& Json, FSoundtrackCatalog& OutCatalog, FString& OutError);

private:
	FSoundtrackContext GatherContext() const;
	void Execute(const FSoundtrackCommand& Command);
	USoundBase* GetSound(const FName& PieceId);

	TUniquePtr<FSoundtrackModel> Model;
	FSoundtrackCatalog Catalog;

	/** Sonidos cargados por id de pieza. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundBase>> Sounds;

	/** Componente de cada voz viva (las que se funden hacia su final también). */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UAudioComponent>> Voices;

	/** Reloj del modelo en segundos reales (avanza también en pausa). */
	double Clock = 0.0;
	float ContextTimer = 0.0f;
	FSoundtrackContext Context;
	bool bCredits = false;
	bool bFinale = false;
	/** Segundos que quedan de atenuación (Duck); 0 = sin atenuar. */
	float DuckRemaining = 0.0f;

	void ApplyDuck(float Gain, float FadeSeconds);
};
