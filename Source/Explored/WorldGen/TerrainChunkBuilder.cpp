#include "WorldGen/TerrainChunkBuilder.h"

#include "WorldGen/TerrainDensity.h"

FVector FTerrainChunkBuilder::ChunkOrigin(const FIntVector& Coord, const FTerrainChunkSettings& Settings)
{
	return FVector(Coord) * Settings.ChunkSizeMeters();
}

FTerrainMeshData FTerrainChunkBuilder::Build(const FTerrainDensity& Density, const FIntVector& Coord,
	const FTerrainChunkSettings& Settings)
{
	const int32 N = Settings.CellsPerChunk;
	const float Voxel = Settings.VoxelSize;
	const FVector Origin = ChunkOrigin(Coord, Settings);

	// Índice de muestra 0 = coordenada global -1 (ver FSurfaceNets).
	FDensityGrid Grid;
	Grid.Init(FIntVector(N + 2), Origin - FVector(Voxel), Voxel);

	for (int32 Y = 0; Y < Grid.Dims.Y; ++Y)
	{
		for (int32 X = 0; X < Grid.Dims.X; ++X)
		{
			const FVector Base = Grid.SamplePosition(X, Y, 0);
			const FTerrainColumn Column = Density.SampleColumn(Base.X, Base.Y);
			for (int32 Z = 0; Z < Grid.Dims.Z; ++Z)
			{
				Grid.Set(X, Y, Z, Density.DensityWithColumn(Grid.SamplePosition(X, Y, Z), Column));
			}
		}
	}

	if (Grid.IsUniform())
	{
		return FTerrainMeshData();
	}

	FTerrainMeshData Mesh = FSurfaceNets::Polygonize(Grid);
	Mesh.Normals.SetNumUninitialized(Mesh.Positions.Num());
	Mesh.Colors.SetNumUninitialized(Mesh.Positions.Num());

	const float NormalStep = Voxel * 0.5f;
	for (int32 I = 0; I < Mesh.Positions.Num(); ++I)
	{
		const FVector World(Mesh.Positions[I]);
		const FVector Normal = Density.Normal(World, NormalStep);
		Mesh.Normals[I] = FVector3f(Normal);
		Mesh.Colors[I] = Density.SurfaceColor(World, Normal);
		// Metros → centímetros, relativo al origen del chunk.
		Mesh.Positions[I] = FVector3f((World - Origin) * 100.0);
	}
	return Mesh;
}

TArray<FIntVector> FTerrainChunkBuilder::FindCandidateChunks(const FTerrainDensity& Density,
	const FTerrainChunkSettings& Settings, const FBox2D& WorldRectMeters)
{
	TArray<FIntVector> Result;
	const float Size = Settings.ChunkSizeMeters();
	const int32 MinX = FMath::FloorToInt32(WorldRectMeters.Min.X / Size);
	const int32 MinY = FMath::FloorToInt32(WorldRectMeters.Min.Y / Size);
	const int32 MaxX = FMath::CeilToInt32(WorldRectMeters.Max.X / Size);
	const int32 MaxY = FMath::CeilToInt32(WorldRectMeters.Max.Y / Size);

	for (int32 CY = MinY; CY < MaxY; ++CY)
	{
		for (int32 CX = MinX; CX < MaxX; ++CX)
		{
			// Incluye el borde de una celda extra que usa el chunk.
			const FBox2D Rect(
				FVector2D(CX * Size - Settings.VoxelSize, CY * Size - Settings.VoxelSize),
				FVector2D((CX + 1) * Size + Settings.VoxelSize, (CY + 1) * Size + Settings.VoxelSize));
			float MinH = 0.0f;
			float MaxH = 0.0f;
			Density.HeightBounds(Rect, Settings.VoxelSize * 2.0f, MinH, MaxH);

			const int32 MinZ = FMath::FloorToInt32(MinH / Size);
			const int32 MaxZ = FMath::FloorToInt32(MaxH / Size);
			for (int32 CZ = MinZ; CZ <= MaxZ; ++CZ)
			{
				Result.Add(FIntVector(CX, CY, CZ));
			}
		}
	}
	return Result;
}
