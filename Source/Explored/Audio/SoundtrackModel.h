#pragma once

#include "CoreMinimal.h"

/**
 * Director musical de Baliza Siete (GDD v3 §10): cinco estados de juego (calma de día,
 * noche, ciclón, ojo casi en silencio y final) más el menú y los créditos.
 *
 * Modelo puro: no reproduce nada. Con el contexto de juego elige el estado y devuelve
 * órdenes (empezar o parar una pieza con su fundido) que ejecuta UExploredMusicSubsystem.
 * Las piezas son grabaciones (Content/Data/soundtrack.json): no hay compases que
 * cuantizar, solo fundidos y silencios entre piezas para que se oiga el paisaje sonoro.
 * Determinista: la misma semilla y las mismas llamadas dan las mismas órdenes.
 */

enum class ESoundtrackState : uint8
{
	Silence,  // Sin contexto todavía.
	Menu,
	Day,      // Calma de día.
	Night,
	Cyclone,  // Muros del ciclón: tensa, sin percusión.
	Eye,      // Ojo del ciclón: casi silencio.
	Finale,   // La baliza encendida: tema de resolución.
	Credits,
	Count
};

EXPLORED_API const TCHAR* LexToString(ESoundtrackState State);

/** Estado a partir de su clave en soundtrack.json («day», «eye»...); false si no se reconoce. */
EXPLORED_API bool SoundtrackStateFromKey(const FString& Key, ESoundtrackState& OutState);

struct EXPLORED_API FSoundtrackPiece
{
	FName Id;
	/** Ruta del asset (/Game/Audio/Musica/<id>.<id>). */
	FString Asset;
	float Seconds = 0.0f;
};

struct EXPLORED_API FSoundtrackStateConfig
{
	/** Índices en FSoundtrackCatalog::Pieces, en orden de reproducción. */
	TArray<int32> Pieces;
	float Gain = 0.5f;
	float FadeIn = 4.0f;
	float FadeOut = 4.0f;
	/** Silencio entre dos piezas del mismo estado (s). */
	float RestMin = 0.0f;
	float RestMax = 0.0f;
	/** La pieza se repite sin silencio (menú, final y créditos). */
	bool bLoop = false;
};

struct EXPLORED_API FSoundtrackCatalog
{
	TArray<FSoundtrackPiece> Pieces;
	FSoundtrackStateConfig States[static_cast<int32>(ESoundtrackState::Count)];

	int32 FindById(const FName& Id) const;
	const FSoundtrackStateConfig& Config(ESoundtrackState State) const { return States[static_cast<int32>(State)]; }
	FSoundtrackStateConfig& Config(ESoundtrackState State) { return States[static_cast<int32>(State)]; }

	/** Cada estado salvo Silence tiene al menos una pieza válida y valores en rango; false con el motivo. */
	bool Validate(FString& OutError) const;
};

/** Lo que el juego sabe y le importa a la música. */
struct EXPLORED_API FSoundtrackContext
{
	bool bInMenu = false;
	bool bCredits = false;
	/** La baliza encendida: desde el encendido hasta el epílogo. */
	bool bFinale = false;
	/** 0 pleno día, 1 noche cerrada. */
	float Night = 0.0f;
	bool bCycloneWall = false;
	bool bCycloneEye = false;
};

enum class ESoundtrackAction : uint8
{
	Start,
	Stop
};

struct EXPLORED_API FSoundtrackCommand
{
	int32 Voice = INDEX_NONE;
	FName PieceId;
	ESoundtrackAction Action = ESoundtrackAction::Start;
	/** Volumen final (0-1) antes del ajuste de Música. */
	float Volume = 0.0f;
	float FadeSeconds = 0.0f;
	bool bLoop = false;
};

class EXPLORED_API FSoundtrackModel
{
public:
	/** Histéresis de la noche (evita ir y venir al amanecer y al anochecer). */
	static constexpr float NightEnter = 0.6f;
	static constexpr float NightExit = 0.4f;

	FSoundtrackModel(const FSoundtrackCatalog& InCatalog, uint32 InSeed);

	/** Prioridad: menú, créditos, final, ojo, ciclón, noche y día. */
	static ESoundtrackState ChooseState(const FSoundtrackContext& Context, ESoundtrackState Previous);

	/** Avanza a Now (segundos reales, nunca hacia atrás) y añade las órdenes nuevas. */
	void Update(double Now, const FSoundtrackContext& Context, TArray<FSoundtrackCommand>& Out);

	ESoundtrackState GetState() const { return State; }
	/** Pieza que suena ahora (NAME_None en un silencio). */
	FName GetCurrentPiece() const;
	/** Instante en que empezará la próxima pieza del estado (negativo si ya suena una). */
	double GetNextStartAt() const { return CurrentVoice == INDEX_NONE ? NextStartAt : -1.0; }

private:
	void StopCurrent(float FadeSeconds, TArray<FSoundtrackCommand>& Out);
	void StartNext(double Now, TArray<FSoundtrackCommand>& Out);
	float Random01();

	FSoundtrackCatalog Catalog;
	uint32 Seed = 0;
	uint32 Draws = 0;
	ESoundtrackState State = ESoundtrackState::Silence;
	int32 CurrentVoice = INDEX_NONE;
	int32 CurrentPiece = INDEX_NONE;
	int32 NextVoice = 1;
	double PieceEndsAt = -1.0;
	double NextStartAt = 0.0;
	/** Siguiente posición de la lista de cada estado (rotación). */
	int32 Cursor[static_cast<int32>(ESoundtrackState::Count)] = {};
};
