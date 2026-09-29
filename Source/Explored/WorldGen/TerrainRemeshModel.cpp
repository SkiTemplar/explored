#include "WorldGen/TerrainRemeshModel.h"

#include "WorldGen/TerrainDensity.h"
#include "WorldGen/TerrainNetSyncModel.h"

namespace TerrainRemeshModelDetail
{
	int32 FloorDiv(int32 A, int32 B)
	{
		const int32 Q = A / B;
		return (A % B != 0 && (A < 0) != (B < 0)) ? Q - 1 : Q;
	}

	/** Entero con signo opcional y como mucho 7 cifras (los chunks caben de sobra). */
	bool ParseSmallInt(const FString& Text, int32& Out)
	{
		const int32 Len = Text.Len();
		const bool bNegative = Len > 0 && Text[0] == TEXT('-');
		const int32 First = bNegative ? 1 : 0;
		if (Len - First < 1 || Len - First > 7)
		{
			return false;
		}
		int32 Value = 0;
		for (int32 I = First; I < Len; ++I)
		{
			const TCHAR C = Text[I];
			if (C < TEXT('0') || C > TEXT('9'))
			{
				return false;
			}
			Value = Value * 10 + static_cast<int32>(C - TEXT('0'));
		}
		Out = bNegative ? -Value : Value;
		return true;
	}

	/** Añade los deltas del chunk de guardado Owner que caen dentro de la ventana. */
	void GatherFromChunk(const FTerrainEditModel& Model, const FTerrainGridWindow& Window, const FIntVector& Owner,
		const TMap<int32, int32>& Deltas, TArray<FTerrainGridDelta>& Out)
	{
		const int32 N = Model.GetSettings().CellsPerChunk;
		const FIntVector Dims = Window.Dims;
		// Parte del chunk que cae en la ventana, en índices locales.
		int32 Lo[3];
		int32 Hi[3];
		int32 Volume = 1;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Lo[Axis] = FMath::Max(0, Window.First[Axis] - Owner[Axis] * N);
			Hi[Axis] = FMath::Min(N - 1, Window.First[Axis] + Dims[Axis] - 1 - Owner[Axis] * N);
			if (Lo[Axis] > Hi[Axis])
			{
				return;
			}
			Volume *= Hi[Axis] - Lo[Axis] + 1;
		}
		auto Emit = [&](int32 Local, int32 Mm)
		{
			FIntVector Global;
			if (Mm == 0 || !Model.LocalToGlobal(Owner, Local, Global))
			{
				return;
			}
			const FIntVector G = Global - Window.First;
			if (G.X < 0 || G.Y < 0 || G.Z < 0 || G.X >= Dims.X || G.Y >= Dims.Y || G.Z >= Dims.Z)
			{
				return;
			}
			Out.Add({G.X + Dims.X * (G.Y + Dims.Y * G.Z), static_cast<float>(Mm) * 0.001f});
		};

		// De un vecino solo se lee la capa pegada a la ventana: si tiene más deltas que
		// muestras esa capa, sale más barato buscarlas una a una.
		if (Deltas.Num() <= Volume)
		{
			for (const auto& Pair : Deltas)
			{
				Emit(Pair.Key, Pair.Value);
			}
			return;
		}
		for (int32 Z = Lo[2]; Z <= Hi[2]; ++Z)
		{
			for (int32 Y = Lo[1]; Y <= Hi[1]; ++Y)
			{
				for (int32 X = Lo[0]; X <= Hi[0]; ++X)
				{
					const int32 Local = X + N * (Y + N * Z);
					if (const int32* Mm = Deltas.Find(Local))
					{
						Emit(Local, *Mm);
					}
				}
			}
		}
	}

	float Lerp(float A, float B, float T) { return A + (B - A) * T; }
}

int32 FTerrainRemeshModel::EditChunksPerRenderChunk(const FTerrainChunkSettings& Render, const FTerrainEditSettings& Edit)
{
	const double Ratio = static_cast<double>(Render.ChunkSizeMeters()) / static_cast<double>(Edit.ChunkSizeMeters());
	const int32 Rounded = FMath::RoundToInt32(Ratio);
	return (Rounded >= 1 && FMath::Abs(Ratio - Rounded) < 1.0e-6) ? Rounded : 0;
}

FIntVector FTerrainRemeshModel::RenderChunkOf(const FIntVector& EditChunk, int32 EditChunksPerRender)
{
	using TerrainRemeshModelDetail::FloorDiv;
	const int32 K = FMath::Max(1, EditChunksPerRender);
	return FIntVector(FloorDiv(EditChunk.X, K), FloorDiv(EditChunk.Y, K), FloorDiv(EditChunk.Z, K));
}

FVector FTerrainRemeshModel::EditChunkOrigin(const FIntVector& EditChunk, const FTerrainEditSettings& Edit)
{
	return FVector(EditChunk.X, EditChunk.Y, EditChunk.Z) * static_cast<double>(Edit.ChunkSizeMeters());
}

FTerrainGridWindow FTerrainRemeshModel::ChunkWindow(const FIntVector& Chunk, const FTerrainEditSettings& Edit)
{
	const int32 N = Edit.CellsPerChunk;
	FTerrainGridWindow Window;
	Window.First = Chunk * N - FIntVector(1);
	Window.Dims = FIntVector(N + 2);
	return Window;
}

FTerrainGridWindow FTerrainRemeshModel::ChunkWindow(const FIntVector& Chunk, const FTerrainEditSettings& Edit, int32 EditChunksPerRender,
	int32 SkirtSamples)
{
	FTerrainGridWindow Window = ChunkWindow(Chunk, Edit);
	const FIntVector Render = RenderChunkOf(Chunk, EditChunksPerRender);
	const int32 K = FMath::Max(1, EditChunksPerRender);
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (SkirtSamples > 0 && Chunk[Axis] == Render[Axis] * K)
		{
			Window.First[Axis] -= SkirtSamples;
			Window.Dims[Axis] += SkirtSamples;
		}
	}
	return Window;
}

void FTerrainRemeshModel::BuildBaseGrid(const FTerrainDensity& Density, const FTerrainEditSettings& Edit, const FTerrainGridWindow& Window,
	FDensityGrid& Out)
{
	const FIntVector First = Window.First;
	Out.Init(Window.Dims, FVector(First.X, First.Y, First.Z) * static_cast<double>(Edit.CellSize), Edit.CellSize);
	for (int32 Y = 0; Y < Out.Dims.Y; ++Y)
	{
		for (int32 X = 0; X < Out.Dims.X; ++X)
		{
			const FVector Base = Out.SamplePosition(X, Y, 0);
			const FTerrainColumn Column = Density.SampleColumn(static_cast<float>(Base.X), static_cast<float>(Base.Y));
			for (int32 Z = 0; Z < Out.Dims.Z; ++Z)
			{
				Out.Set(X, Y, Z, Density.ProceduralDensityWithColumn(Out.SamplePosition(X, Y, Z), Column));
			}
		}
	}
}

void FTerrainRemeshModel::GatherDeltas(const FTerrainEditModel& Model, const FTerrainGridWindow& Window, TArray<FTerrainGridDelta>& Out)
{
	if (Window.Dims.X <= 0 || Window.Dims.Y <= 0 || Window.Dims.Z <= 0)
	{
		return;
	}
	const FIntVector C0 = Model.ChunkOfSample(Window.First);
	const FIntVector C1 = Model.ChunkOfSample(Window.First + Window.Dims - FIntVector(1));
	for (int32 Z = C0.Z; Z <= C1.Z; ++Z)
	{
		for (int32 Y = C0.Y; Y <= C1.Y; ++Y)
		{
			for (int32 X = C0.X; X <= C1.X; ++X)
			{
				const FIntVector Owner(X, Y, Z);
				if (const TMap<int32, int32>* Deltas = Model.FindChunkDeltas(Owner))
				{
					TerrainRemeshModelDetail::GatherFromChunk(Model, Window, Owner, *Deltas, Out);
				}
			}
		}
	}
}

void FTerrainRemeshModel::GatherDeltas(const FTerrainEditModel& Model, const FIntVector& Chunk, TArray<FTerrainGridDelta>& Out)
{
	GatherDeltas(Model, ChunkWindow(Chunk, Model.GetSettings()), Out);
}

void FTerrainRemeshModel::ApplyDeltas(const TArray<FTerrainGridDelta>& Deltas, FDensityGrid& InOut)
{
	for (const FTerrainGridDelta& D : Deltas)
	{
		if (InOut.Values.IsValidIndex(D.Index))
		{
			InOut.Values[D.Index] += D.Delta;
		}
	}
}

FVector FTerrainRemeshModel::GridNormal(const FDensityGrid& Grid, const FVector& P)
{
	using TerrainRemeshModelDetail::Lerp;
	if (Grid.Dims.X < 2 || Grid.Dims.Y < 2 || Grid.Dims.Z < 2 || !(Grid.VoxelSize > 0.0f))
	{
		return FVector::UpVector;
	}
	const FVector Q = (P - Grid.Origin) / static_cast<double>(Grid.VoxelSize);
	int32 I[3];
	float F[3];
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double V = FMath::IsFinite(Q[Axis]) ? Q[Axis] : 0.0;
		const int32 Max = Grid.Dims[Axis] - 2;
		I[Axis] = FMath::Clamp(FMath::FloorToInt32(FMath::Clamp(V, -1.0, static_cast<double>(Max + 1))), 0, Max);
		F[Axis] = static_cast<float>(FMath::Clamp(V - I[Axis], 0.0, 1.0));
	}
	float D[2][2][2];
	for (int32 Z = 0; Z < 2; ++Z)
	{
		for (int32 Y = 0; Y < 2; ++Y)
		{
			for (int32 X = 0; X < 2; ++X)
			{
				D[X][Y][Z] = Grid.Get(I[0] + X, I[1] + Y, I[2] + Z);
			}
		}
	}
	// Derivada del interpolador trilineal dentro de la celda.
	const float Gx = Lerp(Lerp(D[1][0][0] - D[0][0][0], D[1][1][0] - D[0][1][0], F[1]),
		Lerp(D[1][0][1] - D[0][0][1], D[1][1][1] - D[0][1][1], F[1]), F[2]);
	const float Gy = Lerp(Lerp(D[0][1][0] - D[0][0][0], D[1][1][0] - D[1][0][0], F[0]),
		Lerp(D[0][1][1] - D[0][0][1], D[1][1][1] - D[1][0][1], F[0]), F[2]);
	const float Gz = Lerp(Lerp(D[0][0][1] - D[0][0][0], D[1][0][1] - D[1][0][0], F[0]),
		Lerp(D[0][1][1] - D[0][1][0], D[1][1][1] - D[1][1][0], F[0]), F[1]);
	return FVector(Gx, Gy, Gz).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
}

FTerrainMeshData FTerrainRemeshModel::BuildMesh(const FDensityGrid& Grid, const FVector& OriginMeters, const FTerrainDensity* Attributes)
{
	if (Grid.Values.Num() == 0 || Grid.IsUniform())
	{
		return FTerrainMeshData();
	}
	FTerrainMeshData Mesh = FSurfaceNets::Polygonize(Grid);
	const int32 Count = Mesh.Positions.Num();
	Mesh.Normals.SetNum(Count);
	if (Attributes)
	{
		Mesh.Colors.SetNum(Count);
		Mesh.Layers.SetNum(Count);
	}
	for (int32 I = 0; I < Count; ++I)
	{
		const FVector World(Mesh.Positions[I]);
		const FVector Normal = GridNormal(Grid, World);
		Mesh.Normals[I] = FVector3f(Normal);
		if (Attributes)
		{
			Mesh.Colors[I] = Attributes->SurfaceColor(World, Normal);
			Mesh.Layers[I] = Attributes->SurfaceLayers(World, Normal);
		}
		Mesh.Positions[I] = FVector3f((World - OriginMeters) * 100.0);
	}
	return Mesh;
}

void FTerrainRemeshModel::SurfaceEditChunks(const FTerrainDensity& Density, const FTerrainChunkSettings& Render,
	const FTerrainEditSettings& Edit, const FIntVector& RenderChunk, TArray<FIntVector>& Out)
{
	const int32 K = EditChunksPerRenderChunk(Render, Edit);
	if (K <= 0)
	{
		return;
	}
	// Pasos de la retícula por chunk de edición: los de la rejilla horneada (2 m → 4).
	const int32 S = FMath::Max(1, FMath::RoundToInt32(Edit.ChunkSizeMeters() / FMath::Max(0.01f, Render.VoxelSize)));
	const double Step = static_cast<double>(Edit.ChunkSizeMeters()) / S;
	const int32 M = K * S + 1;
	const FVector Origin = FTerrainChunkBuilder::ChunkOrigin(RenderChunk, Render);

	TArray<float> Lattice;
	Lattice.SetNum(M * M * M);
	for (int32 Y = 0; Y < M; ++Y)
	{
		for (int32 X = 0; X < M; ++X)
		{
			const FVector P0 = Origin + FVector(X * Step, Y * Step, 0.0);
			const FTerrainColumn Column = Density.SampleColumn(static_cast<float>(P0.X), static_cast<float>(P0.Y));
			for (int32 Z = 0; Z < M; ++Z)
			{
				Lattice[X + M * (Y + M * Z)] = Density.ProceduralDensityWithColumn(P0 + FVector(0.0, 0.0, Z * Step), Column);
			}
		}
	}

	for (int32 CZ = 0; CZ < K; ++CZ)
	{
		for (int32 CY = 0; CY < K; ++CY)
		{
			for (int32 CX = 0; CX < K; ++CX)
			{
				float Min = TNumericLimits<float>::Max();
				float Max = -TNumericLimits<float>::Max();
				for (int32 Z = CZ * S; Z <= CZ * S + S; ++Z)
				{
					for (int32 Y = CY * S; Y <= CY * S + S; ++Y)
					{
						for (int32 X = CX * S; X <= CX * S + S; ++X)
						{
							const float V = Lattice[X + M * (Y + M * Z)];
							Min = FMath::Min(Min, V);
							Max = FMath::Max(Max, V);
						}
					}
				}
				if (Min <= SurveyMargin && Max >= -SurveyMargin)
				{
					Out.Add(RenderChunk * K + FIntVector(CX, CY, CZ));
				}
			}
		}
	}
}

void FTerrainRemeshModel::EditedEditChunks(const FTerrainEditModel& Model, int32 EditChunksPerRender, const FIntVector& RenderChunk,
	TArray<FIntVector>& Out)
{
	const int32 First = Out.Num();
	for (const FIntVector& Edited : Model.EditedChunks())
	{
		for (int32 DZ = -1; DZ <= 1; ++DZ)
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					const FIntVector Neighbor = Edited + FIntVector(DX, DY, DZ);
					if (RenderChunkOf(Neighbor, EditChunksPerRender) == RenderChunk)
					{
						Out.Add(Neighbor);
					}
				}
			}
		}
	}
	TArray<FIntVector> Added(Out.GetData() + First, Out.Num() - First);
	Out.SetNum(First);
	FTerrainNetSyncModel::SortUnique(Added);
	Out.Append(Added);
}

bool FTerrainRemeshModel::ParseBakedChunkName(const FString& Name, FIntVector& OutCoord)
{
	TArray<FString> Parts;
	Name.ParseIntoArray(Parts, TEXT("_"), true);
	if (Parts.Num() != 5 || !Parts[0].Equals(TEXT("SM"), ESearchCase::CaseSensitive)
		|| !Parts[1].Equals(TEXT("Terrain"), ESearchCase::CaseSensitive))
	{
		return false;
	}
	FIntVector Coord;
	if (!TerrainRemeshModelDetail::ParseSmallInt(Parts[2], Coord.X) || !TerrainRemeshModelDetail::ParseSmallInt(Parts[3], Coord.Y)
		|| !TerrainRemeshModelDetail::ParseSmallInt(Parts[4], Coord.Z))
	{
		return false;
	}
	OutCoord = Coord;
	return true;
}
