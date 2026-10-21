#pragma once

#include "CoreMinimal.h"

#include "WorldGen/GrassFieldModel.h"

/** Assets y distancias de dibujo del césped (grass.json; lo que no entra en el modelo puro). */
struct EXPLORED_API FGrassAssets
{
	/** Ruta de la malla de cada EGrassKind (/Game/Art/K6_Cesped/SM_*_01). */
	FString Meshes[GrassKindCount];
	bool bCastShadow[GrassKindCount] = {};
	FString Material = TEXT("/Game/Art/Materials/M_ToonCesped");
	FString FallbackMaterial = TEXT("/Game/Art/Materials/M_Toon");
	FString FallbackMesh = TEXT("/Game/Art/K1_Naturaleza/SM_GrassClump_01");
	/** Fundido por distancia (m): el material encoge la mata entre ambas y el componente la corta al final. */
	float FadeStartM = 45.0f;
	float FadeEndM = 70.0f;

	/** Rutas del kit K6 y sombras solo en la hierba alta y el helecho. */
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
