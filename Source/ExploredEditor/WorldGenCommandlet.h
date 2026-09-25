#pragma once

#include "Commandlets/Commandlet.h"

#include "WorldGenCommandlet.generated.h"

/**
 * Genera el archipiélago offline.
 *
 * Uso:
 *   UnrealEditor-Cmd Explored.uproject -run=ExploredWorldGen -mode=preview [-seed=N] [-size=1024]
 *   UnrealEditor-Cmd Explored.uproject -run=ExploredWorldGen -mode=bake [-seed=N] [-voxel=2] [-cells=32]
 *     [-region=X0,Y0,X1,Y1]  (en metros; por defecto, el mundo entero)
 */
UCLASS()
class UExploredWorldGenCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UExploredWorldGenCommandlet();

	virtual int32 Main(const FString& Params) override;
};
