#pragma once

#include "CoreMinimal.h"

/**
 * Reglas puras de los efectos de sonido de Baliza Siete (GDD v3 §10): superficie de los pasos,
 * cadencia de pasos y remadas, y cuándo suena una gaviota. Las ejecuta UExploredSfxSubsystem.
 */
enum class EFootSurface : uint8
{
	Sand,
	Grass,
	Rock,
	Wood,
	Water,
	Count
};

EXPLORED_API const TCHAR* LexToString(EFootSurface Surface);

/** Lo que se sabe del suelo bajo los pies. */
struct EXPLORED_API FFootGround
{
	/** El suelo es el terreno de la isla (si no, es una estructura: suelo, muelle, balsa). */
	bool bTerrain = true;
	/** Altura del pie sobre el nivel del mar (m). */
	float HeightM = 2.0f;
	/** Pendiente del suelo (grados). */
	float SlopeDeg = 0.0f;
	/** Profundidad del agua sobre el pie (m); 0 en seco. */
	float WaterDepthM = 0.0f;
};

namespace ExploredSfxRules
{
	/** Agua hasta la espinilla: chapoteo. */
	constexpr float WadeDepthM = 0.05f;
	/** Por debajo de esta altura el terreno es playa (Tools/Island: SAND_TOP). */
	constexpr float SandTopM = 1.5f;
	/** Pendiente a partir de la cual el terreno es roca (Tools/Island: ROCK_SLOPE). */
	constexpr float RockSlopeDeg = 35.0f;
	/** Zancada (cm) andando y corriendo. */
	constexpr float WalkStrideCm = 140.0f;
	constexpr float RunStrideCm = 190.0f;
	/** Velocidad (cm/s) a partir de la cual se corre. */
	constexpr float RunSpeedCmS = 450.0f;
	/** Por debajo de esta velocidad no hay pasos. */
	constexpr float MinStepSpeedCmS = 40.0f;
	/** Una remada cada tantos segundos remando a tope (más espaciadas con menos fuerza). */
	constexpr float PaddlePeriodS = 1.15f;
	/** Gaviotas: entre dos gritos (s), solo de día, cerca de la costa y sin lluvia fuerte. */
	constexpr float GullMinGapS = 9.0f;
	constexpr float GullMaxGapS = 24.0f;

	EXPLORED_API EFootSurface SurfaceFor(const FFootGround& Ground);

	/** Nombre del sonido de un paso: sfx_footstep_<superficie>_0N (N = 1-4). */
	EXPLORED_API FString FootstepSound(EFootSurface Surface, int32 Variant);

	/** Zancada según la velocidad horizontal. */
	EXPLORED_API float StrideFor(float SpeedCmS);

	/** ¿Hay gaviotas? Coast y Night 0-1 (FAmbienceEnvironment), Rain 0-1. */
	EXPLORED_API bool GullsAudible(float Coast, float Night, float Rain);

	/** Espera hasta la próxima gaviota con un azar 0-1. */
	EXPLORED_API float GullGap(float Random01);
}

/** Acumula distancia (o tiempo) y avisa cada vez que se completa un paso (o una remada). */
struct EXPLORED_API FSfxCadence
{
	float Accumulated = 0.0f;

	/** Suma Amount; true si se completa un periodo de Period (y se resta). Periodos no válidos no avisan. */
	bool Advance(float Amount, float Period);

	void Reset() { Accumulated = 0.0f; }
};
