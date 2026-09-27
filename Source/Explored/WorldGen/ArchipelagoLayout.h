#pragma once

#include "CoreMinimal.h"

/** Identidad de cada isla (GDD §3.2). */
enum class EIslandArchetype : uint8
{
	Landing,     // Isla del Amaraje: playa y palmeral, laguna con el avión.
	Emerald,     // Esmeralda: selva densa, cascada.
	Smoke,       // Isla del Humo: volcán con cráter.
	Teeth,       // Los Dientes: islotes rocosos y acantilados.
	Mangrove,    // Manglar de las Voces: llano, canales.
	WhiteSands,  // Arenas Blancas: atolón con laguna.
	Mesa,        // La Meseta: macizo kárstico de caliza, cresta irregular y farallones sueltos.
	Count
};

EXPLORED_API const TCHAR* LexToString(EIslandArchetype Archetype);

/** Lóbulo secundario de la costa (península), en coordenadas locales normalizadas por el radio. */
struct EXPLORED_API FIslandLobe
{
	FVector2D Offset = FVector2D::ZeroVector;
	float Radius = 0.4f;
	/** Alargamiento del lóbulo (1 = circular). */
	float Aspect = 1.0f;
	float Angle = 0.0f;
};

/** Cayo satélite: islote bajo junto a una isla. Centro en coordenadas del mundo. */
struct EXPLORED_API FCayDesc
{
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 30.0f;
	float Height = 2.0f;
	/** Alargamiento (1 = circular) y orientación del eje largo. */
	float Aspect = 1.0f;
	float Angle = 0.0f;
	/** Rocoso (farallón) en lugar de arenoso. */
	bool bRocky = false;
};

/** Parámetros de una isla. Todas las distancias en metros. */
struct EXPLORED_API FIslandDesc
{
	EIslandArchetype Archetype = EIslandArchetype::Landing;
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 500.0f;
	float MaxHeight = 60.0f;
	/** Semilla propia para el detalle de la isla. */
	uint32 Seed = 0;
	/** Rotación de la forma, en radianes. */
	float Rotation = 0.0f;
	/** Subcentros de islotes (solo Los Dientes), relativos al centro. */
	TArray<FVector2D> Islets;
	/** Penínsulas que rompen la silueta elíptica; nunca pasan de 1,1 radios. */
	TArray<FIslandLobe> Lobes;
	/** Cayos satélite en la plataforma exterior. */
	TArray<FCayDesc> Cays;
};

/** Disposición completa del archipiélago. */
struct EXPLORED_API FArchipelagoLayout
{
	/** Semilla del archipiélago publicado. */
	static constexpr uint32 OfficialSeed = 20260926;
	static constexpr float WorldHalfExtent = 3000.0f;
	static constexpr float SeaLevel = 0.0f;
	static constexpr float OceanFloor = -70.0f;
	/** Canal mínimo de agua entre las costas nominales de dos islas. */
	static constexpr float MinChannel = 140.0f;
	/** Canal máximo entre islas consecutivas de la cadena (se cruza nadando o en balsa). */
	static constexpr float MaxChainChannel = 520.0f;

	uint32 Seed = 0;
	/** Islas en orden de la cadena volcánica: de la más joven (volcán) a la más vieja (atolón). */
	TArray<FIslandDesc> Islands;
	/** Dorsal submarina que une la cadena: polilínea por los centros, en metros. */
	TArray<FVector2D> Spine;

	/** Genera la disposición de forma determinista. */
	static FArchipelagoLayout Generate(uint32 InSeed);

	const FIslandDesc* FindIsland(EIslandArchetype Archetype) const;
};
