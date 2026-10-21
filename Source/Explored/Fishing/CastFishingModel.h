#pragma once

#include "CoreMinimal.h"

/**
 * Pesca simplificada de Baliza Siete (GDD v3 §5.4; Content/Data/fish.json): lanzar, esperar y, a
 * los 3 s de la mordida, pulsar para sacar el pez; si no, se escapa. Sin barra de tensión. Un pez
 * cada ~35 s de media, ×1,5 al amanecer (06-08) y al atardecer (17-19); no se pesca de noche
 * (20-06) ni durante el ciclón. La espera es determinista a partir de una semilla (sin FMath::Rand)
 * para que el Spec y el bot la reproduzcan. Modelo puro; AExploredFishingSpot es la capa fina.
 */
struct EXPLORED_API FCastFishingConfig
{
	static constexpr float DefaultMeanBiteSeconds = 35.0f;
	static constexpr float DefaultMinBiteSeconds = 8.0f;
	static constexpr float DefaultBiteWindowSeconds = 3.0f;
	static constexpr float DefaultGoldenHourFactor = 1.5f;

	float MeanBiteSeconds = DefaultMeanBiteSeconds;
	float MinBiteSeconds = DefaultMinBiteSeconds;
	float BiteWindowSeconds = DefaultBiteWindowSeconds;
	float GoldenHourFactor = DefaultGoldenHourFactor;
	/** Horas de juego [desde, hasta). La noche da la vuelta por medianoche. */
	FVector2f Dawn = FVector2f(6.0f, 8.0f);
	FVector2f Dusk = FVector2f(17.0f, 19.0f);
	FVector2f Night = FVector2f(20.0f, 6.0f);
	FName Rod = TEXT("cana_pescar");
	FName Catch = TEXT("pez_crudo");
};

enum class ECastFishingPhase : uint8
{
	Idle,
	/** Sedal en el agua, sin mordida. */
	Waiting,
	/** Ha mordido: hay BiteWindowSeconds para pulsar. */
	Biting,
};

enum class ECastFishingEvent : uint8
{
	None,
	Cast,
	Bite,
	Caught,
	/** Se pulsó tarde (o no se pulsó): el pez se va. */
	Escaped,
	/** Se recogió el sedal antes de la mordida. */
	Reeled,
	/** De noche o con el ciclón no se pesca. */
	Blocked,
};

struct EXPLORED_API FCastFishingState
{
	ECastFishingPhase Phase = ECastFishingPhase::Idle;
	float RemainingSeconds = 0.0f;
	uint32 Seed = 1;
};

struct EXPLORED_API FCastFishingModel
{
	static bool InRange(float Hour, const FVector2f& Range);
	static bool IsNight(const FCastFishingConfig& Config, float Hour);
	static bool CanFish(const FCastFishingConfig& Config, float Hour, bool bStorm);
	/** Multiplicador del ritmo de picada (1 o GoldenHourFactor). */
	static float RateFactor(const FCastFishingConfig& Config, float Hour);
	/** Espera hasta la mordida: uniforme alrededor de la media (MeanBite / factor), nunca menos de MinBite. */
	static float DrawWaitSeconds(const FCastFishingConfig& Config, float Hour, uint32& InOutSeed);

	static FCastFishingState Cast(const FCastFishingState& In, const FCastFishingConfig& Config, float Hour, bool bStorm,
		ECastFishingEvent& OutEvent);
	static FCastFishingState Tick(const FCastFishingState& In, const FCastFishingConfig& Config, float DeltaSeconds, float Hour,
		bool bStorm, ECastFishingEvent& OutEvent);
	/** Pulsar: con mordida saca el pez; antes, recoge el sedal sin nada. */
	static FCastFishingState Pull(const FCastFishingState& In, ECastFishingEvent& OutEvent);
};
