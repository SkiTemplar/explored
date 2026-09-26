#pragma once

#include "CoreMinimal.h"

#include "Core/ExploredRandom.h"

/** Sedal de la caña (biblia §3.5: de fibra o de nailon; el legendario lo da El Rey de Plata). */
enum class EFishingLine : uint8
{
	Fibra,
	Nailon,
	Legendario,
	Count
};

/** Los seis anzuelos de la biblia §3.5, de peor a mejor agarre. */
enum class EFishHook : uint8
{
	Espina,
	Concha,
	MaderaDura,
	Hueso,
	Alambre,
	Legendario,
	Count
};

/**
 * Aparejo con el que se pesca. La calidad de la caña amortigua los tirones
 * (la tensión sube más despacio) y aguanta algo más; el sedal y el anzuelo
 * fijan la carga de rotura: manda el más débil de los dos.
 */
struct EXPLORED_API FFishingTackle
{
	/** 0 = caña improvisada de rama, 1 = caña con carrete de madera bien hecha. */
	float RodQuality01 = 0.5f;
	EFishingLine Line = EFishingLine::Fibra;
	EFishHook Hook = EFishHook::Hueso;

	/** Carga (kgf) que aguanta el sedal, ya con la ayuda de la caña. */
	float LineBreakKgf() const;

	/** Carga (kgf) a la que el anzuelo se abre o se parte. */
	float HookHoldKgf() const;

	/** Carga de rotura efectiva: la menor de sedal y anzuelo. */
	float BreakKgf() const;

	/** El nailon no se deshilacha contra la roca; la fibra, sí. */
	bool ResistsAbrasion() const { return Line != EFishingLine::Fibra; }

	/** Segundos que el anzuelo aguanta con el sedal flojo antes de que el pez se suelte. */
	float SlackGraceSeconds() const;

	/** Velocidad (1/s) a la que la tensión sigue al tirón: más baja con mejor caña. */
	float ShockRate() const;

	/** Metros por segundo que recoge el carrete a fondo. */
	float ReelSpeedMps() const;
};

/** Resultado de la pelea con la caña (GDD §8.9). */
enum class EFishFightOutcome : uint8
{
	InProgress,
	Caught,
	Escaped,
	LineSnapped,
};

/** Lo que hace el pez en cada momento de la pelea. */
enum class EFishFightPhase : uint8
{
	Resting,   // Nada suave y se deja acercar.
	Burst,     // Arrancada: tira fuerte y se lleva sedal.
	Tired,     // Sin fuerzas: se deja traer.
};

/** Parámetros de una pelea concreta: el pez que ha picado y el aparejo. */
struct EXPLORED_API FFishFightParams
{
	/** Tirón típico del pez en una arrancada (kgf). */
	float StrengthKgf = 3.0f;
	/** Aguante del pez: segundos de arrancada a tensión media antes de rendirse. */
	float StaminaSeconds = 10.0f;
	/** 0–1: con qué frecuencia arranca (los depredadores de mar abierto, más). */
	float Aggression = 0.5f;
	/** Distancia inicial del pez (m): la del lance. */
	float StartDistanceM = 15.0f;
	/** Sedal en el carrete: si el pez se lo lleva todo, se parte. */
	float SpoolLengthM = 70.0f;
	/** Tras este tiempo el anzuelo termina saliéndose. */
	float MaxSeconds = 240.0f;

	float BreakKgf = 7.0f;
	float SlackGraceSeconds = 1.2f;
	float ShockRate = 5.5f;
	float ReelSpeedMps = 1.2f;

	/**
	 * Fracción de la carga de rotura que se pierde por segundo de arrancada
	 * cuando el pez roza el sedal contra la roca (El Viejo en su cueva). Solo
	 * afecta a los sedales que no resisten la abrasión.
	 */
	float AbrasionPerSecond = 0.0f;

	/**
	 * Rellena los parámetros del aparejo (rotura, holgura, amortiguación,
	 * carrete). RockAbrasionPerSecond es la del pez; se anula si el sedal la resiste.
	 */
	void ApplyTackle(const FFishingTackle& Tackle, float RockAbrasionPerSecond = 0.0f);
};

/** Estado observable de la pelea (para el HUD y los tests). */
struct EXPLORED_API FFishFightState
{
	EFishFightOutcome Outcome = EFishFightOutcome::InProgress;
	EFishFightPhase Phase = EFishFightPhase::Resting;
	/** Tensión del sedal como fracción de la carga de rotura: 1 = se parte. */
	float Tension01 = 0.3f;
	float DistanceM = 15.0f;
	/** Aguante restante del pez (segundos equivalentes). */
	float Stamina = 10.0f;
	float PullKgf = 0.0f;
	/** Segundos seguidos (con memoria) con el sedal flojo. */
	float SlackSeconds = 0.0f;
	float ElapsedSeconds = 0.0f;
	float PhaseTimeLeft = 1.0f;
	float BurstMultiplier = 1.0f;
};

/**
 * Minijuego de tensión de la caña (GDD §8.9, biblia §4.3). Máquina de estados
 * pura y determinista (semilla): el pez alterna descansos y arrancadas; el
 * jugador recoge (entrada > 0), aguanta (0) o suelta sedal (< 0).
 *
 * - Si la tensión llega a la carga de rotura, el sedal se parte.
 * - Si el sedal va flojo demasiado tiempo, el pez escupe el anzuelo.
 * - Si el pez se lleva todo el sedal del carrete, se parte.
 * - Cansado el pez y a menos de 1,5 m, se saca.
 *
 * Es justo por diseño: recoger cuando el pez descansa y ceder en las
 * arrancadas saca un pez típico casi siempre; recoger sin parar contra un
 * pez fuerte parte el sedal en la primera arrancada.
 */
class EXPLORED_API FFishFight
{
public:
	static constexpr float LandDistanceM = 1.5f;
	static constexpr float SlackTension01 = 0.06f;
	/** Fuerza extra (fracción de la rotura) que añade recoger a fondo. */
	static constexpr float ReelEffort01 = 0.45f;
	/** Paso interno máximo: el resultado apenas depende de los FPS. */
	static constexpr float MaxStepSeconds = 1.0f / 60.0f;

	FFishFight(const FFishFightParams& InParams, uint32 Seed);

	/** Avanza la pelea. ReelInput en [-1, 1]. Devuelve el resultado (InProgress mientras dure). */
	EFishFightOutcome Tick(float DeltaSeconds, float ReelInput);

	const FFishFightState& GetState() const { return State; }
	const FFishFightParams& GetParams() const { return Params; }
	bool IsFinished() const { return State.Outcome != EFishFightOutcome::InProgress; }

	/** Tensión en kgf (para mostrarla o para el desgaste de la caña). */
	float GetTensionKgf() const { return State.Tension01 * Params.BreakKgf; }

private:
	void Step(float Dt, float ReelInput);
	void StartRest();
	void StartBurst();

	FFishFightParams Params;
	FFishFightState State;
	FExploredRandom Rng;
};
