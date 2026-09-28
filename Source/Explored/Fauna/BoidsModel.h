#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

/** Un individuo de un banco o bandada. Centímetros y cm/s en espacio de mundo. */
struct EXPLORED_API FBoidAgent
{
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	/** Identificador estable (sobrevive a RemoveAgent de otros); sirve para escalonar el LOD y la fase de animación. */
	uint32 Id = 0;
};

/** Pesos y límites del modelo. Los pesos son adimensionales; las distancias en cm. */
struct EXPLORED_API FBoidsParams
{
	/** Radio en el que se tienen en cuenta los vecinos (también es el lado de la celda del hash espacial). */
	float NeighborRadiusCm = 300.0f;
	/** Por debajo de esta distancia los vecinos se repelen. */
	float SeparationRadiusCm = 60.0f;
	/** Tope de vecinos para alineación y cohesión (los primeros en el orden determinista del hash); la separación los mira todos. */
	int32 MaxNeighbors = 24;

	float SeparationWeight = 1.6f;
	float AlignmentWeight = 1.0f;
	float CohesionWeight = 0.8f;
	/** Atracción hacia el objetivo del grupo (si lo hay). */
	float TargetWeight = 0.6f;
	/** Huida de las amenazas. */
	float FleeWeight = 3.0f;
	/** Evitar obstáculos (esferas y franjas sin espacio, p. ej. tierra por delante). */
	float AvoidWeight = 3.0f;
	/** Mantenerse dentro de la franja vertical con margen. */
	float BandWeight = 2.0f;
	/** Tendencia a mantener la velocidad de crucero. */
	float CruiseWeight = 0.5f;

	float CruiseSpeedCmS = 100.0f;
	float MinSpeedCmS = 0.0f;
	float MaxSpeedCmS = 250.0f;
	float MaxAccelCmS2 = 400.0f;
	/** Ganancia de la dirección: cuántas veces por segundo se corrige la velocidad deseada (1/s). */
	float SteeringRate = 2.0f;
	/** Escala de la aceleración vertical (los peces y las aves prefieren moverse en horizontal). */
	float VerticalAgility = 0.5f;

	/** Margen dentro de la franja vertical en el que empieza a empujar hacia dentro. */
	float BandMarginCm = 80.0f;
	/** Grosor mínimo de la franja para que un punto sea transitable (menos es «tierra» u orilla sin agua). */
	float MinBandThicknessCm = 40.0f;
	/** Segundos de anticipación para detectar tierra u orilla por delante. */
	float LookAheadSeconds = 1.5f;

	/** Límites horizontales opcionales (caja en XY), con empuje suave y tope duro. */
	bool bUseBounds = false;
	FVector2D BoundsMin = FVector2D(-1.0e7, -1.0e7);
	FVector2D BoundsMax = FVector2D(1.0e7, 1.0e7);
};

/** Franja vertical transitable en un punto: [MinZ, MaxZ]. */
struct EXPLORED_API FBoidBand
{
	double MinZ = -1.0e9;
	double MaxZ = 1.0e9;

	bool IsValid(float MinThickness) const { return MaxZ - MinZ >= MinThickness; }
};

/** Amenaza de la que se huye (jugador, tiburón). */
struct EXPLORED_API FBoidThreat
{
	FVector Position = FVector::ZeroVector;
	float RadiusCm = 500.0f;
	float Weight = 1.0f;
};

/** Obstáculo esférico (roca, casco de la canoa). */
struct EXPLORED_API FBoidObstacle
{
	FVector Center = FVector::ZeroVector;
	float RadiusCm = 100.0f;
};

/** Entorno de un paso: franja vertical, amenazas, obstáculos y objetivo. */
struct EXPLORED_API FBoidsEnvironment
{
	/** Franja vertical en un punto (peces: fondo y superficie; aves: altura mínima y máxima). Sin fijar: sin límites. */
	TFunction<FBoidBand(const FVector&)> Band;
	TArray<FBoidThreat> Threats;
	TArray<FBoidObstacle> Obstacles;
	bool bHasTarget = false;
	FVector Target = FVector::ZeroVector;
	/** Multiplicadores temporales de los pesos (estado del grupo: disperso, reagrupándose…). */
	float CohesionScale = 1.0f;
	float SeparationScale = 1.0f;
	float AlignmentScale = 1.0f;
	float TargetScale = 1.0f;
};

/**
 * Boids genéricos (Reynolds): separación, alineación, cohesión, objetivo,
 * huida, obstáculos, franja vertical, límites y tope de velocidad y
 * aceleración. Los vecinos se buscan con un hash espacial de celdas de lado
 * NeighborRadiusCm (coste casi lineal: 300 agentes son baratos).
 *
 * Determinista: el paso es secuencial, calcula todas las aceleraciones sobre
 * la misma foto del grupo y luego integra, así que el resultado no depende
 * del orden de actualización ni de hilos. Garantías tras cada paso:
 *  - |v| ≤ MaxSpeedCmS y, si MinSpeedCmS > 0, |v| ≥ MinSpeedCmS (aves: siempre en vuelo);
 *  - Z dentro de la franja del punto (los peces nunca rompen la superficie ni
 *    atraviesan el fondo; las aves nunca bajan de su altura mínima);
 *  - nunca se entra en un punto sin franja válida (tierra para los peces).
 */
class EXPLORED_API FBoidsModel
{
public:
	FBoidsModel() = default;
	explicit FBoidsModel(const FBoidsParams& InParams) : Params(InParams) {}

	void SetParams(const FBoidsParams& InParams) { Params = InParams; }
	const FBoidsParams& GetParams() const { return Params; }

	/** Añade un agente (se le asigna un Id nuevo). Devuelve su índice. */
	int32 AddAgent(const FVector& Position, const FVector& Velocity);
	void RemoveAgent(int32 Index);
	void Reset() { Agents.Reset(); }

	/** Avanza DeltaSeconds (se subdivide en pasos de como mucho MaxSubstep). */
	void Step(float DeltaSeconds, const FBoidsEnvironment& Environment);

	const TArray<FBoidAgent>& GetAgents() const { return Agents; }
	int32 Num() const { return Agents.Num(); }

	FVector Centroid() const;
	FVector AverageVelocity() const;
	/** Distancia media al centroide (cm): mide lo disperso que está el grupo. */
	float Spread() const;
	/** Polarización en [0, 1]: módulo de la media de las direcciones (1 = todos en la misma dirección). */
	float Polarization() const;

	/** Índices de los vecinos de un agente dentro de RadiusCm (fuerza bruta; para tests y consultas puntuales). */
	void QueryNeighbors(const FVector& Point, float RadiusCm, TArray<int32>& OutIndices) const;

	/** Paso máximo de integración (s). */
	static constexpr float MaxSubstep = 1.0f / 20.0f;
	/**
	 * Tope de subpasos por Step (2 s de simulación). Un Step más largo (una pausa, un tirón)
	 * pierde el resto: la bandada no lo nota y no se hacen miles de pasadas de vecinos.
	 */
	static constexpr int32 MaxSubstepsPerStep = 40;

private:
	void SubStep(float Dt, const FBoidsEnvironment& Environment);
	FBoidBand BandAt(const FVector& P, const FBoidsEnvironment& Environment) const;

	FBoidsParams Params;
	TArray<FBoidAgent> Agents;
	uint32 NextId = 1;

	// Memoria reutilizada entre pasos (sin reservas en caliente).
	TArray<FVector> Accelerations;
	TArray<int32> SortedIndices;
	TArray<int64> CellKeys;
	TArray<int64> TableKeys;
	TArray<int32> TableStart;
	TArray<int32> TableCount;
};
