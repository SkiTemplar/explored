#include "Misc/AutomationTest.h"

#include "Tests/GrassTestWorld.h"
#include "WorldGen/GrassFieldModel.h"
#include "WorldGen/GrassThinning.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GrassThinningTest
{
	using namespace GrassTestWorld;

	/** Solo parches (la alfombra), a Density por m² en todo el llano. */
	FGrassConfig PatchConfig(float Density)
	{
		FGrassConfig Config = MakeConfig();
		Config.ZoneDensity.Empty();
		Config.DensityPerM2 = Density;
		FGrassMix Patches;
		Patches.Weights[static_cast<int32>(EGrassKind::PatchA)] = 1.0f;
		Patches.Weights[static_cast<int32>(EGrassKind::PatchB)] = 1.0f;
		Patches.Weights[static_cast<int32>(EGrassKind::PatchC)] = 1.0f;
		for (FGrassMix& Mix : Config.Mixes)
		{
			Mix = Patches;
		}
		return Config;
	}

	/** Fracción de puntos (rejilla de 2 cm) del cuadrado [Lo, Lo + Side] dentro de la huella de algún parche. */
	double Coverage(const FGrassConfig& Config, const TArray<FGrassInstance>& All, const FVector2D& Lo, double Side)
	{
		constexpr double Step = 0.02;
		const int32 Cells = FMath::RoundToInt32(Side / Step);
		TArray<uint8> Covered;
		Covered.Init(0, Cells * Cells);
		for (const FGrassInstance& I : All)
		{
			const double R = Config.KindFootprintM[static_cast<int32>(I.Kind)] * I.Scale;
			const int32 X0 = FMath::Max(0, FMath::FloorToInt32((I.Location.X - R - Lo.X) / Step));
			const int32 X1 = FMath::Min(Cells - 1, FMath::CeilToInt32((I.Location.X + R - Lo.X) / Step));
			const int32 Y0 = FMath::Max(0, FMath::FloorToInt32((I.Location.Y - R - Lo.Y) / Step));
			const int32 Y1 = FMath::Min(Cells - 1, FMath::CeilToInt32((I.Location.Y + R - Lo.Y) / Step));
			for (int32 X = X0; X <= X1; ++X)
			{
				for (int32 Y = Y0; Y <= Y1; ++Y)
				{
					const FVector2D P = Lo + FVector2D((X + 0.5) * Step, (Y + 0.5) * Step);
					if (FVector2D::DistSquared(P, FVector2D(I.Location.X, I.Location.Y)) <= R * R)
					{
						Covered[X * Cells + Y] = 1;
					}
				}
			}
		}
		int32 Count = 0;
		for (const uint8 C : Covered)
		{
			Count += C;
		}
		return static_cast<double>(Count) / Covered.Num();
	}
}

BEGIN_DEFINE_SPEC(FGrassThinningSpec, "Explored.WorldGen.GrassThinning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FGrassThinningSpec)

void FGrassThinningSpec::Define()
{
	using namespace GrassThinningTest;

	Describe(TEXT("f(d)"), [this]()
	{
		It(TEXT("es 1 cerca, 0 al final y nunca crece con la distancia"), [this]()
		{
			const FGrassThinning Thin = FGrassConfig::Defaults().Thinning;
			FString Error;
			TestTrue(TEXT("válido"), Thin.Validate(Error));
			TestEqual(TEXT("cerca"), Thin.Fraction(0.0), 1.0f);
			TestEqual(TEXT("borde de la zona densa"), Thin.Fraction(Thin.DenseM), 1.0f);
			TestEqual(TEXT("final"), Thin.Fraction(Thin.EndM), 0.0f);
			TestEqual(TEXT("NaN"), Thin.Fraction(std::numeric_limits<double>::quiet_NaN()), 0.0f);
			int32 Rises = 0;
			int32 LevelDrops = 0;
			int32 Short = 0;
			float Prev = 2.0f;
			int32 PrevLevel = 0;
			for (double D = 0.0; D <= Thin.EndM + 5.0; D += 0.1)
			{
				const float F = Thin.Fraction(D);
				Rises += F > Prev + 1e-6f ? 1 : 0;
				Prev = F;
				const float Grow = Thin.Grow(D);
				TestTrue(TEXT("agrandado acotado"), Grow >= 1.0f && Grow <= Thin.MaxGrow);
				const int32 Level = Thin.LevelFor(D, 0.0);
				if (Level != INDEX_NONE)
				{
					LevelDrops += Level < PrevLevel ? 1 : 0;
					PrevLevel = Level;
					// El nivel tiene todos los parches que el material puede enseñar a esa distancia.
					Short += Thin.LevelFraction(Level) + 1e-6f >= Thin.Reach(D) ? 0 : 1;
				}
			}
			TestEqual(TEXT("f no crece"), Rises, 0);
			TestEqual(TEXT("el nivel no se densifica al alejarse"), LevelDrops, 0);
			TestEqual(TEXT("niveles por debajo de f"), Short, 0);
			TestEqual(TEXT("sin celda más allá del final"), Thin.LevelFor(Thin.EndM + 1.0, 0.0), INDEX_NONE);
			TestTrue(TEXT("el último nivel baja de minLevel"), Thin.LevelFraction(Thin.LevelCount() - 1) <= Thin.MinLevel);
		});

		It(TEXT("encoge cada parche de forma continua según su rango"), [this]()
		{
			const FGrassThinning Thin = FGrassConfig::Defaults().Thinning;
			for (const float Rank : {0.01f, 0.1f, 0.3f, 0.7f, 0.99f})
			{
				float Prev = 1.0f;
				float MaxStep = 0.0f;
				int32 Rises = 0;
				for (double D = 0.0; D <= Thin.EndM; D += 0.05)
				{
					const float V = Thin.Visibility(Rank, D);
					Rises += V > Prev + 1e-6f ? 1 : 0;
					MaxStep = FMath::Max(MaxStep, Prev - V);
					Prev = V;
				}
				TestEqual(TEXT("no vuelve a crecer"), Rises, 0);
				// A paso de 5 cm, ningún parche pierde más de un cuarto de su tamaño de golpe.
				TestTrue(FString::Printf(TEXT("sin salto (rango %.2f, salto %.2f)"), Rank, MaxStep), MaxStep < 0.25f);
				TestEqual(TEXT("desaparecido al final"), Prev, 0.0f);
			}
			TestEqual(TEXT("cerca, entero"), Thin.Visibility(0.99f, 1.0), 1.0f);
		});

		It(TEXT("rechaza parámetros sin sentido"), [this]()
		{
			FString Error;
			FGrassThinning Thin;
			Thin.EndFadeM = Thin.EndM;
			TestFalse(TEXT("fundido más largo que la franja"), Thin.Validate(Error));
			Thin = FGrassThinning();
			Thin.MaxGrow = 0.5f;
			TestFalse(TEXT("encoger para compensar"), Thin.Validate(Error));
			Thin = FGrassThinning();
			Thin.LevelRatio = 1.0f;
			TestFalse(TEXT("niveles que no bajan"), Thin.Validate(Error));
		});
	});

	Describe(TEXT("Niveles"), [this]()
	{
		It(TEXT("cada nivel es un subconjunto del anterior y las franjas suman la celda entera"), [this]()
		{
			const FGrassConfig Config = MakeConfig();
			const TSharedPtr<FGrassWorld> World = MakeWorld();
			const FIntPoint Cell(-5, 4);
			const FGrassThinning& Thin = Config.Thinning;
			TArray<FGrassInstance> Full;
			FGrassFieldModel::GenerateCell(Config, *World, Cell, Full);
			TestTrue(TEXT("hay matas"), Full.Num() > 100);
			int32 Missing = 0;
			int32 BandSum = 0;
			for (int32 Level = 0; Level + 1 < Thin.LevelCount(); ++Level)
			{
				TArray<FGrassInstance> Dense, Sparse, Band;
				FGrassFieldModel::GenerateCell(Config, *World, Cell, 0.0f, Thin.LevelFraction(Level), Dense);
				FGrassFieldModel::GenerateCell(Config, *World, Cell, 0.0f, Thin.LevelFraction(Level + 1), Sparse);
				FGrassFieldModel::GenerateCell(Config, *World, Cell, Thin.LevelFraction(Level + 1), Thin.LevelFraction(Level), Band);
				TestEqual(TEXT("ralo + franja = denso"), Sparse.Num() + Band.Num(), Dense.Num());
				for (const FGrassInstance& S : Sparse)
				{
					bool bFound = false;
					for (const FGrassInstance& D : Dense)
					{
						bFound |= D.Location == S.Location && D.Kind == S.Kind && D.Rank == S.Rank;
					}
					Missing += bFound ? 0 : 1;
				}
				BandSum += Band.Num();
			}
			TArray<FGrassInstance> Last;
			FGrassFieldModel::GenerateCell(Config, *World, Cell, 0.0f, Thin.LevelFraction(Thin.LevelCount() - 1), Last);
			TestEqual(TEXT("parches que faltan en el nivel más denso"), Missing, 0);
			TestEqual(TEXT("las franjas cubren la celda"), BandSum + Last.Num(), Full.Num());
		});

		It(TEXT("la densidad sembrada sigue a f(d) y baja con la distancia"), [this]()
		{
			const FGrassConfig Config = PatchConfig(4.0f);
			FGrassFieldModel Model(Config, MakeWorld());
			const FVector2D Camera(-40.0, -20.0);
			int32 MaxWork = 0;
			Settle(Model, Camera, nullptr, MaxWork);
			const FGrassThinning& Thin = Config.Thinning;
			TArray<FGrassInstance> Live;
			for (int32 X = -20; X <= 20; ++X)
			{
				for (int32 Y = -20; Y <= 20; ++Y)
				{
					if (const TArray<FGrassInstance>* Cell = Model.FindCell(FIntPoint(X, Y)))
					{
						Live.Append(*Cell);
					}
				}
			}
			double Prev = TNumericLimits<double>::Max();
			for (const double Ring : {0.0, 8.0, 16.0, 24.0, 32.0})
			{
				const double Outer = Ring + 6.0;
				const int32 In = CountIn(Live, Camera, Outer) - (Ring > 0.0 ? CountIn(Live, Camera, Ring) : 0);
				const double Area = UE_DOUBLE_PI * (Outer * Outer - Ring * Ring);
				const double Rate = In / Area;
				// Al menos lo que el material puede enseñar en el borde exterior del anillo.
				TestTrue(FString::Printf(TEXT("anillo %.0f m: %.2f / m² ≥ 4·f"), Ring, Rate), Rate >= 0.85 * 4.0 * Thin.Fraction(Outer));
				TestTrue(FString::Printf(TEXT("anillo %.0f m: no más denso que el anterior"), Ring), Rate <= Prev * 1.1);
				Prev = Rate;
			}
			TestTrue(TEXT("cerca, la densidad entera"), CountIn(Live, Camera, 6.0) / (UE_DOUBLE_PI * 36.0) > 3.4);
		});

		It(TEXT("cubre el suelo a 5,5 parches por m²"), [this]()
		{
			const FGrassConfig Config = PatchConfig(5.5f);
			const TSharedPtr<FGrassWorld> World = MakeWorld();
			// Llano sin zonas, caminos ni exclusiones: celdas (-6..-4, -3..-1), cuadrado de 4 × 4 m en el centro.
			const TArray<FGrassInstance> All = Generate(Config, *World, -6, -4, -3, -1);
			const double Covered = Coverage(Config, All, FVector2D(-42.0, -18.0), 4.0);
			TestTrue(FString::Printf(TEXT("cobertura %.1f %% ≥ 90 %%"), Covered * 100.0), Covered >= 0.9);
			const FGrassConfig Sparse = PatchConfig(1.0f);
			const double Bare = Coverage(Sparse, Generate(Sparse, *World, -6, -4, -3, -1), FVector2D(-42.0, -18.0), 4.0);
			TestTrue(TEXT("a 1 por m² se ve el suelo (el test distingue)"), Bare < 0.6);
		});
	});

	Describe(TEXT("Sin saltos"), [this]()
	{
		It(TEXT("al caminar ningún parche aparece ni desaparece con tamaño"), [this]()
		{
			const FGrassConfig Config = PatchConfig(3.0f);
			FGrassFieldModel Model(Config, MakeWorld());
			FFakeComponents Fake;
			FVector2D Camera(-50.0, -40.0);
			int32 MaxWork = 0;
			Settle(Model, Camera, &Fake, MaxWork);
			// 60 fps a 6 m/s en diagonal y vuelta atrás: celdas que entran, suben, bajan y salen.
			const FVector2D Step(0.08, 0.06);
			int32 Changes = 0;
			for (int32 Frame = 0; Frame < 1800; ++Frame)
			{
				Camera += Frame < 1200 ? Step : -Step;
				const FGrassUpdate Update = Model.Update(Camera);
				Changes += Update.CellsBuilt + Update.CellsRemoved;
				TestTrue(TEXT("orden de ranuras"), Fake.Apply(Update, &Config.Thinning, Camera));
				TestTrue(TEXT("presupuesto (más un trabajo)"), Update.Work <= Config.WorkPerFrame + Config.Subdivisions() * Config.Subdivisions());
			}
			TestTrue(TEXT("hubo cambios de nivel"), Changes > 100);
			TestEqual(TEXT("parches que saltan"), Fake.Pops, 0);
			TestEqual(TEXT("visibles = vivas"), Fake.Visible(), Model.GetLiveInstanceCount());
		});

		It(TEXT("cuenta los triángulos del sector a la vista"), [this]()
		{
			const FGrassConfig Config = PatchConfig(3.0f);
			FGrassFieldModel Model(Config, MakeWorld());
			int32 MaxWork = 0;
			Settle(Model, FVector2D(-40.0, -20.0), nullptr, MaxWork);
			int32 Tris[GrassKindCount] = {};
			for (int32& T : Tris)
			{
				T = 10;
			}
			const int64 All = Model.CountTrianglesInView(FVector2D(-40.0, -20.0), FVector2D(1.0, 0.0), UE_PI, Tris);
			const int64 Quarter = Model.CountTrianglesInView(FVector2D(-40.0, -20.0), FVector2D(1.0, 0.0), UE_PI / 4.0f, Tris);
			TestEqual(TEXT("todo el círculo"), All, static_cast<int64>(Model.GetLiveInstanceCount()) * 10);
			TestTrue(TEXT("un sector de 90° ve menos de la mitad"), Quarter > 0 && Quarter * 2 < All);
		});
	});
}

#endif
