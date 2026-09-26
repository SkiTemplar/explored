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
	Mesa,        // La Meseta: pradera alta en terrazas.
	Count
};

EXPLORED_API const TCHAR* LexToString(EIslandArchetype Archetype);

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
};

/** Disposición completa del archipiélago. */
struct EXPLORED_API FArchipelagoLayout
{
	/** Semilla del archipiélago publicado. */
	static constexpr uint32 OfficialSeed = 20260926;
	static constexpr float WorldHalfExtent = 3000.0f;
	static constexpr float SeaLevel = 0.0f;
	static constexpr float OceanFloor = -70.0f;
	/** Canal mínimo de agua entre las costas de dos islas. */
	static constexpr float MinChannel = 220.0f;

	uint32 Seed = 0;
	TArray<FIslandDesc> Islands;

	/** Genera la disposición de forma determinista. */
	static FArchipelagoLayout Generate(uint32 InSeed);

	const FIslandDesc* FindIsland(EIslandArchetype Archetype) const;
};
