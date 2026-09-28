#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/SurfaceNets.h"
#include "WorldGen/TerrainChunkBuilder.h"
#include "WorldGen/TerrainDensity.h"
#include "WorldGen/TerrainSurvey.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TerrainRealismTest
{
	/** Archipiélago oficial muestreado cada 12 m y medido una sola vez para todas las pruebas. */
	struct FFixture
	{
		FTerrainDensity Density;
		FTerrainSampleGrid Grid;
		FTerrainRealismReport Report;

		FFixture()
			: Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed))
			, Grid(FTerrainSurvey::SampleWorld(Density, FArchipelagoLayout::WorldHalfExtent, 12.0f))
			, Report(FTerrainSurvey::Measure(Density, Grid))
		{
		}
	};

	const FFixture& Fixture()
	{
		static const FFixture Instance;
		return Instance;
	}

	int32 FindIsland(const FTerrainDensity& Density, EIslandArchetype Archetype)
	{
		const TArray<FIslandDesc>& Islands = Density.GetLayout().Islands;
		return Islands.IndexOfByPredicate([Archetype](const FIslandDesc& I) { return I.Archetype == Archetype; });
	}

	/** Aristas de un solo triángulo cerca del plano compartido por dos chunks vecinos en X: grietas en la unión. */
	int32 CountSeamEdges(const FTerrainMeshData& A, const FVector3f& OffsetA, const FTerrainMeshData& B, const FVector3f& OffsetB,
		float SeamX, float Band, const FVector3f& Min, const FVector3f& Max)
	{
		auto Key = [](const FVector3f& P)
		{
			return FIntVector(FMath::RoundToInt32(P.X * 0.5f), FMath::RoundToInt32(P.Y * 0.5f), FMath::RoundToInt32(P.Z * 0.5f));
		};
		TMap<TPair<FIntVector, FIntVector>, int32> Uses;
		TMap<TPair<FIntVector, FIntVector>, TPair<FVector3f, FVector3f>> Ends;
		auto Add = [&](const FVector3f& P, const FVector3f& Q)
		{
			const FIntVector KP = Key(P);
			const FIntVector KQ = Key(Q);
			const bool bOrder = KP.X < KQ.X || (KP.X == KQ.X && (KP.Y < KQ.Y || (KP.Y == KQ.Y && KP.Z < KQ.Z)));
			const TPair<FIntVector, FIntVector> Edge = bOrder ? TPair<FIntVector, FIntVector>(KP, KQ) : TPair<FIntVector, FIntVector>(KQ, KP);
			Uses.FindOrAdd(Edge)++;
			Ends.FindOrAdd(Edge) = TPair<FVector3f, FVector3f>(P, Q);
		};
		auto AddMesh = [&Add](const FTerrainMeshData& Mesh, const FVector3f& Offset)
		{
			for (int32 T = 0; T + 2 < Mesh.Indices.Num(); T += 3)
			{
				const FVector3f P0 = Mesh.Positions[Mesh.Indices[T]] + Offset;
				const FVector3f P1 = Mesh.Positions[Mesh.Indices[T + 1]] + Offset;
				const FVector3f P2 = Mesh.Positions[Mesh.Indices[T + 2]] + Offset;
				Add(P0, P1);
				Add(P1, P2);
				Add(P2, P0);
			}
		};
		AddMesh(A, OffsetA);
		AddMesh(B, OffsetB);
		int32 Open = 0;
		for (const auto& Pair : Uses)
		{
			const TPair<FVector3f, FVector3f>& E = Ends[Pair.Key];
			const bool bNearSeam = FMath::Abs(E.Key.X - SeamX) < Band && FMath::Abs(E.Value.X - SeamX) < Band;
			auto Inside = [&](const FVector3f& P) { return P.Y > Min.Y + Band && P.Y < Max.Y - Band && P.Z > Min.Z + Band && P.Z < Max.Z - Band; };
			Open += (Pair.Value == 1 && bNearSeam && Inside(E.Key) && Inside(E.Value)) ? 1 : 0;
		}
		return Open;
	}
}

BEGIN_DEFINE_SPEC(FTerrainRealismSpec, "Explored.WorldGen.Realism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FTerrainRealismSpec)

void FTerrainRealismSpec::Define()
{
	using namespace TerrainRealismTest;

	Describe("Fondo marino", [this]()
	{
		It("no deja bultos someros sueltos: los pocos que quedan no pasan de -2,3 m", [this]()
		{
			// Antes: 117 bultos sin isla (129 con el fondo a -30 m), alguno emergía hasta 16,5 m.
			const FTerrainRealismReport& R = Fixture().Report;
			TestTrue(*FString::Printf(TEXT("bultos sin explicar: %d"), R.UnexplainedBumps), R.UnexplainedBumps <= 8);
			TestTrue(*FString::Printf(TEXT("el más alto a %.2f m"), R.HighestUnexplainedBump), R.HighestUnexplainedBump < -2.3f);
		});

		It("no tiene cotas recortadas ni picos en el histograma de profundidad", [this]()
		{
			// Antes: el 7,8 % del fondo a -70 m exactos (OceanFloor) y un escalón en la dorsal.
			const FSeafloorHistogram& H = Fixture().Report.Seafloor;
			TestTrue(*FString::Printf(TEXT("cota dominante %.2f %% a %.1f m"), H.ModeFraction * 100.0f, H.ModeHeight), H.ModeFraction < 0.02f);
			TestTrue(*FString::Printf(TEXT("pico %.2f a %.1f m"), H.MaxSpike, H.SpikeHeight), H.MaxSpike < 1.85f);
		});

		It("es continuo: sin escalones al final de los cayos, del alcance de las islas ni en el atolón", [this]()
		{
			const FTerrainDensity& Density = Fixture().Density;
			FExploredRandom Rng(2026);
			float Worst = 0.0f;
			FVector2D Where = FVector2D::ZeroVector;
			for (int32 I = 0; I < 60000; ++I)
			{
				const float X = Rng.RangeFloat(-2950.0f, 2950.0f);
				const float Y = Rng.RangeFloat(-2950.0f, 2950.0f);
				const float A = Density.SampleColumn(X, Y).Height;
				const float B = Density.SampleColumn(X + 0.4f, Y + 0.3f).Height;
				if (A < -3.0f && B < -3.0f && FMath::Abs(A - B) > Worst)
				{
					Worst = FMath::Abs(A - B);
					Where = FVector2D(X, Y);
				}
			}
			TestTrue(*FString::Printf(TEXT("salto de %.2f m en medio metro en (%.0f, %.0f)"), Worst, Where.X, Where.Y), Worst < 1.6f);
		});
	});

	Describe("Islas", [this]()
	{
		It("no tienen mesetas lisas ni pozos de gota", [this]()
		{
			const FFixture& F = Fixture();
			for (int32 I = 0; I < F.Report.Islands.Num(); ++I)
			{
				const FIslandRealism& Isl = F.Report.Islands[I];
				const TCHAR* Name = LexToString(F.Density.GetLayout().Islands[I].Archetype);
				TestTrue(*FString::Printf(TEXT("%s: %.1f %% lisa"), Name, Isl.SmoothFraction * 100.0f), Isl.SmoothFraction < 0.03f);
				if (Isl.ErosionPitsAfter >= 0)
				{
					TestTrue(*FString::Printf(TEXT("%s: pozos %d → %d"), Name, Isl.ErosionPitsBefore, Isl.ErosionPitsAfter),
						Isl.ErosionPitsAfter <= Isl.ErosionPitsBefore + 2);
				}
			}
		});

		It("tienen los cauces de la erosión sin dirección preferente de la rejilla", [this]()
		{
			// Se mide en la rejilla de erosión (celdas rebajadas): en el mundo muestreado, el pie
			// de las paredes kársticas también cuenta como "cauce" y su orientación es la de las
			// torres, no la de la rejilla.
			const FFixture& F = Fixture();
			for (int32 I = 0; I < F.Density.GetLayout().Islands.Num(); ++I)
			{
				const FIslandReliefGrid* Relief = F.Density.GetRelief(I);
				if (!Relief || Relief->GetChannelOrientation().SampleCount < 1500)
				{
					continue; // sin erosión o pocas muestras: el histograma es ruido
				}
				const FOrientationStats& C = Relief->GetChannelOrientation();
				const TCHAR* Name = LexToString(F.Density.GetLayout().Islands[I].Archetype);
				TestTrue(*FString::Printf(TEXT("%s: ejes %.2f"), Name, C.AxisExcess), C.AxisExcess > 0.75f && C.AxisExcess < 1.3f);
				TestTrue(*FString::Printf(TEXT("%s: diagonales %.2f"), Name, C.DiagonalExcess), C.DiagonalExcess > 0.75f && C.DiagonalExcess < 1.35f);
			}
		});

		It("erosiona y talla ríos en las islas de tierra, no en el atolón ni en Los Dientes", [this]()
		{
			const FTerrainDensity& Density = Fixture().Density;
			for (EIslandArchetype A : {EIslandArchetype::Landing, EIslandArchetype::Emerald, EIslandArchetype::Smoke,
				EIslandArchetype::Mangrove, EIslandArchetype::Mesa})
			{
				TestNotNull(*FString::Printf(TEXT("%s erosionada"), LexToString(A)), Density.GetRelief(FindIsland(Density, A)));
			}
			TestNull(TEXT("atolón"), Density.GetRelief(FindIsland(Density, EIslandArchetype::WhiteSands)));
			TestNull(TEXT("islotes"), Density.GetRelief(FindIsland(Density, EIslandArchetype::Teeth)));
		});
	});

	Describe("Macizo kárstico (El Nido)", [this]()
	{
		It("tiene paredes casi a plomo, cumbres altas y una laguna interior bajo el mar", [this]()
		{
			const FFixture& F = Fixture();
			const int32 Mesa = FindIsland(F.Density, EIslandArchetype::Mesa);
			const FIslandDesc& Island = F.Density.GetLayout().Islands[Mesa];
			int32 Walls = 0;
			float Highest = 0.0f;
			int32 Lagoon = 0;
			for (float Y = -Island.Radius; Y <= Island.Radius; Y += 4.0f)
			{
				for (float X = -Island.Radius; X <= Island.Radius; X += 4.0f)
				{
					const FVector2D P = Island.Center + FVector2D(X, Y);
					const FTerrainColumn C = F.Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y));
					const float Next = F.Density.SampleColumn(static_cast<float>(P.X) + 4.0f, static_cast<float>(P.Y)).Height;
					Walls += FMath::Abs(Next - C.Height) > 24.0f ? 1 : 0; // más de 80° en 4 m
					Highest = FMath::Max(Highest, C.Height);
					Lagoon += (C.IslandIndex == Mesa && C.NormalizedDistance < 0.7f && C.Height < -1.5f) ? 1 : 0;
				}
			}
			TestTrue(*FString::Printf(TEXT("tramos de pared a plomo: %d"), Walls), Walls >= 150);
			TestTrue(*FString::Printf(TEXT("cumbre a %.0f m"), Highest), Highest > Island.MaxHeight * 0.5f);
			TestTrue(*FString::Printf(TEXT("celdas de laguna interior: %d"), Lagoon), Lagoon >= 30);
		});

		It("no abre grietas entre chunks vecinos en una pared caliza", [this]()
		{
			const FTerrainDensity& Density = Fixture().Density;
			const FIslandDesc& Island = Density.GetLayout().Islands[FindIsland(Density, EIslandArchetype::Mesa)];
			const FTerrainChunkSettings Settings;
			const float Size = Settings.ChunkSizeMeters();
			// Busca, a lo largo de las fronteras entre chunks, un tramo con una pared de caliza.
			for (float Y = -Island.Radius * 0.6f; Y <= Island.Radius * 0.6f; Y += Size)
			{
				for (float X = -Island.Radius * 0.6f; X <= Island.Radius * 0.6f; X += Size)
				{
					const int32 Cx = FMath::FloorToInt32((Island.Center.X + X) / Size);
					const int32 Cy = FMath::FloorToInt32((Island.Center.Y + Y) / Size);
					const float SeamX = (Cx + 1) * Size;
					const float MidY = (Cy + 0.5f) * Size;
					const float H = Density.SampleColumn(SeamX, MidY).Height;
					if (H < 40.0f || FMath::Abs(H - Density.SampleColumn(SeamX - 8.0f, MidY).Height) < 15.0f)
					{
						continue;
					}
					const int32 Cz = FMath::FloorToInt32(H / Size);
					const FTerrainMeshData Left = FTerrainChunkBuilder::Build(Density, FIntVector(Cx, Cy, Cz), Settings);
					const FTerrainMeshData Right = FTerrainChunkBuilder::Build(Density, FIntVector(Cx + 1, Cy, Cz), Settings);
					const FVector3f OriginL(FTerrainChunkBuilder::ChunkOrigin(FIntVector(Cx, Cy, Cz), Settings) * 100.0);
					const FVector3f OriginR(FTerrainChunkBuilder::ChunkOrigin(FIntVector(Cx + 1, Cy, Cz), Settings) * 100.0);
					const FVector3f Min(OriginL);
					const FVector3f Max = OriginL + FVector3f(2.0f * Size, Size, Size) * 100.0f;
					TestTrue(TEXT("hay superficie a ambos lados"), !Left.IsEmpty() && !Right.IsEmpty());
					TestEqual(TEXT("aristas abiertas en la unión"),
						CountSeamEdges(Left, OriginL, Right, OriginR, SeamX * 100.0f, Settings.VoxelSize * 100.0f * 1.5f, Min, Max), 0);
					return;
				}
			}
			AddError(TEXT("no se ha encontrado ninguna pared en una frontera de chunks"));
		});
	});
}

#endif
