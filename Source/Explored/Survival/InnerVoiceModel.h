#pragma once

#include "CoreMinimal.h"

#include "Survival/SurvivalModel.h"

/**
 * Avisos interiores (biblia 01 §6.0): una frase corta que el personaje piensa al
 * cruzar un umbral del cuerpo o al pasarle algo. Nunca un mensaje de sistema.
 *
 * Los textos ES/EN están en Content/Data/survival_needs.json («innerVoice») y llegan
 * aquí por InnerVoiceData.inl (Tools/DataCheck). Este modelo decide cuándo toca cada
 * uno:
 *
 * - Los avisos de umbral se «enganchan» al entrar en el estado y no se repiten hasta
 *   salir con margen (histéresis), para que comer un bocado con el hambre en 39,9 no
 *   repita la frase.
 * - Los de suceso (quemadura, jadeo al emerger) suenan cada vez que pasan.
 * - Evaluate devuelve como mucho una frase por llamada, la más urgente (el orden del
 *   enum); las demás esperan a la siguiente llamada y se descartan si su estado ya ha
 *   pasado antes de decirse.
 *
 * Red (biblia 08 §2.9): el servidor simula el cuerpo y manda los ESurvivalEvent por RPC
 * fiable al dueño; el dueño evalúa este modelo sobre la réplica de su cuerpo (la
 * histéresis absorbe la cuantización a uint8). Nada nuevo viaja por el cable y los
 * demás jugadores no oyen los pensamientos ajenos.
 */
enum class EInnerVoiceLine : uint8
{
	Drowning,
	Heatstroke,
	Hypothermia,
	ThirstZero,
	HungerZero,
	ContactBurn,
	OutOfBreath,
	WoundBleeding,
	ScurvyBleeding,
	RaySting,
	JellyfishSting,
	Poisoned,
	WoundInfected,
	Fever,
	Sprain,
	SleepZero,
	Hallucinating,
	Soaked,
	ColdLow,
	HeatHigh,
	ThirstLow,
	HungerLow,
	SleepLow,
	ScurvyVision,
	ScurvyGums,
	Monotony,
	Count
};

static_assert(static_cast<int32>(EInnerVoiceLine::Count) <= 32, "FInnerVoiceState guarda un bit por aviso en un uint32");

/** Texto de un aviso (survival_needs.json «innerVoice»). */
struct EXPLORED_API FInnerVoiceText
{
	FName Id;
	FString TextEs;
	FString TextEn;
};

/** Memoria de los avisos del jugador: qué está enganchado y qué espera a decirse. */
struct EXPLORED_API FInnerVoiceState
{
	/** Avisos de umbral ya dichos cuyo estado sigue activo. */
	uint32 Latched = 0;
	/** Avisos que tocan y aún no se han dicho. */
	uint32 Pending = 0;
	/** Si en la última observación del oxígeno estaba bajo el agua (para el jadeo al salir). */
	bool bWasUnderwater = false;

	bool IsLatched(EInnerVoiceLine Line) const { return (Latched & Bit(Line)) != 0; }
	bool IsPending(EInnerVoiceLine Line) const { return (Pending & Bit(Line)) != 0; }

	static constexpr uint32 Bit(EInnerVoiceLine Line) { return 1u << static_cast<uint32>(Line); }
};

struct EXPLORED_API FInnerVoiceModel
{
	// Umbrales (biblia 01 §6). El margen de salida evita repetir la frase al rozar el umbral.
	static constexpr float NeedLowBelow = 40.0f;
	static constexpr float SleepLowBelow = 30.0f;
	static constexpr float NeedRearmMargin = 5.0f;
	static constexpr float ColdBelow = 36.0f;
	static constexpr float HypothermiaBelow = 35.0f;
	static constexpr float HeatAbove = 38.5f;
	static constexpr float HeatstrokeAbove = 39.5f;
	static constexpr float TemperatureRearmMargin = 0.3f;
	static constexpr float SoakedAbove = 0.7f;
	static constexpr float SoakedRearmBelow = 0.5f;
	static constexpr float BleedingAbove = 0.05f;
	static constexpr float MonotonyAtHours = 72.0f;
	static constexpr float MonotonyRearmBelowHours = 48.0f;
	static constexpr float GaspOxygenBelow = 0.35f;
	static constexpr float DrowningRearmOxygen = 0.5f;

	/** Id de datos del aviso («hambre_aprieta»); NAME_None fuera de rango. */
	static FName Id(EInnerVoiceLine Line);

	/** Aviso por su id de datos; false si no existe. */
	static bool FromId(FName Id, EInnerVoiceLine& OutLine);

	/** Texto ES/EN del aviso. */
	static const FInnerVoiceText& Text(EInnerVoiceLine Line);

	/** Si el aviso se dispara por un suceso (ESurvivalEvent o el oxígeno) y no por un umbral del cuerpo. */
	static bool IsEventLine(EInnerVoiceLine Line);

	/** Si el cuerpo está dentro del umbral del aviso (solo avisos de umbral). */
	static bool IsActive(EInnerVoiceLine Line, const FSurvivalState& State);

	/** Si el cuerpo ha salido del umbral con margen y el aviso puede volver a sonar. */
	static bool IsCleared(EInnerVoiceLine Line, const FSurvivalState& State);

	/**
	 * Engancha los avisos de lo que ya está pasando sin decirlos: al cargar una partida o
	 * al reaparecer, el personaje no recita de golpe todo lo que le duele.
	 */
	static void Prime(FInnerVoiceState& Voice, const FSurvivalState& State);

	/** Oxígeno (0–1) de USwimComponent tras un paso: jadeo al emerger y ahogo. */
	static void ObserveBreath(FInnerVoiceState& Voice, float Oxygen01, bool bUnderwater);

	/**
	 * Tras un paso del cuerpo (con los sucesos de ese paso): devuelve true y el aviso más
	 * urgente que toca decir ahora, o false si no hay ninguno.
	 */
	static bool Evaluate(FInnerVoiceState& Voice, const FSurvivalState& State, const TArray<ESurvivalEvent>& Events,
		EInnerVoiceLine& OutLine);
};
