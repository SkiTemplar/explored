#pragma once

#include "CoreMinimal.h"

class FGroundField;
class UWorld;

/**
 * Altura del suelo que se dibuja (SM_IslaSal_Terrain) para el código que apoya cosas en él: césped,
 * auditor y bot de playtest y capturas. Un solo lector de Content/Data/ground_field.bin (LoadField),
 * compartido por todos. FIslandLayout::SampleColumn es el relieve analítico y se separa de la malla
 * hasta ~3,4 m desde el relieve de F6: no sirve para colocar nada a ras de suelo.
 */
namespace ExploredGround
{
	/** Lee y valida ground_field.bin. Nulo con el motivo («no existe» o el error de FGroundField::Parse). */
	EXPLORED_API TSharedPtr<const FGroundField> LoadField(FString& OutError);

	/** Campo compartido: se lee la primera vez que se pide (o tras ReloadShared); nulo si no se puede usar. */
	EXPLORED_API TSharedPtr<const FGroundField> GetShared();

	/** Relee el fichero y sustituye el campo compartido (AExploredGrassField::Setup). */
	EXPLORED_API TSharedPtr<const FGroundField> ReloadShared(FString& OutError);

	/**
	 * Altura del suelo en (X, Y) metros. Primero la de la malla según ground_field.bin; si el campo no
	 * está cargado o el punto cae fuera de su rejilla, el impacto más bajo de un trazado vertical contra
	 * lo estático (el terreno queda debajo de rocas y casas); si tampoco hay impacto, SampleColumn.
	 */
	EXPLORED_API float HeightAtMeters(const UWorld* World, double X, double Y);
}
