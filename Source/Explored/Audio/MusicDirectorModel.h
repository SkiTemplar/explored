#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"

/**
 * Director de música adaptativa (GDD §14.3) y flauta de bambú diegética (§8.12).
 *
 * Modelo puro: no reproduce nada. A partir del contexto de juego decide qué pieza
 * de la banda sonora debe sonar y emite órdenes para cada capa (empezar, fundir,
 * parar) con el instante exacto en que deben ocurrir, cuantizado al compás o al
 * tiempo de la pieza que suena. La capa de Unreal (UExploredMusicSubsystem) solo
 * ejecuta esas órdenes sobre UAudioComponent.
 *
 * Las piezas y sus tempos no están escritos aquí: vienen de
 * Content/Data/music_layers.json, que genera Tools/Audio (`explored-audio music-layers`).
 */

/** Papel de una pieza en la banda sonora; coincide con "role" de music_layers.json. */
enum class EMusicRole : uint8
{
	Theme,
	Menu,
	Explore,
	Night,
	Tension,
	Storm,
	Sea,
	Discovery,
	Finale,
	Credits,
	Count
};

EXPLORED_API const TCHAR* LexToString(EMusicRole Role);

/** Papel a partir de su clave en el JSON ("explore", "night"...); false si no se reconoce. */
EXPLORED_API bool MusicRoleFromKey(const FString& Key, EMusicRole& OutRole);

/** Arquetipo de isla a partir de su clave en el JSON ("landing", "whitesands"...). */
EXPLORED_API bool IslandArchetypeFromMusicKey(const FString& Key, EIslandArchetype& OutArchetype);

/** Estado musical que elige el director (la tabla del GDD §14.3 más los silencios). */
enum class EMusicMood : uint8
{
	Silence,      // Sin música (antes de tener contexto).
	Menu,         // Menú principal.
	Credits,      // Rodillo de créditos.
	KeyMoment,    // Tema principal en un momento clave (una vez, sin bucle).
	Explore,      // Exploración diurna por isla, con variaciones.
	Night,        // Noche: minimalista, deja sonar a los insectos.
	Sea,          // Navegación a vela en mar abierto.
	Tension,      // Depredador cerca, hipotermia, cuerpo al límite.
	Storm,        // Ciclón o temporal en curso.
	CycloneHush,  // Silencio antes del ciclón (§14.2).
	Finale,       // Partida del «Limón»: tema principal orquestado.
	Count
};

EXPLORED_API const TCHAR* LexToString(EMusicMood Mood);

/** Final de la partida (variante de la pieza de final en el JSON). */
enum class EMusicFinale : uint8
{
	Rescue,
	Voyage,  // Zarpar en el «Limón».
	Stay
};

/** Una pieza de la banda sonora, tal como la describe music_layers.json. */
struct EXPLORED_API FMusicPiece
{
	FName Id;
	/** Ruta del asset (/Game/Generated/Audio/Musica/<id>.<id>). */
	FString Asset;
	EMusicRole Role = EMusicRole::Count;
	/** Isla ("emerald"), número de motivo ("01") o final ("voyage"); vacío si no aplica. */
	FString Variant;
	float Bpm = 80.0f;
	int32 BeatsPerBar = 4;
	/** Compases de la pieza (los motivos de descubrimiento duran menos de uno). */
	float Bars = 0.0f;
	bool bLoop = false;

	double SecondsPerBeat() const;
	double SecondsPerBar() const;
	/** Duración musical (una vuelta si es bucle), sin la cola de reverberación. */
	double Duration() const;
};

/** Afinación de la flauta de bambú (sección "flute" del JSON). */
struct EXPLORED_API FFluteTuning
{
	FName SampleId;
	FString Asset;
	/** Frecuencia de la muestra grabada (la tónica). */
	float SampleHz = 293.6648f;
	/** Semitonos de cada nota sobre la tónica; vacío = pentatónica mayor por defecto. */
	TArray<int32> Semitones;
};

/** Catálogo de la música: lo rellena la capa de Unreal desde el JSON, o un test. */
struct EXPLORED_API FMusicCatalog
{
	TArray<FMusicPiece> Pieces;
	/** Variaciones diurnas de cada arquetipo (índices en Pieces), la propia primero. */
	TArray<int32> DayVariants[static_cast<int32>(EIslandArchetype::Count)];
	FFluteTuning Flute;

	/** Añade una pieza y devuelve su índice. */
	int32 Add(const FMusicPiece& Piece);

	/** Índice de la pieza con ese id, o INDEX_NONE. */
	int32 FindById(const FName& Id) const;

	/** Primera pieza con ese papel (y variante, si no está vacía), o INDEX_NONE. */
	int32 FindByRole(EMusicRole Role, const FString& Variant = FString()) const;

	/** Todas las piezas con un papel, en el orden del catálogo. */
	void PiecesWithRole(EMusicRole Role, TArray<int32>& Out) const;

	/**
	 * Comprueba que hay al menos una pieza para cada papel que usa el director y que
	 * las piezas tienen tempo y compases válidos. Devuelve false con el motivo.
	 */
	bool Validate(FString& OutError) const;
};

/** Lo que el juego sabe en este instante y le importa a la música. */
struct EXPLORED_API FMusicContext
{
	bool bInMenu = false;
	bool bCredits = false;
	/** Partida del «Limón» (o el final elegido) en curso. */
	bool bFinale = false;
	EMusicFinale Finale = EMusicFinale::Voyage;
	/** Isla en la que está el oyente; Count si está lejos de cualquiera. */
	EIslandArchetype Island = EIslandArchetype::Count;
	/** 0 pleno día, 1 noche cerrada. */
	float Night = 0.0f;
	/** Ciclón en curso. */
	bool bCyclone = false;
	/** Horas de juego hasta el próximo ciclón; negativo si no hay ninguno a la vista. */
	float CycloneEtaHours = -1.0f;
	/** Galerna o tormenta eléctrica (0–1). */
	float Storm = 0.0f;
	/** Navegando a vela en mar abierto. */
	bool bSailing = false;
	/** Cuánto está el oyente bajo el agua (0–1). */
	float Underwater = 0.0f;
	/** Peligro (0–1): depredador cerca, hipotermia, cuerpo al límite. */
	float Danger = 0.0f;
};

/** Parámetros de equilibrado del director. Los valores por defecto son los del juego. */
struct EXPLORED_API FMusicDirectorTuning
{
	// Histéresis de la tabla.
	float NightEnter = 0.6f;
	float NightExit = 0.4f;
	float DangerEnter = 0.5f;
	float DangerExit = 0.3f;
	/** Segundos que se mantiene la tensión después de que el peligro baje. */
	float DangerHoldSeconds = 8.0f;
	float StormEnter = 0.65f;
	float StormExit = 0.45f;
	/** Horas de juego antes de un ciclón en que la música calla (silencio antes del ciclón). */
	float CycloneHushHours = 4.0f;

	// Volumen de cada estado antes del bus y del ajuste de Música: por debajo de 1
	// para que la música nunca tape el ambiente.
	float MoodGain[static_cast<int32>(EMusicMood::Count)] = {
		0.0f,   // Silence
		0.9f,   // Menu
		0.9f,   // Credits
		0.8f,   // KeyMoment
		0.55f,  // Explore
		0.45f,  // Night
		0.6f,   // Sea
		0.7f,   // Tension
		0.6f,   // Storm
		0.0f,   // CycloneHush
		1.0f,   // Finale
	};
	/** Segundos de fundido de entrada de cada estado. */
	float MoodFadeIn[static_cast<int32>(EMusicMood::Count)] = {
		0.0f, 2.0f, 2.0f, 3.0f, 6.0f, 8.0f, 5.0f, 1.5f, 8.0f, 0.0f, 2.0f,
	};
	/** Fundido de salida por defecto al cambiar de estado. */
	float FadeOutSeconds = 4.0f;
	/** Fundido de salida cuando algo urgente (tensión, menú, final) toma el relevo. */
	float UrgentFadeOutSeconds = 1.5f;
	/** Fundido largo hacia el silencio antes del ciclón. */
	float HushFadeOutSeconds = 10.0f;

	// Silencios: la música de exploración, noche y mar descansa tras unas vueltas
	// para que se oiga el paisaje sonoro.
	int32 LoopsBeforeRest = 2;
	float RestFadeOutSeconds = 8.0f;
	float RestMinSeconds = 45.0f;
	float RestMaxSeconds = 120.0f;
	/** Calma tras la tensión, el temporal o un momento clave antes de volver a la exploración. */
	float CalmAfterTensionSeconds = 12.0f;
	float CalmAfterStormSeconds = 30.0f;
	float CalmAfterKeyMomentSeconds = 20.0f;

	// Motivos de descubrimiento.
	float StingCooldownSeconds = 45.0f;
	float StingGain = 0.7f;
	/** Multiplicador de la pieza que suena mientras suena el motivo. */
	float StingDuck = 0.45f;
	float StingDuckFadeSeconds = 0.3f;
	float StingRestoreFadeSeconds = 2.5f;

	// Bajo el agua: la música se atenúa y se filtra (el ambiente submarino manda).
	float UnderwaterVolume = 0.35f;
	float UnderwaterLowPassHz = 900.0f;
	float DryLowPassHz = 20000.0f;
	float BusFadeSeconds = 0.4f;
};

/** Qué hace una orden sobre una capa. */
enum class EMusicLayerAction : uint8
{
	/** Empieza la pieza desde el principio en AtSeconds, fundiendo de 0 a Volume. */
	Start,
	/** En AtSeconds lleva el volumen a Volume en FadeSeconds. */
	Fade,
	/** En AtSeconds funde a 0 en FadeSeconds y libera la capa. */
	Stop,
	/** Descarta ya las órdenes aún pendientes de esa capa (si no había empezado, no empezará). */
	Cancel
};

/** Orden del director para una capa (una instancia de una pieza). */
struct EXPLORED_API FMusicLayerTarget
{
	/** Instancia: la misma pieza puede sonar dos veces a la vez durante un fundido. */
	int32 Voice = INDEX_NONE;
	FName PieceId;
	EMusicLayerAction Action = EMusicLayerAction::Fade;
	/** Volumen objetivo (0–1) antes del bus y del ajuste de Música. */
	float Volume = 0.0f;
	float FadeSeconds = 0.0f;
	/** Instante del reloj del director en que se ejecuta (en un límite de compás o tiempo). */
	double AtSeconds = 0.0;
	bool bLoop = false;

	bool operator==(const FMusicLayerTarget& Other) const;
	bool operator!=(const FMusicLayerTarget& Other) const { return !(*this == Other); }
};

/** Volumen y filtro comunes a toda la música (bajo el agua). */
struct EXPLORED_API FMusicBus
{
	float Volume = 1.0f;
	float LowPassHz = 20000.0f;
	float FadeSeconds = 0.4f;
};

/** Capa viva que conoce el director. */
struct EXPLORED_API FMusicVoice
{
	int32 Id = INDEX_NONE;
	int32 Piece = INDEX_NONE;
	EMusicMood Mood = EMusicMood::Silence;
	bool bSting = false;
	double StartAt = 0.0;
	/** Instante en que empieza a fundirse hacia su final; negativo si sigue sonando. */
	double StopAt = -1.0;
	float StopFade = 0.0f;
	/** Volumen nominal (sin agacharse bajo un motivo). */
	float Volume = 0.0f;
	/** Instante en que se descansa (primaria de exploración, noche o mar); negativo si no. */
	double RestAt = -1.0;
	/** Hay una restauración de volumen pendiente tras un motivo. */
	double DuckRestoreAt = -1.0;
};

/**
 * Director de música. Determinista: la misma semilla y la misma secuencia de
 * llamadas producen exactamente las mismas órdenes. El reloj lo pone quien llama
 * (segundos reales, también en pausa) y nunca retrocede.
 */
class EXPLORED_API FMusicDirectorModel
{
public:
	FMusicDirectorModel(const FMusicCatalog& InCatalog, uint32 InSeed, const FMusicDirectorTuning& InTuning = FMusicDirectorTuning());

	/**
	 * La tabla del GDD §14.3 con sus prioridades: menú, créditos, final, ciclón,
	 * silencio antes del ciclón, tensión, temporal, mar abierto, noche y exploración.
	 * Previous da la histéresis (noche, peligro, temporal).
	 */
	static EMusicMood ChooseMood(const FMusicContext& Context, EMusicMood Previous, const FMusicDirectorTuning& Tuning);

	/** Primer múltiplo de Step contado desde Origin que no es anterior a Now. */
	static double NextBoundary(double Origin, double Now, double Step);

	/** Avanza el director y añade a OutTargets las órdenes nuevas. */
	void Update(double Now, const FMusicContext& Context, TArray<FMusicLayerTarget>& OutTargets);

	/**
	 * Descubrimiento de un punto de interés o de un tramo del mapa: motivo corto en el
	 * siguiente tiempo, con la pieza que suena agachada debajo. Devuelve false si se
	 * descarta (enfriamiento, tensión, temporal, menú, final o silencio antes del ciclón).
	 */
	bool NotifyDiscovery(double Now, TArray<FMusicLayerTarget>& OutTargets);

	/** Momento clave: el tema principal suena una vez en la siguiente actualización. */
	void NotifyKeyMoment();

	EMusicMood GetMood() const { return Mood; }
	const FMusicBus& GetBus() const { return Bus; }
	const TArray<FMusicVoice>& GetVoices() const { return Voices; }
	const FMusicCatalog& GetCatalog() const { return Catalog; }
	/** Pieza principal (la del estado actual), o NAME_None si calla. */
	FName GetPrimaryPiece() const;
	/** Última variación diurna elegida. */
	FName GetLastDayPiece() const;
	/** Instante hasta el que se descansa sin música de estado (negativo si no se descansa). */
	double GetRestUntil() const { return RestUntil; }

private:
	FMusicVoice* FindVoice(int32 VoiceId);
	const FMusicVoice* FindVoice(int32 VoiceId) const;
	void RetireFinished(double Now);
	void TransitionTo(EMusicMood NewMood, double Now, TArray<FMusicLayerTarget>& Out);
	void MaintainMood(double Now, TArray<FMusicLayerTarget>& Out);
	void StartPrimary(int32 PieceIndex, double At, TArray<FMusicLayerTarget>& Out);
	void StopVoice(FMusicVoice& Voice, double At, float Fade, TArray<FMusicLayerTarget>& Out);
	int32 ChoosePieceFor(EMusicMood ForMood);
	int32 ChooseDayVariation(EIslandArchetype ForIsland);
	int32 PickAvoiding(const TArray<int32>& Candidates, int32 Avoid, int32 PreferredFirstWeight);
	float Random01();

	FMusicCatalog Catalog;
	FMusicDirectorTuning Tuning;
	uint32 Seed = 0;
	uint32 Draws = 0;

	TArray<FMusicVoice> Voices;
	int32 NextVoiceId = 1;
	int32 PrimaryVoice = INDEX_NONE;

	EMusicMood Mood = EMusicMood::Silence;
	FMusicContext LastContext;
	EIslandArchetype Island = EIslandArchetype::Landing;
	EIslandArchetype PrimaryIsland = EIslandArchetype::Count;
	double LastNow = 0.0;
	double LastDangerAt = -1.0e9;
	double RestUntil = -1.0;
	double LastStingAt = -1.0e9;
	int32 LastStingPiece = INDEX_NONE;
	int32 LastDayPiece = INDEX_NONE;
	bool bKeyMomentRequested = false;
	bool bFinalePlayed = false;
	FMusicBus Bus;
};

/** Una nota de la flauta lista para sonar. */
struct EXPLORED_API FFluteNote
{
	int32 Index = INDEX_NONE;
	int32 Semitones = 0;
	/** Multiplicador de tono sobre la muestra (2^(semitonos/12)). */
	float PitchMultiplier = 1.0f;
	float FrequencyHz = 0.0f;
	FName SampleId;
};

/**
 * Flauta de bambú (GDD §8.12): cinco notas pentatónicas en cinco teclas. Tocar
 * junto al fuego sube el ánimo: MoraleRatePerHour es la tasa que suma el sistema
 * de supervivencia mientras el jugador toca de verdad (varias notas distintas
 * seguidas, no una tecla aporreada).
 */
class EXPLORED_API FFluteModel
{
public:
	static constexpr int32 NumNotes = 5;

	explicit FFluteModel(const FFluteTuning& InTuning);

	/** Nombres de tecla (FKey::GetFName) de cada nota; por defecto One…Five. */
	void SetKeyBindings(const TArray<FName>& InKeys);
	const TArray<FName>& GetKeyBindings() const { return Keys; }

	/** Nota (0–4) para una tecla; INDEX_NONE si la tecla no es de la flauta. */
	int32 NoteForKey(const FName& KeyName) const;

	/** Tono y muestra de una nota (Index fuera de rango se acota). */
	FFluteNote Note(int32 Index) const;

	/** Registra una nota tocada (segundos reales) y la devuelve. */
	FFluteNote Play(double Now, int32 Index);

	/** Está tocando una melodía: al menos MinNotes notas, y dos distintas, en la ventana. */
	bool IsPerforming(double Now) const;

	/** Puntos de ánimo por hora de juego que suma tocar ahora junto a un fuego (FireHeat 0–1). */
	float MoraleRatePerHour(double Now, float FireHeat) const;

	float PerformWindowSeconds = 8.0f;
	int32 MinNotes = 4;
	/** Ánimo por hora junto a una hoguera plena (se suma al que ya da el calor del fuego). */
	float MaxMoralePerHour = 5.0f;

private:
	FFluteTuning Tuning;
	TArray<FName> Keys;
	TArray<double> RecentTimes;
	TArray<int32> RecentNotes;
};
