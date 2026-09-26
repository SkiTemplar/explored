#pragma once

#include "CoreMinimal.h"
#include "Fauna/FaunaTypes.h"

/**
 * Parámetros de animación procedural para el shader de vértices o las piezas
 * rígidas (GDD §10 y §12: sin esqueletos). Se escriben tal cual en los datos
 * por instancia (ISM) o de primitiva (actores sueltos), índices 0–3; ver
 * docs/tecnico/fauna.md para los nombres de parámetros del material.
 */
struct EXPLORED_API FFaunaAnimParams
{
	/** [0] Fase del ciclo en [0, 1): onda de la columna, aleteo o pulsación. */
	float Phase01 = 0.0f;
	/** [1] Amplitud: fracción de la longitud del cuerpo (onda), grados de ala (aleteo) o contracción de la campana (pulso). */
	float Amplitude = 0.0f;
	/** [2] Frecuencia en Hz (informativa para el shader: ondas por segundo). */
	float FrequencyHz = 0.0f;
	/** [3] Secundario: planeo 0–1 en aves, eje vertical (1) en mamíferos, contracción instantánea 0–1 en medusas. */
	float Secondary = 0.0f;
	/** Alas plegadas (0 abiertas): en esta fauna siempre 0, las aves nunca se posan. */
	float WingFold = 0.0f;
};

/** Estado que acumula la fase para que no salte al cambiar la frecuencia. */
struct EXPLORED_API FFaunaAnimState
{
	float Phase01 = 0.0f;
};

struct EXPLORED_API FFaunaAnimation
{
	/**
	 * Avanza la fase y devuelve los parámetros del instante. SpeedCmS es la
	 * velocidad del animal; VerticalSpeedCmS < 0 (bajando) hace planear a las
	 * aves. bResting (raya enterrada, ballena sumergida) deja el cuerpo quieto.
	 */
	static FFaunaAnimParams Advance(EFaunaSpecies Species, FFaunaAnimState& State, float SpeedCmS,
		float VerticalSpeedCmS, float DeltaSeconds, bool bResting = false);

	/** Fase inicial repartida por semilla (para que un banco no nade al unísono). */
	static float InitialPhase(uint32 Seed);
};
