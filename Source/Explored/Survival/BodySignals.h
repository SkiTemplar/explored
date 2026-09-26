#pragma once

#include "CoreMinimal.h"

#include "Survival/SurvivalModel.h"

/**
 * «El cuerpo como HUD» (GDD §8.3): el estado no se enseña con iconos sino con
 * sensaciones. Este modelo puro traduce FSurvivalState en señales perceptivas
 * que la capa de Unreal lleva a la cámara, el postproceso, las manos y el audio
 * (UBodySignalsComponent), más la lectura del reloj de pulsera del Albatros.
 *
 * Todas las señales van de 0 a 1 salvo que se diga otra cosa, y son monótonas
 * en su causa (más sed nunca emborrona menos). El suavizado se hace aparte
 * (Smooth) para que Evaluate sea una función sin memoria fácil de probar.
 */

/** Causa dominante del borde de pantalla; decide el tinte de la viñeta. */
enum class EBodyTintCause : uint8
{
	None,
	Cold,        // azul
	Heat,        // ámbar (calor o fiebre)
	Thirst,      // amarillo pálido
	Hunger,      // pardo apagado
	Bleeding,    // rojo
	Pain,        // granate
	Poison,      // verde enfermizo
	Exhaustion,  // negro
	Scurvy,      // gris violáceo
	Count
};

/** Contexto instantáneo que no está en FSurvivalState (lo aporta la capa de juego). */
struct EXPLORED_API FBodySignalContext
{
	/** Energía a corto plazo sobre su máximo (1 = descansado; 0 = sin aliento). */
	float EnergyRatio = 1.0f;
	/** Oxígeno en apnea (1 = pulmones llenos). */
	float Oxygen01 = 1.0f;
	/** Esfuerzo físico de este instante (correr, nadar fuerte, trepar): 0–1. */
	float Exertion = 0.0f;
};

/** Señales perceptivas del cuerpo para UI, cámara y audio. */
struct EXPLORED_API FBodySignals
{
	float Breathing = 0.0f;        // ritmo de respiración (0 calma – 1 jadeo)
	float Heartbeat = 0.0f;        // intensidad del latido audible
	float Shivering = 0.0f;        // temblor de cámara por frío o escalofríos
	float StomachGrowl = 0.0f;     // probabilidad por minuto real de que suene el estómago
	float Vignette = 0.0f;         // intensidad del borde de pantalla
	FLinearColor VignetteTint = FLinearColor::Black;
	EBodyTintCause TintCause = EBodyTintCause::None;
	float Blur = 0.0f;             // visión borrosa (sed, intoxicación)
	float HandTremor = 0.0f;       // temblor de las manos en primer plano
	float Desaturation = 0.0f;     // pérdida de color (escorbuto, poca salud)
	float BleedingPulse = 0.0f;    // pulso rojo del borde al ritmo del latido
	float Hallucination = 0.0f;    // deformación y deriva de color
};

/** Nivel grueso de una necesidad en el reloj: sin números, a propósito. */
enum class ENeedLevel : uint8
{
	Good,
	Fair,
	Low,
	Critical,
};

/** Lo que muestra el reloj de pulsera del Albatros (GDD §8.3). */
struct EXPLORED_API FWristWatchReadout
{
	int32 Hour = 0;
	int32 Minute = 0;
	/** Si el jugador ha elegido ver los indicadores (ajuste); si no, solo la hora. */
	bool bShowNeeds = false;
	ENeedLevel Thirst = ENeedLevel::Good;
	ENeedLevel Hunger = ENeedLevel::Good;
	ENeedLevel Rest = ENeedLevel::Good;
	ENeedLevel Warmth = ENeedLevel::Good;
};

struct EXPLORED_API FBodySignalsModel
{
	/** Señales objetivo para un estado (sin suavizar). */
	static FBodySignals Evaluate(const FSurvivalState& State, const FBodySignalContext& Context);

	/** Acerca Current a Target con constantes de tiempo por canal; nunca se pasa del objetivo. */
	static FBodySignals Smooth(const FBodySignals& Current, const FBodySignals& Target, float DeltaSeconds);

	/** Intensidad (0–1) de cada causa de viñeta; la mayor gana el tinte. */
	static float CauseIntensity(const FSurvivalState& State, EBodyTintCause Cause);

	/** Color del borde de cada causa (lineal, alfa 1). */
	static FLinearColor TintFor(EBodyTintCause Cause);

	/** Latidos por minuto para el audio (60–160). */
	static float HeartRateBpm(float Heartbeat);

	/** Respiraciones por minuto para el audio (12–40). */
	static float BreathsPerMinute(float Breathing);

	/** Nivel grueso de una necesidad 0–100. */
	static ENeedLevel CoarseLevel(float Need);

	/** Hora y, si se piden, indicadores gruesos. HoursOfDay puede salirse de [0, 24). */
	static FWristWatchReadout WristWatch(const FSurvivalState& State, float HoursOfDay, bool bShowNeeds);
};
