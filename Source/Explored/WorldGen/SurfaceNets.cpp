#include "WorldGen/SurfaceNets.h"

void FDensityGrid::Init(const FIntVector& InDims, const FVector& InOrigin, float InVoxelSize)
{
	check(InDims.X >= 2 && InDims.Y >= 2 && InDims.Z >= 2);
	Dims = InDims;
	Origin = InOrigin;
	VoxelSize = InVoxelSize;
	Values.SetNumUninitialized(Dims.X * Dims.Y * Dims.Z);
}

bool FDensityGrid::IsUniform() const
{
	if (Values.IsEmpty())
	{
		return true;
	}
	const bool bFirstSolid = Values[0] < 0.0f;
	for (const float V : Values)
	{
		if ((V < 0.0f) != bFirstSolid)
		{
			return false;
		}
	}
	return true;
}

namespace
{
	// Las 12 aristas de una celda, como pares de esquinas (bits x, y, z).
	constexpr int32 CellEdges[12][2] = {
		{0, 1}, {2, 3}, {4, 5}, {6, 7},  // aristas en X
		{0, 2}, {1, 3}, {4, 6}, {5, 7},  // aristas en Y
		{0, 4}, {1, 5}, {2, 6}, {3, 7}}; // aristas en Z

	FORCEINLINE FIntVector CornerOffset(int32 Corner)
	{
		return FIntVector(Corner & 1, (Corner >> 1) & 1, (Corner >> 2) & 1);
	}
}

FTerrainMeshData FSurfaceNets::Polygonize(const FDensityGrid& Grid)
{
	FTerrainMeshData Mesh;
	const FIntVector CellDims = Grid.Dims - FIntVector(1);
	const int32 NumCells = CellDims.X * CellDims.Y * CellDims.Z;
	TArray<int32> CellVertex;
	CellVertex.Init(INDEX_NONE, NumCells);

	auto CellIndex = [&CellDims](int32 X, int32 Y, int32 Z)
	{
		return X + CellDims.X * (Y + CellDims.Y * Z);
	};

	// 1) Un vértice por celda con cambio de signo: media de los cruces de sus aristas.
	for (int32 Z = 0; Z < CellDims.Z; ++Z)
	{
		for (int32 Y = 0; Y < CellDims.Y; ++Y)
		{
			for (int32 X = 0; X < CellDims.X; ++X)
			{
				float Corner[8];
				int32 SolidMask = 0;
				for (int32 C = 0; C < 8; ++C)
				{
					const FIntVector O = CornerOffset(C);
					Corner[C] = Grid.Get(X + O.X, Y + O.Y, Z + O.Z);
					SolidMask |= (Corner[C] < 0.0f ? 1 : 0) << C;
				}
				if (SolidMask == 0 || SolidMask == 0xFF)
				{
					continue;
				}

				FVector Sum = FVector::ZeroVector;
				int32 Crossings = 0;
				for (const auto& Edge : CellEdges)
				{
					const float A = Corner[Edge[0]];
					const float B = Corner[Edge[1]];
					if ((A < 0.0f) == (B < 0.0f))
					{
						continue;
					}
					const float T = A / (A - B);
					const FVector PA(CornerOffset(Edge[0]));
					const FVector PB(CornerOffset(Edge[1]));
					Sum += FMath::Lerp(PA, PB, T);
					++Crossings;
				}

				const FVector Local = Sum / static_cast<double>(Crossings);
				const FVector World = Grid.Origin + (FVector(X, Y, Z) + Local) * Grid.VoxelSize;
				CellVertex[CellIndex(X, Y, Z)] = Mesh.Positions.Add(FVector3f(World));
			}
		}
	}

	// 2) Un quad por arista propia que cruza la superficie.
	auto EmitTriangle = [&Mesh](uint32 A, uint32 B, uint32 C, const FVector3f& Outward)
	{
		const FVector3f& PA = Mesh.Positions[A];
		const FVector3f& PB = Mesh.Positions[B];
		const FVector3f& PC = Mesh.Positions[C];
		const FVector3f Cross = FVector3f::CrossProduct(PB - PA, PC - PA);
		if (Cross.SizeSquared() < 1e-10f)
		{
			return; // Triángulo degenerado.
		}
		const bool bCrossAlongOutward = FVector3f::DotProduct(Cross, Outward) > 0.0f;
		const bool bSwap = bCrossAlongOutward == bFrontFaceCrossOpposesNormal;
		Mesh.Indices.Add(A);
		Mesh.Indices.Add(bSwap ? C : B);
		Mesh.Indices.Add(bSwap ? B : C);
	};

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int32 AxisB = (Axis + 1) % 3;
		const int32 AxisC = (Axis + 2) % 3;
		for (int32 Z = 1; Z < Grid.Dims.Z - 1; ++Z)
		{
			for (int32 Y = 1; Y < Grid.Dims.Y - 1; ++Y)
			{
				for (int32 X = 1; X < Grid.Dims.X - 1; ++X)
				{
					const FIntVector P(X, Y, Z);
					FIntVector Q = P;
					Q[Axis] += 1;
					const float D0 = Grid.Get(P.X, P.Y, P.Z);
					const float D1 = Grid.Get(Q.X, Q.Y, Q.Z);
					if ((D0 < 0.0f) == (D1 < 0.0f))
					{
						continue;
					}

					// Las cuatro celdas que comparten la arista.
					int32 Quad[4];
					bool bValid = true;
					for (int32 K = 0; K < 4 && bValid; ++K)
					{
						FIntVector Cell = P;
						Cell[AxisB] -= (K == 1 || K == 2) ? 1 : 0;
						Cell[AxisC] -= (K >= 2) ? 1 : 0;
						const int32 V = CellVertex[CellIndex(Cell.X, Cell.Y, Cell.Z)];
						bValid = V != INDEX_NONE;
						Quad[K] = V;
					}
					if (!bValid)
					{
						continue;
					}

					// El exterior (aire) está hacia la muestra con densidad positiva.
					FVector3f Outward = FVector3f::ZeroVector;
					Outward[Axis] = D0 < 0.0f ? 1.0f : -1.0f;
					EmitTriangle(Quad[0], Quad[1], Quad[2], Outward);
					EmitTriangle(Quad[0], Quad[2], Quad[3], Outward);
				}
			}
		}
	}

	// 3) Compacta: elimina vértices sin triángulos.
	TArray<int32> Remap;
	Remap.Init(INDEX_NONE, Mesh.Positions.Num());
	TArray<FVector3f> Compact;
	Compact.Reserve(Mesh.Positions.Num());
	for (uint32& I : Mesh.Indices)
	{
		if (Remap[I] == INDEX_NONE)
		{
			Remap[I] = Compact.Add(Mesh.Positions[I]);
		}
		I = static_cast<uint32>(Remap[I]);
	}
	Mesh.Positions = MoveTemp(Compact);
	return Mesh;
}
