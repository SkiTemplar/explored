#include "WorldGen/TerrainEditModel.h"

// Acceso por muestra y por chunk de FTerrainEditModel: lo usan la red (aplicar los parches
// del servidor) y el remallado (leer los deltas de un chunk sin evaluar el campo base).
// Va en su propio fichero para no engordar TerrainEditModel.cpp.

bool FTerrainEditModel::SetSampleDeltaMm(const FIntVector& Global, int32 DeltaMm)
{
	if (DeltaMm > MaxDeltaMm || DeltaMm < -MaxDeltaMm)
	{
		return false;
	}
	SetDeltaMm(Global, DeltaMm);
	return true;
}

bool FTerrainEditModel::LocalToGlobal(const FIntVector& Chunk, int32 LocalIndex, FIntVector& OutGlobal) const
{
	const int32 N = Settings.CellsPerChunk;
	if (N <= 0 || LocalIndex < 0 || LocalIndex >= N * N * N)
	{
		return false;
	}
	const FIntVector Local(LocalIndex % N, (LocalIndex / N) % N, LocalIndex / (N * N));
	OutGlobal = Chunk * N + Local;
	return true;
}

int32 FTerrainEditModel::LocalIndexOf(const FIntVector& Global) const
{
	const int32 N = Settings.CellsPerChunk;
	const FIntVector L = Global - ChunkOfSample(Global) * N;
	return L.X + N * (L.Y + N * L.Z);
}
