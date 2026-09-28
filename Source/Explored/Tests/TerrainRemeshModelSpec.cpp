#include "Misc/AutomationTest.h"

#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"
#include "WorldGen/TerrainRemeshModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TerrainRemeshSpecDetail
{
	const auto Plane = [](const FVector& P) { return static_cast<float>(P.Z - 4.3); };

	/** Rejilla del chunk con el campo base de Plane (lo que hace BuildBaseGrid con FTerrainDensity). */
	FDensityGrid PlaneGrid(const FTerrainEditSettings& Settings, const FIntVector& Chunk)
	{
		const int32 N = Settings.CellsPerChunk;
		const FIntVector First = Chunk * N - FIntVector(1);
		FDensityGrid Grid;
		Grid.Init(FIntVector(N + 2), FVector(First.X, First.Y, First.Z) * static_cast<double>(Settings.CellSize), Settings.CellSize);
		for (int32 I = 0; I < Grid.Values.Num(); ++I)
		{
			const int32 X = I % Grid.Dims.X;
			const int32 Y = (I / Grid.Dims.X) % Grid.Dims.Y;
			const int32 Z = I / (Grid.Dims.X * Grid.Dims.Y);
			Grid.Values[I] = Plane(Grid.SamplePosition(X, Y, Z));
		}
		return Grid;
	}

	FSphereDig Dig(const FVector& Center, float Radius = 0.6f)
	{
		FSphereDig D;
		D.Center = Center;
		D.Radius = Radius;
		D.Material = ETerrainMaterial::Tierra;
		D.ToolTier = 4;
		return D;
	}

	float MinZ(const FTerrainMeshData& Mesh)
	{
		float Min = TNumericLimits<float>::Max();
		for (const FVector3f& P : Mesh.Positions)
		{
			Min = FMath::Min(Min, P.Z);
		}
		return Min;
	}
}

BEGIN_DEFINE_SPEC(FTerrainRemeshModelSpec, "Explored.TerrainRemesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FTerrainRemeshModelSpec)

void FTerrainRemeshModelSpec::Define()
{
	using namespace TerrainRemeshSpecDetail;

	Describe("deltas sobre la base cacheada", [this]()
	{
		It("base + GatherDeltas da la misma rejilla que BuildChunkGrid, con ediciones en los vecinos", [this]()
		{
			FTerrainEditModel Model;
			const FTerrainEditSettings& S = Model.GetSettings();
			const FIntVector Chunk(0, 0, 0);
			// Golpes dentro, en el borde (muestras de los vecinos que lee la rejilla) y en una esquina.
			Model.DigSphere(Dig(FVector(4.0, 4.0, 4.3)), Plane);
			Model.DigSphere(Dig(FVector(-0.1, 4.0, 4.3)), Plane);
			Model.DigSphere(Dig(FVector(8.1, 8.1, 4.3)), Plane);
			Model.SetSampleDeltaMm(FIntVector(-1, -1, -1), 123);
			Model.SetSampleDeltaMm(FIntVector(32, 32, 32), -77);
			Model.SetSampleDeltaMm(FIntVector(33, 0, 0), 999);

			FDensityGrid Expected;
			Model.BuildChunkGrid(Chunk, Plane, Expected);
			FDensityGrid Grid = PlaneGrid(S, Chunk);
			TArray<FTerrainGridDelta> Deltas;
			FTerrainRemeshModel::GatherDeltas(Model, Chunk, Deltas);
			FTerrainRemeshModel::ApplyDeltas(Deltas, Grid);

			TestEqual(TEXT("mismas muestras"), Grid.Values.Num(), Expected.Values.Num());
			float MaxError = 0.0f;
			for (int32 I = 0; I < Grid.Values.Num(); ++I)
			{
				MaxError = FMath::Max(MaxError, FMath::Abs(Grid.Values[I] - Expected.Values[I]));
			}
			TestTrue(FString::Printf(TEXT("error máximo %g"), MaxError), MaxError < 1.0e-4f);
			TSet<int32> Seen;
			for (const FTerrainGridDelta& D : Deltas)
			{
				TestFalse(TEXT("sin índices repetidos"), Seen.Contains(D.Index));
				Seen.Add(D.Index);
			}
		});

		It("el faldón alarga la ventana en la cara baja del chunk de render y lee los deltas de allí", [this]()
		{
			FTerrainEditModel Model;
			const FTerrainEditSettings& S = Model.GetSettings();
			const FIntVector Chunk(8, 3, 0);
			const FTerrainGridWindow Window = FTerrainRemeshModel::ChunkWindow(Chunk, S, 8, 8);
			TestTrue(TEXT("X es la primera del chunk de render: faldón"), Window.First.X == 8 * 32 - 9 && Window.Dims.X == 42);
			TestTrue(TEXT("Y no"), Window.First.Y == 3 * 32 - 1 && Window.Dims.Y == 34);
			TestTrue(TEXT("Z también"), Window.First.Z == -9 && Window.Dims.Z == 42);
			// Ediciones dentro del faldón (en los chunks de guardado de abajo) y fuera de él.
			Model.SetSampleDeltaMm(FIntVector(8 * 32 - 9, 3 * 32 - 1, -9), 111);
			Model.SetSampleDeltaMm(FIntVector(8 * 32 - 5, 3 * 32 + 4, 2), -222);
			Model.SetSampleDeltaMm(FIntVector(8 * 32 - 10, 3 * 32, 0), 333);
			TArray<FTerrainGridDelta> Deltas;
			FTerrainRemeshModel::GatherDeltas(Model, Window, Deltas);
			float Sum = 0.0f;
			for (const FTerrainGridDelta& D : Deltas)
			{
				Sum += D.Delta;
			}
			TestEqual(TEXT("las dos de dentro"), Deltas.Num(), 2);
			TestEqual(TEXT("con su valor"), Sum, 0.111f - 0.222f, 1.0e-6f);
			const FTerrainGridDelta* Corner = Deltas.FindByPredicate([](const FTerrainGridDelta& D) { return D.Index == 0; });
			TestTrue(TEXT("la esquina del faldón es el índice 0"), Corner && FMath::IsNearlyEqual(Corner->Delta, 0.111f));
		});

		It("un chunk sin ediciones alrededor no copia nada", [this]()
		{
			FTerrainEditModel Model;
			Model.DigSphere(Dig(FVector(4.0, 4.0, 4.3)), Plane);
			TArray<FTerrainGridDelta> Deltas;
			FTerrainRemeshModel::GatherDeltas(Model, FIntVector(5, 5, 0), Deltas);
			TestEqual(TEXT("vacío"), Deltas.Num(), 0);
		});
	});

	Describe("malla", [this]()
	{
		It("normales del gradiente de la rejilla", [this]()
		{
			const FTerrainEditSettings S;
			const FDensityGrid Grid = PlaneGrid(S, FIntVector(0, 0, 0));
			TestEqual(TEXT("plano: arriba"), FTerrainRemeshModel::GridNormal(Grid, FVector(3.0, 2.0, 4.3)), FVector::UpVector, 1.0e-4f);
			TestEqual(TEXT("fuera de la rejilla se sujeta"), FTerrainRemeshModel::GridNormal(Grid, FVector(-50.0, 90.0, 4.3)), FVector::UpVector, 1.0e-4f);
			TestEqual(TEXT("NaN: arriba"), FTerrainRemeshModel::GridNormal(Grid, FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0)), FVector::UpVector, 1.0e-4f);
		});

		It("el plano sale en cm relativos al chunk y un golpe lo ahonda", [this]()
		{
			FTerrainEditModel Model;
			const FTerrainEditSettings& S = Model.GetSettings();
			const FIntVector Chunk(0, 0, 0);
			const FVector Origin = FTerrainRemeshModel::EditChunkOrigin(Chunk, S);
			const FTerrainMeshData Flat = FTerrainRemeshModel::BuildMesh(PlaneGrid(S, Chunk), Origin, nullptr);
			TestTrue(TEXT("hay superficie"), Flat.NumTriangles() > 0);
			TestEqual(TEXT("a 430 cm"), MinZ(Flat), 430.0f, 0.5f);
			bool bUp = true;
			for (const FVector3f& N : Flat.Normals)
			{
				bUp &= N.Z > 0.999f;
			}
			TestTrue(TEXT("normales arriba"), bUp);

			for (int32 I = 0; I < 4; ++I)
			{
				Model.DigSphere(Dig(FVector(4.0, 4.0, 4.3 - 0.4 * I)), Plane);
			}
			FDensityGrid Grid = PlaneGrid(S, Chunk);
			TArray<FTerrainGridDelta> Deltas;
			FTerrainRemeshModel::GatherDeltas(Model, Chunk, Deltas);
			FTerrainRemeshModel::ApplyDeltas(Deltas, Grid);
			const FTerrainMeshData Dug = FTerrainRemeshModel::BuildMesh(Grid, Origin, nullptr);
			TestTrue(FString::Printf(TEXT("el hueco baja más de 1 m (fondo a %.0f cm)"), MinZ(Dug)), MinZ(Dug) < 330.0f);
		});

		It("rejilla sin superficie: malla vacía", [this]()
		{
			const FTerrainEditSettings S;
			const FTerrainMeshData Empty = FTerrainRemeshModel::BuildMesh(PlaneGrid(S, FIntVector(0, 0, 3)), FVector::ZeroVector, nullptr);
			TestEqual(TEXT("sin triángulos"), Empty.NumTriangles(), 0);
		});
	});

	Describe("chunks de render y horneado", [this]()
	{
		It("8 chunks de edición por chunk de render, también en negativo", [this]()
		{
			const FTerrainChunkSettings Render;
			const FTerrainEditSettings Edit;
			TestEqual(TEXT("64 / 8"), FTerrainRemeshModel::EditChunksPerRenderChunk(Render, Edit), 8);
			FTerrainEditSettings Odd;
			Odd.CellsPerChunk = 30;
			TestEqual(TEXT("no encaja"), FTerrainRemeshModel::EditChunksPerRenderChunk(Render, Odd), 0);
			TestTrue(TEXT("7 → 0"), FTerrainRemeshModel::RenderChunkOf(FIntVector(7, 0, 0), 8) == FIntVector(0, 0, 0));
			TestTrue(TEXT("8 → 1"), FTerrainRemeshModel::RenderChunkOf(FIntVector(8, 0, 0), 8) == FIntVector(1, 0, 0));
			TestTrue(TEXT("-1 → -1"), FTerrainRemeshModel::RenderChunkOf(FIntVector(-1, -8, -9), 8) == FIntVector(-1, -1, -2));
		});

		It("lee las coordenadas del nombre de la malla horneada", [this]()
		{
			FIntVector Coord;
			TestTrue(TEXT("SM_Terrain_3_-2_0"), FTerrainRemeshModel::ParseBakedChunkName(TEXT("SM_Terrain_3_-2_0"), Coord) && Coord == FIntVector(3, -2, 0));
			TestFalse(TEXT("fondo marino"), FTerrainRemeshModel::ParseBakedChunkName(TEXT("SM_Terrain_DeepFloor"), Coord));
			TestFalse(TEXT("faltan piezas"), FTerrainRemeshModel::ParseBakedChunkName(TEXT("SM_Terrain_1_2"), Coord));
			TestFalse(TEXT("no es número"), FTerrainRemeshModel::ParseBakedChunkName(TEXT("SM_Terrain_1_2_x"), Coord));
			TestFalse(TEXT("demasiado largo"), FTerrainRemeshModel::ParseBakedChunkName(TEXT("SM_Terrain_1_2_123456789"), Coord));
			TestFalse(TEXT("otra malla"), FTerrainRemeshModel::ParseBakedChunkName(TEXT("SM_Rock_1_2_3"), Coord));
		});

		It("los chunks editados y sus vecinos dentro del chunk de render", [this]()
		{
			FTerrainEditModel Model;
			Model.SetSampleDeltaMm(FIntVector(0, 5, 5), 100);     // chunk de edición (0, 0, 0), junto al borde −X
			Model.SetSampleDeltaMm(FIntVector(-1, 5, 5), 100);    // chunk (−1, 0, 0), en el chunk de render (−1, 0, 0)
			TArray<FIntVector> Out;
			FTerrainRemeshModel::EditedEditChunks(Model, 8, FIntVector(0, 0, 0), Out);
			TestTrue(TEXT("el editado"), Out.Contains(FIntVector(0, 0, 0)));
			TestTrue(TEXT("un vecino"), Out.Contains(FIntVector(1, 1, 1)));
			TestFalse(TEXT("nada de otro chunk de render"), Out.Contains(FIntVector(-1, 0, 0)));
			TestEqual(TEXT("vecinos de (0,0,0) y de (−1,0,0) dentro: 2·2·2 + …"), Out.Num(), 8);
		});

		It("el sondeo encuentra la superficie del terreno real y descarta el cielo", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
			const FTerrainChunkSettings Render;
			const FTerrainEditSettings Edit;
			const FIslandDesc* Land = nullptr;
			for (const FIslandDesc& Island : Density.GetLayout().Islands)
			{
				Land = Island.Archetype == EIslandArchetype::Landing ? &Island : Land;
			}
			if (!TestNotNull(TEXT("isla de Landing"), Land))
			{
				return;
			}
			const FVector2D At = Land->Center + FVector2D(Land->Radius * 0.3, 0.0);
			const float Height = Density.SampleColumn(static_cast<float>(At.X), static_cast<float>(At.Y)).Height;
			const FVector Surface(At.X, At.Y, Height);
			const double Size = Render.ChunkSizeMeters();
			const FIntVector RenderChunk(FMath::FloorToInt32(Surface.X / Size), FMath::FloorToInt32(Surface.Y / Size), FMath::FloorToInt32(Surface.Z / Size));
			TArray<FIntVector> Found;
			FTerrainRemeshModel::SurfaceEditChunks(Density, Render, Edit, RenderChunk, Found);
			const double EditSize = Edit.ChunkSizeMeters();
			const FIntVector Expected(FMath::FloorToInt32(Surface.X / EditSize), FMath::FloorToInt32(Surface.Y / EditSize), FMath::FloorToInt32(Surface.Z / EditSize));
			TestTrue(TEXT("el chunk de la superficie"), Found.Contains(Expected));
			TestTrue(FString::Printf(TEXT("menos que el chunk entero (%d de 512)"), Found.Num()), Found.Num() > 0 && Found.Num() < 512);
			TArray<FIntVector> Sky;
			FTerrainRemeshModel::SurfaceEditChunks(Density, Render, Edit, RenderChunk + FIntVector(0, 0, 20), Sky);
			TestEqual(TEXT("a 1 km de altura no hay nada"), Sky.Num(), 0);
		});
	});
}

#endif
