#include "Misc/AutomationTest.h"

#include <limits>

#include "Save/SaveFormat.h"
#include "Save/SaveWorldDeltas.h"
#include "WorldGen/SurfaceNets.h"
#include "WorldGen/TerrainEditModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace TerrainEditSpecDetail
{
	/** Suelo plano en z = 0 (sólido debajo). */
	const auto Flat = [](const FVector& P) { return static_cast<float>(P.Z); };

	/** Montículo de 60 cm de alto y 1,5 m de radio sobre z = 0. */
	const auto Mound = [](const FVector& P)
	{
		const float R = static_cast<float>(FVector2D(P.X, P.Y).Size());
		return static_cast<float>(P.Z) - 0.6f * FMath::Max(0.0f, 1.0f - R / 1.5f);
	};

	/** Hoyo de 60 cm de hondo y 1 m de radio en z = 0. */
	const auto Pit = [](const FVector& P)
	{
		const float R = static_cast<float>(FVector2D(P.X, P.Y).Size());
		return static_cast<float>(P.Z) + 0.6f * FMath::Max(0.0f, 1.0f - R);
	};

	/** Ladera a 45° que sube hacia +X. */
	const auto Slope = [](const FVector& P) { return static_cast<float>(P.Z - P.X); };

	using FSamples = TMap<FIntVector, float>;

	FSamples Snapshot(const FTerrainEditModel& Model, const FBox& Box, FTerrainEditModel::FBaseDensity Base)
	{
		FSamples Out;
		const double H = Model.GetSettings().CellSize;
		for (int32 Z = FMath::CeilToInt32(Box.Min.Z / H); Z <= FMath::FloorToInt32(Box.Max.Z / H); ++Z)
		{
			for (int32 Y = FMath::CeilToInt32(Box.Min.Y / H); Y <= FMath::FloorToInt32(Box.Max.Y / H); ++Y)
			{
				for (int32 X = FMath::CeilToInt32(Box.Min.X / H); X <= FMath::FloorToInt32(Box.Max.X / H); ++X)
				{
					const FIntVector G(X, Y, Z);
					Out.Add(G, Model.SampleDensity(G, Base));
				}
			}
		}
		return Out;
	}

	TArray<FIntVector> Changed(const FSamples& Before, const FSamples& After)
	{
		TArray<FIntVector> Out;
		for (const auto& Pair : Before)
		{
			const float* Now = After.Find(Pair.Key);
			if (Now && *Now != Pair.Value)
			{
				Out.Add(Pair.Key);
			}
		}
		return Out;
	}

	/** Altura de la superficie en la columna (X, Y): primer sólido bajando desde arriba. */
	float SurfaceZ(const FTerrainEditModel& Model, double X, double Y, FTerrainEditModel::FBaseDensity Base)
	{
		for (double Z = 1.5; Z > -8.0; Z -= 0.01)
		{
			if (Model.Density(FVector(X, Y, Z), Base) < 0.0f)
			{
				return static_cast<float>(Z);
			}
		}
		return -8.0f;
	}

	/** Golpes de pico, hacia abajo y repartidos en 1 m², hasta arrancar Volume m³ de un suelo plano. */
	int32 HitsToRemove(ETerrainMaterial Material, int32 Tier, double Volume)
	{
		FTerrainEditModel Model;
		double Removed = 0.0;
		int32 Hits = 0;
		while (Removed < Volume && Hits < 400)
		{
			const double X = -0.35 + 0.35 * (Hits % 3);
			const double Y = -0.35 + 0.35 * ((Hits / 3) % 3);
			FPickaxeHit Hit;
			Hit.ImpactPoint = FVector(X, Y, SurfaceZ(Model, X, Y, Flat));
			Hit.Direction = FVector(0.0, 0.0, -1.0);
			Hit.Material = Material;
			Hit.ToolTier = Tier;
			Hit.Seed = static_cast<uint32>(Hits);
			Removed += Model.Pickaxe(Hit, Flat).VolumeRemoved;
			++Hits;
		}
		return Hits;
	}

	FPickaxeHit DownHit(const FVector& At, ETerrainMaterial Material = ETerrainMaterial::Tierra, int32 Tier = 2, uint32 Seed = 7)
	{
		FPickaxeHit Hit;
		Hit.ImpactPoint = At;
		Hit.Direction = FVector(0.0, 0.0, -1.0);
		Hit.Material = Material;
		Hit.ToolTier = Tier;
		Hit.Seed = Seed;
		return Hit;
	}
}

BEGIN_DEFINE_SPEC(FTerrainEditModelSpec, "Explored.TerrainEdit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FTerrainEditModelSpec)

void FTerrainEditModelSpec::Define()
{
	using namespace TerrainEditSpecDetail;

	Describe("el pico", [this]()
	{
		It("ahueca la roca de forma progresiva hasta abrir aire donde golpea", [this]()
		{
			FTerrainEditModel Model;
			const FVector Probe(0.1, 0.1, -0.6);
			TestTrue(TEXT("al principio es sólido"), Model.Density(Probe, Flat) < 0.0f);
			float Previous = Model.Density(Probe, Flat);
			int32 Hits = 0;
			while (Model.Density(Probe, Flat) < 0.0f && Hits < 20)
			{
				const FTerrainEditResult R = Model.Pickaxe(DownHit(FVector(0.1, 0.1, SurfaceZ(Model, 0.1, 0.1, Flat)), ETerrainMaterial::Basalto, 3, Hits), Flat);
				TestTrue(TEXT("cada golpe arranca algo"), R.VolumeRemoved > 0.0);
				const float Now = Model.Density(Probe, Flat);
				TestTrue(TEXT("la densidad nunca vuelve atrás"), Now >= Previous);
				Previous = Now;
				++Hits;
			}
			TestTrue(*FString::Printf(TEXT("hacen falta varios golpes (%d)"), Hits), Hits >= 2 && Hits < 20);
		});

		It("no toca ninguna muestra fuera del radio máximo del pincel", [this]()
		{
			FTerrainEditModel Model;
			const FBox Box(FVector(-2.0), FVector(2.0));
			const FSamples Before = Snapshot(Model, Box, Flat);
			const FPickaxeHit Hit = DownHit(FVector(0.3, -0.2, 0.0), ETerrainMaterial::Tierra, 4);
			Model.Pickaxe(Hit, Flat);
			const TArray<FIntVector> Moved = Changed(Before, Snapshot(Model, Box, Flat));
			TestTrue(TEXT("el golpe cambia muestras"), Moved.Num() > 0);
			const FVector Center = Hit.ImpactPoint + FVector(0.0, 0.0, -FTerrainEditModel::PickaxeBite);
			const float MaxRadius = FTerrainEditModel::PickaxeRadius * (1.0f + FTerrainEditModel::PickaxeIrregularity);
			for (const FIntVector& G : Moved)
			{
				TestTrue(TEXT("muestra cambiada dentro del radio"), FVector::Dist(Model.SamplePosition(G), Center) < MaxRadius);
			}
			TestEqual(TEXT("el modelo solo guarda lo que ha cambiado"), Model.NumEditedSamples(), Moved.Num());
		});

		It("tiene forma irregular: dos semillas dan huecos distintos del mismo tamaño aproximado", [this]()
		{
			FTerrainEditModel A;
			FTerrainEditModel B;
			const FTerrainEditResult RA = A.Pickaxe(DownHit(FVector::ZeroVector, ETerrainMaterial::Tierra, 2, 1), Flat);
			const FTerrainEditResult RB = B.Pickaxe(DownHit(FVector::ZeroVector, ETerrainMaterial::Tierra, 2, 2), Flat);
			TestTrue(TEXT("formas distintas"), A != B);
			TestTrue(TEXT("volúmenes parecidos (±35 %)"), FMath::Abs(RA.VolumeRemoved - RB.VolumeRemoved) < 0.35 * RA.VolumeRemoved);
		});

		It("arranca menos cuanto más duro es el material: arena > tierra > caliza > basalto > obsidiana", [this]()
		{
			const ETerrainMaterial Order[] = { ETerrainMaterial::Arena, ETerrainMaterial::Tierra, ETerrainMaterial::Caliza,
				ETerrainMaterial::Basalto, ETerrainMaterial::Obsidiana };
			double Previous = TNumericLimits<double>::Max();
			for (ETerrainMaterial Material : Order)
			{
				FTerrainEditModel Model;
				const double Removed = Model.Pickaxe(DownHit(FVector::ZeroVector, Material, 4), Flat).VolumeRemoved;
				TestTrue(TEXT("estrictamente menos que el anterior"), Removed > 0.0 && Removed < Previous);
				Previous = Removed;
			}
		});

		It("rebota sin tocar nada si la herramienta no llega al material", [this]()
		{
			FTerrainEditModel Model;
			const FTerrainEditResult R = Model.Pickaxe(DownHit(FVector::ZeroVector, ETerrainMaterial::Basalto, 2), Flat);
			TestTrue(TEXT("rechazado"), R.bRejected);
			TestFalse(TEXT("sin cambios"), R.Changed());
			TestTrue(TEXT("modelo vacío"), Model.IsEmpty());
			TestEqual(TEXT("sin chunks sucios"), R.DirtyChunks.Num(), 0);
		});

		It("cumple los golpes por m³ de diseño (±30 %)", [this]()
		{
			struct FCase { ETerrainMaterial Material; int32 Tier; };
			const FCase Cases[] = {
				{ ETerrainMaterial::Arena, 2 }, { ETerrainMaterial::Tierra, 2 }, { ETerrainMaterial::Caliza, 2 },
				{ ETerrainMaterial::Basalto, 3 }, { ETerrainMaterial::Obsidiana, 4 }, { ETerrainMaterial::Tierra, 4 },
			};
			for (const FCase& Case : Cases)
			{
				const float Design = FTerrainEditModel::DesignHitsPerCubicMeter(Case.Material, Case.Tier);
				const int32 Hits = HitsToRemove(Case.Material, Case.Tier, 1.0);
				TestTrue(*FString::Printf(TEXT("material %d nivel %d: %d golpes, diseño %.1f"), static_cast<int32>(Case.Material), Case.Tier, Hits, Design),
					FMath::Abs(Hits - Design) <= 0.3f * Design + 1.0f);
			}
		});

		It("informa exactamente del volumen que mide la rejilla", [this]()
		{
			FTerrainEditModel Model;
			const FBox Box(FVector(-2.0), FVector(2.0));
			const double Before = Model.SolidVolume(Box, Flat);
			double Reported = 0.0;
			for (uint32 Seed = 0; Seed < 5; ++Seed)
			{
				Reported += Model.Pickaxe(DownHit(FVector(0.0, 0.0, SurfaceZ(Model, 0.0, 0.0, Flat)), ETerrainMaterial::Caliza, 3, Seed), Flat).VolumeRemoved;
			}
			TestEqual(TEXT("volumen quitado"), Before - Model.SolidVolume(Box, Flat), Reported, 1.0e-6);
		});

		It("es determinista: los mismos golpes dan el mismo terreno y el mismo guardado", [this]()
		{
			FTerrainEditModel A;
			FTerrainEditModel B;
			for (FTerrainEditModel* Model : { &A, &B })
			{
				for (uint32 Seed = 0; Seed < 6; ++Seed)
				{
					FPickaxeHit Hit = DownHit(FVector(0.2 * Seed, 0.0, 0.0), ETerrainMaterial::Tierra, 2, Seed);
					Hit.Direction = FVector(0.3, 0.1, -1.0);
					Model->Pickaxe(Hit, Flat);
				}
			}
			TestTrue(TEXT("mismo modelo"), A == B);
			TestTrue(TEXT("mismo guardado"), A.ToValue() == B.ToValue());
		});
	});

	Describe("los chunks sucios", [this]()
	{
		It("marcan los dos chunks de una muestra en el borde (el anterior también la lee)", [this]()
		{
			FTerrainEditModel Model;
			const int32 N = Model.GetSettings().CellsPerChunk;
			TArray<FIntVector> Readers;
			Model.ChunksReadingSample(FIntVector(0, 5, 5), Readers);
			TestEqual(TEXT("primera muestra: dos chunks"), Readers.Num(), 2);
			TestTrue(TEXT("el suyo"), Readers.Contains(FIntVector(0, 0, 0)));
			TestTrue(TEXT("el anterior"), Readers.Contains(FIntVector(-1, 0, 0)));

			Readers.Reset();
			Model.ChunksReadingSample(FIntVector(N - 1, 5, 5), Readers);
			TestTrue(TEXT("última muestra: también el siguiente"), Readers.Num() == 2 && Readers.Contains(FIntVector(1, 0, 0)));

			Readers.Reset();
			Model.ChunksReadingSample(FIntVector(5, 5, 5), Readers);
			TestEqual(TEXT("muestra interior: un chunk"), Readers.Num(), 1);

			Readers.Reset();
			Model.ChunksReadingSample(FIntVector(0, 0, 0), Readers);
			TestEqual(TEXT("esquina: ocho chunks"), Readers.Num(), 8);
		});

		It("una edición en el borde de chunk marca como sucios los chunks de ambos lados", [this]()
		{
			FTerrainEditModel Model;
			// X = 0 es la frontera entre el chunk -1 y el 0; Z = 0 también.
			const FTerrainEditResult R = Model.Pickaxe(DownHit(FVector(0.0, 3.0, 0.0)), Flat);
			bool bLeft = false;
			bool bRight = false;
			for (const FIntVector& C : R.DirtyChunks)
			{
				bLeft |= C.X == -1;
				bRight |= C.X == 0;
			}
			TestTrue(TEXT("chunk de la izquierda"), bLeft);
			TestTrue(TEXT("chunk de la derecha"), bRight);
		});

		It("son exactamente los que leen alguna muestra cambiada, ordenados y sin repetir", [this]()
		{
			FTerrainEditModel Model;
			// Cerca de la esquina (X = 0, Y = 8, Z = 0) de ocho chunks.
			const FTerrainEditResult R = Model.Pickaxe(DownHit(FVector(-0.1, 7.9, 0.0), ETerrainMaterial::Arena, 4), Flat);
			const FBox Near(FVector(-2.0, 6.0, -2.0), FVector(2.0, 10.0, 2.0));
			const FSamples Base = Snapshot(FTerrainEditModel(), Near, Flat);
			const TArray<FIntVector> Moved = Changed(Base, Snapshot(Model, Near, Flat));
			TArray<FIntVector> Expected;
			for (const FIntVector& G : Moved)
			{
				Model.ChunksReadingSample(G, Expected);
			}
			TestEqual(TEXT("mismo número"), R.DirtyChunks.Num(), Expected.Num());
			for (const FIntVector& C : Expected)
			{
				TestTrue(TEXT("marcado"), R.DirtyChunks.Contains(C));
			}
			for (int32 I = 1; I < R.DirtyChunks.Num(); ++I)
			{
				const FIntVector& A = R.DirtyChunks[I - 1];
				const FIntVector& B = R.DirtyChunks[I];
				TestTrue(TEXT("orden (Z, Y, X) estricto"), A.Z < B.Z || (A.Z == B.Z && (A.Y < B.Y || (A.Y == B.Y && A.X < B.X))));
			}
		});

		It("al remallar un chunk sucio, el hueco aparece en la malla", [this]()
		{
			FTerrainEditModel Model;
			FDensityGrid Grid;
			const FIntVector Chunk(0, 0, -1);
			Model.BuildChunkGrid(Chunk, Flat, Grid);
			const FTerrainMeshData Before = FSurfaceNets::Polygonize(Grid);
			for (uint32 Seed = 0; Seed < 6; ++Seed)
			{
				const FTerrainEditResult R = Model.Pickaxe(DownHit(FVector(4.0, 4.0, SurfaceZ(Model, 4.0, 4.0, Flat)), ETerrainMaterial::Tierra, 3, Seed), Flat);
				TestTrue(TEXT("el chunk de debajo queda sucio"), R.DirtyChunks.Contains(Chunk));
			}
			Model.BuildChunkGrid(Chunk, Flat, Grid);
			TestEqual(TEXT("la rejilla es la del modelo"), Grid.Get(17, 17, 32), Model.SampleDensity(FIntVector(16, 16, -1), Flat));
			const FTerrainMeshData After = FSurfaceNets::Polygonize(Grid);
			float Lowest = 0.0f;
			for (const FVector3f& P : After.Positions)
			{
				Lowest = FMath::Min(Lowest, P.Z);
			}
			TestTrue(TEXT("hay más triángulos (paredes del hoyo)"), After.NumTriangles() > Before.NumTriangles());
			TestTrue(*FString::Printf(TEXT("la malla baja al fondo del hoyo (%.2f m)"), Lowest), Lowest < -0.4f);
		});
	});

	Describe("la pala", [this]()
	{
		It("aplana un montículo hacia el plano y devuelve la tierra que corta", [this]()
		{
			FTerrainEditModel Model;
			FShovelStroke Stroke;
			double Removed = 0.0;
			for (int32 I = 0; I < 6; ++I)
			{
				const FTerrainEditResult R = Model.Shovel(Stroke, Mound);
				TestTrue(TEXT("sin rellenar nada sin tierra"), R.VolumeAdded < 1.0e-9);
				Removed += R.VolumeRemoved;
			}
			TestTrue(TEXT("ha cortado tierra"), Removed > 0.1);
			TestTrue(TEXT("el centro queda a la altura del plano (±5 cm)"), FMath::Abs(SurfaceZ(Model, 0.1, 0.1, Mound)) < 0.05f);
			TestTrue(TEXT("dentro del radio también"), FMath::Abs(SurfaceZ(Model, 0.8, 0.0, Mound)) < 0.05f);
		});

		It("deja un borde suave: el corte decrece hacia fuera y no toca nada más allá del borde", [this]()
		{
			FTerrainEditModel Model;
			FShovelStroke Stroke;
			const FBox Box(FVector(-3.0, -3.0, -1.5), FVector(3.0, 3.0, 1.5));
			const FSamples Before = Snapshot(Model, Box, Mound);
			for (int32 I = 0; I < 6; ++I)
			{
				Model.Shovel(Stroke, Mound);
			}
			const float Inner = SurfaceZ(FTerrainEditModel(), 1.0, 0.0, Mound) - SurfaceZ(Model, 1.0, 0.0, Mound);
			const float Edge = SurfaceZ(FTerrainEditModel(), 1.4, 0.0, Mound) - SurfaceZ(Model, 1.4, 0.0, Mound);
			TestTrue(*FString::Printf(TEXT("corte en el radio %.3f > en el borde %.3f > 0"), Inner, Edge), Inner > Edge && Edge > 0.0f);
			const float Outer = Stroke.Radius + Stroke.EdgeWidth;
			for (const FIntVector& G : Changed(Before, Snapshot(Model, Box, Mound)))
			{
				const FVector P = Model.SamplePosition(G);
				TestTrue(TEXT("dentro del borde"), FVector2D(P.X, P.Y).Size() < Outer && FMath::Abs(P.Z) <= Stroke.VerticalReach);
			}
		});

		It("rellena un hoyo solo con la tierra que lleva", [this]()
		{
			FShovelStroke Stroke;
			Stroke.bMarkPath = false;
			FTerrainEditModel Empty;
			const FTerrainEditResult None = Empty.Shovel(Stroke, Pit);
			TestTrue(TEXT("sin tierra no rellena"), None.VolumeAdded <= None.VolumeRemoved + 1.0e-6);

			FTerrainEditModel Model;
			Stroke.SoilBudget = 0.05;
			const FTerrainEditResult R = Model.Shovel(Stroke, Pit);
			TestTrue(TEXT("rellena algo"), R.VolumeAdded > 0.01);
			TestTrue(*FString::Printf(TEXT("no más de lo que lleva (%.4f)"), R.VolumeAdded), R.VolumeAdded <= Stroke.SoilBudget + R.VolumeRemoved + 1.0e-3);

			FTerrainEditModel Rich;
			Stroke.SoilBudget = 10.0;
			double Added = 0.0;
			for (int32 I = 0; I < 8; ++I)
			{
				Added += Rich.Shovel(Stroke, Pit).VolumeAdded;
			}
			TestTrue(TEXT("con tierra de sobra el hoyo se cierra hasta el plano"), FMath::Abs(SurfaceZ(Rich, 0.0, 0.0, Pit)) < 0.05f);
			TestTrue(TEXT("y gasta como mucho el volumen del hoyo"), Added < 0.6 * PI * 1.0 / 3.0 + 0.1);
		});

		It("compacta y marca camino en la segunda pasada; picar encima lo deshace", [this]()
		{
			FTerrainEditModel Model;
			FShovelStroke Stroke;
			Model.Shovel(Stroke, Flat);
			TestEqual(TEXT("primera pasada"), Model.Compaction(0.1, 0.1), FTerrainEditModel::CompactionPerStroke);
			TestFalse(TEXT("aún no es camino"), Model.IsPath(0.1, 0.1));
			const FTerrainEditResult R = Model.Shovel(Stroke, Flat);
			TestTrue(TEXT("camino"), Model.IsPath(0.1, 0.1));
			TestFalse(TEXT("fuera del radio no"), Model.IsPath(1.6, 0.0));
			TestTrue(TEXT("marcar camino pide remallar (capa de superficie)"), R.DirtyChunks.Num() > 0);
			TestTrue(TEXT("el camino compactado es más duro"),
				Model.HardnessAt(ETerrainMaterial::Tierra, FVector(0.1, 0.1, 0.0)) > FTerrainEditModel::MaterialInfo(ETerrainMaterial::Tierra).Hardness);
			for (int32 I = 0; I < 5; ++I)
			{
				Model.Shovel(Stroke, Flat);
			}
			TestEqual(TEXT("tope de compactación"), Model.Compaction(0.1, 0.1), 100);
			Model.Pickaxe(DownHit(FVector(0.1, 0.1, 0.0)), Flat);
			TestFalse(TEXT("picado deja de ser camino"), Model.IsPath(0.1, 0.1));
		});

		It("al marcar camino junto al borde de un chunk remalla también el vecino que lee la huella", [this]()
		{
			FTerrainEditModel Model;
			FShovelStroke Stroke;
			Stroke.Center = FVector(7.5, 4.0, 0.1);
			Stroke.Radius = 1.0f;
			Model.Shovel(Stroke, Flat);
			const FTerrainEditResult R = Model.Shovel(Stroke, Flat);
			TestTrue(TEXT("la huella cruza x = 8 m"), Model.IsPath(8.1, 4.0));
			const int32 NeighbourX = Model.ChunkOfSample(FIntVector(
				FMath::FloorToInt32(8.1 / Model.GetSettings().CellSize), 0, 0)).X;
			TestTrue(TEXT("el chunk vecino queda sucio"),
				R.DirtyChunks.ContainsByPredicate([NeighbourX](const FIntVector& C) { return C.X == NeighbourX; }));
		});

		It("no puede con la caliza ni con la roca", [this]()
		{
			FTerrainEditModel Model;
			FShovelStroke Stroke;
			Stroke.Material = ETerrainMaterial::Caliza;
			TestTrue(TEXT("rechazado"), Model.Shovel(Stroke, Mound).bRejected);
			TestTrue(TEXT("sin cambios ni camino"), Model.IsEmpty());
		});
	});

	Describe("la conservación de la tierra", [this]()
	{
		It("cavar y volver a echar la misma tierra deja el mismo volumen de sólido", [this]()
		{
			FTerrainEditModel Model;
			// Diez golpes que vacían el pincel entero bajan unos 4 m: la caja debe cubrirlos.
			const FBox Box(FVector(-3.0, -3.0, -7.0), FVector(3.0, 3.0, 2.0));
			const double Original = Model.SolidVolume(Box, Flat);
			double Carried = 0.0;
			for (uint32 Seed = 0; Seed < 10; ++Seed)
			{
				const double X = 0.2 * (Seed % 3);
				Carried += Model.Pickaxe(DownHit(FVector(X, 0.0, SurfaceZ(Model, X, 0.0, Flat)), ETerrainMaterial::Tierra, 3, Seed), Flat).VolumeRemoved;
			}
			TestEqual(TEXT("lo que se lleva es lo que falta"), Original - Model.SolidVolume(Box, Flat), Carried, 1.0e-6);

			double Budget = Carried;
			for (int32 I = 0; I < 30 && Budget > 1.0e-4; ++I)
			{
				FSoilPlacement Place;
				Place.Center = FVector(0.2, 0.0, SurfaceZ(Model, 0.2, 0.0, Flat));
				Place.Radius = 0.6f;
				Place.SoilBudget = Budget;
				const FTerrainEditResult R = Model.PlaceSoil(Place, Flat);
				TestTrue(TEXT("no echa más de lo que lleva"), R.VolumeAdded <= Budget + 1.0e-3);
				TestTrue(TEXT("echar tierra no quita"), R.VolumeRemoved < 1.0e-9);
				Budget -= R.VolumeAdded;
			}
			TestTrue(*FString::Printf(TEXT("gasta toda la tierra (sobran %.4f m³)"), Budget), FMath::Abs(Budget) < 2.0e-3);
			TestEqual(TEXT("mismo sólido que al principio"), Model.SolidVolume(Box, Flat), Original, 2.0e-3);
		});

		It("echar tierra sin llevar nada no hace nada", [this]()
		{
			FTerrainEditModel Model;
			FSoilPlacement Place;
			Place.SoilBudget = 0.0;
			TestFalse(TEXT("sin cambios"), Model.PlaceSoil(Place, Flat).Changed());
			TestTrue(TEXT("vacío"), Model.IsEmpty());
		});
	});

	Describe("las escaleras picadas", [this]()
	{
		It("se ajustan a la rejilla de 30 cm y a ocho rumbos", [this]()
		{
			FStairCarve In;
			In.Start = FVector(1.04, -0.62, 0.44);
			In.Direction = FVector(1.0, 0.2, 0.7);
			In.StepRise = 0.33f;
			In.StepRun = 0.2f;
			In.NumSteps = 200;
			FStairCarve Out;
			TestTrue(TEXT("válida"), FTerrainEditModel::SnapStairs(In, Out));
			TestEqual(TEXT("rumbo este"), Out.Direction, FVector(1.0, 0.0, 0.0));
			TestEqual(TEXT("origen en rejilla"), Out.Start, FVector(0.9, -0.6, 0.3), 1.0e-5f);
			TestEqual(TEXT("contrahuella 30 cm"), Out.StepRise, 0.3f, 1.0e-5f);
			TestEqual(TEXT("huella mínima 30 cm"), Out.StepRun, 0.3f, 1.0e-5f);
			TestEqual(TEXT("tope de peldaños"), Out.NumSteps, 64);

			In.Direction = FVector(-1.0, -1.1, 0.0);
			In.StepRise = -0.5f;
			FTerrainEditModel::SnapStairs(In, Out);
			TestEqual(TEXT("rumbo diagonal"), Out.Direction, FVector(-0.70710678, -0.70710678, 0.0), 1.0e-5f);
			TestEqual(TEXT("bajando, tope de 45 cm"), Out.StepRise, -0.45f, 1.0e-5f);

			In.Direction = FVector(0.0, 0.0, 1.0);
			TestFalse(TEXT("sin dirección horizontal no hay escalera"), FTerrainEditModel::SnapStairs(In, Out));
		});

		It("tallan huellas planas en una ladera y dejan roca debajo", [this]()
		{
			FTerrainEditModel Model;
			FStairCarve Stairs;
			Stairs.Start = FVector(0.0, 0.0, 0.0);
			Stairs.NumSteps = 5;
			const FTerrainEditResult R = Model.CarveStairs(Stairs, Slope);
			TestTrue(TEXT("arranca tierra"), R.VolumeRemoved > 0.0);
			for (int32 J = 1; J < Stairs.NumSteps; ++J)
			{
				const double U = J * Stairs.StepRun + Stairs.StepRun * 0.5;
				const double Tread = J * Stairs.StepRise;
				TestTrue(*FString::Printf(TEXT("peldaño %d: antes sólido sobre la huella"), J), Slope(FVector(U, 0.0, Tread + 0.08)) < 0.0f);
				TestTrue(*FString::Printf(TEXT("peldaño %d: aire sobre la huella"), J), Model.Density(FVector(U, 0.0, Tread + 0.08), Slope) > 0.0f);
				TestTrue(*FString::Printf(TEXT("peldaño %d: sólido bajo la huella"), J), Model.Density(FVector(U, 0.0, Tread - 0.1), Slope) < 0.0f);
				TestTrue(*FString::Printf(TEXT("peldaño %d: aire a la altura de la cabeza"), J), Model.Density(FVector(U, 0.0, Tread + 1.8), Slope) > 0.0f);
			}
			TestTrue(TEXT("los lados de la escalera siguen siendo ladera"), Model.Density(FVector(0.9, 1.0, 0.5), Slope) < 0.0f);
		});

		It("no tocan nada fuera de su volumen más una celda", [this]()
		{
			FTerrainEditModel Model;
			FStairCarve Stairs;
			Stairs.NumSteps = 4;
			const FBox Box(FVector(-2.0, -2.0, -2.0), FVector(4.0, 2.0, 5.0));
			const FSamples Before = Snapshot(Model, Box, Slope);
			Model.CarveStairs(Stairs, Slope);
			const float H = Model.GetSettings().CellSize;
			const float Length = Stairs.NumSteps * Stairs.StepRun;
			const TArray<FIntVector> Moved = Changed(Before, Snapshot(Model, Box, Slope));
			TestTrue(TEXT("hay cambios"), Moved.Num() > 0);
			for (const FIntVector& G : Moved)
			{
				const FVector P = Model.SamplePosition(G);
				const bool bInside = P.X > -H && P.X < Length + H && FMath::Abs(P.Y) < Stairs.Width * 0.5f + H
					&& P.Z > -H && P.Z < Stairs.StepRise * (Stairs.NumSteps - 1) + Stairs.Headroom + H;
				TestTrue(*FString::Printf(TEXT("dentro del volumen de la escalera (%.2f, %.2f, %.2f)"), P.X, P.Y, P.Z), bInside);
			}
		});

		It("con tope de volumen se tallan poco a poco y acaban igual que de una vez", [this]()
		{
			FStairCarve Stairs;
			Stairs.NumSteps = 4;
			FTerrainEditModel Whole;
			const double Total = Whole.CarveStairs(Stairs, Slope).VolumeRemoved;

			FTerrainEditModel Model;
			Stairs.MaxVolume = 0.08;
			double Sum = 0.0;
			int32 Calls = 0;
			for (; Calls < 200; ++Calls)
			{
				const FTerrainEditResult R = Model.CarveStairs(Stairs, Slope);
				TestTrue(TEXT("cada llamada respeta el tope"), R.VolumeRemoved <= Stairs.MaxVolume + 2.0e-3);
				if (!R.Changed())
				{
					break;
				}
				Sum += R.VolumeRemoved;
			}
			TestTrue(*FString::Printf(TEXT("varias llamadas (%d)"), Calls), Calls >= FMath::FloorToInt32(Total / Stairs.MaxVolume));
			TestEqual(TEXT("mismo volumen al final"), Sum, Total, 5.0e-3);
		});

		It("bajan en una mina desde suelo plano", [this]()
		{
			FTerrainEditModel Model;
			FStairCarve Stairs;
			Stairs.StepRise = -0.3f;
			Stairs.NumSteps = 6;
			Stairs.Width = 1.2f;
			Model.CarveStairs(Stairs, Flat);
			for (int32 J = 1; J < Stairs.NumSteps; ++J)
			{
				const double U = J * Stairs.StepRun + Stairs.StepRun * 0.5;
				const double Tread = -J * 0.3;
				TestTrue(*FString::Printf(TEXT("peldaño %d: aire sobre la huella"), J), Model.Density(FVector(U, 0.0, Tread + 0.08), Flat) > 0.0f);
				TestTrue(*FString::Printf(TEXT("peldaño %d: sólido bajo la huella"), J), Model.Density(FVector(U, 0.0, Tread - 0.1), Flat) < 0.0f);
			}
		});

		It("exigen herramienta para el material", [this]()
		{
			FTerrainEditModel Model;
			FStairCarve Stairs;
			Stairs.Material = ETerrainMaterial::Obsidiana;
			Stairs.ToolTier = 3;
			TestTrue(TEXT("rechazado"), Model.CarveStairs(Stairs, Slope).bRejected);
			TestTrue(TEXT("vacío"), Model.IsEmpty());
		});
	});

	Describe("las entradas no válidas", [this]()
	{
		It("se rechazan sin tocar la rejilla y el guardado sigue cargando", [this]()
		{
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			const double Inf = std::numeric_limits<double>::infinity();
			FTerrainEditModel Model;
			Model.Pickaxe(DownHit(FVector(0.1, 0.1, 0.0)), Flat);
			const FSaveValue Before = Model.ToValue();

			// Antes un pico con X NaN escribía deltas en X = INT_MIN y FromValue rechazaba todo el guardado.
			TestTrue(TEXT("pico en NaN"), Model.Pickaxe(DownHit(FVector(NaN, 0.1, 0.0)), Flat).bRejected);
			FPickaxeHit Sideways = DownHit(FVector(0.1, 0.1, 0.0));
			Sideways.Direction = FVector(Inf, 0.0, 0.0);
			TestTrue(TEXT("dirección infinita"), Model.Pickaxe(Sideways, Flat).bRejected);

			FShovelStroke Stroke;
			Stroke.Center = FVector(0.0, Inf, 0.0);
			TestTrue(TEXT("pala en el infinito"), Model.Shovel(Stroke, Flat).bRejected);
			Stroke.Center = FVector::ZeroVector;
			Stroke.Radius = 1.0e30f; // antes recorría columnas sin tope (cuelgue)
			TestTrue(TEXT("pala con radio enorme"), Model.Shovel(Stroke, Flat).bRejected);

			FSoilPlacement Place;
			Place.SoilBudget = 1.0;
			Place.Radius = 1.0e6f;
			TestTrue(TEXT("tierra con radio enorme"), Model.PlaceSoil(Place, Flat).bRejected);
			Place.Radius = 0.5f;
			Place.Center = FVector(NaN);
			TestTrue(TEXT("tierra en NaN"), Model.PlaceSoil(Place, Flat).bRejected);

			FStairCarve Stairs;
			Stairs.NumSteps = 1000000000; // antes, caja y bucle de peldaños sin tope
			TestTrue(TEXT("mil millones de peldaños"), Model.CarveStairs(Stairs, Slope).bRejected);
			Stairs.NumSteps = 6;
			Stairs.Start = FVector(0.0, 0.0, NaN);
			TestTrue(TEXT("escalera en NaN"), Model.CarveStairs(Stairs, Slope).bRejected);
			Stairs.Start = FVector::ZeroVector;
			Stairs.StepRise = std::numeric_limits<float>::infinity();
			TestTrue(TEXT("contrahuella infinita"), Model.CarveStairs(Stairs, Slope).bRejected);

			TestTrue(TEXT("nada ha cambiado"), FSaveText::Write(Model.ToValue()) == FSaveText::Write(Before));
			FTerrainEditModel Loaded;
			TestTrue(TEXT("el guardado carga"), Loaded.FromValue(Model.ToValue()));
		});

		It("un presupuesto de tierra o de volumen no finito no da tierra ni tallado gratis", [this]()
		{
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			const double Inf = std::numeric_limits<double>::infinity();
			for (const double Budget : { NaN, Inf })
			{
				FTerrainEditModel Model;
				FSoilPlacement Place;
				Place.Center = FVector(0.0, 0.0, 0.3);
				Place.SoilBudget = Budget;
				TestFalse(TEXT("echar tierra"), Model.PlaceSoil(Place, Flat).Changed());

				FShovelStroke Stroke;
				Stroke.bMarkPath = false;
				Stroke.SoilBudget = Budget;
				const FTerrainEditResult R = Model.Shovel(Stroke, Pit);
				TestTrue(TEXT("la pala solo rellena con lo que corta"), R.VolumeAdded <= R.VolumeRemoved + 1.0e-6);

				FStairCarve Stairs;
				Stairs.MaxVolume = Budget;
				TestTrue(TEXT("escalera rechazada"), Model.CarveStairs(Stairs, Slope).bRejected);
			}
		});
	});

	Describe("el guardado", [this]()
	{
		auto BuildEdited = [](FTerrainEditModel& Model)
		{
			for (uint32 Seed = 0; Seed < 4; ++Seed)
			{
				Model.Pickaxe(DownHit(FVector(-7.9 + 0.3 * Seed, 0.1, 0.0), ETerrainMaterial::Tierra, 2, Seed), Flat);
			}
			FShovelStroke Stroke;
			Stroke.Center = FVector(20.0, -5.0, 0.0);
			Model.Shovel(Stroke, Mound);
			Model.Shovel(Stroke, Mound);
			FStairCarve Stairs;
			Stairs.Start = FVector(40.2, 3.0, 0.0);
			Stairs.StepRise = -0.3f;
			Model.CarveStairs(Stairs, Flat);
		};

		It("vuelve igual tras pasar por el texto de la partida", [this, BuildEdited]()
		{
			FTerrainEditModel Model;
			BuildEdited(Model);
			FSaveWorldDeltas World;
			World.Terrain = Model.ToValue();
			FSaveArchive Ar;
			World.Save(Ar);
			FSaveValue Parsed;
			FString Error;
			TestTrue(TEXT("texto válido"), FSaveText::Parse(FSaveText::Write(Ar.GetRoot()), Parsed, Error));
			FSaveWorldDeltas LoadedWorld;
			LoadedWorld.Load(FSaveArchive(Parsed));
			TestTrue(TEXT("sección world igual"), LoadedWorld == World);
			FTerrainEditModel Loaded;
			TestTrue(TEXT("carga"), Loaded.FromValue(LoadedWorld.Terrain));
			TestTrue(TEXT("mismo terreno"), Loaded == Model);
			TestTrue(TEXT("mismos caminos"), Loaded.IsPath(20.1, -5.1));
			TestEqual(TEXT("misma densidad"), Loaded.Density(FVector(-7.5, 0.1, -0.2), Flat), Model.Density(FVector(-7.5, 0.1, -0.2), Flat));
		});

		It("escribe lo mismo sea cual sea el orden de ediciones independientes", [this]()
		{
			FTerrainEditModel A;
			FTerrainEditModel B;
			const FPickaxeHit First = DownHit(FVector(-30.0, 0.0, 0.0));
			const FPickaxeHit Second = DownHit(FVector(30.0, 12.0, 0.0));
			A.Pickaxe(First, Flat);
			A.Pickaxe(Second, Flat);
			B.Pickaxe(Second, Flat);
			B.Pickaxe(First, Flat);
			TestEqual(TEXT("mismo texto"), FSaveText::Write(A.ToValue()), FSaveText::Write(B.ToValue()));
		});

		It("rechaza otra rejilla y datos manipulados, y queda vacío", [this, BuildEdited]()
		{
			FTerrainEditModel Model;
			BuildEdited(Model);
			// Versión 1 (texto): cada campo se puede manipular por separado. La 2 tiene su spec en TerrainEditsSpec.
			const FSaveValue Good = Model.ToValue(1);
			FTerrainEditModel FromV1;
			TestTrue(TEXT("la versión 1 aún se lee"), FromV1.FromValue(Good) && FromV1 == Model);

			FTerrainEditSettings Coarse;
			Coarse.CellSize = 0.5f;
			FTerrainEditModel Other(Coarse);
			TestFalse(TEXT("otra celda"), Other.FromValue(Good));
			TestTrue(TEXT("vacío"), Other.IsEmpty());

			auto Tampered = [&Good](TFunctionRef<void(FSaveValue&)> Edit)
			{
				FSaveValue Copy = Good;
				Edit(Copy);
				FTerrainEditModel Target;
				const bool bOk = Target.FromValue(Copy);
				return !bOk && Target.IsEmpty();
			};
			TestTrue(TEXT("versión desconocida"), Tampered([](FSaveValue& V) { V.Set(TEXT("v"), FSaveValue::MakeInt(3)); }));
			TestTrue(TEXT("índice fuera del chunk"), Tampered([](FSaveValue& V)
			{
				FSaveValue* Runs = V.Find(TEXT("chunks"))->AtMutable(0)->AtMutable(3);
				*Runs->AtMutable(0) = FSaveValue::MakeInt(32 * 32 * 32);
			}));
			TestTrue(TEXT("delta desmesurado"), Tampered([](FSaveValue& V)
			{
				FSaveValue* Runs = V.Find(TEXT("chunks"))->AtMutable(0)->AtMutable(3);
				*Runs->AtMutable(2) = FSaveValue::MakeInt(FTerrainEditModel::MaxDeltaMm + 1);
			}));
			TestTrue(TEXT("tramo que se solapa"), Tampered([](FSaveValue& V)
			{
				FSaveValue* Runs = V.Find(TEXT("chunks"))->AtMutable(0)->AtMutable(3);
				Runs->Add(FSaveValue::MakeInt(0));
				Runs->Add(FSaveValue::MakeInt(1));
				Runs->Add(FSaveValue::MakeInt(5));
			}));
			TestTrue(TEXT("chunk repetido"), Tampered([](FSaveValue& V)
			{
				FSaveValue* List = V.Find(TEXT("chunks"));
				List->Add(List->At(0));
			}));
			TestTrue(TEXT("coordenada de chunk que desborda int32"), Tampered([](FSaveValue& V)
			{
				*V.Find(TEXT("chunks"))->AtMutable(0)->AtMutable(0) = FSaveValue::MakeInt(1 << 30);
			}));
			TestTrue(TEXT("camino con compactación imposible"), Tampered([](FSaveValue& V)
			{
				*V.Find(TEXT("paths"))->AtMutable(2) = FSaveValue::MakeInt(101);
			}));
		});
	});
	Describe("las entradas inválidas", [this]()
	{
		It("un golpe no finito o fuera del mundo se rechaza sin tocar nada y el guardado sigue cargando", [this]()
		{
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			const double Inf = std::numeric_limits<double>::infinity();
			FTerrainEditModel Model;
			TestTrue(TEXT("golpe válido"), Model.Pickaxe(DownHit(FVector(0.0, 0.0, 0.0)), Flat).Changed());
			const FSaveValue Before = Model.ToValue();

			const FPickaxeHit Bad[] = {
				DownHit(FVector(NaN, 0.0, 0.0)),
				DownHit(FVector(0.0, Inf, 0.0)),
				DownHit(FVector(0.0, 0.0, -1.0e12)),
				DownHit(FVector(FTerrainEditModel::MaxWorldCoordinate * 2.0, 0.0, 0.0)),
			};
			for (const FPickaxeHit& Hit : Bad)
			{
				const FTerrainEditResult R = Model.Pickaxe(Hit, Flat);
				TestTrue(TEXT("rechazado"), R.bRejected);
				TestFalse(TEXT("sin cambios"), R.Changed());
				TestEqual(TEXT("sin chunks sucios"), R.DirtyChunks.Num(), 0);
			}
			FPickaxeHit InfDir = DownHit(FVector(0.0, 0.0, 0.0));
			InfDir.Direction = FVector(Inf, 0.0, 0.0);
			TestTrue(TEXT("dirección infinita rechazada"), Model.Pickaxe(InfDir, Flat).bRejected);

			TestTrue(TEXT("el terreno no ha cambiado"), Model.ToValue() == Before);
			FTerrainEditModel Loaded;
			TestTrue(TEXT("el guardado carga"), Loaded.FromValue(Model.ToValue()));
			TestTrue(TEXT("y vuelve igual"), Loaded == Model);
		});

		It("la pala y echar tierra rechazan pinceles no finitos o desmesurados sin recorrerlos", [this]()
		{
			const float NaNf = std::numeric_limits<float>::quiet_NaN();
			FTerrainEditModel Model;
			auto Stroke = [](TFunctionRef<void(FShovelStroke&)> Edit)
			{
				FShovelStroke S;
				S.Center = FVector(0.0, 0.0, 0.3);
				Edit(S);
				return S;
			};
			const FShovelStroke BadStrokes[] = {
				// 200 m de radio recorrería miles de millones de muestras: la partida se colgaría.
				Stroke([](FShovelStroke& S) { S.Radius = 200.0f; }),
				Stroke([&](FShovelStroke& S) { S.Radius = NaNf; }),
				Stroke([](FShovelStroke& S) { S.EdgeWidth = 50.0f; }),
				Stroke([](FShovelStroke& S) { S.VerticalReach = 1.0e6f; }),
				Stroke([&](FShovelStroke& S) { S.EdgeWidth = NaNf; }),
				Stroke([](FShovelStroke& S) { S.SoilBudget = std::numeric_limits<double>::quiet_NaN(); }),
				Stroke([](FShovelStroke& S) { S.Center.X = std::numeric_limits<double>::infinity(); }),
				Stroke([](FShovelStroke& S) { S.PlaneNormal = FVector(0.0, std::numeric_limits<double>::quiet_NaN(), 1.0); }),
			};
			for (const FShovelStroke& S : BadStrokes)
			{
				const FTerrainEditResult R = Model.Shovel(S, Mound);
				TestTrue(TEXT("pala rechazada"), R.bRejected);
				TestEqual(TEXT("sin chunks sucios"), R.DirtyChunks.Num(), 0);
			}

			FSoilPlacement Place;
			Place.SoilBudget = 1.0;
			Place.Radius = 50.0f;
			TestTrue(TEXT("tierra: radio desmesurado"), Model.PlaceSoil(Place, Pit).bRejected);
			Place.Radius = NaNf;
			TestTrue(TEXT("tierra: radio NaN"), Model.PlaceSoil(Place, Pit).bRejected);
			Place.Radius = 0.5f;
			Place.SoilBudget = std::numeric_limits<double>::infinity();
			TestTrue(TEXT("tierra: presupuesto infinito"), Model.PlaceSoil(Place, Pit).bRejected);
			Place.SoilBudget = 1.0;
			Place.Center = FVector(0.0, std::numeric_limits<double>::quiet_NaN(), 0.0);
			TestTrue(TEXT("tierra: centro NaN"), Model.PlaceSoil(Place, Pit).bRejected);
			TestTrue(TEXT("vacío"), Model.IsEmpty());

			// En el tope sí se admite.
			FShovelStroke Max = Stroke([](FShovelStroke& S) {});
			Max.Radius = FTerrainEditModel::MaxBrushExtent;
			Max.EdgeWidth = FTerrainEditModel::MaxBrushExtent;
			TestFalse(TEXT("pala en el tope admitida"), Model.Shovel(Max, Mound).bRejected);
		});

		It("compactar una franja fuera del mundo o desmesurada no recorre nada", [this]()
		{
			FTerrainEditModel Model;
			// X = 1e9 da la columna centinela MAX_int32: el bucle desbordaba (UB).
			TestEqual(TEXT("fuera del mundo"), Model.CompactStrip(FVector(1.0e9, 0.0, 0.0), FVector(1.0e9, 1.0, 0.0), 0.75f).DirtyChunks.Num(), 0);
			// Dentro del mundo pero a 200 km: ~10^12 columnas.
			TestEqual(TEXT("tramo desmesurado"), Model.CompactStrip(FVector(-99000.0, 0.0, 0.0), FVector(99000.0, 0.0, 0.0), 0.75f).DirtyChunks.Num(), 0);
			TestEqual(TEXT("ancho desmesurado"), Model.CompactStrip(FVector::ZeroVector, FVector(1.0, 0.0, 0.0), 500.0f).DirtyChunks.Num(), 0);
			TestEqual(TEXT("sin camino"), Model.NumPathColumns(), 0);
			// Un tramo de pala normal sí compacta.
			TestTrue(TEXT("tramo de 4 m"), Model.CompactStrip(FVector::ZeroVector, FVector(4.0, 0.0, 0.0), 0.75f).DirtyChunks.Num() > 0);
			TestTrue(TEXT("con camino"), Model.NumPathColumns() > 0);
		});

		It("las escaleras rechazan lo que pasa de los topes y lo no finito; lo ajustado siempre se talla", [this]()
		{
			const float NaNf = std::numeric_limits<float>::quiet_NaN();
			FTerrainEditModel Model;
			auto Carve = [](TFunctionRef<void(FStairCarve&)> Edit)
			{
				FStairCarve S;
				Edit(S);
				return S;
			};
			const FStairCarve Bad[] = {
				Carve([](FStairCarve& S) { S.NumSteps = 100000; }),
				Carve([](FStairCarve& S) { S.Width = 40.0f; }),
				Carve([](FStairCarve& S) { S.Headroom = 1.0e6f; }),
				Carve([](FStairCarve& S) { S.StepRun = 5.0f; }),
				Carve([](FStairCarve& S) { S.StepRise = -3.0f; }),
				Carve([&](FStairCarve& S) { S.StepRise = NaNf; }),
				Carve([&](FStairCarve& S) { S.Width = NaNf; }),
				Carve([](FStairCarve& S) { S.Start.Z = std::numeric_limits<double>::quiet_NaN(); }),
				Carve([](FStairCarve& S) { S.Direction = FVector(std::numeric_limits<double>::infinity(), 0.0, 0.0); }),
				Carve([](FStairCarve& S) { S.MaxVolume = std::numeric_limits<double>::quiet_NaN(); }),
			};
			for (const FStairCarve& S : Bad)
			{
				TestTrue(TEXT("escalera rechazada"), Model.CarveStairs(S, Slope).bRejected);
			}
			TestTrue(TEXT("vacío"), Model.IsEmpty());

			FStairCarve Out;
			TestFalse(TEXT("SnapStairs: dirección NaN"),
				FTerrainEditModel::SnapStairs(Carve([](FStairCarve& S) { S.Direction.X = std::numeric_limits<double>::quiet_NaN(); }), Out));
			TestFalse(TEXT("SnapStairs: arranque infinito"),
				FTerrainEditModel::SnapStairs(Carve([](FStairCarve& S) { S.Start.Y = std::numeric_limits<double>::infinity(); }), Out));
			TestFalse(TEXT("SnapStairs: huella NaN"),
				FTerrainEditModel::SnapStairs(Carve([&](FStairCarve& S) { S.StepRun = NaNf; }), Out));

			// Todo al máximo: SnapStairs lo deja justo en los topes y CarveStairs lo acepta.
			const FStairCarve Huge = Carve([](FStairCarve& S)
			{
				S.NumSteps = 500;
				S.StepRise = -9.0f;
				S.StepRun = 9.0f;
				S.Width = 9.0f;
				S.Headroom = 9.0f;
				S.MaxVolume = 0.1;
			});
			TestTrue(TEXT("se ajusta"), FTerrainEditModel::SnapStairs(Huge, Out));
			const FTerrainEditResult R = Model.CarveStairs(Out, Flat);
			TestFalse(TEXT("ajustada: admitida"), R.bRejected);
			TestTrue(TEXT("ajustada: talla"), R.Changed());
		});
	});
}

#endif
