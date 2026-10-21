#pragma once

#include "CoreMinimal.h"

#include "WorldGen/GrassFieldModel.h"

/** Assets y parámetros de dibujo del césped (grass.json; lo que no entra en el modelo puro). */
struct EXPLORED_API FGrassAssets
{
	/** Ruta de la malla de cada EGrassKind (/Game/Art/K6_Cesped/SM_*_01). */
	FString Meshes[GrassKindCount];
	/** Sombras proyectadas por malla (en el césped, todas apagadas: en la prueba de PCG costaron 10 fps). */
	bool bCastShadow[GrassKindCount] = {};
	FString Material = TEXT("/Game/Art/Materials/M_ToonCesped");
	FString FallbackMaterial = TEXT("/Game/Art/Materials/M_Toon");
	FString FallbackMesh = TEXT("/Game/Art/K1_Naturaleza/SM_GrassClump_01");
	/** Colección con el adelgazamiento (Grass*) que comparten M_ToonCesped y M_TerrainToon. */
	FString ParameterCollection = TEXT("/Game/Art/Materials/MPC_Terreno");
	/** El viento del material se apaga entre estas distancias (m): lejos no se ve y cuesta. */
	float WindFadeStartM = 30.0f;
	float WindFadeEndM = 45.0f;

	/** Rutas del kit K6, sin sombras. */
	static FGrassAssets Defaults();
};

/** Lectura de Content/Data/grass.json (capa de motor; lo valida también Tools/DataCheck). */
namespace ExploredGrass
{
	EXPLORED_API FString GrassJsonPath();
	EXPLORED_API FString GroundFieldPath();

	/** Interpreta grass.json sobre los valores por defecto. False con el motivo si algo no vale. */
	EXPLORED_API bool ParseGrassJson(const FString& Json, FGrassConfig& OutConfig, FGrassAssets& OutAssets, FString& OutError);
}
