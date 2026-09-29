#include "Misc/AutomationTest.h"

#include "HAL/PlatformTime.h"
#include "Save/SaveFormat.h"
#include "Save/SaveWorldDeltas.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainChunkBuilder.h"
#include "WorldGen/TerrainDensity.h"
#include "WorldGen/TerrainEdits.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace TerrainEditsSpecDetail
{
	/** Suelo plano en z = 0 (sólido debajo). */
	const auto Flat = [](const FVector& P) { return static_cast<float>(P.Z); };

	/** Roca maciza: la superficie queda 100 m por encima de todo lo que se prueba. */
	const auto Deep = [](const FVector& P) { return static_cast<float>(P.Z - 100.0); };

	/** Generador lineal congruente: los barridos aleatorios son siempre los mismos. */
	struct FLcg
	{
		uint64 State;
		explicit FLcg(uint64 Seed) : State(Seed * 6364136223846793005ull + 1442695040888963407ull) {}
		double Unit()
		{
			State = State * 6364136223846793005ull + 1442695040888963407ull;
			return static_cast<double>(State >> 11) / static_cast<double>(1ull << 53);
		}
		double Range(double Lo, double Hi) { return Lo + (Hi - Lo) * Unit(); }
	};

	FTerrainDigHit MakeHit(const FVector& At, ETerrainMaterial Material, ETerrainDigTool Tool)
	{
		FTerrainDigHit Hit;
		Hit.ImpactPoint = At;
		Hit.Material = Material;
		Hit.Tool = Tool;
		return Hit;
	}

	bool Contains(const TArray<FIntVector>& Set, const FIntVector& Chunk)
	{
		for (const FIntVector& C : Set)
		{
			if (C == Chunk)
			{
				return true;
			}
		}
		return false;
	}

	bool IsSubset(const TArray<FIntVector>& Sub, const TArray<FIntVector>& Super)
	{
		for (const FIntVector& C : Sub)
		{
			if (!Contains(Super, C))
			{
				return false;
			}
		}
		return true;
	}

	bool IsSortedUnique(const TArray<FIntVector>& Chunks)
	{
		for (int32 I = 1; I < Chunks.Num(); ++I)
		{
			const FIntVector& A = Chunks[I - 1];
			const FIntVector& B = Chunks[I];
			const bool bLess = A.Z != B.Z ? A.Z < B.Z : (A.Y != B.Y ? A.Y < B.Y : A.X < B.X);
			if (!bLess)
			{
				return false;
			}
		}
		return true;
	}

	/** Chunks de edición que leen alguna muestra cuyo delta difiere entre los dos estados (fuerza bruta). */
	TArray<FIntVector> ChunksWithChangedSamples(const FTerrainEditModel& Before, const FTerrainEditModel& After, const FBox& Box)
	{
		TArray<FIntVector> Out;
		const double H = After.GetSettings().CellSize;
		for (int32 Z = FMath::FloorToInt32(Box.Min.Z / H) - 1; Z <= FMath::CeilToInt32(Box.Max.Z / H) + 1; ++Z)
		{
			for (int32 Y = FMath::FloorToInt32(Box.Min.Y / H) - 1; Y <= FMath::CeilToInt32(Box.Max.Y / H) + 1; ++Y)
			{
				for (int32 X = FMath::FloorToInt32(Box.Min.X / H) - 1; X <= FMath::CeilToInt32(Box.Max.X / H) + 1; ++X)
				{
					const FIntVector G(X, Y, Z);
					if (Before.SampleDelta(G) != After.SampleDelta(G))
					{
						After.ChunksReadingSample(G, Out);
					}
				}
			}
		}
		return Out;
	}

	bool SameMesh(const FTerrainMeshData& A, const FTerrainMeshData& B)
	{
		if (A.Positions.Num() != B.Positions.Num() || A.Indices.Num() != B.Indices.Num())
		{
			return false;
		}
		for (int32 I = 0; I < A.Positions.Num(); ++I)
		{
			if (A.Positions[I].X != B.Positions[I].X || A.Positions[I].Y != B.Positions[I].Y || A.Positions[I].Z != B.Positions[I].Z)
			{
				return false;
			}
		}
		for (int32 I = 0; I < A.Indices.Num(); ++I)
		{
			if (A.Indices[I] != B.Indices[I])
			{
				return false;
			}
		}
		return true;
	}

	/** Mina de prueba: galerías en tierra, un paso de pico en caliza y un camino que se vuelve a picar. */
	void BuildMine(FTerrainEdits& Edits)
	{
		for (int32 I = 0; I < 40; ++I)
		{
			const FVector At(-7.6 + 0.3 * I, 0.2 * (I % 3), -0.2 * (I % 4));
			Edits.Dig(MakeHit(At, ETerrainMaterial::Tierra, ETerrainDigTool::PalaTosca), Flat);
		}
		for (int32 I = 0; I < 6; ++I)
		{
			Edits.Dig(MakeHit(FVector(20.0, -4.0 + 0.4 * I, -1.0), ETerrainMaterial::Caliza, ETerrainDigTool::PicoPiedra), Flat);
		}
		FShovelStroke Stroke;
		Stroke.Center = FVector(-3.0, 5.0, 0.0);
		Edits.GetModel().Shovel(Stroke, Flat);
		Edits.GetModel().Shovel(Stroke, Flat);
		Stroke.Center = FVector(-3.0, 9.0, 0.0);
		Edits.GetModel().Shovel(Stroke, Flat);
	}

	/** Blob de la versión 2 escrito a mano, para fabricar entradas manipuladas. */
	struct FBlob
	{
		TArray<uint8> Bytes;
		FBlob& U(uint64 V)
		{
			while (V >= 0x80) { Bytes.Add(static_cast<uint8>(V | 0x80)); V >>= 7; }
			Bytes.Add(static_cast<uint8>(V));
			return *this;
		}
		FBlob& S(int64 V) { return U((static_cast<uint64>(V) << 1) ^ static_cast<uint64>(V >> 63)); }
		FString Text() const { return FSaveBase64::Encode(Bytes); }
	};

	FSaveValue V2(const FString& Chunks, int64 Samples, const FString& Paths = FBlob().U(0).Text(), int64 Columns = 0)
	{
		FSaveValue Root = FSaveValue::MakeObject();
		Root.Set(TEXT("v"), FSaveValue::MakeInt(2));
		Root.Set(TEXT("cell"), FSaveValue::MakeFloat(0.25f));
		Root.Set(TEXT("n"), FSaveValue::MakeInt(32));
		Root.Set(TEXT("samples"), FSaveValue::MakeInt(Samples));
		Root.Set(TEXT("columns"), FSaveValue::MakeInt(Columns));
		Root.Set(TEXT("chunks"), FSaveValue::MakeString(Chunks));
		Root.Set(TEXT("paths"), FSaveValue::MakeString(Paths));
		return Root;
	}

	/** Carga el valor en un modelo nuevo: true si lo acepta. Si lo rechaza, el modelo tiene que quedar vacío. */
	bool Accepts(const FSaveValue& Value, bool& bOutEmptyOnFailure)
	{
		FTerrainEditModel Model;
		const bool bOk = Model.FromValue(Value);
		bOutEmptyOnFailure = bOk || Model.IsEmpty();
		return bOk;
	}
}

BEGIN_DEFINE_SPEC(FTerrainEditsSpec, "Explored.TerrainEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTerrainEditsSpec)

void FTerrainEditsSpec::Define()
{
	using namespace TerrainEditsSpecDetail;

	Describe("la densidad del mundo", [this]()
	{
		It("sin capa, o con una capa vacía, es exactamente el campo procedural", [this]()
		{
			FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
			const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
			if (!TestNotNull(TEXT("Landing"), Landing))
			{
				return;
			}
			const FVector C(Landing->Center.X, Landing->Center.Y, 0.0);
			TArray<FVector> Points;
			for (int32 I = 0; I < 64; ++I)
			{
				Points.Add(C + FVector(13.7 * (I % 8), -9.1 * (I / 8), -6.0 + 0.37 * I));
			}
			bool bSame = true;
			for (const FVector& P : Points)
			{
				bSame &= Density.Density(P) == Density.ProceduralDensity(P);
			}
			TestTrue(TEXT("sin capa"), bSame);
			TestNull(TEXT("sin capa enganchada"), Density.GetEdits());

			Density.SetEdits(MakeShared<FTerrainEdits>());
			for (const FVector& P : Points)
			{
				bSame &= Density.Density(P) == Density.ProceduralDensity(P);
				bSame &= Density.DensityWithColumn(P, Density.SampleColumn(P.X, P.Y)) == Density.ProceduralDensity(P);
			}
			TestTrue(TEXT("con capa vacía"), bSame);
		});

		It("consulta las ediciones: el hueco se ve en Density, en las normales y en el chunk de render", [this]()
		{
			FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
			const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
			if (!TestNotNull(TEXT("Landing"), Landing))
			{
				return;
			}
			const FTerrainChunkSettings Render;
			// Una muestra de la rejilla de render (múltiplo de 2 m) justo bajo la superficie de Landing.
			const double X = FMath::RoundToDouble(Landing->Center.X / 2.0) * 2.0;
			const double Y = FMath::RoundToDouble(Landing->Center.Y / 2.0) * 2.0;
			const float Height = Density.SampleColumn(static_cast<float>(X), static_cast<float>(Y)).Height;
			const FVector Target(X, Y, FMath::FloorToDouble(Height / 2.0) * 2.0);
			const FIntVector RenderChunk(FMath::FloorToInt32(Target.X / Render.ChunkSizeMeters()),
				FMath::FloorToInt32(Target.Y / Render.ChunkSizeMeters()), FMath::FloorToInt32(Target.Z / Render.ChunkSizeMeters()));
			const FTerrainMeshData Before = FTerrainChunkBuilder::Build(Density, RenderChunk, Render);
			const FIntVector FarChunk = RenderChunk + FIntVector(3, 0, 0);
			const FTerrainMeshData FarBefore = FTerrainChunkBuilder::Build(Density, FarChunk, Render);
			TestTrue(TEXT("la muestra de render es sólida antes de cavar"), Density.Density(Target) < 0.0f);

			TSharedPtr<FTerrainEdits> Edits = MakeShared<FTerrainEdits>();
			Density.SetEdits(Edits);
			TArray<FIntVector> Touched;
			int32 Hits = 0;
			while (Density.Density(Target) <= 0.0f && Hits < 60)
			{
				const FTerrainDigResult R = Edits->Dig(MakeHit(Target, ETerrainMaterial::Tierra, ETerrainDigTool::PicoRescatado), Density);
				Touched.Append(R.RenderChunks);
				++Hits;
			}
			AddInfo(FString::Printf(TEXT("golpes hasta abrir la muestra de render: %d"), Hits));
			TestTrue(TEXT("la muestra queda en aire"), Density.Density(Target) > 0.0f);
			TestTrue(TEXT("el chunk de render está entre los invalidados"), Contains(Touched, RenderChunk));
			TestFalse(TEXT("el lejano no"), Contains(Touched, FarChunk));

			// Density con capa = base procedural + delta de la capa, bit a bit.
			FLcg Random(11);
			bool bExact = true;
			bool bChangedSomewhere = false;
			for (int32 I = 0; I < 400; ++I)
			{
				const FVector P = Target + FVector(Random.Range(-1.0, 1.0), Random.Range(-1.0, 1.0), Random.Range(-1.0, 1.0));
				const float WithEdits = Density.Density(P);
				bExact &= WithEdits == Edits->Density(P, [&Density](const FVector& Q) { return Density.ProceduralDensity(Q); });
				bChangedSomewhere |= WithEdits != Density.ProceduralDensity(P);
			}
			TestTrue(TEXT("Density = procedural + delta"), bExact);
			TestTrue(TEXT("el hueco cambia la densidad"), bChangedSomewhere);
			TestTrue(TEXT("la normal del hueco apunta hacia su centro, no hacia arriba"),
				(Density.Normal(Target + FVector(0.3, 0.0, 0.0), 0.1f) | FVector(-1.0, 0.0, 0.0)) > 0.3);

			TestFalse(TEXT("la malla del chunk cambia"), SameMesh(Before, FTerrainChunkBuilder::Build(Density, RenderChunk, Render)));
			TestTrue(TEXT("la del lejano no"), SameMesh(FarBefore, FTerrainChunkBuilder::Build(Density, FarChunk, Render)));
			Density.SetEdits(nullptr);
			TestTrue(TEXT("al soltar la capa vuelve el terreno original"), SameMesh(Before, FTerrainChunkBuilder::Build(Density, RenderChunk, Render)));
		});

		It("los chunks de render con ediciones incluyen una mina muy por debajo de la superficie", [this]()
		{
			FTerrainEdits Edits;
			Edits.Dig(MakeHit(FVector(10.0, 10.0, -150.0), ETerrainMaterial::Basalto, ETerrainDigTool::PicoTallado), Deep);
			const TArray<FIntVector> Chunks = Edits.EditedRenderChunks();
			TestTrue(TEXT("el de la mina"), Contains(Chunks, FIntVector(0, 0, -3)));
			TestTrue(TEXT("ordenados y sin repetir"), IsSortedUnique(Chunks));
		});
	});

	Describe("el picado por esfera", [this]()
	{
		It("usa el radio y el tiempo de golpe de cada herramienta (biblia 02 §2.2)", [this]()
		{
			struct FRow { ETerrainDigTool Tool; int32 Tier; float Radius; float Seconds; };
			const FRow Rows[] = {
				{ ETerrainDigTool::PalaTosca, 1, 0.35f, 1.2f },
				{ ETerrainDigTool::PicoPiedra, 2, 0.40f, 1.3f },
				{ ETerrainDigTool::PicoTallado, 3, 0.42f, 1.2f },
				{ ETerrainDigTool::PicoObsidiana, 4, 0.50f, 1.0f },
				{ ETerrainDigTool::PicoRescatado, 4, 0.55f, 1.1f },
			};
			for (const FRow& Row : Rows)
			{
				const FTerrainDigToolInfo& Info = FTerrainEdits::ToolInfo(Row.Tool);
				TestEqual(TEXT("nivel"), Info.ToolTier, Row.Tier);
				TestEqual(TEXT("radio"), Info.Radius, Row.Radius);
				TestEqual(TEXT("segundos"), Info.SecondsPerHit, Row.Seconds);

				// La esfera no toca nada más allá de su radio, y el golpe dura lo de la tabla.
				FTerrainEdits Edits;
				const FVector At(0.13, -0.07, 0.02);
				const FTerrainDigResult R = Edits.Dig(MakeHit(At, ETerrainMaterial::Arena, Row.Tool), Flat);
				TestTrue(TEXT("cava"), R.Edit.Changed());
				TestEqual(TEXT("duración"), R.Seconds, Row.Seconds);
				bool bInside = true;
				for (const FIntVector& Chunk : Edits.GetModel().EditedChunks())
				{
					const int32 N = Edits.GetModel().GetSettings().CellsPerChunk;
					for (int32 Local = 0; Local < N * N * N; ++Local)
					{
						const FIntVector G = Chunk * N + FIntVector(Local % N, (Local / N) % N, Local / (N * N));
						if (Edits.GetModel().SampleDelta(G) != 0.0f)
						{
							bInside &= (Edits.GetModel().SamplePosition(G) - At).Size() < Edits.GetModel().SphereDigReach(Row.Radius);
						}
					}
				}
				TestTrue(TEXT("solo muestras dentro del radio más media celda"), bInside);
			}
		});

		It("en superficie blanda vacía la esfera entera, y rompe el suelo en un hueco redondo", [this]()
		{
			FTerrainEdits Edits;
			const FVector At(0.0, 0.0, 0.0);
			const FTerrainDigResult R = Edits.Dig(MakeHit(At, ETerrainMaterial::Tierra, ETerrainDigTool::PicoPiedra), Flat);
			// Media esfera de 0,40 m (≈ 0,134 m³) cabe en el tope de 1/6 m³: sale entera.
			TestTrue(TEXT("por debajo del tope"), R.Edit.VolumeRemoved <= R.MaxVolume + 1.0e-6);
			bool bOpen = true;
			for (double Z = -0.30; Z <= 0.0; Z += 0.1)
			{
				for (double X = -0.25; X <= 0.25; X += 0.25)
				{
					if (FVector(X, 0.0, Z).Size() < 0.30)
					{
						bOpen &= Edits.Density(FVector(X, 0.0, Z), Flat) > 0.0f;
					}
				}
			}
			TestTrue(TEXT("aire dentro de la esfera"), bOpen);
			TestTrue(TEXT("roca fuera de ella"), Edits.Density(FVector(0.0, 0.0, -0.75), Flat) < 0.0f);
		});

		It("bajo tierra arranca justo el sólido de un golpe: 1 / golpes por m³ del material", [this]()
		{
			struct FRow { ETerrainMaterial Material; ETerrainDigTool Tool; float Hits; };
			// Espejo de mining.json/materials/hitsPerM3 (lo comprueba DataCheck contra la fórmula).
			const FRow Rows[] = {
				{ ETerrainMaterial::Tierra, ETerrainDigTool::PalaTosca, 6.0f },
				{ ETerrainMaterial::Arena, ETerrainDigTool::PalaTosca, 6.0f },
				{ ETerrainMaterial::Caliza, ETerrainDigTool::PicoPiedra, 12.0f },
				{ ETerrainMaterial::Caliza, ETerrainDigTool::PicoTallado, 8.0f },
				{ ETerrainMaterial::Basalto, ETerrainDigTool::PicoTallado, 18.0f },
				{ ETerrainMaterial::Basalto, ETerrainDigTool::PicoRescatado, 12.0f },
				{ ETerrainMaterial::Obsidiana, ETerrainDigTool::PicoObsidiana, 24.0f },
			};
			for (const FRow& Row : Rows)
			{
				TestEqual(TEXT("golpes por m³"), FTerrainEdits::HitsPerCubicMeter(Row.Material, Row.Tool), Row.Hits, 1.0e-3f);
				TestEqual(TEXT("segundos por m³"), FTerrainEdits::SecondsPerCubicMeter(Row.Material, Row.Tool),
					Row.Hits * FTerrainEdits::ToolInfo(Row.Tool).SecondsPerHit, 1.0e-3f);
				FTerrainEdits Edits;
				const FTerrainDigResult R = Edits.Dig(MakeHit(FVector(0.1, 0.2, -5.0), Row.Material, Row.Tool), Deep);
				TestEqual(TEXT("tope del golpe"), R.MaxVolume, 1.0 / Row.Hits, 1.0e-6);
				TestTrue(TEXT("arranca el tope (±2 %)"), FMath::Abs(R.Edit.VolumeRemoved - R.MaxVolume) <= 0.02 * R.MaxVolume);
			}
		});

		It("rebota sin tocar nada si la herramienta no llega al estrato, pero el golpe cuesta su tiempo", [this]()
		{
			FTerrainEdits Edits;
			const FTerrainDigResult Shovel = Edits.Dig(MakeHit(FVector::ZeroVector, ETerrainMaterial::Caliza, ETerrainDigTool::PalaTosca), Flat);
			TestTrue(TEXT("pala contra caliza"), Shovel.Edit.bRejected && !Shovel.Edit.Changed());
			TestEqual(TEXT("dura un golpe de pala"), Shovel.Seconds, 1.2f);
			TestTrue(TEXT("sin chunks que remallar"), Shovel.RenderChunks.IsEmpty() && Shovel.Edit.DirtyChunks.IsEmpty());
			TestTrue(TEXT("pico de piedra contra basalto"),
				Edits.Dig(MakeHit(FVector::ZeroVector, ETerrainMaterial::Basalto, ETerrainDigTool::PicoPiedra), Flat).Edit.bRejected);
			TestTrue(TEXT("pico tallado contra obsidiana"),
				Edits.Dig(MakeHit(FVector::ZeroVector, ETerrainMaterial::Obsidiana, ETerrainDigTool::PicoTallado), Flat).Edit.bRejected);
			TestTrue(TEXT("nada editado"), Edits.IsEmpty());
			TestFalse(TEXT("la pala sí cava tierra, arena y arcilla (dureza 1)"),
				Edits.Dig(MakeHit(FVector::ZeroVector, ETerrainMaterial::Tierra, ETerrainDigTool::PalaTosca), Flat).Edit.bRejected);
		});

		It("un camino compactado cuesta más de picar", [this]()
		{
			FTerrainEdits Plain;
			FTerrainEdits Path;
			FShovelStroke Stroke;
			Stroke.Center = FVector(0.0, 0.0, -2.0);
			Stroke.VerticalReach = 0.25f;
			Path.GetModel().Shovel(Stroke, Deep);
			Path.GetModel().Shovel(Stroke, Deep);
			TestTrue(TEXT("es camino"), Path.GetModel().IsPath(0.1, 0.1));
			// Con la pala (herramienta mínima): con un pico mejor manda el suelo de 6 golpes por m³.
			const FTerrainDigHit Hit = MakeHit(FVector(0.1, 0.1, -3.0), ETerrainMaterial::Tierra, ETerrainDigTool::PalaTosca);
			const FTerrainDigResult A = Plain.Dig(Hit, Deep);
			const FTerrainDigResult B = Path.Dig(Hit, Deep);
			TestTrue(TEXT("menos sólido por golpe"), B.MaxVolume < A.MaxVolume && B.Edit.VolumeRemoved < A.Edit.VolumeRemoved);
		});

		It("ignora golpes con coordenadas o volumen no finitos", [this]()
		{
			FTerrainEdits Edits;
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			TestFalse(TEXT("impacto NaN"),
				Edits.Dig(MakeHit(FVector(NaN, 0.0, 0.0), ETerrainMaterial::Tierra, ETerrainDigTool::PalaTosca), Flat).Edit.Changed());
			TestFalse(TEXT("impacto infinito"),
				Edits.Dig(MakeHit(FVector(0.0, 0.0, std::numeric_limits<double>::infinity()), ETerrainMaterial::Tierra, ETerrainDigTool::PalaTosca), Flat).Edit.Changed());
			FSphereDig Sphere;
			Sphere.Radius = NaN;
			TestFalse(TEXT("radio NaN"), Edits.GetModel().DigSphere(Sphere, Flat).Changed());
			Sphere.Radius = 0.4f;
			Sphere.MaxVolume = -1.0;
			TestFalse(TEXT("tope negativo"), Edits.GetModel().DigSphere(Sphere, Flat).Changed());
			// Finito pero fuera del mundo: sus chunks no caben en el int16 del códec de red.
			Sphere.MaxVolume = 0.0;
			Sphere.Center = FVector(3.0e5, 0.0, 0.0);
			const FTerrainEditResult Far = Edits.GetModel().DigSphere(Sphere, Flat);
			TestFalse(TEXT("esfera fuera del mundo"), Far.Changed());
			TestTrue(TEXT("y se marca como rechazada"), Far.bRejected);
			// Radio desmesurado: con MaxVolume = 0 («sin tope») vaciaba unos 7000 m³ de una vez.
			Sphere.Center = FVector(0.1, 0.1, -3.0);
			Sphere.Radius = 12.0f;
			TestTrue(TEXT("radio de 12 m (> MaxBrushExtent)"), Edits.GetModel().DigSphere(Sphere, Flat).bRejected);
			// Tope NaN: con -ffast-math `!(MaxVolume >= 0)` lo dejaba pasar y salía la esfera entera.
			Sphere.Radius = 0.5f;
			Sphere.MaxVolume = std::numeric_limits<double>::quiet_NaN();
			TestFalse(TEXT("tope NaN"), Edits.GetModel().DigSphere(Sphere, Flat).Changed());
			TestTrue(TEXT("nada editado"), Edits.IsEmpty());
			// El pico y la pala de la PR #40 pasan por la misma consulta de camino.
			FPickaxeHit NaNHit;
			NaNHit.ImpactPoint = FVector(NaN);
			TestFalse(TEXT("pico con impacto NaN"), Edits.GetModel().Pickaxe(NaNHit, Flat).Changed());
			TestEqual(TEXT("compactación en NaN"), Edits.GetModel().Compaction(NaN, 0.0), 0);
			FShovelStroke Stroke;
			Stroke.Center = FVector(0.0, NaN, 0.0);
			TestFalse(TEXT("pala con centro NaN"), Edits.GetModel().Shovel(Stroke, Flat).Changed());
			TestTrue(TEXT("y no deja un camino fantasma"), Edits.GetModel().NumPathColumns() == 0);
			float Delta = 1.0f;
			TestFalse(TEXT("DeltaAt con NaN"), Edits.DeltaAt(FVector(NaN), Delta));
			TestFalse(TEXT("DeltaAt fuera de rango"), Edits.DeltaAt(FVector(1.0e12, 0.0, 0.0), Delta));
			TestEqual(TEXT("delta 0"), Delta, 0.0f);
		});

		It("suelta tierra_suelta, arena o piedra, 6 unidades por m³ sin perder los restos", [this]()
		{
			TestEqual(TEXT("tierra"), FString(FTerrainEdits::LootItemId(ETerrainMaterial::Tierra)), FString(TEXT("tierra_suelta")));
			TestEqual(TEXT("arena"), FString(FTerrainEdits::LootItemId(ETerrainMaterial::Arena)), FString(TEXT("arena")));
			TestEqual(TEXT("caliza"), FString(FTerrainEdits::LootItemId(ETerrainMaterial::Caliza)), FString(TEXT("caliza")));
			TestEqual(TEXT("basalto"), FString(FTerrainEdits::LootItemId(ETerrainMaterial::Basalto)), FString(TEXT("basalto")));
			TestEqual(TEXT("obsidiana"), FString(FTerrainEdits::LootItemId(ETerrainMaterial::Obsidiana)), FString(TEXT("obsidiana")));

			// Seis golpes bajo tierra con la pala son 1 m³: seis unidades, aunque el centro caiga en una muestra.
			FTerrainEdits Edits;
			double Remainder = 0.0;
			int32 Units = 0;
			for (int32 I = 0; I < 6; ++I)
			{
				const FTerrainDigResult R = Edits.Dig(MakeHit(FVector(3.0 * I, 0.0, -5.0), ETerrainMaterial::Tierra, ETerrainDigTool::PalaTosca), Deep);
				Units += FTerrainEdits::TakeLootUnits(R.Edit.VolumeRemoved, Remainder);
			}
			TestEqual(TEXT("seis unidades"), Units, 6);
			double Rest = 0.0;
			TestEqual(TEXT("media unidad no da nada…"), FTerrainEdits::TakeLootUnits(0.5 / 6.0, Rest), 0);
			TestEqual(TEXT("…pero se guarda para el siguiente golpe"), FTerrainEdits::TakeLootUnits(0.5 / 6.0, Rest), 1);
			Rest = 0.25;
			TestEqual(TEXT("NaN no da nada"), FTerrainEdits::TakeLootUnits(std::numeric_limits<double>::quiet_NaN(), Rest), 0);
			TestEqual(TEXT("ni borra el resto"), Rest, 0.25);
			TestEqual(TEXT("volumen negativo tampoco"), FTerrainEdits::TakeLootUnits(-1.0, Rest), 0);
		});

		It("el servidor descarta los golpes más rápidos que el 85 % de su duración", [this]()
		{
			TestTrue(TEXT("a su ritmo"), FTerrainEdits::IsCadenceValid(ETerrainDigTool::PicoPiedra, 1.3f));
			TestTrue(TEXT("con la tolerancia"), FTerrainEdits::IsCadenceValid(ETerrainDigTool::PicoPiedra, 1.3f * 0.85f + 1.0e-4f));
			TestFalse(TEXT("demasiado rápido"), FTerrainEdits::IsCadenceValid(ETerrainDigTool::PicoPiedra, 1.0f));
			TestFalse(TEXT("NaN"), FTerrainEdits::IsCadenceValid(ETerrainDigTool::PicoObsidiana, std::numeric_limits<float>::quiet_NaN()));
			TestTrue(TEXT("la obsidiana es más rápida"), FTerrainEdits::IsCadenceValid(ETerrainDigTool::PicoObsidiana, 0.9f));
		});

		It("es determinista: los mismos golpes dan el mismo guardado, y la misma suma por chunk", [this]()
		{
			FTerrainEdits A;
			FTerrainEdits B;
			BuildMine(A);
			BuildMine(B);
			TestTrue(TEXT("mismo estado"), A == B);
			TestEqual(TEXT("mismo texto"), FSaveText::Write(A.GetModel().ToValue()), FSaveText::Write(B.GetModel().ToValue()));
			bool bSameChecksums = true;
			for (const FIntVector& Chunk : A.GetModel().EditedChunks())
			{
				bSameChecksums &= A.ChunkChecksum(Chunk) == B.ChunkChecksum(Chunk);
			}
			TestTrue(TEXT("misma suma"), bSameChecksums);

			const FIntVector First = A.GetModel().EditedChunks()[0];
			const uint32 Before = A.ChunkChecksum(First);
			A.Dig(MakeHit(A.GetModel().SamplePosition(First * 32 + FIntVector(16)), ETerrainMaterial::Tierra, ETerrainDigTool::PicoRescatado), Deep);
			TestTrue(TEXT("un golpe cambia la suma"), A.ChunkChecksum(First) != Before);
			TestTrue(TEXT("chunk sin editar: base de FNV-1a"), A.ChunkChecksum(FIntVector(1000, 0, 0)) == 2166136261u);
		});
	});

	Describe("los chunks a invalidar", [this]()
	{
		It("son 1 en el centro de un chunk, 2 en una cara, 4 en una arista y 8 en una esquina", [this]()
		{
			const FTerrainEditModel Model;
			struct FCase { FVector Center; int32 Expected; };
			const FCase Cases[] = {
				{ FVector(4.0, 4.0, 4.0), 1 },
				{ FVector(8.0, 4.0, 4.0), 2 },
				{ FVector(8.0, 8.0, 4.0), 4 },
				{ FVector(8.0, 8.0, 8.0), 8 },
				{ FVector(0.0, 0.0, 0.0), 8 },
				{ FVector(-8.0, -8.0, -8.0), 8 },
				{ FVector(-4.0, 16.0, -4.0), 2 },
			};
			for (const FCase& Case : Cases)
			{
				TArray<FIntVector> Chunks;
				Model.ChunksTouchedBySphere(Case.Center, 0.4f, Chunks);
				TestEqual(*FString::Printf(TEXT("chunks en (%.0f, %.0f, %.0f)"), Case.Center.X, Case.Center.Y, Case.Center.Z), Chunks.Num(), Case.Expected);
				TestTrue(TEXT("ordenados y sin repetir"), IsSortedUnique(Chunks));
			}
			TArray<FIntVector> Corner;
			Model.ChunksTouchedBySphere(FVector(0.0, 0.0, 0.0), 0.4f, Corner);
			TestTrue(TEXT("la esquina del origen toca (-1, -1, -1) y (0, 0, 0)"),
				Contains(Corner, FIntVector(-1, -1, -1)) && Contains(Corner, FIntVector(0, 0, 0)) && Contains(Corner, FIntVector(-1, 0, -1)));
			// La muestra 31 (7,75 m) la lee también el chunk 1: una esfera que solo la alcanza a ella ya son dos.
			TArray<FIntVector> NearFace;
			Model.ChunksTouchedBySphere(FVector(7.6, 4.0, 4.0), 0.2f, NearFace);
			TestEqual(TEXT("la muestra de solape del borde"), NearFace.Num(), 2);
			TArray<FIntVector> None;
			Model.ChunksTouchedBySphere(FVector(4.0), 0.0f, None);
			Model.ChunksTouchedBySphere(FVector(4.0), std::numeric_limits<float>::quiet_NaN(), None);
			TestTrue(TEXT("radio nulo o NaN: nada"), None.IsEmpty());
		});

		It("contienen lo que ensucia de verdad cada golpe, y los sucios son justo los que leen una muestra cambiada", [this]()
		{
			FLcg Random(0xC0FFEE);
			const ETerrainDigTool Tools[] = { ETerrainDigTool::PalaTosca, ETerrainDigTool::PicoPiedra, ETerrainDigTool::PicoRescatado };
			FTerrainEdits Edits;
			int32 Violations = 0;
			int32 Mismatches = 0;
			int32 CornerHits = 0;
			for (int32 I = 0; I < 300; ++I)
			{
				// Cerca de caras, aristas y esquinas de chunk (múltiplos de 8 m), en roca y junto a la superficie.
				const FVector At(8.0 * FMath::FloorToDouble(Random.Range(-2.0, 2.0)) + Random.Range(-0.6, 0.6),
					8.0 * FMath::FloorToDouble(Random.Range(-2.0, 2.0)) + Random.Range(-0.6, 0.6),
					Random.Range(-0.6, 0.6));
				const ETerrainDigTool Tool = Tools[I % 3];
				TArray<FIntVector> EditChunks;
				TArray<FIntVector> RenderChunks;
				Edits.ChunksToInvalidate(At, Tool, EditChunks, RenderChunks);
				CornerHits += EditChunks.Num() == 8 ? 1 : 0;
				const FTerrainEditModel Before = Edits.GetModel();
				const FTerrainDigResult R = Edits.Dig(MakeHit(At, ETerrainMaterial::Tierra, Tool), Flat);
				Violations += IsSubset(R.Edit.DirtyChunks, EditChunks) && IsSubset(R.RenderChunks, RenderChunks) ? 0 : 1;

				TArray<FIntVector> Truth = ChunksWithChangedSamples(Before, Edits.GetModel(), FBox(At - FVector(1.0), At + FVector(1.0)));
				Truth.Sort([](const FIntVector& A, const FIntVector& B)
				{
					return A.Z != B.Z ? A.Z < B.Z : (A.Y != B.Y ? A.Y < B.Y : A.X < B.X);
				});
				TArray<FIntVector> Unique;
				for (const FIntVector& C : Truth)
				{
					if (Unique.Num() == 0 || Unique.Last() != C)
					{
						Unique.Add(C);
					}
				}
				Mismatches += Unique == R.Edit.DirtyChunks ? 0 : 1;
			}
			AddInfo(FString::Printf(TEXT("300 golpes junto a bordes: %d tocaban 8 chunks"), CornerHits));
			TestEqual(TEXT("golpes que ensucian fuera de lo previsto"), Violations, 0);
			TestEqual(TEXT("golpes con chunks sucios de más o de menos"), Mismatches, 0);
			TestTrue(TEXT("el barrido pasa por esquinas"), CornerHits > 0);
		});

		It("los chunks de render cubren toda la densidad que cambia, también cruzando el borde de 64 m", [this]()
		{
			const FTerrainChunkSettings Render;
			const double S = Render.ChunkSizeMeters();
			const double V = Render.VoxelSize;
			FLcg Random(77);
			int32 Missing = 0;
			int32 Crossing = 0;
			for (int32 I = 0; I < 40; ++I)
			{
				FTerrainEdits Edits;
				const FVector At(S + Random.Range(-3.5, 3.5), -S + Random.Range(-3.5, 3.5), Random.Range(-3.5, 3.5));
				const FTerrainDigResult R = Edits.Dig(MakeHit(At, ETerrainMaterial::Tierra, ETerrainDigTool::PicoRescatado), Flat);
				Crossing += R.RenderChunks.Num() > 1 ? 1 : 0;
				// Toda posición donde cambia la densidad la lee cada chunk cuya rejilla (±1 vóxel)
				// o normales (±½ vóxel más) la alcanzan: ese chunk tiene que estar en la lista.
				for (double Z = -0.9; Z <= 0.9; Z += 0.1)
				{
					for (double Y = -0.9; Y <= 0.9; Y += 0.1)
					{
						for (double X = -0.9; X <= 0.9; X += 0.1)
						{
							const FVector P = At + FVector(X, Y, Z);
							float Delta = 0.0f;
							if (!Edits.DeltaAt(P, Delta) || Delta == 0.0f)
							{
								continue;
							}
							for (int32 CZ = -2; CZ <= 1; ++CZ)
							{
								for (int32 CY = -2; CY <= 0; ++CY)
								{
									for (int32 CX = 0; CX <= 2; ++CX)
									{
										const FVector Lo = FVector(CX, CY, CZ) * S - FVector(1.5 * V);
										const FVector Hi = FVector(CX + 1, CY + 1, CZ + 1) * S + FVector(1.5 * V);
										const bool bReads = P.X >= Lo.X && P.X <= Hi.X && P.Y >= Lo.Y && P.Y <= Hi.Y && P.Z >= Lo.Z && P.Z <= Hi.Z;
										Missing += bReads && !Contains(R.RenderChunks, FIntVector(CX, CY, CZ)) ? 1 : 0;
									}
								}
							}
						}
					}
				}
				TestTrue(TEXT("ordenados y sin repetir"), IsSortedUnique(R.RenderChunks));
			}
			TestEqual(TEXT("puntos cambiados que lee un chunk no invalidado"), Missing, 0);
			TestTrue(TEXT("algún golpe cruza el borde de render"), Crossing > 0);
		});

		It("un golpe en mitad de un chunk de render solo invalida ese", [this]()
		{
			FTerrainEdits Edits;
			const FTerrainDigResult R = Edits.Dig(MakeHit(FVector(32.0, 32.0, 32.0), ETerrainMaterial::Tierra, ETerrainDigTool::PicoPiedra), Deep);
			TestEqual(TEXT("un chunk"), R.RenderChunks.Num(), 1);
			TestTrue(TEXT("el (0, 0, 0)"), Contains(R.RenderChunks, FIntVector(0, 0, 0)));
		});
	});

	Describe("la capa terrain del guardado", [this]()
	{
		It("vuelve exactamente igual por FSaveWorldDeltas, el archivo y el texto de la partida", [this]()
		{
			FTerrainEdits Edits;
			BuildMine(Edits);
			FSaveWorldDeltas World;
			World.Seed = 20260926;
			Edits.SaveTo(World);
			TestTrue(TEXT("capa escrita"), World.Terrain.IsObject());
			TestEqual(TEXT("versión 2"), World.Terrain.Find(TEXT("v"))->AsInt(), static_cast<int64>(2));

			FSaveArchive Ar;
			World.Save(Ar);
			FSaveDocument Document;
			Document.Header.Seed = World.Seed;
			Document.Sections.Set(TEXT("world"), Ar.GetRoot());
			const FString Text = FSaveCodec::Encode(Document);
			FSaveDocument Read;
			FString Error;
			if (!TestTrue(TEXT("se lee"), FSaveCodec::Decode(Text, FSaveMigrations(), ExploredSave::CurrentFormatVersion, Read, Error) == ESaveLoadResult::Ok))
			{
				return;
			}
			FSaveWorldDeltas LoadedWorld;
			LoadedWorld.Load(FSaveArchive(*Read.Sections.Find(TEXT("world"))));
			TestTrue(TEXT("sección world igual"), LoadedWorld == World);
			FTerrainEdits Loaded;
			TestTrue(TEXT("carga"), Loaded.LoadFrom(LoadedWorld));
			TestTrue(TEXT("mismas ediciones"), Loaded == Edits);
			TestTrue(TEXT("mismos caminos"), Loaded.GetModel().IsPath(-3.0, 5.0) && !Loaded.GetModel().IsPath(-3.0, 9.0));
			TestEqual(TEXT("mismo texto al volver a guardar"), FSaveText::Write(Loaded.GetModel().ToValue()), FSaveText::Write(Edits.GetModel().ToValue()));

			FTerrainEditModel FromV1;
			TestTrue(TEXT("la versión 1 da el mismo estado"), FromV1.FromValue(Edits.GetModel().ToValue(1)) && FromV1 == Edits.GetModel());
		});

		It("sin ediciones no escribe capa, y una capa ausente carga vacía", [this]()
		{
			FTerrainEdits Edits;
			FSaveWorldDeltas World;
			Edits.SaveTo(World);
			TestTrue(TEXT("nula"), World.Terrain.IsNull());
			FTerrainEdits Loaded;
			BuildMine(Loaded);
			TestTrue(TEXT("carga"), Loaded.LoadFrom(World));
			TestTrue(TEXT("y deja vacío lo que hubiera"), Loaded.IsEmpty());
		});

		It("una partida truncada en cualquier punto no revienta ni carga a medias", [this]()
		{
			FTerrainEdits Edits;
			BuildMine(Edits);
			FSaveWorldDeltas World;
			Edits.SaveTo(World);
			FSaveArchive Ar;
			World.Save(Ar);
			const FString Text = FSaveText::Write(Ar.GetRoot(), ESaveTextStyle::Compact);
			int32 Loaded = 0;
			int32 Partial = 0;
			for (int32 Len = 0; Len < Text.Len(); ++Len)
			{
				FSaveValue Parsed;
				FString Error;
				if (!FSaveText::Parse(Text.Left(Len), Parsed, Error))
				{
					continue;
				}
				FSaveWorldDeltas Truncated;
				Truncated.Load(FSaveArchive(Parsed));
				FTerrainEdits Target;
				if (Target.LoadFrom(Truncated))
				{
					++Loaded;
					Partial += Target.IsEmpty() || Target == Edits ? 0 : 1;
				}
				else
				{
					Partial += Target.IsEmpty() ? 0 : 1;
				}
			}
			AddInfo(FString::Printf(TEXT("%d prefijos del texto; %d se leen como partida sin terreno"), Text.Len(), Loaded));
			TestEqual(TEXT("cargas a medias"), Partial, 0);
		});

		It("un blob truncado, con bytes cambiados o con basura detrás se rechaza entero", [this]()
		{
			FTerrainEdits Edits;
			BuildMine(Edits);
			const FSaveValue Good = Edits.GetModel().ToValue();
			const FString Blob = Good.Find(TEXT("chunks"))->AsString();
			const FString Paths = Good.Find(TEXT("paths"))->AsString();
			int32 Accepted = 0;
			int32 NotEmpty = 0;
			auto Try = [&](const FSaveValue& Value)
			{
				bool bEmpty = true;
				Accepted += Accepts(Value, bEmpty) ? 1 : 0;
				NotEmpty += bEmpty ? 0 : 1;
			};
			for (int32 Len = 0; Len < Blob.Len(); ++Len)
			{
				FSaveValue Cut = Good;
				Cut.Set(TEXT("chunks"), FSaveValue::MakeString(Blob.Left(Len)));
				Try(Cut);
			}
			for (int32 Len = 0; Len < Paths.Len(); ++Len)
			{
				FSaveValue Cut = Good;
				Cut.Set(TEXT("paths"), FSaveValue::MakeString(Paths.Left(Len)));
				Try(Cut);
			}
			TestEqual(TEXT("prefijos aceptados"), Accepted, 0);

			// Cada byte cambiado: no puede leer fuera del blob (ASan) ni dejar el modelo a medias.
			TArray<uint8> Bytes;
			TestTrue(TEXT("decodifica"), FSaveBase64::Decode(Blob, Bytes));
			int32 Flipped = 0;
			for (int32 I = 0; I < Bytes.Num(); ++I)
			{
				static const uint8 Masks[] = { 0x01, 0x80, 0xFF };
				for (const uint8 Mask : Masks)
				{
					TArray<uint8> Mutated = Bytes;
					Mutated[I] ^= Mask;
					FSaveValue Bad = Good;
					Bad.Set(TEXT("chunks"), FSaveValue::MakeString(FSaveBase64::Encode(Mutated)));
					bool bEmpty = true;
					Flipped += Accepts(Bad, bEmpty) ? 1 : 0;
					NotEmpty += bEmpty ? 0 : 1;
				}
			}
			AddInfo(FString::Printf(TEXT("%d bytes × 3 cambios: %d todavía se leen (deltas distintos, estructura válida)"), Bytes.Num(), Flipped));
			TArray<uint8> Longer = Bytes;
			Longer.Add(0);
			FSaveValue Trailing = Good;
			Trailing.Set(TEXT("chunks"), FSaveValue::MakeString(FSaveBase64::Encode(Longer)));
			Try(Trailing);
			TestEqual(TEXT("con basura detrás"), Accepted, 0);
			TestEqual(TEXT("modelos a medias tras un rechazo"), NotEmpty, 0);
		});

		It("rechaza versiones, cuentas y valores manipulados", [this]()
		{
			bool bEmpty = true;
			// Un chunk (0, 0, 0) con un tramo de dos muestras: 5 mm y 7 mm.
			const FString OneChunk = FBlob().U(1).S(0).S(0).S(0).U(1).U(10).U(1).S(5).S(2).Text();
			TestTrue(TEXT("el blob de referencia se lee"), Accepts(V2(OneChunk, 2), bEmpty));
			FTerrainEditModel Check;
			Check.FromValue(V2(OneChunk, 2));
			TestEqual(TEXT("muestra 10"), Check.SampleDelta(FIntVector(10, 0, 0)), 0.005f);
			TestEqual(TEXT("muestra 11"), Check.SampleDelta(FIntVector(11, 0, 0)), 0.007f);

			auto Rejects = [&bEmpty](const FSaveValue& Value) { return !Accepts(Value, bEmpty) && bEmpty; };
			FSaveValue Future = V2(OneChunk, 2);
			Future.Set(TEXT("v"), FSaveValue::MakeInt(3));
			TestTrue(TEXT("versión futura"), Rejects(Future));
			TestTrue(TEXT("cuenta de muestras que no cuadra"), Rejects(V2(OneChunk, 3)));
			TestTrue(TEXT("cuenta de columnas que no cuadra"), Rejects(V2(OneChunk, 2, FBlob().U(0).Text(), 1)));
			TestTrue(TEXT("delta cero"), Rejects(V2(FBlob().U(1).S(0).S(0).S(0).U(1).U(0).U(0).S(0).Text(), 1)));
			TestTrue(TEXT("delta desmesurado"), Rejects(V2(FBlob().U(1).S(0).S(0).S(0).U(1).U(0).U(0).S(FTerrainEditModel::MaxDeltaMm + 1).Text(), 1)));
			TestTrue(TEXT("tramo que sale del chunk"), Rejects(V2(FBlob().U(1).S(0).S(0).S(0).U(1).U(32 * 32 * 32 - 1).U(1).S(1).S(1).Text(), 2)));
			TestTrue(TEXT("chunk sin tramos"), Rejects(V2(FBlob().U(1).S(0).S(0).S(0).U(0).Text(), 0)));
			TestTrue(TEXT("chunk repetido"), Rejects(V2(FBlob().U(2).S(0).S(0).S(0).U(1).U(0).U(0).S(1).S(0).S(0).S(0).U(1).U(0).U(0).S(1).Text(), 2)));
			TestTrue(TEXT("coordenada que desborda int32"), Rejects(V2(FBlob().U(1).S(int64(1) << 40).S(0).S(0).U(1).U(0).U(0).S(1).Text(), 1)));
			TestTrue(TEXT("cuenta de chunks gigante"), Rejects(V2(FBlob().U(uint64(1) << 60).Text(), 0)));
			TestTrue(TEXT("tramo gigante"), Rejects(V2(FBlob().U(1).S(0).S(0).S(0).U(1).U(0).U(uint64(1) << 40).Text(), 0)));
			TArray<uint8> Overlong;
			for (int32 I = 0; I < 11; ++I)
			{
				Overlong.Add(0x80);
			}
			Overlong.Add(0x00);
			TestTrue(TEXT("varint de más de 10 bytes"), Rejects(V2(FSaveBase64::Encode(Overlong), 0)));
			TestTrue(TEXT("base64 con caracteres ajenos"), Rejects(V2(OneChunk + TEXT("!"), 2)));
			TestTrue(TEXT("base64 con bits sobrantes"), Rejects(V2(TEXT("AB"), 0)));
			FSaveValue NotString = V2(OneChunk, 2);
			NotString.Set(TEXT("chunks"), FSaveValue::MakeArray());
			TestTrue(TEXT("chunks que no son texto"), Rejects(NotString));
			TestTrue(TEXT("columna con compactación 0"), Rejects(V2(OneChunk, 2, FBlob().U(1).S(3).S(4).U(0).Text(), 1)));
			TestTrue(TEXT("columna repetida"), Rejects(V2(OneChunk, 2, FBlob().U(2).S(3).S(4).U(50).S(0).S(0).U(60).Text(), 2)));
			TestTrue(TEXT("columna fuera de int32"), Rejects(V2(OneChunk, 2, FBlob().U(1).S(int64(1) << 33).S(0).U(50).Text(), 1)));
			FSaveValue NaNCell = V2(OneChunk, 2);
			NaNCell.Set(TEXT("cell"), FSaveValue::MakeDouble(1.0e300));
			TestTrue(TEXT("celda desmesurada"), Rejects(NaNCell));

			// La capa del guardado es opaca: un valor ilegible se descarta al leerla.
			FSaveWorldDeltas World;
			World.Terrain = FSaveValue::MakeString(TEXT("basura"));
			FTerrainEdits Edits;
			BuildMine(Edits);
			TestFalse(TEXT("capa ilegible"), Edits.LoadFrom(World));
			TestTrue(TEXT("y el mundo arranca sin cavar"), Edits.IsEmpty());
		});

		It("base64: ida y vuelta de todas las longitudes y rechazo de lo no canónico", [this]()
		{
			bool bAll = true;
			TArray<uint8> Bytes;
			for (int32 Len = 0; Len < 40; ++Len)
			{
				TArray<uint8> Decoded;
				bAll &= FSaveBase64::Decode(FSaveBase64::Encode(Bytes), Decoded) && Decoded == Bytes;
				Bytes.Add(static_cast<uint8>(Len * 37 + 11));
			}
			TestTrue(TEXT("ida y vuelta"), bAll);
			TArray<uint8> Out;
			TestFalse(TEXT("resto de 1 carácter"), FSaveBase64::Decode(TEXT("QUJDR"), Out));
			TestFalse(TEXT("relleno"), FSaveBase64::Decode(TEXT("QQ=="), Out));
			TestFalse(TEXT("pasa del máximo"), FSaveBase64::Decode(TEXT("QUJD"), Out, 2));
			TestTrue(TEXT("vacío tras rechazar"), Out.IsEmpty());
		});
	});

	Describe("la prueba de estrés", [this]()
	{
		It("50.000 golpes: guarda y carga una mina entera, y mide tamaño y tiempo", [this]()
		{
			static constexpr int32 NumHits = 50000;
			static constexpr int32 NumGalleries = 10;
			static constexpr int32 HitsPerGallery = NumHits / NumGalleries;
			FTerrainEdits Edits;
			double Removed = 0.0;
			int32 Rejected = 0;

			// Diez galerías de 1,5 × 1,5 m y ≈ 190 m, cavadas a pala en tierra y a pico en basalto,
			// en roca maciza (cada golpe arranca lo suyo), a 6 m unas de otras.
			const double DigStart = FPlatformTime::Seconds();
			for (int32 I = 0; I < NumHits; ++I)
			{
				const int32 Gallery = I / HitsPerGallery;
				const int32 K = I % HitsPerGallery;
				const int32 Slot = K % 9;
				const int32 Step = K / 9;
				const FVector At(0.35 * Step - 90.0, 6.0 * Gallery - 30.0 + 0.5 * (Slot % 3 - 1), 0.5 * (Slot / 3 - 1));
				const bool bRock = Gallery % 5 == 4;
				const FTerrainDigResult R = Edits.Dig(MakeHit(At,
					bRock ? ETerrainMaterial::Basalto : ETerrainMaterial::Tierra,
					bRock ? ETerrainDigTool::PicoTallado : ETerrainDigTool::PalaTosca), Deep);
				Removed += R.Edit.VolumeRemoved;
				Rejected += R.Edit.bRejected ? 1 : 0;
			}
			const double DigSeconds = FPlatformTime::Seconds() - DigStart;
			const int32 Samples = Edits.GetModel().NumEditedSamples();
			const int32 Chunks = Edits.GetModel().EditedChunks().Num();

			// Guardado completo: capa → sección world → documento de la partida (texto con sangría).
			const double SaveStart = FPlatformTime::Seconds();
			FSaveWorldDeltas World;
			Edits.SaveTo(World);
			FSaveArchive Ar;
			World.Save(Ar);
			FSaveDocument Document;
			Document.Sections.Set(TEXT("world"), Ar.GetRoot());
			const FString Text = FSaveCodec::Encode(Document);
			const double SaveSeconds = FPlatformTime::Seconds() - SaveStart;

			const double LoadStart = FPlatformTime::Seconds();
			FSaveDocument Read;
			FString Error;
			const bool bDecoded = FSaveCodec::Decode(Text, FSaveMigrations(), ExploredSave::CurrentFormatVersion, Read, Error) == ESaveLoadResult::Ok;
			FSaveWorldDeltas LoadedWorld;
			FTerrainEdits Loaded;
			bool bLoaded = false;
			if (bDecoded)
			{
				LoadedWorld.Load(FSaveArchive(*Read.Sections.Find(TEXT("world"))));
				bLoaded = Loaded.LoadFrom(LoadedWorld);
			}
			const double LoadSeconds = FPlatformTime::Seconds() - LoadStart;

			// El mismo estado en la versión 1 (texto con sangría), para comparar.
			FSaveValue V1Root = FSaveValue::MakeObject();
			V1Root.Set(TEXT("terrain"), Edits.GetModel().ToValue(1));
			const int32 V1Bytes = FSaveText::Write(V1Root).Len();

			const int32 Bytes = Text.Len();
			AddInfo(FString::Printf(TEXT("%d golpes (%d rebotes): %.1f m³ vaciados, %d muestras en %d chunks de edición"),
				NumHits, Rejected, Removed, Samples, Chunks));
			AddInfo(FString::Printf(TEXT("cavar: %.3f s (%.1f µs por golpe)"), DigSeconds, DigSeconds * 1.0e6 / NumHits));
			AddInfo(FString::Printf(TEXT("guardado v2: %d bytes (%.2f B por muestra), %.3f s en escribir y %.3f s en leer"),
				Bytes, static_cast<double>(Bytes) / FMath::Max(Samples, 1), SaveSeconds, LoadSeconds));
			AddInfo(FString::Printf(TEXT("el mismo terreno en la versión 1: %d bytes (×%.1f)"), V1Bytes, static_cast<double>(V1Bytes) / FMath::Max(Bytes, 1)));

			TestEqual(TEXT("ningún rebote"), Rejected, 0);
			TestTrue(TEXT("una mina de verdad: más de 3.000 m³"), Removed > 3000.0);
			TestTrue(TEXT("la partida se lee"), bDecoded && bLoaded);
			TestTrue(TEXT("round-trip exacto"), Loaded == Edits);
			TestEqual(TEXT("mismo texto al volver a guardar"), FSaveText::Write(Loaded.GetModel().ToValue()), FSaveText::Write(Edits.GetModel().ToValue()));
			// Presupuestos: 2 bytes por muestra (se miden ≈ 1,4) y menos de la mitad que la versión 1.
			TestTrue(TEXT("menos de 2 bytes por muestra"), Bytes < 2 * Samples + 4096);
			TestTrue(TEXT("menos de la mitad que la versión 1"), Bytes * 2 < V1Bytes);
			// Tiempo con margen para ASan y máquinas lentas: en una compilación normal es < 2 s.
			TestTrue(TEXT("cavar en menos de 60 s"), DigSeconds < 60.0);
			TestTrue(TEXT("guardar y cargar en menos de 20 s"), SaveSeconds + LoadSeconds < 20.0);
		});
	});
}

#endif
