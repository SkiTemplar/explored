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

/** Lo que el juego mide cada medio segundo para decidir qué pistas pedir. */
struct EXPLORED_API FTutorialFacts
{
	/** Segundos jugando (sin menú ni pausa) desde la partida nueva o la carga. */
	float PlayingSeconds = 0.0f;
	/** Iria mira algo con lo que se puede interactuar. */
	bool bHasFocus = false;
	/** Barra de sed (0-100, 100 = bien). */
	float Thirst = 100.0f;
	/** Lleva algo que se bebe (coco). */
	bool bCarriesDrink = false;
	/** Fibras de palma en la mochila. */
	int32 Fibres = 0;
	/** Es hora de dormir (y no hay ciclón). */
	bool bSleepHour = false;
	int32 PagesRead = 0;
	bool bHasBarometer = false;
	bool bOnRaft = false;
};

/** Umbrales de los disparadores (GDD v3 §4). */
struct EXPLORED_API FTutorialTriggerTuning
{
	/** Espera tras empezar a jugar antes de la pista de mover. */
	float MoveDelaySeconds = 2.0f;
	/** Sed a la que se enseña a beber aunque no se lleve nada (justo antes del aviso «Tengo sed», a 40). */
	float DrinkThirst = 45.0f;
	/** Fibras con las que ya se puede trenzar (receta «cuerda»). */
	int32 CraftFibres = 2;
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

	/**
	 * Pistas cuyo disparador se cumple con estos hechos, en el orden del enum (sin mirar el estado).
	 * Ninguna antes de MoveDelaySeconds jugando, para que la de mover salga la primera.
	 */
	static TArray<ETutorialHint> Triggered(const FTutorialFacts& Facts, const FTutorialTriggerTuning& Tuning = FTutorialTriggerTuning());

	/** Pide todas las pistas disparadas que aún no salieron ni esperan. */
	static FTutorialHintState RequestTriggered(const FTutorialHintState& State, const FTutorialFacts& Facts,
		const FTutorialTriggerTuning& Tuning = FTutorialTriggerTuning());

	/** Estado de una partida guardada antes de que existieran las pistas: todas mostradas. */
	static FTutorialHintState AllShownState();

	/** Id estable de la pista («tut_mover», «tut_remar»…), clave del texto; NAME_None si no es válida. */
	static FName TextId(ETutorialHint Hint);

	static FTutorialHintRecord ToRecord(const FTutorialHintState& State);

	/**
	 * Reconstruye el estado. Rechaza (bOk falso, estado vacío) un registro con
	 * ids fuera de rango, repetidos en la cola o encolados ya mostrados.
	 */
	static FTutorialHintResult FromRecord(const FTutorialHintRecord& Record);
};
