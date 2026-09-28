#pragma once

#include "CoreMinimal.h"
#include "Fauna/FaunaTypes.h"

/** Estados de las criaturas marinas individuales (los bancos y bandadas van en FaunaGroups.h). */
enum class EMarineState : uint8
{
	Wander,     // patrulla o deambula por su zona
	Buried,     // raya enterrada en la arena
	Flee,       // raya o tortuga se alejan del jugador
	Settle,     // raya frenando para volver a enterrarse
	Drift,      // medusa a la deriva
	Curious,    // tiburón de arrecife se acerca a mirar
	Circle,     // tiburón de arrecife da vueltas alrededor del jugador
	Stalk,      // tiburón tigre acecha en círculos cada vez más cerrados
	Attack,     // embestida
	Retreat,    // se aleja tras morder o perder el interés
	Accompany,  // delfines junto a la canoa
	Jump,       // delfín en el aire
	Breathe,    // tortuga sube a respirar
	Nesting,    // tortuga hacia la playa (gancho para el desove de P-EVENTS)
	Travel,     // ballena en ruta
	Sounding,   // ballena sumergida, fuera de la vista
};

EXPLORED_API const TCHAR* LexToString(EMarineState State);

struct EXPLORED_API FMarineBrainConfig
{
	EFaunaSpecies Species = EFaunaSpecies::ReefShark;
	FVector SpawnCm = FVector::ZeroVector;
	/** Centro de su zona (arrecife, laguna, talud o mar abierto). */
	FVector HomeCm = FVector::ZeroVector;
	float HomeRadiusCm = 3000.0f;
	uint32 Seed = 1;
	/** Rumbo de viaje de la ballena. */
	FVector2D TravelDirection = FVector2D(1.0, 0.0);
	/** Lado de la canoa por el que nada el delfín (+1 estribor, −1 babor) y separación extra. */
	float EscortSide = 1.0f;
	float EscortOffsetCm = 0.0f;
};

/** Lo que ha pasado en un paso y que la capa de UE convierte en daño, sonido o efectos. */
struct EXPLORED_API FMarineBrainEvents
{
	bool bBite = false;
	float BiteDamage = 0.0f;
	bool bSting = false;
	float StingDamage = 0.0f;
	/** El delfín sale del agua (chapoteo y sonido). */
	bool bJumped = false;
	/** Soplido de la ballena (visible y audible de lejos). */
	bool bBlow = false;
	/** La tortuga ha llegado a la orilla: el evento de desove toma el relevo. */
	bool bNestingArrived = false;
	/** Empieza a huir (raya que se levanta de la arena, tortuga asustada). */
	bool bStartled = false;
};

/**
 * Cerebro de una criatura marina (GDD §10): máquina de estados por especie,
 * percepción (vista en cono, oído, olfato con la corriente) y ciclo diario.
 * Movimiento cinemático en cm; la capa de UE solo copia posición y rumbo.
 *
 * Garantías tras cada paso: nunca entra en agua más somera que la mínima de
 * su especie (el tiburón tigre solo en aguas profundas, la tortuga solo al
 * desovar llega a la orilla), nunca atraviesa el fondo y solo el delfín al
 * saltar sale por encima de la superficie. La ballena visible nunca está más
 * cerca del jugador que WhaleMinDistanceCm.
 */
class EXPLORED_API FMarineCreatureBrain
{
public:
	FMarineCreatureBrain() = default;
	FMarineCreatureBrain(const FMarineBrainConfig& InConfig, const FFaunaWorldQuery& World) { Init(InConfig, World); }

	void Init(const FMarineBrainConfig& InConfig, const FFaunaWorldQuery& World);
	FMarineBrainEvents Tick(float DeltaSeconds, const FFaunaStimuli& Stimuli, const FFaunaWorldQuery& World);

	EFaunaSpecies GetSpecies() const { return Config.Species; }
	EMarineState GetState() const { return State; }
	float GetStateSeconds() const { return StateSeconds; }
	const FVector& GetPosition() const { return Position; }
	const FVector& GetVelocity() const { return Velocity; }
	/** Dirección horizontal a la que mira (unitaria). */
	FVector GetForward() const;
	bool IsVisible() const { return bVisible; }

	/** ¿Pica en ese punto? Medusa siempre; raya solo enterrada (se pisa). */
	bool IsStingHazard(const FVector& PointCm) const;
	float GetStingRadiusCm() const;

	/** Gancho de P-EVENTS: la tortuga nada hacia la playa (luna llena) y avisa al llegar con bNestingArrived. */
	void BeginNesting(const FVector& BeachCm);
	void CancelNesting();

	/** ¿Es agua de tiburón tigre (mar abierto profundo, el límite natural del mundo)? */
	static bool IsTigerSharkWater(const FFaunaWorldQuery& World, const FVector2D& PointCm);

	/**
	 * Tirada de ataque del tiburón de arrecife en un encuentro (determinista por
	 * semilla y número de encuentro): rara sin sangre, más probable con olor a
	 * sangre y nunca en modo Explorador.
	 */
	static bool ReefSharkAttackRoll(uint32 Seed, int32 Encounter, float Smell, bool bPeaceful);

	/** Paso máximo de simulación (s): las embestidas y los saltos no atraviesan nada. */
	static constexpr float MaxStepSeconds = 0.05f;
	/**
	 * Tope de pasos por Tick (2 s). Un Tick más largo (una pausa, un tirón) pierde el resto
	 * en vez de hacer miles de pasos.
	 */
	static constexpr int32 MaxStepsPerTick = 40;

	// Ajustes de diseño (cm, s y puntos de daño).
	static constexpr float RayStingRadiusCm = 70.0f;
	static constexpr float RayFleeDistanceCm = 1500.0f;
	static constexpr float RayShuffleSenseCm = 700.0f;
	static constexpr float JellyStingRadiusCm = 150.0f;
	static constexpr float ReefSharkCircleRadiusCm = 450.0f;
	static constexpr float ReefSharkCircleSeconds = 15.0f;
	static constexpr float ReefSharkBaseAttackChance = 0.03f;
	static constexpr float ReefSharkBloodAttackChance = 0.25f;
	static constexpr float TigerSharkStalkSeconds = 12.0f;
	static constexpr float DolphinEscortRangeCm = 4000.0f;
	static constexpr float DolphinMinBoatSpeedCmS = 150.0f;
	static constexpr float TurtleShyRadiusCm = 400.0f;
	static constexpr float WhaleMinDistanceCm = 15000.0f;
	static constexpr float WhaleAvoidRadiusCm = 22000.0f;

private:
	void TickStingray(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out);
	void TickJellyfish(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out);
	void TickReefShark(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out);
	void TickTigerShark(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out);
	void TickDolphin(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out);
	void TickTurtle(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out);
	void TickWhale(float Dt, const FFaunaStimuli& S, const FFaunaWorldQuery& W, FMarineBrainEvents& Out);

	void SetState(EMarineState NewState);
	float NextRandom();
	float RandomRange(float Min, float Max) { return Min + (Max - Min) * NextRandom(); }

	/** ¿Detecta al jugador por vista u oído? (solo si está en el agua o en una embarcación). */
	bool SensesPlayer(const FFaunaStimuli& S, float SightRangeCm, float HalfAngleDeg, float HearingRangeCm) const;
	/** Velocidad deseada hacia un punto de deambular dentro de su zona. */
	FVector WanderVelocity(float Dt, const FFaunaWorldQuery& W, float Speed, float DepthFraction);
	void SteerTo(const FVector& DesiredVelocity, float MaxAccelCmS2, float Dt);
	/** Integra con las restricciones de profundidad, fondo y superficie. */
	void Move(float Dt, const FFaunaWorldQuery& W, float MinDepthCm, bool bAllowAir = false);
	/** Mantiene Z dentro del agua en su posición actual. */
	void ClampToWater(const FFaunaWorldQuery& W, bool bAllowAir);

	FMarineBrainConfig Config;
	EMarineState State = EMarineState::Wander;
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector2D Heading = FVector2D(1.0, 0.0);
	bool bVisible = true;
	float StateSeconds = 0.0f;
	float Cooldown = 0.0f;
	float Timer = 0.0f;
	float EventTimer = 0.0f;
	float StingCooldown = 0.0f;
	int32 Encounters = 0;
	bool bAttackDecided = false;
	float CircleAngle = 0.0f;
	float CircleSign = 1.0f;
	float NotEscortingSeconds = 0.0f;
	FVector WanderGoal = FVector::ZeroVector;
	float WanderGoalSeconds = 0.0f;
	bool bHasWanderGoal = false;
	FVector NestTarget = FVector::ZeroVector;
	bool bNestingRequested = false;
	bool bNestingDone = false;
	FVector2D TravelDirection = FVector2D(1.0, 0.0);
	double LocalTime = 0.0;
	uint32 RandomCounter = 0;
};
