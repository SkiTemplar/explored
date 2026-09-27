#include "Misc/AutomationTest.h"

#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/SurfaceNets.h"
#include "WorldGen/TerrainChunkBuilder.h"
#include "WorldGen/TerrainDensity.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace WorldGenTest
{
	constexpr uint32 OfficialSeed = 20260926;

	/** Rejilla de una esfera SDF con la convención de muestra extra de FSurfaceNets. */
	FDensityGrid SphereGrid(const FIntVector& CellOrigin, int32 Cells, float Radius, const FVector& Center)
	{
		FDensityGrid Grid;
		Grid.Init(FIntVector(Cells + 2), FVector(CellOrigin) - FVector(1.0f), 1.0f);
		for (int32 Z = 0; Z < Grid.Dims.Z; ++Z)
		{
			for (int32 Y = 0; Y < Grid.Dims.Y; ++Y)
			{
				for (int32 X = 0; X < Grid.Dims.X; ++X)
				{
					Grid.Set(X, Y, Z, FVector::Dist(Grid.SamplePosition(X, Y, Z), Center) - Radius);
				}
			}
		}
		return Grid;
	}

	/** Cuenta cuántas aristas no están compartidas exactamente por dos triángulos (posiciones soldadas). */
	int32 CountOpenEdges(const TArray<const FTerrainMeshData*>& Meshes)
	{
		auto Key = [](const FVector3f& P)
		{
			return FIntVector(FMath::RoundToInt32(P.X * 1000.0f), FMath::RoundToInt32(P.Y * 1000.0f), FMath::RoundToInt32(P.Z * 1000.0f));
		};
		TMap<TPair<FIntVector, FIntVector>, int32> EdgeUse;
		auto AddEdge = [&EdgeUse](const FIntVector& A, const FIntVector& B)
		{
			const bool bOrder = A.X < B.X || (A.X == B.X && (A.Y < B.Y || (A.Y == B.Y && A.Z < B.Z)));
			EdgeUse.FindOrAdd(bOrder ? TPair<FIntVector, FIntVector>(A, B) : TPair<FIntVector, FIntVector>(B, A))++;
		};
		for (const FTerrainMeshData* Mesh : Meshes)
		{
			for (int32 T = 0; T < Mesh->Indices.Num(); T += 3)
			{
				const FIntVector A = Key(Mesh->Positions[Mesh->Indices[T]]);
				const FIntVector B = Key(Mesh->Positions[Mesh->Indices[T + 1]]);
				const FIntVector C = Key(Mesh->Positions[Mesh->Indices[T + 2]]);
				AddEdge(A, B);
				AddEdge(B, C);
				AddEdge(C, A);
			}
		}
		int32 Open = 0;
		for (const auto& Pair : EdgeUse)
		{
			Open += Pair.Value != 2 ? 1 : 0;
		}
		return Open;
	}
}

BEGIN_DEFINE_SPEC(FWorldGenSpec, "Explored.WorldGen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FWorldGenSpec)

void FWorldGenSpec::Define()
{
	using namespace WorldGenTest;

	Describe("FArchipelagoLayout", [this]()
	{
		It("genera siete islas con arquetipos únicos", [this]()
		{
			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(OfficialSeed);
			TestEqual(TEXT("Número de islas"), Layout.Islands.Num(), static_cast<int32>(EIslandArchetype::Count));
			TSet<EIslandArchetype> Seen;
			for (const FIslandDesc& I : Layout.Islands)
			{
				Seen.Add(I.Archetype);
			}
			TestEqual(TEXT("Arquetipos distintos"), Seen.Num(), static_cast<int32>(EIslandArchetype::Count));
		});

		It("respeta el canal mínimo y los límites del mundo en muchas semillas", [this]()
		{
			for (uint32 Seed = 1; Seed <= 40; ++Seed)
			{
				const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(Seed);
				for (int32 A = 0; A < Layout.Islands.Num(); ++A)
				{
					const FIslandDesc& IA = Layout.Islands[A];
					const float Reach = FMath::Max(FMath::Abs(IA.Center.X), FMath::Abs(IA.Center.Y)) + IA.Radius;
					if (Reach > FArchipelagoLayout::WorldHalfExtent)
					{
						AddError(FString::Printf(TEXT("Semilla %u: isla %s fuera del mundo"), Seed, LexToString(IA.Archetype)));
						return;
					}
					for (int32 B = A + 1; B < Layout.Islands.Num(); ++B)
					{
						const FIslandDesc& IB = Layout.Islands[B];
						const float Gap = FVector2D::Distance(IA.Center, IB.Center) - IA.Radius - IB.Radius;
						if (Gap < FArchipelagoLayout::MinChannel - 1.0f)
						{
							AddError(FString::Printf(TEXT("Semilla %u: canal de %.1f m entre %s y %s"), Seed, Gap,
								LexToString(IA.Archetype), LexToString(IB.Archetype)));
							return;
						}
					}
				}
			}
		});

		It("es determinista", [this]()
		{
			const FArchipelagoLayout A = FArchipelagoLayout::Generate(OfficialSeed);
			const FArchipelagoLayout B = FArchipelagoLayout::Generate(OfficialSeed);
			for (int32 I = 0; I < A.Islands.Num(); ++I)
			{
				TestEqual(TEXT("Centro"), A.Islands[I].Center, B.Islands[I].Center);
				TestEqual(TEXT("Radio"), A.Islands[I].Radius, B.Islands[I].Radius);
			}
		});

		It("coloca Los Dientes con entre cinco y ocho islotes", [this]()
		{
			// El layout debe vivir en una variable: FindIsland devuelve un puntero a su interior.
			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(OfficialSeed);
			const FIslandDesc* Teeth = Layout.FindIsland(EIslandArchetype::Teeth);
			TestNotNull(TEXT("Existe"), Teeth);
			if (Teeth)
			{
				TestTrue(TEXT("Islotes"), Teeth->Islets.Num() >= 5 && Teeth->Islets.Num() <= 8);
			}
		});

		It("encadena las islas con estrechos navegables y una dorsal continua", [this]()
		{
			for (uint32 Seed = 1; Seed <= 40; ++Seed)
			{
				const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(Seed);
				TestEqual(TEXT("Dorsal"), Layout.Spine.Num(), Layout.Islands.Num() + 2);
				TestEqual(TEXT("Empieza en el volcán"), Layout.Islands[0].Archetype, EIslandArchetype::Smoke);
				TestEqual(TEXT("Acaba en el atolón"), Layout.Islands.Last().Archetype, EIslandArchetype::WhiteSands);
				for (int32 I = 0; I + 1 < Layout.Islands.Num(); ++I)
				{
					const FIslandDesc& A = Layout.Islands[I];
					const FIslandDesc& B = Layout.Islands[I + 1];
					const float Gap = FVector2D::Distance(A.Center, B.Center) - A.Radius - B.Radius;
					if (Gap > FArchipelagoLayout::MaxChainChannel + 1.0f)
					{
						AddError(FString::Printf(TEXT("Semilla %u: estrecho de %.0f m entre %s y %s"), Seed, Gap,
							LexToString(A.Archetype), LexToString(B.Archetype)));
						return;
					}
				}
			}
		});

		It("mantiene penínsulas y cayos dentro de su isla y lejos de las demás", [this]()
		{
			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(OfficialSeed);
			int32 CayCount = 0;
			for (const FIslandDesc& Island : Layout.Islands)
			{
				for (const FIslandLobe& Lobe : Island.Lobes)
				{
					TestTrue(TEXT("Península dentro de 1,1 radios"), Lobe.Offset.Size() + Lobe.Radius <= 1.1f + KINDA_SMALL_NUMBER);
				}
				for (const FCayDesc& Cay : Island.Cays)
				{
					++CayCount;
					for (const FIslandDesc& Other : Layout.Islands)
					{
						if (&Other != &Island)
						{
							TestTrue(TEXT("Cayo lejos de otras islas"), FVector2D::Distance(Other.Center, Cay.Center) > Other.Radius * 1.35f);
						}
					}
				}
			}
			TestTrue(TEXT("Hay cayos satélite"), CayCount >= 5);
		});

		It("hace emerger los cayos satélite", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(OfficialSeed));
			int32 Emerged = 0;
			int32 Total = 0;
			for (const FIslandDesc& Island : Density.GetLayout().Islands)
			{
				for (const FCayDesc& Cay : Island.Cays)
				{
					++Total;
					Emerged += Density.SampleColumn(Cay.Center.X, Cay.Center.Y).Height > 0.5f ? 1 : 0;
				}
			}
			TestEqual(TEXT("Cayos sobre el agua"), Emerged, Total);
		});
	});

	Describe("FTerrainDensity", [this]()
	{
		It("tiene tierra emergida cerca del centro de cada isla alta", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(OfficialSeed));
			for (const FIslandDesc& I : Density.GetLayout().Islands)
			{
				if (I.Archetype == EIslandArchetype::WhiteSands || I.Archetype == EIslandArchetype::Teeth)
				{
					continue; // Anillo y archipiélago de islotes: el centro puede ser agua.
				}
				// Busca la altura máxima en un disco del 30 % del radio.
				float Best = -1000.0f;
				for (int32 K = 0; K < 64; ++K)
				{
					const float A = K * UE_TWO_PI / 64.0f;
					const float R = I.Radius * 0.3f * (K % 4) / 3.0f;
					Best = FMath::Max(Best, Density.SampleColumn(I.Center.X + FMath::Cos(A) * R, I.Center.Y + FMath::Sin(A) * R).Height);
				}
				if (Best < 2.0f)
				{
					AddError(FString::Printf(TEXT("%s no emerge (máximo %.1f m)"), LexToString(I.Archetype), Best));
				}
			}
		});

		It("deja mar profundo lejos de las islas", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(OfficialSeed));
			const FTerrainColumn Corner = Density.SampleColumn(2950.0f, 2950.0f);
			TestTrue(TEXT("Fondo profundo en la esquina"), Corner.Height < -40.0f);
			TestTrue(TEXT("Agua sobre el fondo"), Density.Density(FVector(2950.0f, 2950.0f, -10.0f)) > 0.0f);
			TestTrue(TEXT("Sólido bajo el fondo"), Density.Density(FVector(2950.0f, 2950.0f, -120.0f)) < 0.0f);
		});

		It("da la misma densidad para la misma semilla", [this]()
		{
			const FTerrainDensity A(FArchipelagoLayout::Generate(OfficialSeed));
			const FTerrainDensity B(FArchipelagoLayout::Generate(OfficialSeed));
			const FVector P(123.4, -567.8, 9.1);
			TestEqual(TEXT("Densidad"), A.Density(P), B.Density(P));
		});

		It("genera cuevas en las islas montañosas", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(OfficialSeed));
			TestTrue(TEXT("Hay cuevas"), Density.GetCaves().Num() >= 8);
		});

		It("reparte las capas de textura: arena en la orilla, roca en los cortados y nunca más del 100 %", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(OfficialSeed));
			const FVector Up = FVector::UpVector;
			const FVector Cliff = FVector(1.0, 0.0, 0.2).GetSafeNormal();

			const FVector4f Beach = Density.SurfaceLayers(FVector(2950.0, 2950.0, 0.3), Up);
			TestTrue(TEXT("Arena en la línea de costa"), Beach.X > 0.8f);
			const FVector4f Wall = Density.SurfaceLayers(FVector(2950.0, 2950.0, 40.0), Cliff);
			TestTrue(TEXT("Roca en pendiente fuerte"), Wall.Z > 0.95f);

			for (const FIslandDesc& I : Density.GetLayout().Islands)
			{
				const FVector P(I.Center.X, I.Center.Y, 12.0);
				const FVector4f L = Density.SurfaceLayers(P, Up);
				const float Sum = L.X + L.Y + L.Z;
				if (Sum > 1.001f || L.X < 0.0f || L.Y < 0.0f || L.Z < 0.0f || L.W < 0.0f || L.W > 1.0f)
				{
					AddError(FString::Printf(TEXT("%s: capas fuera de rango (%.2f, %.2f, %.2f, %.2f)"),
						LexToString(I.Archetype), L.X, L.Y, L.Z, L.W));
				}
				if (I.Archetype == EIslandArchetype::Smoke)
				{
					TestEqual(TEXT("La Humeante es volcánica"), L.W, 1.0f);
				}
			}
		});
	});

	Describe("FSurfaceNets", [this]()
	{
		It("produce una esfera cerrada, bien orientada y en el radio correcto", [this]()
		{
			const FVector Center(8.0f, 8.0f, 8.0f);
			const FDensityGrid Grid = SphereGrid(FIntVector::ZeroValue, 16, 5.0f, Center);
			const FTerrainMeshData Mesh = FSurfaceNets::Polygonize(Grid);
			TestTrue(TEXT("Tiene triángulos"), Mesh.NumTriangles() > 100);
			TestEqual(TEXT("Aristas abiertas"), CountOpenEdges({&Mesh}), 0);

			int32 Misoriented = 0;
			for (int32 T = 0; T < Mesh.Indices.Num(); T += 3)
			{
				const FVector3f A = Mesh.Positions[Mesh.Indices[T]];
				const FVector3f B = Mesh.Positions[Mesh.Indices[T + 1]];
				const FVector3f C = Mesh.Positions[Mesh.Indices[T + 2]];
				const FVector3f Outward = (A + B + C) / 3.0f - FVector3f(Center);
				const float Dot = FVector3f::DotProduct(FVector3f::CrossProduct(B - A, C - A), Outward);
				Misoriented += (Dot > 0.0f) == FSurfaceNets::bFrontFaceCrossOpposesNormal ? 1 : 0;
			}
			TestEqual(TEXT("Triángulos mal orientados"), Misoriented, 0);

			for (const FVector3f& P : Mesh.Positions)
			{
				const float R = FVector3f::Dist(P, FVector3f(Center));
				if (FMath::Abs(R - 5.0f) > 0.35f)
				{
					AddError(FString::Printf(TEXT("Vértice a %.3f del centro"), R));
					return;
				}
			}
		});

		It("no deja grietas ni solapes entre dos chunks vecinos", [this]()
		{
			const FVector Center(16.0f, 8.0f, 8.0f);
			const FDensityGrid Left = SphereGrid(FIntVector(0, 0, 0), 16, 6.0f, Center);
			const FDensityGrid Right = SphereGrid(FIntVector(16, 0, 0), 16, 6.0f, Center);
			const FTerrainMeshData MeshL = FSurfaceNets::Polygonize(Left);
			const FTerrainMeshData MeshR = FSurfaceNets::Polygonize(Right);
			TestTrue(TEXT("Ambos tienen geometría"), !MeshL.IsEmpty() && !MeshR.IsEmpty());
			TestEqual(TEXT("Aristas abiertas en la unión"), CountOpenEdges({&MeshL, &MeshR}), 0);
		});

		It("devuelve una malla vacía para una rejilla uniforme", [this]()
		{
			FDensityGrid Grid;
			Grid.Init(FIntVector(6), FVector::ZeroVector, 1.0f);
			for (float& V : Grid.Values)
			{
				V = 1.0f;
			}
			TestTrue(TEXT("Uniforme"), Grid.IsUniform());
			TestTrue(TEXT("Vacía"), FSurfaceNets::Polygonize(Grid).IsEmpty());
		});
	});

	Describe("FTerrainChunkBuilder", [this]()
	{
		It("genera en la costa de la isla de inicio una malla con normales y colores", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(OfficialSeed));
			const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
			const FTerrainChunkSettings Settings;
			const float Size = Settings.ChunkSizeMeters();
			const FIntVector Coord(
				FMath::FloorToInt32(Landing->Center.X / Size),
				FMath::FloorToInt32(Landing->Center.Y / Size),
				0);
			const FTerrainMeshData Mesh = FTerrainChunkBuilder::Build(Density, Coord, Settings);
			TestFalse(TEXT("No vacía"), Mesh.IsEmpty());
			TestEqual(TEXT("Normales por vértice"), Mesh.Normals.Num(), Mesh.Positions.Num());
			TestEqual(TEXT("Colores por vértice"), Mesh.Colors.Num(), Mesh.Positions.Num());
		});

		It("encuentra chunks candidatos en tierra y omite el mar profundo", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(OfficialSeed));
			const FTerrainChunkSettings Settings;
			const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
			const FBox2D Rect(Landing->Center - FVector2D(64.0f), Landing->Center + FVector2D(64.0f));
			const TArray<FIntVector> Chunks = FTerrainChunkBuilder::FindCandidateChunks(Density, Settings, Rect);
			TestTrue(TEXT("Hay candidatos"), Chunks.Num() > 0);
			TestTrue(TEXT("Pocas capas verticales por columna"), Chunks.Num() <= 4 * 4 * 5);
		});
	});
}

#endif
