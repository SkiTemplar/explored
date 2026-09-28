#include "Misc/AutomationTest.h"

#include "WorldGen/FellingModel.h"
#include "WorldGen/TreeFallModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FTreeFallModelSpec, "Explored.TreeFall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TArray<FFellingProfile> Profiles;
	const FFellingProfile& Get(const TCHAR* Species)
	{
		const FFellingProfile* P = FFellingModel::FindProfile(Profiles, FName(Species));
		check(P);
		return *P;
	}
	/** Ángulo con signo de A a B, en grados. */
	static double AngleDeg(const FVector2D& A, const FVector2D& B)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(A.X * B.Y - A.Y * B.X, A.X * B.X + A.Y * B.Y));
	}
	static FTreeFallWind Wind(const FVector2D& Dir, float Strength)
	{
		FTreeFallWind W;
		W.Direction = Dir;
		W.Strength = Strength;
		return W;
	}
	static FTreeFallObstacle Piece(const FVector2D& Center, int32 Tier, float MaxIntegrity, double Top = 250.0, double Radius = 100.0)
	{
		FTreeFallObstacle O;
		O.Kind = ETreeFallObstacleKind::Building;
		O.Center = Center;
		O.RadiusCm = Radius;
		O.TopCm = Top;
		O.TierOrder = Tier;
		O.MaxIntegrity = MaxIntegrity;
		return O;
	}
END_DEFINE_SPEC(FTreeFallModelSpec)

void FTreeFallModelSpec::Define()
{
	BeforeEach([this]()
	{
		Profiles = FFellingModel::DefaultProfiles();
	});

	Describe("el viento", [this]()
	{
		It("a 0 no desvía nada: cae exactamente hacia el golpe, sople hacia donde sople", [this]()
		{
			const FVector2D Hit(0.6, 0.8);
			for (const FVector2D& WindDir : { FVector2D(1.0, 0.0), FVector2D(0.0, -1.0), FVector2D(-0.6, -0.8), FVector2D::ZeroVector })
			{
				TestTrue(TEXT("sin cambio"), FTreeFallModel::ApplyWind(Hit, Wind(WindDir, 0.0f), 20.0) == Hit.GetSafeNormal());
			}
			FFellingProgress Progress;
			const FFellingProfile& Giant = Get(TEXT("JungleGiant"));
			while (!FFellingModel::ApplyHit(Giant, Progress, EFellingTool::Edge, Hit)) {}
			const FVector2D Dir = FTreeFallModel::ResolveDirection(Giant, Progress, FVector2D::ZeroVector, Wind(FVector2D(1.0, 0.0), 0.0f), 5u);
			TestTrue(TEXT("ResolveDirection sin viento = golpe final"), Dir.Equals(Hit.GetSafeNormal(), 1.0e-12));
		});

		It("al máximo y de lado desvía exactamente el tope de la especie: 20°, la palmera 15°", [this]()
		{
			const FVector2D Hit(1.0, 0.0);
			const FFellingProfile& Giant = Get(TEXT("JungleGiant"));
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			TestEqual(TEXT("gigante, viento a +Y"), AngleDeg(Hit, FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(0.0, 1.0), 1.0f), Giant.WindDeviationDeg)), 20.0, 1.0e-9);
			TestEqual(TEXT("gigante, viento a -Y"), AngleDeg(Hit, FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(0.0, -1.0), 1.0f), Giant.WindDeviationDeg)), -20.0, 1.0e-9);
			TestEqual(TEXT("palmera, viento a +Y"), AngleDeg(Hit, FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(0.0, 3.0), 1.0f), Palm.WindDeviationDeg)), 15.0, 1.0e-9);
			TestEqual(TEXT("a media fuerza, la mitad"), AngleDeg(Hit, FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(0.0, 1.0), 0.5f), 20.0)), 10.0, 1.0e-9);
			TestEqual(TEXT("más de 1 se acota a 1"), AngleDeg(Hit, FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(0.0, 1.0), 7.0f), 20.0)), 20.0, 1.0e-9);
		});

		It("al máximo nunca pasa del tope ni de la propia dirección del viento", [this]()
		{
			const FVector2D Hit(1.0, 0.0);
			for (int32 Deg = -179; Deg <= 179; ++Deg)
			{
				const double Rad = FMath::DegreesToRadians((double)Deg);
				const FVector2D Out = FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)), 1.0f), 20.0);
				const double Turn = AngleDeg(Hit, Out);
				const double Expected = FMath::Sign((double)Deg) * FMath::Min(FMath::Abs((double)Deg), 20.0);
				if (!TestEqual(*FString::Printf(TEXT("viento a %d°"), Deg), Turn, Expected, 1.0e-6)) { return; }
				TestEqual(TEXT("unitaria"), Out.Size(), 1.0, 1.0e-9);
			}
		});

		It("al máximo de cara o de espaldas no desvía (no hay lado)", [this]()
		{
			const FVector2D Hit(0.0, 1.0);
			TestTrue(TEXT("de espaldas"), FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(0.0, 5.0), 1.0f), 20.0).Equals(Hit, 1.0e-12));
			TestTrue(TEXT("de cara"), FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(0.0, -5.0), 1.0f), 20.0).Equals(Hit, 1.0e-12));
		});

		It("ignora viento NaN o infinito y tope negativo", [this]()
		{
			const FVector2D Hit(1.0, 0.0);
			TestTrue(TEXT("dirección NaN"), FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(NAN, 1.0), 1.0f), 20.0) == Hit);
			TestTrue(TEXT("fuerza NaN"), FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(0.0, 1.0), NAN), 20.0) == Hit);
			TestTrue(TEXT("fuerza infinita"), FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(0.0, 1.0), INFINITY), 20.0) == Hit);
			TestTrue(TEXT("tope negativo"), FTreeFallModel::ApplyWind(Hit, Wind(FVector2D(0.0, 1.0), 1.0f), -20.0) == Hit);
		});

		It("el arbusto (tope 0) no se desvía aunque sople al máximo", [this]()
		{
			FFellingProgress Progress;
			const FFellingProfile& Shrub = Get(TEXT("Shrub"));
			FFellingModel::ApplyHit(Shrub, Progress, EFellingTool::Hands, FVector2D(1.0, 0.0));
			TestTrue(TEXT("igual"), FTreeFallModel::ResolveDirection(Shrub, Progress, FVector2D::ZeroVector, Wind(FVector2D(0.0, 1.0), 1.0f), 1u).Equals(FVector2D(1.0, 0.0), 1.0e-12));
		});

		It("es determinista: los dos extremos de la red calculan la misma dirección", [this]()
		{
			FFellingProgress Progress;
			const FFellingProfile& Wide = Get(TEXT("JungleWide"));
			while (!FFellingModel::ApplyHit(Wide, Progress, EFellingTool::Hands, FVector2D(-0.3, 0.9))) {}
			const FVector2D A = FTreeFallModel::ResolveDirection(Wide, Progress, FVector2D(0.1, 0.05), Wind(FVector2D(0.2, -1.0), 0.73f), 99u);
			const FVector2D B = FTreeFallModel::ResolveDirection(Wide, Progress, FVector2D(0.1, 0.05), Wind(FVector2D(0.2, -1.0), 0.73f), 99u);
			TestTrue(TEXT("bit a bit"), A == B);
		});
	});

	Describe("la colisión", [this]()
	{
		It("sin nada en medio llega al suelo", [this]()
		{
			const FTreeFallResult R = FTreeFallModel::Resolve(Get(TEXT("Palm")), FVector2D::ZeroVector, FVector2D(1.0, 0.0), {});
			TestEqual(TEXT("90°"), R.RestAngleDeg, 90.0);
			TestEqual(TEXT("alcance entero"), R.ReachFraction, 1.0);
			TestEqual(TEXT("nada aplastado"), R.Crushed.Num(), 0);
			TestEqual(TEXT("nada lo para"), R.StoppedBy.ObstacleIndex, (int32)INDEX_NONE);
		});

		It("aplasta la construcción ligera (palma y bambú) con el 40 % de su integridad y sigue cayendo", [this]()
		{
			TArray<FTreeFallObstacle> Obstacles;
			Obstacles.Add(Piece(FVector2D(600.0, 0.0), 1, 80.0f));	// bambú, más lejos
			Obstacles.Add(Piece(FVector2D(300.0, 0.0), 0, 50.0f));	// palma, más cerca
			const FTreeFallResult R = FTreeFallModel::Resolve(Get(TEXT("Palm")), FVector2D::ZeroVector, FVector2D(1.0, 0.0), Obstacles);
			if (TestEqual(TEXT("dos aplastadas"), R.Crushed.Num(), 2))
			{
				TestEqual(TEXT("primero la más cercana"), R.Crushed[0].ObstacleIndex, 1);
				TestEqual(TEXT("40 % de 50"), R.Crushed[0].Damage, 20.0f);
				TestEqual(TEXT("40 % de 80"), R.Crushed[1].Damage, 32.0f);
				TestTrue(TEXT("en orden de ángulo"), R.Crushed[0].ContactAngleDeg < R.Crushed[1].ContactAngleDeg);
			}
			TestEqual(TEXT("llega al suelo"), R.RestAngleDeg, 90.0);
		});

		It("la construcción pesada lo para apoyado y lo que queda detrás no se toca", [this]()
		{
			TArray<FTreeFallObstacle> Obstacles;
			Obstacles.Add(Piece(FVector2D(300.0, 0.0), 0, 50.0f));	// palma delante
			Obstacles.Add(Piece(FVector2D(500.0, 0.0), 3, 400.0f));	// piedra
			Obstacles.Add(Piece(FVector2D(800.0, 0.0), 0, 50.0f));	// palma detrás
			const FTreeFallResult R = FTreeFallModel::Resolve(Get(TEXT("Palm")), FVector2D::ZeroVector, FVector2D(1.0, 0.0), Obstacles);
			TestEqual(TEXT("solo la de delante"), R.Crushed.Num(), 1);
			TestEqual(TEXT("para la piedra"), R.StoppedBy.ObstacleIndex, 1);
			TestEqual(TEXT("sin daño a la piedra"), R.StoppedBy.Damage, 0.0f);
			// Cara cercana a 500 − 100 − 20 = 380 cm, arriba a 250 cm: atan(380 / 250).
			TestEqual(TEXT("ángulo de apoyo"), R.RestAngleDeg, FMath::RadiansToDegrees(FMath::Atan2(380.0, 250.0)), 1.0e-9);
			TestEqual(TEXT("alcance = seno"), R.ReachFraction, FMath::Sin(FMath::Atan2(380.0, 250.0)), 1.0e-12);
		});

		It("una loma del terreno lo para igual, sin daño", [this]()
		{
			FTreeFallObstacle Hill;
			Hill.Kind = ETreeFallObstacleKind::Terrain;
			Hill.Center = FVector2D(0.0, 700.0);
			Hill.RadiusCm = 50.0;
			Hill.TopCm = 300.0;
			const FTreeFallResult R = FTreeFallModel::Resolve(Get(TEXT("JungleGiant")), FVector2D::ZeroVector, FVector2D(0.0, 1.0), { Hill });
			TestEqual(TEXT("la loma lo para"), R.StoppedBy.ObstacleIndex, 0);
			TestTrue(TEXT("apoyado"), R.RestAngleDeg < 90.0 && R.ReachFraction < 1.0);
			TestEqual(TEXT("sin aplastar"), R.Crushed.Num(), 0);
		});

		It("no toca lo que queda fuera del alcance, a un lado, detrás, por debajo o hundido", [this]()
		{
			const FFellingProfile& Palm = Get(TEXT("Palm")); // 9 m
			TArray<FTreeFallObstacle> Obstacles;
			Obstacles.Add(Piece(FVector2D(1000.0, 0.0), 3, 100.0f));			// más allá de la punta
			Obstacles.Add(Piece(FVector2D(400.0, 200.0), 3, 100.0f, 250.0, 150.0));	// a un lado: 200 ≥ 150 + 20
			Obstacles.Add(Piece(FVector2D(-400.0, 0.0), 3, 100.0f));			// detrás
			Obstacles.Add(Piece(FVector2D(400.0, 0.0), 3, 100.0f, 0.0));		// a ras de suelo (TopCm 0)
			Obstacles.Add(Piece(FVector2D(400.0, 0.0), 3, 100.0f, -80.0));		// en un hoyo
			const FTreeFallResult R = FTreeFallModel::Resolve(Palm, FVector2D::ZeroVector, FVector2D(1.0, 0.0), Obstacles);
			TestEqual(TEXT("al suelo"), R.RestAngleDeg, 90.0);
			TestEqual(TEXT("sin contacto"), R.Crushed.Num() + (R.StoppedBy.ObstacleIndex != INDEX_NONE ? 1 : 0), 0);
		});

		It("la punta alcanza justo una pieza a la distancia exacta de la altura y no una más allá", [this]()
		{
			const FFellingProfile& Palm = Get(TEXT("Palm")); // 900 cm
			// Cara cercana en 900·sen(60°), arriba en 900·cos(60°): la esquina está sobre el arco.
			const double Near = 900.0 * FMath::Sin(FMath::DegreesToRadians(60.0));
			const double Top = 900.0 * FMath::Cos(FMath::DegreesToRadians(60.0));
			TArray<FTreeFallObstacle> On = { Piece(FVector2D(Near + 120.0 - 1.0e-6, 0.0), 3, 100.0f, Top) };
			TestEqual(TEXT("en el arco: lo para"), FTreeFallModel::Resolve(Palm, FVector2D::ZeroVector, FVector2D(1.0, 0.0), On).StoppedBy.ObstacleIndex, 0);
			TArray<FTreeFallObstacle> Off = { Piece(FVector2D(Near + 120.0 + 1.0, 0.0), 3, 100.0f, Top) };
			TestEqual(TEXT("fuera: al suelo"), FTreeFallModel::Resolve(Palm, FVector2D::ZeroVector, FVector2D(1.0, 0.0), Off).StoppedBy.ObstacleIndex, (int32)INDEX_NONE);
		});

		It("una pieza que envuelve la base lo deja en pie (0°) y a igual ángulo gana el índice menor", [this]()
		{
			TArray<FTreeFallObstacle> Obstacles;
			Obstacles.Add(Piece(FVector2D(500.0, 0.0), 2, 100.0f, 250.0, 100.0));
			Obstacles.Add(Piece(FVector2D(500.0, 0.0), 2, 100.0f, 250.0, 100.0));
			Obstacles.Add(Piece(FVector2D(50.0, 0.0), 3, 100.0f, 250.0, 100.0));
			const FTreeFallResult R = FTreeFallModel::Resolve(Get(TEXT("JungleGiant")), FVector2D::ZeroVector, FVector2D(1.0, 0.0), Obstacles);
			TestEqual(TEXT("la que envuelve la base"), R.StoppedBy.ObstacleIndex, 2);
			TestEqual(TEXT("en pie"), R.RestAngleDeg, 0.0);
			TestEqual(TEXT("sin alcance"), R.ReachFraction, 0.0);
			Obstacles.RemoveAt(2);
			TestEqual(TEXT("empate: índice menor"), FTreeFallModel::Resolve(Get(TEXT("JungleGiant")), FVector2D::ZeroVector, FVector2D(1.0, 0.0), Obstacles).StoppedBy.ObstacleIndex, 0);
		});

		It("el arbusto no cae ni toca nada; entradas corruptas no rompen nada", [this]()
		{
			TArray<FTreeFallObstacle> Obstacles = { Piece(FVector2D(50.0, 0.0), 0, 50.0f) };
			const FTreeFallResult Shrub = FTreeFallModel::Resolve(Get(TEXT("Shrub")), FVector2D::ZeroVector, FVector2D(1.0, 0.0), Obstacles);
			TestEqual(TEXT("arbusto: nada aplastado"), Shrub.Crushed.Num(), 0);
			TestEqual(TEXT("arbusto: alcance 0"), Shrub.ReachFraction, 0.0);

			TArray<FTreeFallObstacle> Bad;
			Bad.Add(Piece(FVector2D(NAN, 0.0), 3, 100.0f));
			Bad.Add(Piece(FVector2D(300.0, 0.0), 3, 100.0f, NAN));
			Bad.Add(Piece(FVector2D(300.0, 0.0), 3, 100.0f, 250.0, INFINITY));
			Bad.Add(Piece(FVector2D(300.0, 0.0), 0, NAN));
			const FTreeFallResult R = FTreeFallModel::Resolve(Get(TEXT("Palm")), FVector2D::ZeroVector, FVector2D(NAN, 0.0), Bad);
			TestTrue(TEXT("dirección válida"), FMath::IsFinite(R.Direction.X) && FMath::IsFinite(R.Direction.Y) && FMath::Abs(R.Direction.Size() - 1.0) < 1.0e-9);
			for (const FTreeFallHit& Hit : R.Crushed)
			{
				TestTrue(TEXT("daño finito"), FMath::IsFinite(Hit.Damage) && Hit.Damage >= 0.0f);
			}
			TestTrue(TEXT("ángulo finito"), FMath::IsFinite(R.RestAngleDeg));
		});

		It("apoyado, los troncos quedan más cerca de la base (ComputeFellDrops con ReachFraction)", [this]()
		{
			const FFellingProfile& Giant = Get(TEXT("JungleGiant"));
			FExploredRandom A(8), B(8);
			const TArray<FFellingDrop> Flat = FFellingModel::ComputeFellDrops(Giant, FVector2D::ZeroVector, FVector2D(1.0, 0.0), A);
			const TArray<FFellingDrop> Leaning = FFellingModel::ComputeFellDrops(Giant, FVector2D::ZeroVector, FVector2D(1.0, 0.0), B, 0.5);
			TestEqual(TEXT("mismas unidades"), Flat.Num(), Leaning.Num());
			for (int32 i = 0; i < FMath::Min(Flat.Num(), Leaning.Num()); ++i)
			{
				if (Flat[i].Kind == EFellingYieldKind::Log)
				{
					TestEqual(TEXT("a la mitad"), Leaning[i].Position.X, Flat[i].Position.X * 0.5, 1.0e-6);
				}
			}
			FExploredRandom C(8);
			for (const FFellingDrop& D : FFellingModel::ComputeFellDrops(Giant, FVector2D::ZeroVector, FVector2D(1.0, 0.0), C, NAN))
			{
				TestTrue(TEXT("NaN = cae entero, posición finita"), FMath::IsFinite(D.Position.X));
			}
		});
	});
}

#endif
