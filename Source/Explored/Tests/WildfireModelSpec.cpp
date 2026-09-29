#include "Misc/AutomationTest.h"

#include "WorldGen/WildfireModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FWildfireModelSpec, "Explored.Wildfire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	/** Rectángulo de hierba [MinX, MaxX] × [MinY, MaxY]; arena fuera. */
	static FWildfireModel::FFuelQuery Patch(int32 MinX, int32 MinY, int32 MaxX, int32 MaxY, EFireFuel Fuel = EFireFuel::Grass)
	{
		return [=](FIntPoint C) { return (C.X >= MinX && C.X <= MaxX && C.Y >= MinY && C.Y <= MaxY) ? Fuel : EFireFuel::None; };
	}
	static FWildfireModel::FFuelQuery Everywhere(EFireFuel Fuel = EFireFuel::Grass)
	{
		return [=](FIntPoint) { return Fuel; };
	}
	static FWildfireConditions Dry()
	{
		FWildfireConditions C;
		C.Season = ESeason::Dry;
		C.Weather = EWeatherState::Clear;
		C.Wind = 0.0f;
		return C;
	}
	/** Observador en el centro de una celda. */
	static TArray<FVector2D> At(FIntPoint Cell) { return { FWildfireModel::CellCenter(Cell) }; }

	/** Avanza segundo a segundo hasta que se apaga (o Limit pasos). Devuelve los pasos usados. */
	static int32 BurnOut(FWildfireModel& M, int64& Second, const FWildfireConditions& C, const TArray<FVector2D>& Obs,
		int32 Limit, TArray<FIntPoint>* OutIgnited = nullptr, TArray<FIntPoint>* OutBurnedOut = nullptr)
	{
		int32 Steps = 0;
		while (M.IsActive() && Steps < Limit)
		{
			const FWildfireStepResult R = M.Advance(++Second, 0, C, Obs);
			if (OutIgnited) { OutIgnited->Append(R.Ignited); }
			if (OutBurnedOut) { OutBurnedOut->Append(R.BurnedOut); }
			++Steps;
		}
		return Steps;
	}
	static int32 CountState(const FWildfireModel& M, int32 MinX, int32 MinY, int32 MaxX, int32 MaxY, EFireCellState State, int64 Minute = 0)
	{
		int32 N = 0;
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				N += M.StateAt(FIntPoint(X, Y), Minute) == State ? 1 : 0;
			}
		}
		return N;
	}
END_DEFINE_SPEC(FWildfireModelSpec)

void FWildfireModelSpec::Define()
{
	Describe("probabilidad de contagio (biblia 02 §6)", [this]()
	{
		It("45 % en seca, 13,5 % en estaciones húmedas, 9 % con niebla y 0 con chubasco o más", [this]()
		{
			TestEqual(TEXT("seca"), FWildfireModel::BaseChancePermille(ESeason::Dry, EWeatherState::Clear), 450);
			TestEqual(TEXT("ola de calor"), FWildfireModel::BaseChancePermille(ESeason::Dry, EWeatherState::HeatWave), 450);
			TestEqual(TEXT("primeras lluvias"), FWildfireModel::BaseChancePermille(ESeason::FirstRains, EWeatherState::Clear), 135);
			TestEqual(TEXT("monzón"), FWildfireModel::BaseChancePermille(ESeason::Monsoon, EWeatherState::Cloudy), 135);
			TestEqual(TEXT("ciclones"), FWildfireModel::BaseChancePermille(ESeason::Cyclones, EWeatherState::Gale), 135);
			TestEqual(TEXT("niebla"), FWildfireModel::BaseChancePermille(ESeason::Dry, EWeatherState::MorningFog), 90);
			TestEqual(TEXT("llovizna no apaga"), FWildfireModel::BaseChancePermille(ESeason::Dry, EWeatherState::LightRain), 450);
			TestEqual(TEXT("chubasco"), FWildfireModel::BaseChancePermille(ESeason::Dry, EWeatherState::Shower), 0);
			TestEqual(TEXT("tormenta"), FWildfireModel::BaseChancePermille(ESeason::Dry, EWeatherState::Thunderstorm), 0);
			TestEqual(TEXT("ciclón"), FWildfireModel::BaseChancePermille(ESeason::Cyclones, EWeatherState::Cyclone), 0);
		});

		It("el viento suma 25 puntos a favor, los resta en contra y no toca el través", [this]()
		{
			FWildfireConditions C = Dry();
			C.Wind = 0.6f;
			C.WindDirection = FVector2D(3.0, 0.0); // sin normalizar
			TestEqual(TEXT("a favor"), FWildfireModel::SpreadChancePermille(C, FIntPoint(1, 0)), 700);
			TestEqual(TEXT("en contra"), FWildfireModel::SpreadChancePermille(C, FIntPoint(-1, 0)), 200);
			TestEqual(TEXT("través"), FWildfireModel::SpreadChancePermille(C, FIntPoint(0, 1)), 450);
			TestEqual(TEXT("diagonal a favor"), FWildfireModel::SpreadChancePermille(C, FIntPoint(1, 1)), 450 + 177);
			C.Season = ESeason::Monsoon;
			TestEqual(TEXT("húmeda en contra no baja de 0"), FWildfireModel::SpreadChancePermille(C, FIntPoint(-1, 0)), 0);
			TestEqual(TEXT("húmeda diagonal en contra"), FWildfireModel::SpreadChancePermille(C, FIntPoint(-1, 1)), 0);
			C.Weather = EWeatherState::Shower;
			TestEqual(TEXT("el viento no enciende bajo la lluvia"), FWildfireModel::SpreadChancePermille(C, FIntPoint(1, 0)), 0);
		});

		It("sin viento, con calma, con dirección nula o no finita no hay sesgo", [this]()
		{
			FWildfireConditions C = Dry();
			C.WindDirection = FVector2D(1.0, 0.0);
			C.Wind = FWildfireModel::CalmWind * 0.5f;
			TestEqual(TEXT("calma"), FWildfireModel::SpreadChancePermille(C, FIntPoint(1, 0)), 450);
			C.Wind = 1.0f;
			C.WindDirection = FVector2D::ZeroVector;
			TestEqual(TEXT("dirección nula"), FWildfireModel::SpreadChancePermille(C, FIntPoint(1, 0)), 450);
			C.WindDirection = FVector2D(NAN, 1.0);
			TestEqual(TEXT("NaN"), FWildfireModel::SpreadChancePermille(C, FIntPoint(1, 0)), 450);
			C.WindDirection = FVector2D(1.0, 0.0);
			C.Wind = NAN;
			TestEqual(TEXT("fuerza NaN"), FWildfireModel::SpreadChancePermille(C, FIntPoint(1, 0)), 450);
		});

		It("una dirección finita pero enorme sesga igual que la unitaria", [this]()
		{
			FWildfireConditions C = Dry();
			C.Wind = 1.0f;
			C.WindDirection = FVector2D(1.0e308, 1.0e308);
			TestEqual(TEXT("diagonal a favor"), FWildfireModel::SpreadChancePermille(C, FIntPoint(1, 1)), 700);
			TestEqual(TEXT("diagonal en contra"), FWildfireModel::SpreadChancePermille(C, FIntPoint(-1, -1)), 200);
			C.WindDirection = FVector2D(-1.0e300, 0.0);
			TestEqual(TEXT("de frente"), FWildfireModel::SpreadChancePermille(C, FIntPoint(-1, 0)), 700);
		});
	});

	Describe("rejilla y chunks", [this]()
	{
		It("divide por defecto también con coordenadas negativas", [this]()
		{
			TestTrue(TEXT("0"), FWildfireModel::ChunkOf(FIntPoint(0, 0)) == FIntPoint(0, 0));
			TestTrue(TEXT("15"), FWildfireModel::ChunkOf(FIntPoint(15, 15)) == FIntPoint(0, 0));
			TestTrue(TEXT("16"), FWildfireModel::ChunkOf(FIntPoint(16, 16)) == FIntPoint(1, 1));
			TestTrue(TEXT("-1"), FWildfireModel::ChunkOf(FIntPoint(-1, -1)) == FIntPoint(-1, -1));
			TestTrue(TEXT("-16"), FWildfireModel::ChunkOf(FIntPoint(-16, -16)) == FIntPoint(-1, -1));
			TestTrue(TEXT("-17"), FWildfireModel::ChunkOf(FIntPoint(-17, 0)) == FIntPoint(-2, 0));
			TestTrue(TEXT("celda de -1 cm"), FWildfireModel::CellAt(FVector2D(-1.0, 199.9)) == FIntPoint(-1, 0));
			TestTrue(TEXT("celda NaN"), FWildfireModel::CellAt(FVector2D(NAN, INFINITY)) == FIntPoint(0, 0));
		});
	});

	Describe("encender", [this]()
	{
		It("solo prende hierba o matorral sin quemar ni mojar", [this]()
		{
			FWildfireModel M(7u, Patch(0, 0, 3, 3));
			TestFalse(TEXT("arena"), M.Ignite(FIntPoint(10, 10), 0));
			TestTrue(TEXT("hierba"), M.Ignite(FIntPoint(1, 1), 0));
			TestFalse(TEXT("ya arde"), M.Ignite(FIntPoint(1, 1), 0));
			TestEqual(TEXT("una"), M.NumBurning(), 1);
			M.Douse(FIntPoint(3, 3), 0, 0, 60);
			TestTrue(TEXT("mojada"), M.IsWet(FIntPoint(3, 3), 30));
			TestFalse(TEXT("no prende mojada"), M.Ignite(FIntPoint(3, 3), 30));
			TestTrue(TEXT("prende seca"), M.Ignite(FIntPoint(3, 3), 60));
		});

		It("sin consulta de combustible o con un valor fuera de rango no prende nada", [this]()
		{
			FWildfireModel Null(1u, nullptr);
			TestFalse(TEXT("sin consulta"), Null.Ignite(FIntPoint(0, 0), 0));
			FWildfireModel Bad(1u, [](FIntPoint) { return (EFireFuel)200; });
			TestFalse(TEXT("valor basura"), Bad.Ignite(FIntPoint(0, 0), 0));
			TestEqual(TEXT("nada guardado"), Bad.NumStoredCells(), 0);
		});
	});

	Describe("contagio", [this]()
	{
		It("quema toda una mancha de hierba en seca, no sale de ella y se apaga sola", [this]()
		{
			FWildfireModel M(42u, Patch(-10, -10, 9, 9));
			M.Ignite(FIntPoint(0, 0), 0);
			int64 Second = 0;
			TArray<FIntPoint> Ignited, BurnedOut;
			const int32 Steps = BurnOut(M, Second, Dry(), At(FIntPoint(0, 0)), 2000, &Ignited, &BurnedOut);
			TestFalse(TEXT("apagado"), M.IsActive());
			TestTrue(TEXT("tarda más que una celda"), Steps > FWildfireModel::GrassBurnSteps);
			TestTrue(TEXT("40 × 40 m en menos de un minuto (GDD v2 §3.15)"), Steps < 60);
			TestEqual(TEXT("toda quemada (cruza los bordes de chunk en 0)"), CountState(M, -10, -10, 9, 9, EFireCellState::Burnt), 400);
			TestEqual(TEXT("nada fuera"), M.NumStoredCells(), 400);
			// Cada celda prende una sola vez y todo lo que prende acaba quemado.
			TSet<FIntPoint> Unique;
			for (const FIntPoint& C : Ignited) { Unique.Add(C); }
			TestEqual(TEXT("sin repetidas"), Unique.Num(), Ignited.Num());
			TestEqual(TEXT("prendidas + la inicial = apagadas"), Ignited.Num() + 1, BurnedOut.Num());
		});

		It("con viento en estación húmeda nunca avanza contra el viento", [this]()
		{
			FWildfireModel M(3u, Everywhere());
			const FIntPoint Origin(0, 0);
			M.Ignite(Origin, 0);
			FWildfireConditions C;
			C.Season = ESeason::Monsoon;
			C.Wind = 0.8f;
			C.WindDirection = FVector2D(1.0, 0.0);
			int64 Second = 0;
			TArray<FIntPoint> Ignited;
			for (int32 i = 0; i < 40; ++i)
			{
				Ignited.Append(M.Advance(++Second, 0, C, At(Origin)).Ignited);
			}
			TestTrue(TEXT("ha avanzado"), Ignited.Num() > 5);
			int32 MaxX = 0;
			for (const FIntPoint& Cell : Ignited)
			{
				TestTrue(TEXT("nunca a barlovento"), Cell.X >= Origin.X);
				MaxX = FMath::Max(MaxX, Cell.X);
			}
			TestTrue(TEXT("corre a sotavento"), MaxX >= 5);
		});

		It("es determinista y no depende de cómo se trocea el avance", [this]()
		{
			auto Run = [](bool bChunked)
			{
				FWildfireModel M(99u, Patch(-20, -20, 20, 20, EFireFuel::Shrub));
				M.Ignite(FIntPoint(0, 0), 0);
				M.Ignite(FIntPoint(-15, 12), 0);
				FWildfireConditions C = Dry();
				C.Wind = 0.5f;
				C.WindDirection = FVector2D(1.0, 2.0);
				const TArray<FVector2D> Obs = At(FIntPoint(0, 0));
				for (int64 S = 0; S < 120;)
				{
					const int64 Next = bChunked ? FMath::Min<int64>(S + 1 + (S % 4), 120) : S + 1; // trozos de 1 a 4 s
					M.Advance(Next, 0, C, Obs);
					S = Next;
				}
				return M.Save();
			};
			TestTrue(TEXT("mismo resultado"), Run(false) == Run(true));
			TestTrue(TEXT("repetible"), Run(true) == Run(true));
		});

		It("una semilla distinta da otro incendio", [this]()
		{
			auto Run = [](uint32 Seed)
			{
				FWildfireModel M(Seed, Everywhere());
				M.Ignite(FIntPoint(0, 0), 0);
				FWildfireConditions C;
				C.Season = ESeason::Monsoon;
				int64 S = 0;
				for (int32 i = 0; i < 30; ++i) { M.Advance(++S, 0, C, At(FIntPoint(0, 0))); }
				return M.Save();
			};
			TestTrue(TEXT("distinto"), Run(1u) != Run(2u));
		});

		It("respeta el tope de 64 celdas que prenden por chunk y paso", [this]()
		{
			FWildfireModel M(5u, Everywhere());
			for (int32 Y = 0; Y < FWildfireModel::ChunkCells; ++Y)
			{
				for (int32 X = 0; X < FWildfireModel::ChunkCells; ++X)
				{
					if ((X + Y) % 2 == 0) { M.Ignite(FIntPoint(X, Y), 0); }
				}
			}
			FWildfireConditions C = Dry();
			const FWildfireStepResult R = M.Advance(1, 0, C, At(FIntPoint(8, 8)));
			TMap<FIntPoint, int32> PerChunk;
			for (const FIntPoint& Cell : R.Ignited) { ++PerChunk.FindOrAdd(FWildfireModel::ChunkOf(Cell)); }
			for (const TPair<FIntPoint, int32>& Pair : PerChunk)
			{
				TestTrue(TEXT("≤ 64 por chunk"), Pair.Value <= FWildfireModel::MaxIgnitionsPerChunkStep);
			}
			TestEqual(TEXT("el chunk lleno llega al tope"), PerChunk.FindRef(FIntPoint(0, 0)), FWildfireModel::MaxIgnitionsPerChunkStep);
			TestTrue(TEXT("el resto espera"), R.IgnitionsDeferred > 0);
		});

		It("un cortafuegos mojado de una celda de ancho detiene el frente", [this]()
		{
			FWildfireModel M(11u, Patch(0, 0, 20, 10));
			for (int32 Y = 0; Y <= 10; ++Y) { M.Douse(FIntPoint(10, Y), 0, 0, 600); }
			M.Ignite(FIntPoint(0, 5), 0);
			int64 Second = 0;
			BurnOut(M, Second, Dry(), At(FIntPoint(10, 5)), 3000);
			TestEqual(TEXT("lado quemado"), CountState(M, 0, 0, 9, 10, EFireCellState::Burnt), 110);
			TestEqual(TEXT("cortafuegos intacto"), CountState(M, 10, 0, 10, 10, EFireCellState::Unburnt), 11);
			TestEqual(TEXT("otro lado intacto"), CountState(M, 11, 0, 20, 10, EFireCellState::Unburnt), 110);
		});
	});

	Describe("solo se simula cerca de los jugadores", [this]()
	{
		It("un fuego a 500 m no avanza ni se revisa; al acercarse sigue donde estaba", [this]()
		{
			FWildfireModel M(8u, Everywhere());
			const FIntPoint Near(0, 0), Far(250, 0); // 500 m
			M.Ignite(Near, 0);
			M.Ignite(Far, 0);
			const TArray<FVector2D> Obs = At(Near);
			int64 Second = 0;
			for (int32 i = 0; i < 5; ++i)
			{
				const FWildfireStepResult R = M.Advance(++Second, 0, Dry(), Obs);
				for (const FIntPoint& C : R.Ignited)
				{
					TestTrue(TEXT("solo cerca"), FVector2D::Distance(FWildfireModel::CellCenter(C), Obs[0]) < 10000.0);
				}
			}
			const FWildfireCell* FarCell = M.FindCell(Far);
			TestEqual(TEXT("lejos no consume"), FarCell->BurnStepsLeft, FWildfireModel::GrassBurnSteps);
			TestEqual(TEXT("lejos no contagia"), CountState(M, 249, -1, 251, 1, EFireCellState::Burning), 1);

			// Coste: solo se revisan las celdas ardiendo cercanas.
			const int32 NearBurning = M.NumBurning() - 1;
			const FWildfireStepResult R = M.Advance(++Second, 0, Dry(), Obs);
			TestEqual(TEXT("revisadas = ardiendo cerca"), R.CellsVisited, NearBurning);

			const FWildfireStepResult Moved = M.Advance(++Second, 0, Dry(), At(Far));
			TestTrue(TEXT("ahora sí"), M.FindCell(Far)->BurnStepsLeft < FWildfireModel::GrassBurnSteps);
			TestEqual(TEXT("y el de cerca se congela: solo se revisa la de lejos"), Moved.CellsVisited, 1);
		});

		It("sin jugadores o con posiciones no finitas no hace nada", [this]()
		{
			FWildfireModel M(8u, Everywhere());
			M.Ignite(FIntPoint(0, 0), 0);
			const FWildfireStepResult R = M.Advance(1, 0, Dry(), {});
			TestEqual(TEXT("nada revisado"), R.CellsVisited, 0);
			const FWildfireStepResult R2 = M.Advance(2, 0, Dry(), { FVector2D(NAN, NAN) });
			TestEqual(TEXT("NaN ignorado"), R2.CellsVisited, 0);
			TestEqual(TEXT("sigue igual"), M.FindCell(FIntPoint(0, 0))->BurnStepsLeft, FWildfireModel::GrassBurnSteps);
		});

		It("el radio llega al borde del chunk, no a su centro", [this]()
		{
			FWildfireModel M(8u, Everywhere());
			M.Ignite(FIntPoint(0, 0), 0); // chunk (0, 0): x de 0 a 32 m
			const double Edge = (double)FWildfireModel::ChunkCells * FWildfireModel::CellSizeCm;
			TestEqual(TEXT("justo dentro"), M.Advance(1, 0, Dry(), { FVector2D(Edge + FWildfireModel::ActiveRadiusCm - 1.0, 100.0) }).CellsVisited, 1);
			TestEqual(TEXT("justo fuera"), M.Advance(2, 0, Dry(), { FVector2D(Edge + FWildfireModel::ActiveRadiusCm + 1.0, 100.0) }).CellsVisited, 0);
		});

		It("acumula como mucho 4 pasos y no retrocede", [this]()
		{
			FWildfireModel M(8u, Everywhere());
			M.Ignite(FIntPoint(0, 0), 0);
			const FWildfireStepResult R = M.Advance(1000, 0, Dry(), At(FIntPoint(0, 0)));
			TestEqual(TEXT("4 pasos"), R.StepsSimulated, FWildfireModel::MaxCatchUpSteps);
			TestEqual(TEXT("el resto se descarta"), R.StepsDropped, 996);
			TestEqual(TEXT("mismo segundo"), M.Advance(1000, 0, Dry(), At(FIntPoint(0, 0))).StepsSimulated, 0);
			TestEqual(TEXT("segundo anterior"), M.Advance(10, 0, Dry(), At(FIntPoint(0, 0))).StepsSimulated, 0);
			TestEqual(TEXT("reloj"), M.GetLastSecond(), (int64)1000);
		});
	});

	Describe("apagar", [this]()
	{
		It("un chubasco apaga todo el incendio, también lo congelado lejos", [this]()
		{
			FWildfireModel M(8u, Everywhere());
			M.Ignite(FIntPoint(0, 0), 0);
			M.Ignite(FIntPoint(5000, 0), 0);
			FWildfireConditions C = Dry();
			C.Weather = EWeatherState::Shower;
			const FWildfireStepResult R = M.Advance(1, 30, C, At(FIntPoint(0, 0)));
			TestTrue(TEXT("apagado por lluvia"), R.bRainQuenched);
			TestFalse(TEXT("nada arde"), M.IsActive());
			TestEqual(TEXT("dos apagadas"), R.BurnedOut.Num(), 2);
			TestTrue(TEXT("quemada"), M.StateAt(FIntPoint(5000, 0), 30) == EFireCellState::Burnt);
			TestEqual(TEXT("minuto de apagado"), M.FindCell(FIntPoint(5000, 0))->BurntMinute, (int64)30);
		});

		It("un cubo apaga lo que arde en su radio y deja la celda quemada", [this]()
		{
			FWildfireModel M(8u, Everywhere());
			M.Ignite(FIntPoint(0, 0), 0);
			M.Ignite(FIntPoint(1, 0), 0);
			M.Ignite(FIntPoint(4, 0), 0);
			M.Douse(FIntPoint(0, 0), 1, 10, 60);
			TestEqual(TEXT("queda una"), M.NumBurning(), 1);
			TestTrue(TEXT("quemada"), M.StateAt(FIntPoint(1, 0), 10) == EFireCellState::Burnt);
			TestTrue(TEXT("vecina mojada"), M.IsWet(FIntPoint(0, 1), 10));
			TestFalse(TEXT("la quemada no se moja (no tiene combustible)"), M.IsWet(FIntPoint(0, 0), 10));
			M.Douse(FIntPoint(0, 0), 1000, 10, 60); // radio acotado
			TestTrue(TEXT("dentro del radio acotado"), M.StateAt(FIntPoint(4, 0), 10) == EFireCellState::Burnt);
			TestTrue(TEXT("radio 8 moja la celda 8"), M.IsWet(FIntPoint(8, 0), 10));
			TestFalse(TEXT("y no la 9"), M.IsWet(FIntPoint(9, 0), 10));
		});
	});

	Describe("rebrote y ceniza", [this]()
	{
		It("minutos y celdas extremos no desbordan", [this]()
		{
			// En la cota todo funciona sin desbordar (BurntMinute + rebrote, NowMinute + WetMinutes).
			const int64 Late = FWildfireModel::MaxAbsMinute - 10;
			FWildfireModel M(8u, Everywhere());
			TestTrue(TEXT("prende"), M.Ignite(FIntPoint(0, 0), Late));
			M.Douse(FIntPoint(0, 0), 1, Late, INT64_MAX);
			TestTrue(TEXT("sigue quemada"), M.StateAt(FIntPoint(0, 0), Late) == EFireCellState::Burnt);
			TestEqual(TEXT("con ceniza"), M.AshAt(FIntPoint(0, 0), Late), 1);
			TestTrue(TEXT("la vecina queda mojada"), M.IsWet(FIntPoint(1, 0), Late));
			TestFalse(TEXT("celda fuera de la cota de CellAt"), M.Ignite(FIntPoint(MAX_int32, 0), 0));
			// Fuera de la cota no se escribe nada.
			const FSaveValue Before = M.Save();
			TestFalse(TEXT("no prende fuera de la cota"), M.Ignite(FIntPoint(5, 5), INT64_MAX - 5));
			M.Douse(FIntPoint(5, 5), 1, INT64_MIN, 60);
			TestTrue(TEXT("ni moja fuera de la cota"), M.Save() == Before);
		});

		It("lo que guarda el juego se vuelve a cargar, también con minutos en la cota", [this]()
		{
			// Antes Douse saturaba WetUntil a INT64_MAX y Load rechazaba la partida entera.
			FWildfireModel M(8u, Everywhere());
			M.Ignite(FIntPoint(0, 0), -FWildfireModel::MaxAbsMinute);
			M.Douse(FIntPoint(0, 0), 2, -FWildfireModel::MaxAbsMinute, INT64_MAX);
			M.Douse(FIntPoint(9, 0), 1, FWildfireModel::MaxAbsMinute - 1, INT64_MAX);
			M.Ignite(FIntPoint(20, 0), 0);
			M.Advance(FWildfireModel::MaxAbsSecond, 0, Dry(), At(FIntPoint(20, 0)));
			const FSaveValue Saved = M.Save();
			FWildfireModel B(8u, Everywhere());
			TestTrue(TEXT("se carga"), B.Load(Saved));
			TestTrue(TEXT("igual"), B.Save() == Saved);
			TestTrue(TEXT("sigue mojada"), B.IsWet(FIntPoint(10, 0), FWildfireModel::MaxAbsMinute - 1));
		});

		It("un reloj fuera de la cota no congela el fuego", [this]()
		{
			FWildfireModel M(8u, Everywhere());
			M.Ignite(FIntPoint(0, 0), 0);
			// Antes adoptaba el segundo 1e15 y ningún segundo real volvía a simular: ardía para siempre.
			TestEqual(TEXT("segundo absurdo"), M.Advance(FWildfireModel::MaxAbsSecond + 1, 0, Dry(), At(FIntPoint(0, 0))).StepsSimulated, 0);
			TestEqual(TEXT("minuto absurdo"), M.Advance(5, INT64_MIN, Dry(), At(FIntPoint(0, 0))).StepsSimulated, 0);
			TestEqual(TEXT("no los adopta"), M.GetLastSecond(), (int64)0);
			TestEqual(TEXT("el reloj real sigue"), M.Advance(1, 0, Dry(), At(FIntPoint(0, 0))).StepsSimulated, 1);
			FSaveValue Bad = M.Save();
			Bad.Set(TEXT("lastSecond"), FSaveValue::MakeInt(FWildfireModel::MaxAbsSecond + 1));
			TestFalse(TEXT("Load rechaza un segundo fuera de la cota"), M.Load(Bad));
		});

		It("ceniza 3 días, hierba a los 12 y matorral a los 25; Prune olvida lo rebrotado", [this]()
		{
			const int64 Day = FWildfireModel::MinutesPerDay;
			FWildfireModel M(8u, [](FIntPoint C) { return C.X == 0 ? EFireFuel::Grass : (C.X == 5 ? EFireFuel::Shrub : EFireFuel::None); });
			M.Ignite(FIntPoint(0, 0), 0);
			M.Ignite(FIntPoint(5, 0), 0);
			M.Douse(FIntPoint(0, 0), 0, 100, 0);
			M.Douse(FIntPoint(5, 0), 0, 100, 0);
			TestEqual(TEXT("ceniza"), M.AshAt(FIntPoint(0, 0), 100), 1);
			TestEqual(TEXT("ceniza al final del día 3"), M.AshAt(FIntPoint(0, 0), 100 + 3 * Day - 1), 1);
			TestEqual(TEXT("sin ceniza tras 3 días"), M.AshAt(FIntPoint(0, 0), 100 + 3 * Day), 0);
			TestEqual(TEXT("se recoge una vez"), M.TakeAsh(FIntPoint(5, 0), 200), 1);
			TestEqual(TEXT("y no más"), M.TakeAsh(FIntPoint(5, 0), 200), 0);
			TestTrue(TEXT("sin combustible mientras está quemada"), M.FuelAt(FIntPoint(0, 0), 100 + 12 * Day - 1) == EFireFuel::None);
			TestFalse(TEXT("no prende quemada"), M.Ignite(FIntPoint(0, 0), 100 + 12 * Day - 1));
			TestTrue(TEXT("hierba rebrota a los 12 días"), M.StateAt(FIntPoint(0, 0), 100 + 12 * Day) == EFireCellState::Unburnt);
			TestTrue(TEXT("matorral aún no"), M.StateAt(FIntPoint(5, 0), 100 + 12 * Day) == EFireCellState::Burnt);
			TestTrue(TEXT("matorral a los 25"), M.StateAt(FIntPoint(5, 0), 100 + 25 * Day) == EFireCellState::Unburnt);
			TestEqual(TEXT("Prune a los 12 días"), M.Prune(100 + 12 * Day), 1);
			TestEqual(TEXT("Prune a los 25 días"), M.Prune(100 + 25 * Day), 1);
			TestEqual(TEXT("vacío"), M.NumStoredCells(), 0);
			TestTrue(TEXT("vuelve a prender"), M.Ignite(FIntPoint(0, 0), 100 + 25 * Day));
		});

		It("Prune no olvida una celda mojada hasta que se seca", [this]()
		{
			FWildfireModel M(8u, Everywhere());
			M.Douse(FIntPoint(0, 0), 0, 0, 60);
			TestEqual(TEXT("mojada se queda"), M.Prune(59), 0);
			TestEqual(TEXT("seca se va"), M.Prune(60), 1);
		});
	});

	Describe("guardado", [this]()
	{
		It("guardar a mitad de incendio y cargar sigue exactamente igual", [this]()
		{
			FWildfireModel A(21u, Patch(-12, -12, 12, 12));
			A.Ignite(FIntPoint(0, 0), 0);
			A.Douse(FIntPoint(4, 4), 1, 0, 600);
			const TArray<FVector2D> Obs = At(FIntPoint(0, 0));
			int64 S = 0;
			for (int32 i = 0; i < 15; ++i) { A.Advance(++S, 0, Dry(), Obs); }
			FWildfireModel B(21u, Patch(-12, -12, 12, 12));
			TestTrue(TEXT("carga"), B.Load(A.Save()));
			TestTrue(TEXT("mismo guardado"), A.Save() == B.Save());
			TestEqual(TEXT("mismas ardiendo"), A.NumBurning(), B.NumBurning());
			for (int32 i = 0; i < 40; ++i)
			{
				++S;
				A.Advance(S, 0, Dry(), Obs);
				B.Advance(S, 0, Dry(), Obs);
			}
			TestTrue(TEXT("siguen iguales"), A.Save() == B.Save());
		});

		It("rechaza partidas mal formadas sin tocar el estado", [this]()
		{
			FWildfireModel M(1u, Everywhere());
			M.Ignite(FIntPoint(3, 3), 0);
			const FSaveValue Good = M.Save();

			auto WithRow = [&Good](TArray<FSaveValue> Row)
			{
				FSaveValue V = Good;
				FSaveValue Cells = FSaveValue::MakeArray();
				FSaveValue R = FSaveValue::MakeArray();
				for (FSaveValue& E : Row) { R.Add(E); }
				Cells.Add(R);
				V.Set(TEXT("cells"), Cells);
				return V;
			};
			auto I = [](int64 X) { return FSaveValue::MakeInt(X); };
			const FSaveValue F = FSaveValue::MakeBool(false);

			FSaveValue WrongVersion = Good;
			WrongVersion.Set(TEXT("version"), I(99));
			TestFalse(TEXT("versión"), M.Load(WrongVersion));
			TestFalse(TEXT("no es objeto"), M.Load(FSaveValue::MakeInt(1)));
			TestFalse(TEXT("arde sin combustible"), M.Load(WithRow({ I(0), I(0), I(1), I(0), I(5), I(0), I(0), F })));
			TestFalse(TEXT("arde sin tiempo"), M.Load(WithRow({ I(0), I(0), I(1), I(1), I(0), I(0), I(0), F })));
			TestFalse(TEXT("estado fuera de rango"), M.Load(WithRow({ I(0), I(0), I(7), I(1), I(5), I(0), I(0), F })));
			TestFalse(TEXT("combustible fuera de rango"), M.Load(WithRow({ I(0), I(0), I(0), I(9), I(0), I(0), I(0), F })));
			TestFalse(TEXT("pasos de más"), M.Load(WithRow({ I(0), I(0), I(1), I(1), I(100000), I(0), I(0), F })));
			TestFalse(TEXT("coordenada fuera de int32"), M.Load(WithRow({ I(1ll << 40), I(0), I(0), I(1), I(0), I(0), I(0), F })));
			TestFalse(TEXT("fila corta"), M.Load(WithRow({ I(0), I(0) })));
			TestFalse(TEXT("celda fuera de ±1e9"), M.Load(WithRow({ I(MAX_int32), I(0), I(0), I(1), I(0), I(0), I(0), F })));
			TestFalse(TEXT("minuto de quema extremo"), M.Load(WithRow({ I(0), I(0), I(2), I(1), I(0), I(INT64_MAX), I(0), F })));
			TestFalse(TEXT("minuto de humedad extremo"), M.Load(WithRow({ I(0), I(0), I(0), I(1), I(0), I(0), I(INT64_MIN), F })));
			FSaveValue BadClock = Good;
			BadClock.Set(TEXT("lastSecond"), I(INT64_MIN));
			TestFalse(TEXT("segundo INT64_MIN (Advance se colgaba)"), M.Load(BadClock));
			BadClock.Set(TEXT("lastSecond"), I(-1));
			TestFalse(TEXT("segundo negativo"), M.Load(BadClock));
			FSaveValue Dup = Good;
			FSaveValue* Cells = Dup.Find(TEXT("cells"));
			Cells->Add(Cells->At(0));
			TestFalse(TEXT("celda repetida"), M.Load(Dup));
			TestTrue(TEXT("estado intacto"), M.Save() == Good);
			TestEqual(TEXT("sigue ardiendo"), M.NumBurning(), 1);
		});
	});
}

#endif
