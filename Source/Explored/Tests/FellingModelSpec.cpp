#include "Misc/AutomationTest.h"

#include "WorldGen/FellingModel.h"
#include "WorldGen/HarvestModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FFellingModelSpec, "Explored.Felling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TArray<FFellingProfile> Profiles;
	const FFellingProfile& Get(const TCHAR* Species)
	{
		const FFellingProfile* P = FFellingModel::FindProfile(Profiles, FName(Species));
		check(P);
		return *P;
	}
	int32 HitsUntilFelled(const FFellingProfile& P, EFellingTool Tool)
	{
		FFellingProgress Progress;
		for (int32 i = 1; i <= 1000; ++i)
		{
			if (FFellingModel::ApplyHit(P, Progress, Tool, FVector2D(1.0, 0.0)))
			{
				return i;
			}
		}
		return -1;
	}
END_DEFINE_SPEC(FFellingModelSpec)

void FFellingModelSpec::Define()
{
	BeforeEach([this]()
	{
		Profiles = FFellingModel::DefaultProfiles();
	});

	Describe("los perfiles por defecto", [this]()
	{
		It("cubren las especies leñosas y coinciden con FHarvestModel a mano y con filo", [this]()
		{
			const TArray<FHarvestSpeciesRule> Rules = FHarvestModel::DefaultRules();
			for (const TCHAR* Species : { TEXT("Palm"), TEXT("JungleGiant"), TEXT("JungleWide"),
				TEXT("Mangrove"), TEXT("Understory"), TEXT("Shrub") })
			{
				const FFellingProfile* P = FFellingModel::FindProfile(Profiles, FName(Species));
				const FHarvestSpeciesRule* R = FHarvestModel::FindRule(Rules, FName(Species));
				if (!TestNotNull(*FString::Printf(TEXT("perfil de %s"), Species), P) || !TestNotNull(TEXT("regla"), R))
				{
					continue;
				}
				TestEqual(*FString::Printf(TEXT("%s a mano"), Species), FFellingModel::HitsRequired(*P, EFellingTool::Hands), FHarvestModel::HitsRequired(*R, false));
				TestEqual(*FString::Printf(TEXT("%s con filo"), Species), FFellingModel::HitsRequired(*P, EFellingTool::Edge), FHarvestModel::HitsRequired(*R, true));
			}
		});

		It("tienen datos sanos: rendimiento con mínimo ≤ máximo, rebrote y ramas del suelo con objeto", [this]()
		{
			for (const FFellingProfile& P : Profiles)
			{
				TestTrue(*FString::Printf(TEXT("%s: algo con que talarlo"), *P.Species.ToString()), FFellingModel::HitsRequired(P, EFellingTool::Hands) > 0);
				TestTrue(TEXT("el filo nunca es peor que la mano"), FFellingModel::HitsRequired(P, EFellingTool::Edge) <= FFellingModel::HitsRequired(P, EFellingTool::Hands));
				for (const FFellingYield& Y : P.Yields)
				{
					TestTrue(*FString::Printf(TEXT("%s/%s: 0 ≤ mín ≤ máx"), *P.Species.ToString(), *Y.ItemId.ToString()), 0 <= Y.MinCount && Y.MinCount <= Y.MaxCount);
				}
				TestTrue(TEXT("rebrota (el director: todo árbol vuelve salvo que se arranque)"), P.StumpRegrowDays > 0);
				TestTrue(TEXT("se puede arrancar con pala"), P.UprootShovelHits > 0);
				TestFalse(TEXT("ramas del suelo con objeto"), P.GroundBranchCapacity > 0 && P.GroundBranchItem.IsNone());
			}
		});

		It("solo el arbusto no cae (se desbroza en el sitio)", [this]()
		{
			for (const FFellingProfile& P : Profiles)
			{
				TestEqual(*FString::Printf(TEXT("%s cae"), *P.Species.ToString()), P.HeightMeters > 0.0f, P.Species != FName(TEXT("Shrub")));
			}
		});
	});

	Describe("ApplyHit", [this]()
	{
		It("tumba exactamente al golpe N con cada herramienta y cada especie (sin error de redondeo)", [this]()
		{
			for (const FFellingProfile& P : Profiles)
			{
				for (int32 T = 0; T < (int32)EFellingTool::Count; ++T)
				{
					const int32 Required = FFellingModel::HitsRequired(P, (EFellingTool)T);
					if (Required > 0)
					{
						TestEqual(*FString::Printf(TEXT("%s herramienta %d"), *P.Species.ToString(), T), HitsUntilFelled(P, (EFellingTool)T), Required);
					}
				}
			}
		});

		It("también es exacto con un número de golpes que no divide a WorkToFell (17, 97, 500)", [this]()
		{
			FFellingProfile P = Get(TEXT("Palm"));
			for (const int32 N : { 17, 97, 500 })
			{
				P.HitsByTool[(int32)EFellingTool::Hands] = N;
				TestEqual(*FString::Printf(TEXT("%d golpes"), N), HitsUntilFelled(P, EFellingTool::Hands), N);
			}
		});

		It("mezclar herramientas suma fracciones: 2 de 4 con filo + 4 de 8 a mano tumba la palmera", [this]()
		{
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			FFellingProgress Progress;
			bool bFelled = false;
			for (int32 i = 0; i < 2; ++i) { bFelled = FFellingModel::ApplyHit(Palm, Progress, EFellingTool::Edge, FVector2D(1.0, 0.0)); }
			TestFalse(TEXT("a la mitad sigue en pie"), bFelled);
			for (int32 i = 0; i < 3; ++i) { bFelled = FFellingModel::ApplyHit(Palm, Progress, EFellingTool::Hands, FVector2D(1.0, 0.0)); }
			TestFalse(TEXT("a 7/8 sigue en pie"), bFelled);
			TestTrue(TEXT("el golpe que completa lo tumba"), FFellingModel::ApplyHit(Palm, Progress, EFellingTool::Hands, FVector2D(1.0, 0.0)));
		});

		It("una herramienta que no sirve no avanza y un árbol ya tumbado no vuelve a caer", [this]()
		{
			const FFellingProfile& Giant = Get(TEXT("JungleGiant"));
			FFellingProgress Progress;
			TestFalse(TEXT("la pala no tala"), FFellingModel::ApplyHit(Giant, Progress, EFellingTool::Shovel, FVector2D(1.0, 0.0)));
			TestEqual(TEXT("sin trabajo"), Progress.Work, 0);
			TestTrue(TEXT("sin empuje"), Progress.Push.IsZero());
			while (!FFellingModel::ApplyHit(Giant, Progress, EFellingTool::Edge, FVector2D(0.0, 1.0))) {}
			TestFalse(TEXT("un golpe más no cae otra vez"), FFellingModel::ApplyHit(Giant, Progress, EFellingTool::Edge, FVector2D(0.0, 1.0)));
			TestEqual(TEXT("trabajo saturado"), Progress.Work, FFellingModel::WorkToFell);
		});
	});

	Describe("ResolveFallDirection", [this]()
	{
		It("en llano cae hacia donde empujan los golpes (lado contrario al jugador)", [this]()
		{
			FFellingProgress Progress;
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			for (int32 i = 0; i < 4; ++i) { FFellingModel::ApplyHit(Palm, Progress, EFellingTool::Edge, FVector2D(0.0, -3.0)); }
			TestTrue(TEXT("hacia -Y"), FFellingModel::ResolveFallDirection(Progress, FVector2D::ZeroVector, 7u).Equals(FVector2D(0.0, -1.0), 1.0e-6));
		});

		It("en pendiente fuerte manda la pendiente aunque se golpee desde abajo", [this]()
		{
			FFellingProgress Progress;
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			// El jugador está cuesta abajo (+X) y golpea hacia -X, cuesta arriba; pendiente de 45° hacia +X.
			for (int32 i = 0; i < 4; ++i) { FFellingModel::ApplyHit(Palm, Progress, EFellingTool::Edge, FVector2D(-1.0, 0.0)); }
			const FVector2D Dir = FFellingModel::ResolveFallDirection(Progress, FVector2D(1.0, 0.0), 7u);
			TestTrue(TEXT("cae cuesta abajo"), Dir.X > 0.99);
		});

		It("en pendiente suave el golpe lateral desvía la caída pero no la invierte", [this]()
		{
			FFellingProgress Progress;
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			for (int32 i = 0; i < 4; ++i) { FFellingModel::ApplyHit(Palm, Progress, EFellingTool::Edge, FVector2D(0.0, 1.0)); }
			// tan 10° ≈ 0,176 cuesta abajo hacia +X.
			const FVector2D Dir = FFellingModel::ResolveFallDirection(Progress, FVector2D(0.176, 0.0), 7u);
			TestTrue(TEXT("sobre todo hacia el golpe"), Dir.Y > 0.9);
			TestTrue(TEXT("algo cuesta abajo"), Dir.X > 0.1);
			TestEqual(TEXT("unitaria"), Dir.Size(), 1.0, 1.0e-9);
		});

		It("con golpes opuestos en llano cae hacia una dirección determinista por semilla", [this]()
		{
			FFellingProgress Progress;
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			FFellingModel::ApplyHit(Palm, Progress, EFellingTool::Edge, FVector2D(1.0, 0.0));
			FFellingModel::ApplyHit(Palm, Progress, EFellingTool::Edge, FVector2D(-1.0, 0.0));
			const FVector2D A = FFellingModel::ResolveFallDirection(Progress, FVector2D::ZeroVector, 42u);
			const FVector2D B = FFellingModel::ResolveFallDirection(Progress, FVector2D::ZeroVector, 42u);
			const FVector2D C = FFellingModel::ResolveFallDirection(Progress, FVector2D::ZeroVector, 43u);
			TestEqual(TEXT("unitaria (no NaN ni cero)"), A.Size(), 1.0, 1.0e-6);
			TestTrue(TEXT("misma semilla, misma dirección"), A == B);
			TestTrue(TEXT("otra semilla, otra dirección"), !A.Equals(C, 1.0e-3));
		});
	});

	Describe("ComputeFellDrops", [this]()
	{
		It("conserva el recuento: cada unidad tirada aparece una vez, y ningún tipo sale de su rango", [this]()
		{
			for (const FFellingProfile& P : Profiles)
			{
				for (uint64 Seed = 1; Seed <= 50; ++Seed)
				{
					FExploredRandom Random(Seed);
					const TArray<FFellingDrop> Drops = FFellingModel::ComputeFellDrops(P, FVector2D(100.0, 200.0), FVector2D(1.0, 0.0), Random);
					for (const FFellingYield& Y : P.Yields)
					{
						int32 N = 0;
						for (const FFellingDrop& D : Drops) { N += (D.ItemId == Y.ItemId && D.Kind == Y.Kind) ? 1 : 0; }
						if (!TestTrue(*FString::Printf(TEXT("%s/%s en [%d, %d]: %d"), *P.Species.ToString(), *Y.ItemId.ToString(), Y.MinCount, Y.MaxCount, N),
							Y.MinCount <= N && N <= Y.MaxCount))
						{
							return;
						}
					}
				}
			}
		});

		It("es determinista con la misma semilla", [this]()
		{
			const FFellingProfile& Giant = Get(TEXT("JungleGiant"));
			FExploredRandom A(99), B(99);
			const TArray<FFellingDrop> DA = FFellingModel::ComputeFellDrops(Giant, FVector2D(-5000.0, 300.0), FVector2D(0.6, 0.8), A);
			const TArray<FFellingDrop> DB = FFellingModel::ComputeFellDrops(Giant, FVector2D(-5000.0, 300.0), FVector2D(0.6, 0.8), B);
			TestEqual(TEXT("mismo número"), DA.Num(), DB.Num());
			for (int32 i = 0; i < FMath::Min(DA.Num(), DB.Num()); ++i)
			{
				TestTrue(TEXT("misma unidad"), DA[i].ItemId == DB[i].ItemId && DA[i].Position == DB[i].Position);
			}
		});

		It("reparte los troncos a lo largo del tronco caído, en orden y sin salirse de la altura", [this]()
		{
			const FFellingProfile& Giant = Get(TEXT("JungleGiant"));
			const FVector2D Base(1000.0, -1000.0);
			const FVector2D Dir(0.0, 1.0);
			FExploredRandom Random(3);
			double Last = -1.0;
			int32 Logs = 0;
			for (const FFellingDrop& D : FFellingModel::ComputeFellDrops(Giant, Base, Dir, Random))
			{
				if (D.Kind != EFellingYieldKind::Log) { continue; }
				++Logs;
				const FVector2D Rel = D.Position - Base;
				TestEqual(TEXT("sobre la línea de caída"), Rel.X, 0.0, 1.0e-6);
				TestTrue(TEXT("avanzan"), Rel.Y > Last);
				TestTrue(TEXT("dentro del tronco"), Rel.Y > 0.0 && Rel.Y < Giant.HeightMeters * 100.0);
				Last = Rel.Y;
			}
			TestTrue(TEXT("al menos los troncos mínimos"), Logs >= 4);
		});

		It("coloca frutos dentro del radio de la copa y el arbusto lo deja todo junto a la base", [this]()
		{
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			const FVector2D Base(0.0, 0.0);
			const FVector2D Crown = FVector2D(1.0, 0.0) * (Palm.HeightMeters * 100.0 * 0.85);
			for (uint64 Seed = 1; Seed <= 30; ++Seed)
			{
				FExploredRandom Random(Seed);
				for (const FFellingDrop& D : FFellingModel::ComputeFellDrops(Palm, Base, FVector2D(1.0, 0.0), Random))
				{
					if (D.Kind == EFellingYieldKind::Fruit)
					{
						TestTrue(TEXT("fruto en la copa"), FVector2D::Distance(D.Position, Crown) <= Palm.CrownRadiusMeters * 100.0 + 1.0e-6);
					}
				}
			}
			const FFellingProfile& Shrub = Get(TEXT("Shrub"));
			FExploredRandom Random(5);
			for (const FFellingDrop& D : FFellingModel::ComputeFellDrops(Shrub, FVector2D(50.0, 50.0), FVector2D(1.0, 0.0), Random))
			{
				TestTrue(TEXT("junto a la base"), FVector2D::Distance(D.Position, FVector2D(50.0, 50.0)) <= Shrub.CrownRadiusMeters * 100.0);
			}
		});

		It("aguanta una dirección de caída nula y un perfil vacío", [this]()
		{
			FFellingProfile Empty;
			FExploredRandom Random(1);
			TestEqual(TEXT("perfil vacío, nada"), FFellingModel::ComputeFellDrops(Empty, FVector2D::ZeroVector, FVector2D::ZeroVector, Random).Num(), 0);
			FExploredRandom R2(1);
			for (const FFellingDrop& D : FFellingModel::ComputeFellDrops(Get(TEXT("Palm")), FVector2D::ZeroVector, FVector2D::ZeroVector, R2))
			{
				TestTrue(TEXT("posición finita"), FMath::IsFinite(D.Position.X) && FMath::IsFinite(D.Position.Y));
			}
		});

		It("asigna a su celda los troncos que cruzan un borde en coordenadas negativas", [this]()
		{
			// Tronco de 22 m que cae desde x = -10 m hacia +X: cruza el borde x = 0 entre las celdas -1 y 0.
			const FFellingProfile& Giant = Get(TEXT("JungleGiant"));
			FExploredRandom Random(11);
			bool bSawNegative = false;
			bool bSawPositive = false;
			for (const FFellingDrop& D : FFellingModel::ComputeFellDrops(Giant, FVector2D(-1000.0, -1.0), FVector2D(1.0, 0.0), Random))
			{
				if (D.Kind != EFellingYieldKind::Log) { continue; }
				const FIntPoint Cell = FFellingModel::CellOf(D.Position, 3200.0);
				TestEqual(TEXT("fila -1 (y = -1 cm)"), Cell.Y, -1);
				TestEqual(TEXT("celda por suelo"), Cell.X, D.Position.X < 0.0 ? -1 : 0);
				bSawNegative |= Cell.X == -1;
				bSawPositive |= Cell.X == 0;
			}
			TestTrue(TEXT("hay troncos a ambos lados del borde"), bSawNegative && bSawPositive);
		});
	});

	Describe("CellOf", [this]()
	{
		It("usa suelo, no truncamiento, y el borde exacto pertenece a la celda de la derecha", [this]()
		{
			TestTrue(TEXT("-0,5 → -1"), FFellingModel::CellOf(FVector2D(-0.5, -0.5), 100.0) == FIntPoint(-1, -1));
			TestTrue(TEXT("-100 → -1"), FFellingModel::CellOf(FVector2D(-100.0, 0.0), 100.0) == FIntPoint(-1, 0));
			TestTrue(TEXT("-100,001 → -2"), FFellingModel::CellOf(FVector2D(-100.001, 0.0), 100.0) == FIntPoint(-2, 0));
			TestTrue(TEXT("100 → 1"), FFellingModel::CellOf(FVector2D(100.0, 99.999), 100.0) == FIntPoint(1, 0));
			TestTrue(TEXT("tamaño de celda inválido → origen"), FFellingModel::CellOf(FVector2D(5000.0, 5000.0), 0.0) == FIntPoint(0, 0));
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			const double Inf = std::numeric_limits<double>::infinity();
			TestTrue(TEXT("tamaño NaN → origen"), FFellingModel::CellOf(FVector2D(5000.0, 5000.0), NaN) == FIntPoint(0, 0));
			TestTrue(TEXT("posición NaN → origen"), FFellingModel::CellOf(FVector2D(NaN, 5000.0), 100.0) == FIntPoint(0, 0));
			TestTrue(TEXT("posición infinita → origen"), FFellingModel::CellOf(FVector2D(5000.0, -Inf), 100.0) == FIntPoint(0, 0));
			const FIntPoint Far = FFellingModel::CellOf(FVector2D(1.0e300, -1.0e300), 100.0);
			TestTrue(TEXT("lejísimos: recortado sin desbordar"), Far.X > 0 && Far.Y < 0);
		});
	});

	Describe("el tocón", [this]()
	{
		It("pasa de tocón a brote a adulto en los días del perfil, con escala creciente", [this]()
		{
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			FStumpState Stump;
			Stump.FelledAtMinute = 10000;
			const int64 Day = FFellingModel::MinutesPerDay;
			const int64 Sprout = Stump.FelledAtMinute + Palm.StumpRegrowDays * Day;
			const int64 Mature = Sprout + Palm.SaplingToMatureDays * Day;
			TestTrue(TEXT("recién talado"), FFellingModel::StageAt(Palm, Stump, Stump.FelledAtMinute) == EStumpStage::Stump);
			TestTrue(TEXT("un minuto antes del brote"), FFellingModel::StageAt(Palm, Stump, Sprout - 1) == EStumpStage::Stump);
			TestTrue(TEXT("brota justo a su hora"), FFellingModel::StageAt(Palm, Stump, Sprout) == EStumpStage::Sapling);
			TestTrue(TEXT("un minuto antes de adulto"), FFellingModel::StageAt(Palm, Stump, Mature - 1) == EStumpStage::Sapling);
			TestTrue(TEXT("adulto"), FFellingModel::StageAt(Palm, Stump, Mature) == EStumpStage::Mature);
			TestEqual(TEXT("tocón escala 0"), FFellingModel::GrowthScaleAt(Palm, Stump, Sprout - 1), 0.0f);
			TestEqual(TEXT("brote escala inicial"), FFellingModel::GrowthScaleAt(Palm, Stump, Sprout), FFellingModel::SaplingStartScale);
			TestEqual(TEXT("adulto escala 1"), FFellingModel::GrowthScaleAt(Palm, Stump, Mature), 1.0f);
			float Prev = 0.0f;
			for (int64 T = Sprout; T <= Mature; T += Day)
			{
				const float S = FFellingModel::GrowthScaleAt(Palm, Stump, T);
				TestTrue(TEXT("crece monótona"), S >= Prev);
				Prev = S;
			}
		});

		It("con reloj anterior a la tala (partida manipulada) sigue siendo tocón", [this]()
		{
			FStumpState Stump;
			Stump.FelledAtMinute = 5000;
			TestTrue(TEXT("tocón"), FFellingModel::StageAt(Get(TEXT("Palm")), Stump, 0) == EStumpStage::Stump);
			TestEqual(TEXT("escala 0"), FFellingModel::GrowthScaleAt(Get(TEXT("Palm")), Stump, -100000), 0.0f);
		});

		It("con minutos extremos guardados no desborda", [this]()
		{
			const FFellingProfile& Palm = Get(TEXT("Palm"));
			FStumpState Stump;
			// Antes 100 - INT64_MIN desbordaba a negativo: tocón para siempre.
			Stump.FelledAtMinute = TNumericLimits<int64>::Min();
			TestTrue(TEXT("talado hace muchísimo: adulto"), FFellingModel::StageAt(Palm, Stump, 100) == EStumpStage::Mature);
			TestEqual(TEXT("escala 1"), FFellingModel::GrowthScaleAt(Palm, Stump, 100), 1.0f);
			Stump.FelledAtMinute = TNumericLimits<int64>::Max();
			TestTrue(TEXT("talado en el futuro: tocón"), FFellingModel::StageAt(Palm, Stump, TNumericLimits<int64>::Min()) == EStumpStage::Stump);
		});

		It("no rebrota si el perfil tiene 0 días de rebrote", [this]()
		{
			FFellingProfile P = Get(TEXT("Palm"));
			P.StumpRegrowDays = 0;
			FStumpState Stump;
			TestTrue(TEXT("tocón para siempre"), FFellingModel::StageAt(P, Stump, 1000 * FFellingModel::MinutesPerDay) == EStumpStage::Stump);
		});

		It("solo la pala lo arranca, en los golpes del perfil, y arrancado no rebrota nunca", [this]()
		{
			const FFellingProfile& Giant = Get(TEXT("JungleGiant"));
			FStumpState Stump;
			TestFalse(TEXT("el hacha no arranca"), FFellingModel::ApplyUprootHit(Giant, Stump, EFellingTool::Edge, 0));
			TestEqual(TEXT("sin trabajo"), Stump.UprootWork, 0);
			int32 Hits = 0;
			while (!FFellingModel::ApplyUprootHit(Giant, Stump, EFellingTool::Shovel, 0) && Hits < 100) { ++Hits; }
			TestEqual(TEXT("golpes de pala"), Hits + 1, Giant.UprootShovelHits);
			TestTrue(TEXT("arrancado"), FFellingModel::StageAt(Giant, Stump, 1000000 * FFellingModel::MinutesPerDay) == EStumpStage::Uprooted);
			TestEqual(TEXT("escala 0"), FFellingModel::GrowthScaleAt(Giant, Stump, 1000000 * FFellingModel::MinutesPerDay), 0.0f);
			TestFalse(TEXT("no se arranca dos veces"), FFellingModel::ApplyUprootHit(Giant, Stump, EFellingTool::Shovel, 0));
		});

		It("un brote se puede arrancar; un adulto rebrotado no (se tala otra vez)", [this]()
		{
			const FFellingProfile& Shrub = Get(TEXT("Shrub"));
			const int64 Day = FFellingModel::MinutesPerDay;
			FStumpState Sapling;
			TestTrue(TEXT("brote arrancado de un golpe"), FFellingModel::ApplyUprootHit(Shrub, Sapling, EFellingTool::Shovel, Shrub.StumpRegrowDays * Day));
			FStumpState Adult;
			TestFalse(TEXT("adulto no"), FFellingModel::ApplyUprootHit(Shrub, Adult, EFellingTool::Shovel, (Shrub.StumpRegrowDays + Shrub.SaplingToMatureDays) * Day));
		});
	});
}

#endif
