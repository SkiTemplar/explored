#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
struct FSoftObjectPath;

/**
 * Carga defensiva de mallas referenciadas por ruta (código y datos JSON).
 *
 * El arte v1 (/Game/Generated/Meshes) se borró en el reset v2 y el kit nuevo aún no está
 * importado: una ruta que falte no debe romper la partida ni llenar el log. Se comprueba que
 * el paquete exista antes de cargarlo (así no aparece «Failed to load» del motor), se avisa
 * una sola vez por ruta y, si procede, se devuelve una forma básica del motor.
 */
namespace ExploredAssets
{
	inline const TCHAR* const FallbackCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	inline const TCHAR* const FallbackSpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	/** La malla si su paquete existe; si no, nullptr y un aviso (una vez por ruta). */
	EXPLORED_API UStaticMesh* LoadMeshIfPresent(const FSoftObjectPath& Path);

	/** La malla si existe; si no, la forma del motor indicada (aviso una vez por ruta). */
	EXPLORED_API UStaticMesh* LoadMeshOrFallback(const FSoftObjectPath& Path, const TCHAR* FallbackPath = FallbackCubePath);
}
