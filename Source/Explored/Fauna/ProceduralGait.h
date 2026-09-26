#pragma once

#include "CoreMinimal.h"

/** Tipo de locomoción (coincide con «locomotion.type» de animals.json). */
enum class ELocomotion : uint8
{
	Quadruped,      // jabalí, perra
	Octopod,        // cangrejos (patas alternas en grupos)
	BipedClimber,   // mono
	Reptile,        // iguana (columna ondulante)
	Bird,
	Swimmer,        // la ondulación la hace el material
};

/** Pose de una pata en un instante: giros en grados respecto al reposo. */
struct EXPLORED_API FLegPose
{
	float UpperPitch = 0.0f;  // adelante (+) / atrás (−)
	float LowerPitch = 0.0f;  // flexión de la rodilla (siempre ≥ 0 en la fase de vuelo)
	float Lift = 0.0f;        // 0 apoyada, 1 en lo alto del paso
};

/** Pose del cuerpo completo para el animador. */
struct EXPLORED_API FBodyPose
{
	float BodyBob = 0.0f;      // cm arriba/abajo
	float BodyRoll = 0.0f;     // grados
	float HeadPitch = 0.0f;
	float TailYaw = 0.0f;
	float SpineYaw = 0.0f;     // reptiles
	float WingFlap = 0.0f;     // grados (aves)
	float WingFold = 0.0f;     // 0 abiertas, 1 plegadas
};

/**
 * Marchas procedurales. Funciones puras de (fase, velocidad): el animador
 * acumula la fase según la distancia recorrida para que los pies no patinen.
 */
struct EXPLORED_API FProceduralGait
{
	/** Desfase de cada pata (0–1) según la marcha; índice por pata en orden FL, FR, BL, BR, … */
	static float LegPhaseOffset(ELocomotion Type, int32 LegIndex, float SpeedRatio);

	/** Pose de una pata con fase global Phase (0–1 por ciclo) y velocidad relativa (0 parado, 1 máxima). */
	static FLegPose Leg(ELocomotion Type, int32 LegIndex, float Phase, float SpeedRatio);

	/** Pose del cuerpo: balanceo, cola, respiración en reposo. Time en segundos para lo que no depende del paso. */
	static FBodyPose Body(ELocomotion Type, float Phase, float SpeedRatio, float Time);

	/** Avance de fase por centímetro recorrido, a partir de la zancada. */
	static float PhasePerCm(float StrideCm) { return StrideCm > 1.0f ? 1.0f / StrideCm : 0.0f; }
};
