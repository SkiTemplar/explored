#pragma once

#include "CoreMinimal.h"
#include "Fauna/FaunaTypes.h"

/** Zona marina de una celda, de la orilla al mar abierto. */
enum class EMarineZone : uint8
{
	Land,      // tierra emergida: sin fauna marina
	Shallows,  // fondos arenosos someros (< 4 m)
	Lagoon,    // laguna de un atolón (Arenas Blancas)
	Reef,      // arrecife (4–15 m junto a la costa)
	Slope,     // talud (15–40 m)
	Deep,      // aguas profundas (> 40 m): mar abierto
};

EXPLORED_API const TCHAR* LexToString(EMarineZone Zone);

/** Lo que la capa de UE sabe de una celda (centímetros). */
struct EXPLORED_API FFaunaCellContext
{
	/** Altura del fondo en el centro de la celda (cm; > 0 es tierra). */
	float SeabedZCm = -7000.0f;
	/** Distancia a la costa más cercana (cm). */
	float DistanceToCoastCm = 1.0e6f;
	/** Dentro de la laguna de un atolón. */
	bool bLagoon = false;
	/** Junto a Los Dientes (islotes de aves: fragatas y colonias). */
	bool bNearTeeth = false;
	/** Paso de ballenas activo (evento de P-EVENTS, «cada ~10 días»). */
	bool bWhalePassage = false;
	/** Hora local para el ciclo diario. */
	float Hours = 12.0f;
};

/** Una aparición: especie, punto dentro de la celda, tamaño del grupo y semilla propia. */
struct EXPLORED_API FFaunaSpawn
{
	EFaunaSpecies Species = EFaunaSpecies::ReefFish;
	FVector PositionCm = FVector::ZeroVector;
	int32 GroupSize = 1;
	uint32 Seed = 0;
};

/**
 * Reglas de población por zona, profundidad y hora. Deterministas: la misma
 * semilla y celda dan siempre lo mismo. La tirada de cada especie es fija por
 * celda y la hora solo sube o baja el umbral, así que al pasar las horas los
 * grupos aparecen o desaparecen sin barajar los que siguen.
 */
struct EXPLORED_API FFaunaSpawnRules
{
	/** Lado de la celda de población (cm). */
	static constexpr float CellSizeCm = 10000.0f;

	/** Profundidad (cm, positiva) a partir de la cual empieza cada zona. */
	static constexpr float ReefMinDepthCm = 400.0f;
	static constexpr float SlopeMinDepthCm = 1500.0f;
	static constexpr float DeepMinDepthCm = 4000.0f;
	/** Las aves solo aparecen a menos de esto de la costa (salvo en Los Dientes). */
	static constexpr float BirdCoastRangeCm = 40000.0f;
	/** Las ballenas solo lejos de la costa. */
	static constexpr float WhaleMinCoastDistanceCm = 30000.0f;

	static EMarineZone ClassifyZone(float SeabedZCm, bool bLagoon);

	/** Probabilidad de que la especie aparezca en una celda (ya con la hora aplicada). */
	static float Chance(EFaunaSpecies Species, const FFaunaCellContext& Context);

	/** Rango del tamaño de grupo por especie. */
	static void GroupSizeRange(EFaunaSpecies Species, int32& OutMin, int32& OutMax);

	/** Apariciones de una celda (se añaden a Out). */
	static void SpawnsForCell(uint32 WorldSeed, const FIntPoint& Cell, const FFaunaCellContext& Context, TArray<FFaunaSpawn>& Out);

	static FIntPoint CellOf(const FVector2D& PointCm);
	static FVector2D CellCenter(const FIntPoint& Cell);
};

/** Nivel de actualización por distancia (GDD §17.4, «IA de fauna con LOD de actualización»). */
enum class EFaunaLodTier : uint8
{
	Full,     // cada fotograma
	Reduced,  // uno de cada ReducedInterval fotogramas, con el tiempo acumulado
	Frozen,   // sin actualizar (lejos o fuera de la vista)
};

struct EXPLORED_API FFaunaLodSettings
{
	float FullRadiusCm = 4000.0f;
	float ReducedRadiusCm = 15000.0f;
	/** Margen relativo para no oscilar entre niveles en el borde. */
	float Hysteresis = 0.1f;
	int32 ReducedInterval = 4;
};

/** Política pura de LOD de la IA de fauna. */
struct EXPLORED_API FFaunaLod
{
	/** Radios por especie: las ballenas y las aves se ven (y se mueven) de más lejos. */
	static FFaunaLodSettings ForSpecies(EFaunaSpecies Species);

	/** Nivel con histéresis: para subir de nivel hay que acercarse un poco más del radio y para bajar alejarse un poco más. */
	static EFaunaLodTier Tier(float DistanceCm, EFaunaLodTier Previous, const FFaunaLodSettings& Settings);

	/** ¿Toca actualizar este agente en este fotograma? Escalonado por Id para repartir la carga. */
	static bool ShouldTick(EFaunaLodTier Tier, uint64 Frame, uint32 AgentId, int32 ReducedInterval);

	/** Delta que recibe el agente cuando se actualiza: el acumulado de su intervalo. */
	static float TickDelta(EFaunaLodTier Tier, float FrameDeltaSeconds, int32 ReducedInterval);
};
