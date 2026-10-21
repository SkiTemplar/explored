#pragma once

#include "CoreMinimal.h"

/** Una instancia decorativa de island_decor.json (metros, grados). */
struct EXPLORED_API FIslandDecorInstance
{
	int32 Mesh = 0;
	FVector Location = FVector::ZeroVector;
	float YawDeg = 0.0f;
	float Scale = 1.0f;
};

/**
 * Vegetación y rocas decorativas de la Isla de Sal (Content/Data/island_decor.json, lo genera
 * Tools/Island con «uv run island-decor»). Solo decoración: el juego la dibuja con instancias
 * sin colisión; no interviene en ninguna regla.
 */
struct EXPLORED_API FIslandDecor
{
	static constexpr int32 MaxInstances = 3000;

	/** Rutas /Game/Art/K1_Naturaleza/SM_*_01 de cada índice de malla. */
	TArray<FString> Meshes;
	TArray<FIslandDecorInstance> Instances;

	/** Comprueba índices de malla, rutas, escalas y coordenadas finitas; el primer error en OutError. */
	bool Validate(FString& OutError) const;

	/** Número de instancias por índice de malla (mismo tamaño que Meshes). */
	TArray<int32> CountPerMesh() const;
};
