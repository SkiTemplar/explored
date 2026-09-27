#include "WorldGen/VegetationHarvestState.h"

uint32 GetTypeHash(const FVegetationInstanceKey& Key)
{
	return HashCombine(HashCombine(GetTypeHash(Key.Cell), GetTypeHash(Key.Component)), GetTypeHash(Key.Index));
}
