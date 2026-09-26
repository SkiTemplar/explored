#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Audio/MusicDirectorModel.h"

#include "ExploredMusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Música adaptativa en juego (GDD §14.3). Reúne el contexto de los sistemas que ya
 * existen (menú, hora, clima y ciclones, isla y agua del oyente), se lo pasa a
 * FMusicDirectorModel y ejecuta sus órdenes con un grupo de UAudioComponent: cada
 * capa arranca, se funde o se para en el instante cuantizado que devuelve el
 * modelo. Aplica el volumen de Música de los ajustes.
 *
 * Otros sistemas avisan con NotifyDiscovery() (punto de interés, tramo del mapa),
 * NotifyKeyMoment(), SetDanger() (fauna, cuerpo), SetSailing() (barcos) y
 * SetFinale() (partida del «Limón»).
 *
 * Las piezas se leen de Content/Data/music_layers.json (lo genera Tools/Audio).
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

	/** Descubrimiento de un punto de interés o de un tramo del mapa: motivo corto si procede. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Música")
	void NotifyDiscovery();

	/** Momento clave de la historia: suena el tema principal una vez. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Música")
	void NotifyKeyMoment();

	/**
	 * Peligro (0–1) de una fuente (depredador, hipotermia...). Se usa el máximo de las
	 * fuentes vivas; cada fuente debe refrescarlo mientras dure, porque caduca en DangerTimeoutSeconds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|Música")
	void SetDanger(float Danger01, FName Source = NAME_None);

	/** Navegación a vela en mar abierto (lo pondrá el sistema de barcos). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Música")
	void SetSailing(bool bInSailing) { bSailing = bInSailing; }

	/** Créditos en pantalla. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Música")
	void SetCredits(bool bInCredits) { bCredits = bInCredits; }

	/** Partida final (por defecto el «Limón» zarpando). */
	void SetFinale(bool bActive, EMusicFinale Finale = EMusicFinale::Voyage);

	/** Flauta de bambú: toca la nota 0–4 y devuelve false si no hay muestra cargada. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Música")
	bool PlayFluteNote(int32 NoteIndex);

	/** Nota de flauta asociada a una tecla (INDEX_NONE si no es de la flauta). */
	int32 FluteNoteForKey(const FName& KeyName) const { return Flute ? Flute->NoteForKey(KeyName) : INDEX_NONE; }

	/** Ánimo por hora de juego que suma tocar ahora junto a un fuego (FireHeat 0–1), para la supervivencia. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Música")
	float GetFluteMoraleRatePerHour(float FireHeat) const;

	/** Estado musical actual (depuración). */
	EMusicMood GetMood() const { return Director ? Director->GetMood() : EMusicMood::Silence; }

	/** Carga el catálogo desde el texto de music_layers.json. */
	static bool ParseMusicLayers(const FString& Json, FMusicCatalog& OutCatalog, FString& OutError);

	/** Segundos sin refresco tras los que una fuente de peligro deja de contar. */
	static constexpr float DangerTimeoutSeconds = 2.0f;

private:
	/** Una capa viva: un componente de audio con su fundido en curso en el reloj del director. */
	struct FLayer
	{
		int32 Voice = INDEX_NONE;
		TWeakObjectPtr<UAudioComponent> Component;
		bool bStarted = false;
		bool bLoop = false;
		bool bStopAfterFade = false;
		float FromVolume = 0.0f;
		float ToVolume = 0.0f;
		double FadeStart = 0.0;
		float FadeSeconds = 0.0f;
		float Volume = 0.0f;
	};

	/** Danger por fuente, con el instante del último refresco. */
	struct FDangerSource
	{
		float Value = 0.0f;
		double Time = 0.0;
	};

	FMusicContext GatherContext();
	void Enqueue(const TArray<FMusicLayerTarget>& Targets);
	void Execute(const FMusicLayerTarget& Target);
	void UpdateLayers(float DeltaTime);
	USoundBase* GetSound(const FName& PieceId);
	UAudioComponent* AcquireComponent(USoundBase* Sound);
	float GetSettingsVolume(bool bMusic) const;

	TUniquePtr<FMusicDirectorModel> Director;
	TUniquePtr<FFluteModel> Flute;
	FMusicCatalog Catalog;

	/** Sonidos cargados por id de pieza (y la muestra de flauta). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundBase>> Sounds;

	/** Grupo de componentes reutilizables (se crean según hacen falta). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Pool;

	TArray<FLayer> Layers;
	TArray<FMusicLayerTarget> Pending;
	TMap<FName, FDangerSource> Danger;

	/** Reloj del director en segundos reales (avanza también en pausa). */
	double Clock = 0.0;
	float ContextTimer = 0.0f;
	float CycloneTimer = 0.0f;
	float CycloneEtaHours = -1.0f;
	FMusicContext Context;
	bool bSailing = false;
	bool bCredits = false;
	bool bFinale = false;
	EMusicFinale FinaleKind = EMusicFinale::Voyage;
	float BusVolume = 1.0f;
	float BusLowPassHz = 20000.0f;
	double LastFluteNoteTime = -1.0;
};
