#pragma once

#include "CoreMinimal.h"

/**
 * Pistas de enseñanza de Baliza Siete (GDD v3 §4): 8 mensajes contextuales de
 * una línea que salen una sola vez y nunca bloquean. El juego pide una pista
 * al darse su disparador; si ya salió o ya espera en la cola, la petición se
 * ignora. El HUD saca de una en una con PopNext.
 *
 * Funciones puras: devuelven un estado nuevo. El estado se guarda como un
 * registro plano (máscara de mostradas y cola en orden).
 */

enum class ETutorialHint : uint8
{
	Move,
	Pickup,
	Drink,
	Craft,
	Sleep,
	Notebook,
	Barometer,
	Paddle,
	Count
};

struct EXPLORED_API FTutorialHintState
{
	/** Bit i = ETutorialHint i ya mostrada. */
	uint8 ShownMask = 0;
	/** Pendientes, en orden de llegada, sin repetir y sin ninguna ya mostrada. */
	TArray<ETutorialHint> Queue;

	bool operator==(const FTutorialHintState& Other) const { return ShownMask == Other.ShownMask && Queue == Other.Queue; }
};

struct EXPLORED_API FTutorialHintRecord
{
	uint8 ShownMask = 0;
	TArray<uint8> Queue;
};

struct EXPLORED_API FTutorialHintResult
{
	bool bOk = false;
	FTutorialHintState State;
	/** En PopNext: la pista que hay que mostrar. */
	ETutorialHint Hint = ETutorialHint::Count;
};

struct EXPLORED_API FTutorialHintModel
{
	static constexpr int32 HintCount = static_cast<int32>(ETutorialHint::Count);

	static bool IsValid(ETutorialHint Hint);
	static bool WasShown(const FTutorialHintState& State, ETutorialHint Hint);
	static bool IsQueued(const FTutorialHintState& State, ETutorialHint Hint);
	static int32 ShownCount(const FTutorialHintState& State);
	static bool AllShown(const FTutorialHintState& State);

	/** Encola una pista; falla si no es válida, ya se mostró o ya está en la cola. */
	static FTutorialHintResult Request(const FTutorialHintState& State, ETutorialHint Hint);

	/** Saca la primera pista de la cola y la marca como mostrada; falla con la cola vacía. */
	static FTutorialHintResult PopNext(const FTutorialHintState& State);

	/** Id del texto en story_es («tut_mover», «tut_remar»…); NAME_None si no es válida. */
	static FName TextId(ETutorialHint Hint);

	static FTutorialHintRecord ToRecord(const FTutorialHintState& State);

	/**
	 * Reconstruye el estado. Rechaza (bOk falso, estado vacío) un registro con
	 * ids fuera de rango, repetidos en la cola o encolados ya mostrados.
	 */
	static FTutorialHintResult FromRecord(const FTutorialHintRecord& Record);
};
