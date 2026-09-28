#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/MineHazardModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MineHazardSpecDetail
{
	/** Caja de 24 × 12 m (celdas de 0,5 m) de z = −4 a z = 10; el suelo queda en z = 8. */
	FMineGridSettings Box(float Cell = 0.5f)
	{
		FMineGridSettings S;
		S.Origin = FVector(0.0, 0.0, -4.0);
		S.CellSize = Cell;
		const int32 PerMeter = FMath::RoundToInt32(1.0f / Cell);
		S.Size = FIntVector(24 * PerMeter, 12 * PerMeter, 14 * PerMeter);
		return S;
	}

	/** Roca por debajo de SurfaceZ y aire (cielo) por encima. */
	void Ground(FMineHazardModel& M, double SurfaceZ = 8.0)
	{
		M.FillFromDensity(
			[SurfaceZ](const FVector& P) { return static_cast<float>(P.Z - SurfaceZ); },
			[](const FVector&) { return false; });
	}

	/** Pone en State las celdas cuyo centro cae dentro de la caja [Min, Max]. */
	void Carve(FMineHazardModel& M, const FVector& Min, const FVector& Max, EMineCell State = EMineCell::Open)
	{
		const FIntVector Size = M.GetSettings().Size;
		for (int32 Z = 0; Z < Size.Z; ++Z)
		{
			for (int32 Y = 0; Y < Size.Y; ++Y)
			{
				for (int32 X = 0; X < Size.X; ++X)
				{
					const FIntVector C(X, Y, Z);
					const FVector P = M.CellCenter(C);
					if (P.X > Min.X && P.X < Max.X && P.Y > Min.Y && P.Y < Max.Y && P.Z > Min.Z && P.Z < Max.Z)
					{
						M.SetCell(C, State);
					}
				}
			}
		}
	}

	/** Sala bajo 2 m de roca: suelo en z = 4, techo en z = 6. */
	void Chamber(FMineHazardModel& M, double X0, double X1, double Y0, double Y1, double Z0 = 4.0, double Z1 = 6.0)
	{
		Carve(M, FVector(X0, Y0, Z0), FVector(X1, Y1, Z1));
	}

	int32 Count(const FMineHazardResult& R, EMineHazardEventKind Kind)
	{
		int32 N = 0;
		for (const FMineHazardEvent& E : R.Events)
		{
			N += E.Kind == Kind ? 1 : 0;
		}
		return N;
	}

	FMineWaterEnvironment Dry()
	{
		FMineWaterEnvironment Env;
		Env.SeaLevel = -100.0;
		Env.WaterTable = -100.0;
		return Env;
	}
}

BEGIN_DEFINE_SPEC(FMineHazardModelSpec, "Explored.MineHazard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FMineHazardModelSpec)

void FMineHazardModelSpec::Define()
{
	using namespace MineHazardSpecDetail;

	Describe("derrumbe: más de 3 m de luz sin apoyo (biblia 02 §2.4)", [this]()
	{
		It("una sala de 3 m justos aguanta y una de 3,5 m no", [this]()
		{
			FMineHazardModel Held(Box());
			Ground(Held);
			Chamber(Held, 2.0, 5.0, 2.0, 8.0);
			TestEqual(TEXT("3 m: todo el techo apoyado"), Held.UnsupportedRoofCells().Num(), 0);

			FMineHazardModel Falls(Box());
			Ground(Falls);
			Chamber(Falls, 2.0, 5.5, 2.0, 8.0);
			TestTrue(TEXT("3,5 m: hay techo sin apoyo"), Falls.UnsupportedRoofCells().Num() > 0);
			for (const FIntVector& C : Falls.UnsupportedRoofCells())
			{
				TestEqual(TEXT("solo la franja central"), Falls.CellCenter(C).X, 3.75, 1.0e-9);
			}
		});

		It("con celdas de 0,25 m el límite es el mismo: 3 m aguantan, 3,25 m no", [this]()
		{
			FMineHazardModel Held(Box(0.25f));
			Ground(Held);
			Chamber(Held, 2.0, 5.0, 2.0, 8.0);
			TestEqual(TEXT("3 m"), Held.UnsupportedRoofCells().Num(), 0);

			FMineHazardModel Falls(Box(0.25f));
			Ground(Falls);
			Chamber(Falls, 2.0, 5.25, 2.0, 8.0);
			TestTrue(TEXT("3,25 m"), Falls.UnsupportedRoofCells().Num() > 0);
		});

		It("la luz es la anchura, no el largo: una galería de 1 m y 20 m de largo aguanta", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Chamber(M, 1.0, 21.0, 5.0, 6.0);
			TestEqual(TEXT("sin techo suelto"), M.UnsupportedRoofCells().Num(), 0);
			const FMineHazardResult R = M.Advance(60000, Dry());
			TestEqual(TEXT("nada se cae"), R.CollapsedCells.Num(), 0);
		});

		It("un hoyo abierto al cielo no tiene techo que caer", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(2.0, 2.0, 5.0), FVector(8.0, 8.0, 11.0));
			TestEqual(TEXT("sin techo"), M.UnsupportedRoofCells().Num(), 0);
		});

		It("cruje a los 6 s y cae a los 8 s, rellenando el hueco hasta el suelo", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Chamber(M, 2.0, 6.0, 2.0, 6.0);
			const TArray<FIntVector> Roof = M.UnsupportedRoofCells();
			TestEqual(TEXT("un cuadrado de 1 m en el centro"), Roof.Num(), 4);

			FMineHazardResult R = M.Advance(5750, Dry());
			TestEqual(TEXT("5,75 s: aún nada"), R.Events.Num(), 0);
			R = M.Advance(250, Dry());
			TestEqual(TEXT("6 s: crujido"), Count(R, EMineHazardEventKind::CollapseWarning), 1);
			TestEqual(TEXT("en el centro"), R.Events[0].Location, FVector(4.0, 4.0, 5.75), 1.0e-6f);
			R = M.Advance(1750, Dry());
			TestEqual(TEXT("7,75 s: sigue en pie"), R.CollapsedCells.Num(), 0);
			TestEqual(TEXT("expuesto 7,75 s"), M.ExposureMs(Roof[0]), 7750);
			R = M.Advance(250, Dry());
			TestEqual(TEXT("8 s: se viene abajo"), Count(R, EMineHazardEventKind::Collapse), 1);
			TestEqual(TEXT("cuatro columnas de 2 m"), R.CollapsedCells.Num(), 16);
			for (const FIntVector& C : R.CollapsedCells)
			{
				TestTrue(TEXT("escombro"), M.GetCell(C) == EMineCell::Solid);
			}
			TestEqual(TEXT("el resto de la sala ya se apoya en el escombro"), M.UnsupportedRoofCells().Num(), 0);
			TestEqual(TEXT("y no vuelve a caer"), M.Advance(30000, Dry()).CollapsedCells.Num(), 0);
		});

		It("no depende de cómo se trocee el tiempo", [this]()
		{
			auto Run = [](TArray<int32> Steps)
			{
				FMineHazardModel M(Box());
				Ground(M);
				Chamber(M, 2.0, 7.0, 2.0, 9.0);
				TArray<FIntVector> Fallen;
				TArray<int32> Kinds;
				for (const int32 Ms : Steps)
				{
					const FMineHazardResult R = M.Advance(Ms, Dry());
					Fallen.Append(R.CollapsedCells);
					for (const FMineHazardEvent& E : R.Events)
					{
						Kinds.Add(static_cast<int32>(E.Kind));
					}
				}
				return TPair<TArray<FIntVector>, TArray<int32>>(Fallen, Kinds);
			};
			TArray<int32> Fine;
			for (int32 I = 0; I < 80; ++I)
			{
				Fine.Add(250);
			}
			const auto A = Run({ 20000 });
			const auto B = Run(Fine);
			const auto C = Run({ 3333, 4667, 1, 11999 });
			TestTrue(TEXT("algo cae"), A.Key.Num() > 0);
			TestTrue(TEXT("mismo escombro de una vez que a trozos"), A.Key == B.Key && A.Key == C.Key);
			TestTrue(TEXT("mismos avisos"), A.Value == B.Value && A.Value == C.Value);
		});

		It("un salto de tiempo enorme no desborda: como mucho 60 s por llamada", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			const FMineHazardResult R = M.Advance(MAX_int32, Dry());
			TestEqual(TEXT("240 revisiones"), R.Ticks, FMineHazardModel::MaxTicksPerAdvance);
			TestEqual(TEXT("tiempo negativo: nada"), M.Advance(-5, Dry()).Ticks, 0);
		});
	});

	Describe("viga de apoyo", [this]()
	{
		It("una viga en el centro antes de los 8 s lo evita (logro viga_a_tiempo)", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Chamber(M, 2.0, 6.0, 2.0, 6.0);
			M.Advance(7000, Dry());
			TestTrue(TEXT("cabe"), M.CanPlaceBeam(FVector(4.1, 4.1, 4.2)));
			TestTrue(TEXT("se pone"), M.PlaceBeam(FVector(4.1, 4.1, 4.2)));
			const FMineHazardResult R = M.Advance(250, Dry());
			TestEqual(TEXT("salvado"), Count(R, EMineHazardEventKind::CollapseAverted), 1);
			TestEqual(TEXT("sin techo suelto"), M.UnsupportedRoofCells().Num(), 0);
			TestEqual(TEXT("y ya no cae"), M.Advance(60000, Dry()).CollapsedCells.Num(), 0);
		});

		It("quitar la viga vuelve a empezar la cuenta desde cero", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Chamber(M, 2.0, 6.0, 2.0, 6.0);
			M.PlaceBeam(FVector(4.1, 4.1, 5.9));
			M.Advance(20000, Dry());
			TestTrue(TEXT("se quita desde cualquier altura del poste"), M.RemoveBeam(FVector(4.1, 4.1, 4.1)));
			TestEqual(TEXT("7,75 s después, en pie"), M.Advance(7750, Dry()).CollapsedCells.Num(), 0);
			TestEqual(TEXT("a los 8 s cae"), Count(M.Advance(250, Dry()), EMineHazardEventKind::Collapse), 1);
		});

		It("apoya a 1,5 m: en el centro salva una sala de 4 m, pero no una de 6 m", [this]()
		{
			FMineHazardModel Four(Box());
			Ground(Four);
			Chamber(Four, 2.0, 6.0, 2.0, 6.0);
			Four.PlaceBeam(FVector(3.9, 3.9, 4.5));
			TestEqual(TEXT("4 m con viga"), Four.UnsupportedRoofCells().Num(), 0);

			FMineHazardModel Six(Box());
			Ground(Six);
			Chamber(Six, 2.0, 8.0, 2.0, 8.0);
			Six.PlaceBeam(FVector(5.1, 5.1, 4.5));
			TestTrue(TEXT("6 m con una sola viga, no"), Six.UnsupportedRoofCells().Num() > 0);
			Six.PlaceBeam(FVector(3.6, 3.6, 4.5));
			Six.PlaceBeam(FVector(6.6, 6.6, 4.5));
			Six.PlaceBeam(FVector(3.6, 6.6, 4.5));
			Six.PlaceBeam(FVector(6.6, 3.6, 4.5));
			TestEqual(TEXT("con cinco, sí"), Six.UnsupportedRoofCells().Num(), 0);
		});

		It("solo bajo techo, sobre suelo firme, de 3 m como mucho y una por columna", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Chamber(M, 2.0, 6.0, 2.0, 6.0, 4.0, 7.0);    // 3 m de alto
			Chamber(M, 10.0, 14.0, 2.0, 6.0, 3.5, 7.0);  // 3,5 m de alto
			Carve(M, FVector(16.0, 2.0, 5.0), FVector(20.0, 6.0, 11.0));  // hoyo al cielo
			TestTrue(TEXT("3 m justos"), M.CanPlaceBeam(FVector(4.1, 4.1, 4.1)));
			TestFalse(TEXT("3,5 m"), M.CanPlaceBeam(FVector(12.1, 4.1, 4.1)));
			TestFalse(TEXT("sin techo"), M.CanPlaceBeam(FVector(18.1, 4.1, 5.1)));
			TestFalse(TEXT("en la roca"), M.CanPlaceBeam(FVector(4.1, 4.1, 2.0)));
			TestFalse(TEXT("fuera de la caja"), M.CanPlaceBeam(FVector(-3.0, 4.1, 4.1)));
			TestFalse(TEXT("punto no finito"), M.CanPlaceBeam(FVector(NAN, 4.1, 4.1)));
			TestTrue(TEXT("la primera"), M.PlaceBeam(FVector(4.1, 4.1, 4.1)));
			TestFalse(TEXT("otra en el mismo poste"), M.PlaceBeam(FVector(4.1, 4.1, 6.4)));
			TestEqual(TEXT("una"), M.NumBeams(), 1);
			FMineHazardModel Sea(Box());
			Ground(Sea);
			Chamber(Sea, 2.0, 6.0, 2.0, 6.0);
			Carve(Sea, FVector(3.5, 3.5, 3.0), FVector(4.5, 4.5, 4.0), EMineCell::Sea);
			TestFalse(TEXT("sobre agua de mar, no"), Sea.CanPlaceBeam(FVector(3.9, 3.9, 4.2)));
		});

		It("si se pica bajo la viga y queda colgando, se pierde y el techo vuelve a contar", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Chamber(M, 2.0, 6.0, 2.0, 6.0);
			M.PlaceBeam(FVector(4.1, 4.1, 4.2));
			TestEqual(TEXT("apoyado"), M.UnsupportedRoofCells().Num(), 0);
			Carve(M, FVector(4.0, 4.0, 1.0), FVector(4.5, 4.5, 4.0));
			const FMineHazardResult R = M.Advance(250, Dry());
			TestEqual(TEXT("viga perdida"), Count(R, EMineHazardEventKind::BeamLost), 1);
			TestEqual(TEXT("sin vigas"), M.NumBeams(), 0);
			TestTrue(TEXT("el techo vuelve a estar suelto"), M.UnsupportedRoofCells().Num() > 0);
		});
	});

	Describe("aire viciado a más de 15 m de una salida", [this]()
	{
		It("la distancia se mide por dentro de la galería y el límite es 15 m", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(0.0, 0.0, 4.0), FVector(1.0, 1.0, 11.0));  // boca vertical
			Carve(M, FVector(0.0, 0.0, 4.0), FVector(24.0, 1.0, 5.0));  // galería de 1 × 1 m
			TestEqual(TEXT("la boca ve el cielo"), M.AirDistanceMeters(M.CellOf(FVector(0.5, 0.5, 4.5))), 0.0f);
			TestEqual(TEXT("a 15 m justos"), M.AirDistanceMeters(M.CellOf(FVector(15.75, 0.25, 4.25))), 15.0f, 1.0e-5f);
			TestFalse(TEXT("15 m: aún se respira"), M.IsStaleAir(FVector(15.75, 0.25, 4.25)));
			TestTrue(TEXT("15,5 m: viciado"), M.IsStaleAir(FVector(16.25, 0.25, 4.25)));
			TestTrue(TEXT("al fondo, viciado"), M.IsStaleAir(FVector(23.75, 0.75, 4.75)));
			TestFalse(TEXT("en la roca no hay aire que medir"), M.IsStaleAir(FVector(10.0, 5.0, 2.0)));
			TestFalse(TEXT("fuera de la caja"), M.IsStaleAir(FVector(-10.0, 0.0, 4.5)));

			// Una chimenea al fondo airea toda la galería.
			Carve(M, FVector(20.0, 0.0, 4.0), FVector(20.5, 0.5, 11.0));
			TestFalse(TEXT("con chimenea, el fondo respira"), M.IsStaleAir(FVector(23.75, 0.75, 4.75)));
			TestFalse(TEXT("y el punto que antes era el peor"), M.IsStaleAir(FVector(16.25, 0.25, 4.25)));
		});

		It("una bolsa cerrada sin camino a la salida está viciada aunque esté cerca", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(2.0, 2.0, 6.0), FVector(3.0, 3.0, 7.0));
			TestTrue(TEXT("sin camino"), M.AirDistanceMeters(M.CellOf(FVector(2.5, 2.5, 6.5))) < 0.0f);
			TestTrue(TEXT("viciada"), M.IsStaleAir(FVector(2.5, 2.5, 6.5)));
		});

		It("una pared sella el aire y el aire no se cuela por una arista de roca", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(0.0, 0.0, 4.0), FVector(1.0, 1.0, 11.0));
			Carve(M, FVector(0.0, 0.0, 4.0), FVector(8.0, 1.0, 5.0));
			TestFalse(TEXT("abierta"), M.IsStaleAir(FVector(6.0, 0.5, 4.5)));
			Carve(M, FVector(3.0, 0.0, 4.0), FVector(3.5, 1.0, 5.0), EMineCell::Wall);
			TestTrue(TEXT("tras la pared"), M.IsStaleAir(FVector(6.0, 0.5, 4.5)));

			// Dos celdas que solo se tocan por una arista no comparten aire.
			FMineHazardModel Edge(Box());
			Ground(Edge);
			Carve(Edge, FVector(4.0, 4.0, 6.0), FVector(4.5, 4.5, 11.0));
			Carve(Edge, FVector(4.5, 4.5, 6.0), FVector(5.0, 5.0, 6.5));
			TestTrue(TEXT("por la arista, no"), Edge.AirDistanceMeters(Edge.CellOf(FVector(4.75, 4.75, 6.25))) < 0.0f);
			Carve(Edge, FVector(4.5, 4.0, 6.0), FVector(5.0, 4.5, 6.5));
			TestEqual(TEXT("con un lado abierto, rodea por las caras: 2 celdas"),
				Edge.AirDistanceMeters(Edge.CellOf(FVector(4.75, 4.75, 6.25))), 1.0f, 1.0e-5f);
			Carve(Edge, FVector(4.0, 4.5, 6.0), FVector(4.5, 5.0, 6.5));
			TestEqual(TEXT("con los dos lados abiertos, en diagonal: 1,4 celdas"),
				Edge.AirDistanceMeters(Edge.CellOf(FVector(4.75, 4.75, 6.25))), 0.7f, 1.0e-5f);
		});

		It("el aire baja un 4 %/min tras 2 min de gracia, marea bajo el 20 % y nunca mata", [this]()
		{
			FMineAirState S;
			FMineHazardModel::AdvanceAir(S, true, 120.0f);
			TestEqual(TEXT("gracia"), S.Air, 1.0f);
			TestFalse(TEXT("aún respira bien"), FMineHazardModel::AirSignals(S).bLaboredBreathing);
			FMineHazardModel::AdvanceAir(S, true, 60.0f);
			TestEqual(TEXT("un minuto después, 96 %"), S.Air, 0.96f, 1.0e-5f);
			TestTrue(TEXT("respiración agitada"), FMineHazardModel::AirSignals(S).bLaboredBreathing);
			TestEqual(TEXT("sin mareo"), FMineHazardModel::AirSignals(S).Dizziness, 0.0f);
			FMineHazardModel::AdvanceAir(S, true, 19.0f * 60.0f);
			TestEqual(TEXT("20 min tras la gracia: 20 %"), S.Air, 0.2f, 1.0e-4f);
			FMineHazardModel::AdvanceAir(S, true, 2.5f * 60.0f);
			TestEqual(TEXT("mareo a medias"), FMineHazardModel::AirSignals(S).Dizziness, 0.5f, 1.0e-3f);
			FMineHazardModel::AdvanceAir(S, true, 3600.0f);
			TestEqual(TEXT("tope en 0"), S.Air, 0.0f);
			TestEqual(TEXT("mareo completo"), FMineHazardModel::AirSignals(S).Dizziness, 1.0f);
			FMineHazardModel::AdvanceAir(S, false, 60.0f);
			TestEqual(TEXT("al salir recupera un 25 %/min"), S.Air, 0.25f, 1.0e-5f);
			TestEqual(TEXT("y la gracia vuelve a empezar"), S.StaleSeconds, 0.0f);
		});

		It("el aire no depende de cómo se trocee el tiempo y aguanta entradas rotas", [this]()
		{
			FMineAirState A;
			FMineAirState B;
			FMineHazardModel::AdvanceAir(A, true, 600.0f);
			for (int32 I = 0; I < 600; ++I)
			{
				FMineHazardModel::AdvanceAir(B, true, 1.0f);
			}
			TestEqual(TEXT("mismo aire"), A.Air, B.Air, 1.0e-4f);
			FMineAirState Broken;
			Broken.Air = NAN;
			Broken.StaleSeconds = -INFINITY;
			FMineHazardModel::AdvanceAir(Broken, true, NAN);
			TestEqual(TEXT("aire roto vuelve a 1"), Broken.Air, 1.0f);
			TestEqual(TEXT("gracia rota vuelve a 0"), Broken.StaleSeconds, 0.0f);
			FMineHazardModel::AdvanceAir(Broken, true, -30.0f);
			TestEqual(TEXT("tiempo negativo no hace nada"), Broken.Air, 1.0f);
		});
	});

	Describe("inundación (biblia 02 §2.4 y §2.7)", [this]()
	{
		It("una galería que baja del nivel freático se llena 1 m cada 40 s hasta la cota 0", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(0.0, 0.0, -2.0), FVector(1.0, 1.0, 11.0));
			FMineWaterEnvironment Env;
			Env.SeaLevel = -100.0;
			Env.WaterTable = 0.0;
			const FIntVector Bottom = M.CellOf(FVector(0.5, 0.5, -1.75));
			TestTrue(TEXT("seca al empezar"), M.WaterLevel(Bottom) == TNumericLimits<double>::Lowest());
			FMineHazardResult R = M.Advance(40000, Env);
			TestTrue(TEXT("entra agua"), R.bWaterEntering);
			TestEqual(TEXT("40 s: 1 m"), M.WaterLevel(Bottom), -1.0, 1.0e-9);
			TestEqual(TEXT("profundidad en el fondo"), M.WaterDepthAt(FVector(0.5, 0.5, -1.75)), 0.75, 1.0e-9);
			M.Advance(40000, Env);
			TestEqual(TEXT("80 s: en la cota 0"), M.WaterLevel(Bottom), 0.0, 1.0e-9);
			R = M.Advance(40000, Env);
			TestFalse(TEXT("ya no sube"), R.bWaterEntering);
			TestEqual(TEXT("no pasa de la cota 0"), M.WaterLevel(Bottom), 0.0, 1.0e-9);
		});

		It("con lluvia intensa sube un 50 % más rápido", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(0.0, 0.0, -3.0), FVector(1.0, 1.0, 11.0));
			FMineWaterEnvironment Env;
			Env.SeaLevel = -100.0;
			Env.WaterTable = 0.0;
			Env.bHeavyRain = true;
			M.Advance(40000, Env);
			TestEqual(TEXT("1,5 m en 40 s"), M.WaterLevel(M.CellOf(FVector(0.5, 0.5, -2.5))), -1.5, 1.0e-9);
		});

		It("por encima del nivel freático y sin tocar el mar no entra agua", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(0.0, 0.0, 0.5), FVector(1.0, 1.0, 11.0));
			FMineWaterEnvironment Env;
			Env.WaterTable = 0.0;
			Env.SeaLevel = 5.0;
			const FMineHazardResult R = M.Advance(60000, Env);
			TestFalse(TEXT("seca"), R.bWaterEntering);
			TestEqual(TEXT("sin agua"), M.WaterDepthAt(FVector(0.5, 0.5, 0.75)), 0.0);
		});

		It("abierta al mar sube y baja con la marea; una pared la sella y el agua se queda", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(10.0, 0.0, 1.0), FVector(20.0, 1.0, 3.0));
			Carve(M, FVector(20.0, 0.0, 1.0), FVector(24.0, 1.0, 3.0), EMineCell::Sea);
			FMineWaterEnvironment Env;
			Env.WaterTable = 0.0;
			Env.SeaLevel = 2.0;
			const FIntVector Cell = M.CellOf(FVector(12.0, 0.5, 1.25));
			M.Advance(60000, Env);
			TestEqual(TEXT("pleamar: 1 m de agua"), M.WaterLevel(Cell), 2.0, 1.0e-9);
			Env.SeaLevel = 1.5;
			M.Advance(20000, Env);
			TestEqual(TEXT("baja con la marea al mismo ritmo"), M.WaterLevel(Cell), 1.5, 1.0e-9);
			Env.SeaLevel = 2.5;
			M.Advance(8000, Env);
			TestEqual(TEXT("vuelve a subir"), M.WaterLevel(Cell), 1.7, 1.0e-9);
			Carve(M, FVector(19.5, 0.0, 1.0), FVector(20.0, 1.0, 3.0), EMineCell::Wall);
			Env.SeaLevel = -1.0;
			M.Advance(60000, Env);
			TestEqual(TEXT("sellada, el agua se queda"), M.WaterLevel(Cell), 1.7, 1.0e-9);
			Env.SeaLevel = 3.0;
			const FMineHazardResult R = M.Advance(60000, Env);
			TestFalse(TEXT("y no entra más"), R.bWaterEntering);
		});

		It("sellada antes de abrir la brecha no se inunda", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(10.0, 0.0, 1.0), FVector(19.5, 1.0, 3.0));
			Carve(M, FVector(19.5, 0.0, 1.0), FVector(20.0, 1.0, 3.0), EMineCell::Wall);
			Carve(M, FVector(20.0, 0.0, 1.0), FVector(24.0, 1.0, 3.0), EMineCell::Sea);
			FMineWaterEnvironment Env;
			Env.WaterTable = 0.0;
			Env.SeaLevel = 2.5;
			TestFalse(TEXT("no entra agua"), M.Advance(60000, Env).bWaterEntering);
		});

		It("al unir una galería inundada con otra seca, el agua se reparte al nivel más alto", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(10.0, 0.0, 1.0), FVector(15.0, 1.0, 3.0));
			Carve(M, FVector(15.5, 0.0, 1.0), FVector(20.0, 1.0, 3.0));
			Carve(M, FVector(20.0, 0.0, 1.0), FVector(24.0, 1.0, 3.0), EMineCell::Sea);
			FMineWaterEnvironment Env;
			Env.WaterTable = 0.0;
			Env.SeaLevel = 2.0;
			M.Advance(60000, Env);
			// Además de las dos galerías, el aire sobre el suelo cuenta como una.
			const int32 Before = M.NumGalleries();
			TestEqual(TEXT("dos galerías y el cielo"), Before, 3);
			TestTrue(TEXT("la de tierra, seca"), M.WaterLevel(M.CellOf(FVector(12.0, 0.5, 1.25))) == TNumericLimits<double>::Lowest());
			Carve(M, FVector(15.0, 0.0, 1.0), FVector(15.5, 1.0, 3.0));
			TestEqual(TEXT("una menos"), M.NumGalleries(), Before - 1);
			TestEqual(TEXT("comparten nivel"), M.WaterLevel(M.CellOf(FVector(12.0, 0.5, 1.25))), 2.0, 1.0e-9);
		});

		It("la crecida de monzón inunda al 30 % y se vacía en 2 días", [this]()
		{
			TestEqual(TEXT("durante"), FMineHazardModel::MonsoonFloodFraction(true, 0.0), 0.3);
			TestEqual(TEXT("al acabar"), FMineHazardModel::MonsoonFloodFraction(false, 0.0), 0.3);
			TestEqual(TEXT("un día después"), FMineHazardModel::MonsoonFloodFraction(false, 24.0), 0.15, 1.0e-12);
			TestEqual(TEXT("dos días"), FMineHazardModel::MonsoonFloodFraction(false, 48.0), 0.0);
			TestEqual(TEXT("horas no finitas"), FMineHazardModel::MonsoonFloodFraction(false, NAN), 0.0);
			TestEqual(TEXT("horas negativas"), FMineHazardModel::MonsoonFloodFraction(false, -3.0), 0.3);
		});

		It("mar y nivel freático no finitos no inundan nada", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(0.0, 0.0, -2.0), FVector(1.0, 1.0, 11.0));
			FMineWaterEnvironment Env;
			Env.SeaLevel = NAN;
			Env.WaterTable = INFINITY;
			TestFalse(TEXT("sin agua"), M.Advance(60000, Env).bWaterEntering);
		});
	});

	Describe("guardado, bordes y determinismo", [this]()
	{
		It("el agua va y vuelve con el guardado", [this]()
		{
			FMineHazardModel M(Box());
			Ground(M);
			Carve(M, FVector(0.0, 0.0, -3.0), FVector(1.0, 1.0, 11.0));
			FMineWaterEnvironment Env;
			Env.SeaLevel = -100.0;
			Env.WaterTable = 0.0;
			M.Advance(33000, Env);
			const FIntVector Cell = M.CellOf(FVector(0.5, 0.5, -2.75));
			FMineHazardModel Loaded(Box());
			Ground(Loaded);
			Carve(Loaded, FVector(0.0, 0.0, -3.0), FVector(1.0, 1.0, 11.0));
			TestTrue(TEXT("carga"), Loaded.FromValue(M.ToValue()));
			TestEqual(TEXT("mismo nivel (al milímetro)"), Loaded.WaterLevel(Cell), M.WaterLevel(Cell), 1.0e-3);
			TestTrue(TEXT("mismo guardado"), Loaded.ToValue() == M.ToValue());
		});

		It("rechaza guardados rotos sin inventarse agua", [this]()
		{
			auto Doc = [](FSaveValue Row, int64 Version = 1)
			{
				FSaveValue Root = FSaveValue::MakeObject();
				Root.Set(TEXT("v"), FSaveValue::MakeInt(Version));
				FSaveValue List = FSaveValue::MakeArray();
				List.Add(Row);
				Root.Set(TEXT("water"), List);
				return Root;
			};
			auto Row = [](int64 A, int64 B, int64 C, int64 D)
			{
				FSaveValue R = FSaveValue::MakeArray();
				R.Add(FSaveValue::MakeInt(A));
				R.Add(FSaveValue::MakeInt(B));
				R.Add(FSaveValue::MakeInt(C));
				R.Add(FSaveValue::MakeInt(D));
				return R;
			};
			FSaveValue Three = FSaveValue::MakeArray();
			Three.Add(FSaveValue::MakeInt(1));
			FSaveValue WithDouble = FSaveValue::MakeArray();
			WithDouble.Add(FSaveValue::MakeInt(1));
			WithDouble.Add(FSaveValue::MakeInt(1));
			WithDouble.Add(FSaveValue::MakeInt(1));
			WithDouble.Add(FSaveValue::MakeDouble(0.5));
			const TArray<FSaveValue> Bad = {
				FSaveValue::MakeString(TEXT("agua")),
				Doc(Row(250, 250, -2750, 0), 2),
				Doc(Three),
				Doc(WithDouble),
				Doc(Row(250, 250, -2750, static_cast<int64>(1) << 50)),
			};
			for (int32 I = 0; I < Bad.Num(); ++I)
			{
				FMineHazardModel M(Box());
				Ground(M);
				Carve(M, FVector(0.0, 0.0, -3.0), FVector(1.0, 1.0, 11.0));
				TestFalse(FString::Printf(TEXT("caso %d"), I), M.FromValue(Bad[I]));
				TestEqual(*(FString::Printf(TEXT("caso %d seco"), I)), M.WaterDepthAt(FVector(0.5, 0.5, -2.75)), 0.0);
			}
			// Un nivel en una celda de roca o fuera de la caja se descarta sin más.
			FMineHazardModel M(Box());
			Ground(M);
			TestTrue(TEXT("punto en la roca"), M.FromValue(Doc(Row(5000, 5000, 0, 3000))));
			TestEqual(TEXT("sin galerías con agua"), M.ToValue().Find(TEXT("water"))->Num(), 0);
		});

		It("una caja rota o gigante se sanea sin reventar", [this]()
		{
			FMineGridSettings S;
			S.CellSize = NAN;
			S.Size = FIntVector(100000, -3, 100000);
			S.Origin = FVector(INFINITY, 0.0, 0.0);
			FMineHazardModel M(S);
			const FIntVector Size = M.GetSettings().Size;
			TestEqual(TEXT("celda por defecto"), M.GetSettings().CellSize, 0.5f);
			TestTrue(TEXT("tamaño acotado"), static_cast<int64>(Size.X) * Size.Y * Size.Z <= FMineHazardModel::MaxCells);
			TestTrue(TEXT("al menos una celda por eje"), Size.X >= 1 && Size.Y >= 1 && Size.Z >= 1);
			TestTrue(TEXT("fuera de la caja es roca"), M.GetCell(FIntVector(-1, 0, 0)) == EMineCell::Solid);
			M.SetCell(FIntVector(-1, 0, 0), EMineCell::Open);
			TestTrue(TEXT("y no se puede tocar"), M.GetCell(FIntVector(-1, 0, 0)) == EMineCell::Solid);
			M.SetCell(FIntVector(0, 0, 0), static_cast<EMineCell>(200));
			TestTrue(TEXT("estado inválido ignorado"), M.GetCell(FIntVector(0, 0, 0)) == EMineCell::Solid);
		});

		It("misma semilla, mismas galerías: mismos derrumbes, mismo aire y misma agua", [this]()
		{
			auto Run = [](uint64 Seed, bool bReverse)
			{
				FMineHazardModel M(Box());
				Ground(M);
				FExploredRandom Rng(Seed);
				TArray<FBox> Rooms;
				for (int32 I = 0; I < 25; ++I)
				{
					const FVector Min(Rng.RangeFloat(0.0f, 20.0f), Rng.RangeFloat(0.0f, 9.0f), Rng.RangeFloat(-3.0f, 5.0f));
					const FVector Ext(Rng.RangeFloat(0.5f, 5.0f), Rng.RangeFloat(0.5f, 5.0f), Rng.RangeFloat(0.5f, 3.0f));
					Rooms.Add(FBox(Min, Min + Ext));
				}
				// Una boca para que haya aire y galerías con distancias reales.
				Rooms.Add(FBox(FVector(0.0, 0.0, -3.0), FVector(1.0, 1.0, 11.0)));
				if (bReverse)
				{
					for (int32 I = Rooms.Num() - 1; I >= 0; --I)
					{
						Carve(M, Rooms[I].Min, Rooms[I].Max);
					}
				}
				else
				{
					for (const FBox& B : Rooms)
					{
						Carve(M, B.Min, B.Max);
					}
				}
				FMineWaterEnvironment Env;
				Env.WaterTable = 0.0;
				Env.SeaLevel = -100.0;
				TArray<FString> Log;
				for (int32 Step = 0; Step < 6; ++Step)
				{
					const FMineHazardResult R = M.Advance(5000, Env);
					for (const FMineHazardEvent& E : R.Events)
					{
						Log.Add(FString::Printf(TEXT("%d %.3f %.3f %.3f %d"), static_cast<int32>(E.Kind), E.Location.X, E.Location.Y, E.Location.Z, E.Cells));
					}
					Log.Add(FString::Printf(TEXT("caídas %d agua %d"), R.CollapsedCells.Num(), R.bWaterEntering ? 1 : 0));
				}
				double AirSum = 0.0;
				const FIntVector Size = M.GetSettings().Size;
				for (int32 Z = 0; Z < Size.Z; ++Z)
				{
					for (int32 Y = 0; Y < Size.Y; ++Y)
					{
						for (int32 X = 0; X < Size.X; ++X)
						{
							AirSum += M.AirDistanceMeters(FIntVector(X, Y, Z));
						}
					}
				}
				Log.Add(FString::Printf(TEXT("aire %.4f"), AirSum));
				Log.Add(FSaveText::Write(M.ToValue(), ESaveTextStyle::Compact));
				return Log;
			};
			const TArray<FString> A = Run(20260928, false);
			const TArray<FString> B = Run(20260928, false);
			const TArray<FString> C = Run(20260928, true);
			const TArray<FString> D = Run(7, false);
			TestTrue(TEXT("misma semilla, mismo resultado"), A == B);
			TestTrue(TEXT("el orden en que se pican las salas no importa"), A == C);
			TestFalse(TEXT("otra semilla, otra mina"), A == D);
		});
	});
}

#endif
