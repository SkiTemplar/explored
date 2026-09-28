#include "Misc/AutomationTest.h"

#include "WorldGen/SandModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FSandModelSpec, "Explored.Sand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	/** Entorno seco: la pleamar muy por debajo y el foco en el origen. */
	FSandEnvironment Dry()
	{
		FSandEnvironment Env;
		Env.HighTide = -10.0;
		return Env;
	}
	/** Marea de un medio ciclo: pleamar y bajamar en metros. */
	FSandTide Tide(double High, double Low, bool bSpring = false)
	{
		FSandTide T;
		T.HighTide = High;
		T.LowTide = Low;
		T.bSpring = bSpring;
		return T;
	}
	/** Deltas de todas las columnas de un chunk. */
	TMap<FIntPoint, int32> Snapshot(const FSandModel& Model, const FIntPoint& Chunk)
	{
		const int32 N = Model.GetSettings().CellsPerChunk;
		TMap<FIntPoint, int32> Out;
		for (int32 Y = Chunk.Y * N; Y < Chunk.Y * N + N; ++Y)
		{
			for (int32 X = Chunk.X * N; X < Chunk.X * N + N; ++X)
			{
				Out.Add(FIntPoint(X, Y), Model.DeltaMm(FIntPoint(X, Y)));
			}
		}
		return Out;
	}
	/** Columnas del chunk cuyo delta ha cambiado desde la instantánea. */
	int32 ChangedInChunk(const FSandModel& Model, const TMap<FIntPoint, int32>& Before, const FIntPoint& Chunk)
	{
		int32 Count = 0;
		for (const auto& Pair : Before)
		{
			Count += Model.DeltaMm(Pair.Key) != Pair.Value ? 1 : 0;
		}
		(void)Chunk;
		return Count;
	}
	/** Tick hasta que no queda nada sucio cerca del foco; devuelve los pasos (o -1 si no se asienta). */
	int32 Settle(FSandModel& Model, const FSandEnvironment& Env, TFunctionRef<double(double, double)> Base, int32 MaxTicks = 4000)
	{
		for (int32 T = 0; T < MaxTicks; ++T)
		{
			const FSandResult R = Model.Tick(Env, Base);
			if (R.ActiveColumns == 0)
			{
				return T;
			}
		}
		return -1;
	}
	/** Mayor desnivel (mm) entre vecinas 4-conectadas en el cuadrado [−R, R]² de columnas alrededor de C. */
	int32 MaxDrop(const FSandModel& Model, const FIntPoint& C, int32 R)
	{
		int32 Max = 0;
		for (int32 Y = C.Y - R; Y <= C.Y + R; ++Y)
		{
			for (int32 X = C.X - R; X <= C.X + R; ++X)
			{
				const int32 D = Model.DeltaMm(FIntPoint(X, Y));
				Max = FMath::Max(Max, FMath::Abs(D - Model.DeltaMm(FIntPoint(X + 1, Y))));
				Max = FMath::Max(Max, FMath::Abs(D - Model.DeltaMm(FIntPoint(X, Y + 1))));
			}
		}
		return Max;
	}
	FSandBrush Spike(const FVector2D& Center, int64 Mass, float Radius = 1.2f)
	{
		FSandBrush B;
		B.Center = Center;
		B.Radius = Radius;
		B.Depth = 2.0f;
		B.MassBudget = Mass;
		return B;
	}
END_DEFINE_SPEC(FSandModelSpec)

void FSandModelSpec::Define()
{
	auto Flat = [](double, double) { return 0.0; };

	Describe("números de diseño", [this]()
	{
		It("el reposo seco es 34° y el húmedo 45° en celdas de 0,25 m", [this]()
		{
			TestEqual(TEXT("seco"), FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f), 168);
			TestEqual(TEXT("húmedo"), FSandModel::ReposeDropMm(FSandModel::WetReposeDeg, 0.25f), 250);
		});

		It("el oleaje rellena el 20 % en la pleamar, más hacia el agua, y nada por encima (biblia 02 §5.2)", [this]()
		{
			// Pleamar a 0 mm y bajamar a −1000 mm.
			TestEqual(TEXT("en la pleamar"), FSandModel::RefillMilli(0, 0, -1000, false), 200);
			TestEqual(TEXT("a media franja"), FSandModel::RefillMilli(-500, 0, -1000, false), 400);
			TestEqual(TEXT("en la bajamar"), FSandModel::RefillMilli(-1000, 0, -1000, false), 600);
			TestEqual(TEXT("bajo la bajamar"), FSandModel::RefillMilli(-3000, 0, -1000, false), 600);
			TestEqual(TEXT("por encima de la pleamar"), FSandModel::RefillMilli(1, 0, -1000, false), 0);
			TestEqual(TEXT("marea viva en la pleamar: 35 %"), FSandModel::RefillMilli(0, 0, -1000, true), 350);
			TestEqual(TEXT("marea viva en la bajamar"), FSandModel::RefillMilli(-1000, 0, -1000, true), 750);
			int32 Previous = -1;
			for (int32 H = 0; H >= -1000; H -= 50)
			{
				const int32 R = FSandModel::RefillMilli(H, 0, -1000, false);
				TestTrue(TEXT("crece hacia el agua"), R > Previous);
				Previous = R;
			}
			// Marea degenerada: sin franja o con la bajamar por encima de la pleamar, sin dividir por cero.
			TestEqual(TEXT("pleamar = bajamar"), FSandModel::RefillMilli(0, 0, 0, false), 600);
			TestEqual(TEXT("bajamar sobre la pleamar"), FSandModel::RefillMilli(-10, 0, 500, false), 600);
		});

		It("masa y m³ van y vuelven", [this]()
		{
			const FSandModel Model;
			TestEqual(TEXT("1 m³"), Model.CubicMetersToMass(1.0), static_cast<int64>(16000));
			TestEqual(TEXT("ida y vuelta"), Model.MassToCubicMeters(Model.CubicMetersToMass(0.37)), 0.37, 1.0e-9);
		});
	});

	Describe("masa", [this, Flat]()
	{
		It("cavar y volver a echar lo mismo deja la masa en cero", [this, Flat]()
		{
			FSandModel Model;
			FSandBrush B;
			B.Center = FVector2D(1.1, -0.7);
			const FSandResult Dug = Model.Dig(B, Flat);
			TestTrue(TEXT("ha cavado"), Dug.Mass > 0);
			TestEqual(TEXT("Σ delta = −cavado"), Model.TotalMass(), -Dug.Mass);
			B.MassBudget = Dug.Mass;
			const FSandResult Piled = Model.Pile(B, Flat);
			TestEqual(TEXT("echa todo lo del cubo"), Piled.Mass, Dug.Mass);
			TestEqual(TEXT("masa neta cero"), Model.TotalMass(), static_cast<int64>(0));
		});

		It("la avalancha no crea ni destruye arena, ni siquiera cruzando chunks en negativo", [this, Flat]()
		{
			FSandModel Model;
			const FSandResult Piled = Model.Pile(Spike(FVector2D(-0.1, 0.05), 40000), Flat);
			TestEqual(TEXT("apila todo"), Piled.Mass, static_cast<int64>(40000));
			const FSandEnvironment Env = Dry();
			for (int32 T = 0; T < 300; ++T)
			{
				Model.Tick(Env, Flat);
				if (!TestEqual(TEXT("masa constante"), Model.TotalMass(), static_cast<int64>(40000)))
				{
					break;
				}
			}
			TestTrue(TEXT("se ha repartido por varios chunks"), Model.EditedChunks().Num() >= 4);
		});

		It("el oleaje tampoco: lo que rellena o alisa sale del banco del mar o vuelve a él", [this, Flat]()
		{
			FSandModel Model;
			FSandBrush B;
			B.Radius = 0.8f;
			B.Depth = 0.3f;
			const int64 Dug = Model.Dig(B, Flat).Mass;
			B.Center = FVector2D(3.0, 0.0);
			B.MassBudget = 9000;
			const int64 Piled = Model.Pile(B, Flat).Mass;
			for (int32 Half = 0; Half < 8; ++Half)
			{
				const FSandResult R = Model.ApplyHalfTide(Tide(0.5, -0.5, Half % 4 == 0), Flat);
				TestEqual(TEXT("Σ delta + banco constante"), Model.TotalMass() + Model.SeaBankMass(), Piled - Dug);
				if (Half == 0)
				{
					TestTrue(TEXT("el primer medio ciclo mueve arena"), R.Changed());
				}
			}
			TestEqual(TEXT("al final el mar lo ha devuelto todo a su sitio"), Model.TotalMass(), static_cast<int64>(0));
			TestEqual(TEXT("y se ha quedado la diferencia"), Model.SeaBankMass(), Piled - Dug);
		});

		It("no se cava más allá de la capa de arena ni la avalancha la atraviesa", [this, Flat]()
		{
			FSandModel Model;
			FSandBrush B;
			B.Radius = 0.3f;
			B.Depth = 1.0f;
			for (int32 I = 0; I < 5; ++I)
			{
				Model.Dig(B, Flat);
			}
			TestEqual(TEXT("fondo en la roca"), Model.DeltaMm(FIntPoint(0, 0)), -FSandModel::MaxDigDepthMm);
			Model.Dig(B, Flat);
			TestEqual(TEXT("el centro ya no baja"), Model.DeltaMm(FIntPoint(0, 0)), -FSandModel::MaxDigDepthMm);
			Settle(Model, Dry(), Flat);
			for (int32 Y = -12; Y <= 12; ++Y)
			{
				for (int32 X = -12; X <= 12; ++X)
				{
					TestTrue(TEXT("sin atravesar la roca"), Model.DeltaMm(FIntPoint(X, Y)) >= -FSandModel::MaxDigDepthMm);
				}
			}
		});

		It("el presupuesto de la pasada se respeta y apilar sin arena no hace nada", [this, Flat]()
		{
			FSandModel Model;
			FSandBrush B;
			B.Radius = 1.0f;
			B.MassBudget = 777;
			TestTrue(TEXT("cavar con tope"), Model.Dig(B, Flat).Mass <= 777);
			const int64 Before = Model.TotalMass();
			B.MassBudget = 0;
			const FSandResult R = Model.Pile(B, Flat);
			TestFalse(TEXT("nada"), R.Changed());
			TestEqual(TEXT("masa igual"), Model.TotalMass(), Before);
		});
	});

	Describe("avalancha", [this, Flat]()
	{
		It("un terreno sin tocar no se mueve aunque sea más empinado que el reposo", [this]()
		{
			// Duna natural a 50°: más que el reposo seco, pero es la base, no arena movida.
			auto Dune = [](double X, double) { return X * FMath::Tan(FMath::DegreesToRadians(50.0)); };
			FSandModel Model;
			FSandBrush B;
			B.Center = FVector2D(3.0, 0.0);
			B.Depth = 0.01f;
			B.MassBudget = 50;
			Model.Pile(B, Dune);
			Settle(Model, Dry(), Dune);
			for (int32 X = -4; X <= 4; ++X)
			{
				TestEqual(TEXT("la ladera base sigue igual"), Model.DeltaMm(FIntPoint(X, 0)), 0);
			}
		});

		It("un montón seco se derrumba hasta el ángulo de reposo de 34°", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D::ZeroVector, 40000), Flat);
			TestTrue(TEXT("se asienta"), Settle(Model, Dry(), Flat) >= 0);
			const int32 Drop = MaxDrop(Model, FIntPoint(0, 0), 20);
			TestTrue(TEXT("no más empinado que 34°"), Drop <= FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
			TestTrue(TEXT("pero no aplanado del todo"), Drop >= FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f) / 2);
		});

		It("la arena húmeda aguanta más pendiente y, al secarse, se vuelve a derrumbar", [this, Flat]()
		{
			FSandModel Wet;
			FSandModel DryModel;
			FSandEnvironment Rain = Dry();
			Rain.bRaining = true;
			Wet.Pile(Spike(FVector2D::ZeroVector, 40000), Flat);
			DryModel.Pile(Spike(FVector2D::ZeroVector, 40000), Flat);
			TestTrue(TEXT("húmedo se asienta"), Settle(Wet, Rain, Flat) >= 0);
			TestTrue(TEXT("seco se asienta"), Settle(DryModel, Dry(), Flat) >= 0);
			const int32 WetDrop = MaxDrop(Wet, FIntPoint(0, 0), 20);
			TestTrue(TEXT("húmedo hasta 45°"), WetDrop <= FSandModel::ReposeDropMm(FSandModel::WetReposeDeg, 0.25f));
			TestTrue(TEXT("húmedo más empinado que seco"), WetDrop > FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
			TestTrue(TEXT("cima húmeda más alta"), Wet.DeltaMm(FIntPoint(0, 0)) > DryModel.DeltaMm(FIntPoint(0, 0)));

			// Deja de llover: nadie ha tocado el montón, pero tiene que volver a moverse.
			TestEqual(TEXT("dormido"), Wet.NumDirtyColumns(), 0);
			TestTrue(TEXT("se vuelve a asentar"), Settle(Wet, Dry(), Flat) > 0);
			TestTrue(TEXT("ahora a 34°"), MaxDrop(Wet, FIntPoint(0, 0), 20) <= FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
		});

		It("la franja bajo la pleamar es húmeda y, si la pleamar del día baja, la arena se seca y se revisa", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D::ZeroVector, 40000), Flat);
			FSandEnvironment Spring = Dry();
			Spring.HighTide = 5.0;
			TestTrue(TEXT("se asienta húmedo"), Settle(Model, Spring, Flat) >= 0);
			TestTrue(TEXT("a más de 34°"), MaxDrop(Model, FIntPoint(0, 0), 20) > FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
			TestEqual(TEXT("dormido"), Model.NumDirtyColumns(), 0);
			const FSandResult R = Model.Tick(Dry(), Flat);
			TestTrue(TEXT("despierta"), R.ActiveColumns > 0);
			Settle(Model, Dry(), Flat);
			TestTrue(TEXT("ahora a 34°"), MaxDrop(Model, FIntPoint(0, 0), 20) <= FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
		});

		It("la arena que se seca lejos de todos los jugadores se derrumba a 34° cuando llega alguien", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D::ZeroVector, 30000), Flat);
			FSandEnvironment Wet = Dry();
			Wet.HighTide = 1.0;
			TestTrue(TEXT("se asienta húmedo"), Settle(Model, Wet, Flat) >= 0);
			TestTrue(TEXT("a más de 34°"), MaxDrop(Model, FIntPoint(0, 0), 20) > FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
			// La pleamar baja mientras el jugador está a 500 m: la arena se congela, pero seca.
			FSandEnvironment DryFar = Dry();
			DryFar.Focus = FVector2D(500.0, 0.0);
			for (int32 T = 0; T < 10; ++T)
			{
				Model.Tick(DryFar, Flat);
			}
			TestTrue(TEXT("congelada mientras nadie mira"), MaxDrop(Model, FIntPoint(0, 0), 20) > FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
			const FSandResult R = Model.Tick(Dry(), Flat);
			TestTrue(TEXT("despierta al volver"), R.ActiveColumns > 0);
			Settle(Model, Dry(), Flat);
			TestTrue(TEXT("ahora a 34°"), MaxDrop(Model, FIntPoint(0, 0), 20) <= FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
		});

		It("un agujero de paredes verticales se derrumba y el borde cae dentro", [this, Flat]()
		{
			FSandModel Model;
			FSandBrush B;
			B.Radius = 0.3f;
			B.Depth = 1.2f;
			Model.Dig(B, Flat);
			const int32 Before = Model.DeltaMm(FIntPoint(0, 0));
			Settle(Model, Dry(), Flat);
			TestTrue(TEXT("el fondo sube"), Model.DeltaMm(FIntPoint(0, 0)) > Before);
			TestTrue(TEXT("el borde baja"), Model.DeltaMm(FIntPoint(2, 0)) < 0);
		});
	});

	Describe("bordes de chunk y determinismo", [this, Flat]()
	{
		It("un montón en la esquina de cuatro chunks es simétrico y igual que uno en el centro de un chunk", [this, Flat]()
		{
			// Columna (0, 0): esquina de los chunks (−1,−1), (0,−1), (−1,0) y (0,0).
			// Columna (16, 16): centro del chunk (0, 0). La física no puede notar la diferencia.
			// Montón pequeño para que no entre el tope de red por chunk, que sí distingue
			// (a propósito) un montón repartido entre cuatro chunks de uno dentro de uno solo.
			FSandModel Corner;
			FSandModel Inside;
			// Sin tope de masa: el reparto del resto de un tope rompería la simetría del pincel, no de la física.
			Corner.Pile(Spike(FVector2D(0.0, 0.0), MAX_int32, 0.5f), Flat);
			Inside.Pile(Spike(FVector2D(4.0, 4.0), MAX_int32, 0.5f), Flat);
			FSandEnvironment EnvCorner = Dry();
			FSandEnvironment EnvInside = Dry();
			EnvInside.Focus = FVector2D(4.0, 4.0);
			int32 Deferred = 0;
			for (int32 T = 0; T < 400; ++T)
			{
				const FSandResult A = Corner.Tick(EnvCorner, Flat);
				const FSandResult B = Inside.Tick(EnvInside, Flat);
				Deferred += A.DeferredColumns + B.DeferredColumns;
				if (A.ActiveColumns == 0 && B.ActiveColumns == 0)
				{
					break;
				}
			}
			TestEqual(TEXT("sin tope de red"), Deferred, 0);
			TestEqual(TEXT("asentados"), Corner.NumDirtyColumns() + Inside.NumDirtyColumns(), 0);
			bool bSymmetric = true;
			bool bTranslated = true;
			for (int32 Y = -14; Y <= 14; ++Y)
			{
				for (int32 X = -14; X <= 14; ++X)
				{
					const int32 D = Corner.DeltaMm(FIntPoint(X, Y));
					bSymmetric &= D == Corner.DeltaMm(FIntPoint(-X, Y)) && D == Corner.DeltaMm(FIntPoint(X, -Y)) && D == Corner.DeltaMm(FIntPoint(Y, X));
					bTranslated &= D == Inside.DeltaMm(FIntPoint(X + 16, Y + 16));
				}
			}
			TestTrue(TEXT("simétrico respecto a la esquina"), bSymmetric);
			TestTrue(TEXT("igual que en el centro del chunk"), bTranslated);
			TestTrue(TEXT("se ha derrumbado"), Corner.DeltaMm(FIntPoint(0, 0)) < 2000);
			TestEqual(TEXT("masa"), Corner.TotalMass(), Inside.TotalMass());
		});

		It("marca sucios los chunks que leen la columna: 1 dentro, 2 en un borde, 4 en una esquina", [this]()
		{
			const FSandModel Model;
			TArray<FIntPoint> Out;
			Model.ChunksReadingColumn(FIntPoint(16, 16), Out);
			TestEqual(TEXT("dentro"), Out.Num(), 1);
			Out.Reset();
			Model.ChunksReadingColumn(FIntPoint(0, 16), Out);
			TestEqual(TEXT("borde"), Out.Num(), 2);
			TestTrue(TEXT("incluye el chunk negativo"), Out.Contains(FIntPoint(-1, 0)));
			Out.Reset();
			Model.ChunksReadingColumn(FIntPoint(-1, -1), Out);
			TestEqual(TEXT("esquina en negativo"), Out.Num(), 4);
			TestTrue(TEXT("incluye el (0, 0), que la lee como margen"), Out.Contains(FIntPoint(0, 0)));
			Out.Reset();
			Model.ChunksReadingColumn(FIntPoint(33, 2), Out);
			TestTrue(TEXT("columna 33 la lee el chunk 0 como margen"), Out.Contains(FIntPoint(0, 0)) && Out.Contains(FIntPoint(1, 0)));
		});

		It("la edición en el borde devuelve los chunks de ambos lados", [this, Flat]()
		{
			FSandModel Model;
			FSandBrush B;
			B.Center = FVector2D(8.0, 2.0);
			B.Radius = 0.4f;
			const FSandResult R = Model.Dig(B, Flat);
			TestTrue(TEXT("chunk 0"), R.DirtyChunks.Contains(FIntPoint(0, 0)));
			TestTrue(TEXT("chunk 1"), R.DirtyChunks.Contains(FIntPoint(1, 0)));
			for (int32 I = 1; I < R.DirtyChunks.Num(); ++I)
			{
				const FIntPoint& A = R.DirtyChunks[I - 1];
				const FIntPoint& C = R.DirtyChunks[I];
				TestTrue(TEXT("ordenados y sin repetir"), A.Y < C.Y || (A.Y == C.Y && A.X < C.X));
			}
		});

		It("una revisión por segundo, troceada como sea", [this, Flat]()
		{
			FSandModel A;
			FSandModel B;
			A.Pile(Spike(FVector2D(0.3, 0.2), 40000), Flat);
			B.Pile(Spike(FVector2D(0.3, 0.2), 40000), Flat);
			FSandEnvironment Env;
			Env.HighTide = 0.3;
			for (int32 S = 0; S < 5; ++S)
			{
				A.Advance(1000, Env, Flat);
				for (int32 I = 0; I < 10; ++I)
				{
					B.Advance(100, Env, Flat);
				}
			}
			TestTrue(TEXT("iguales"), A == B);
			// Y 370 ms no revisan nada hasta completar el segundo.
			FSandModel C;
			C.Pile(Spike(FVector2D::ZeroVector, 1000), Flat);
			TestEqual(TEXT("sin revisión"), C.Advance(370, Env, Flat).Ticks, 0);
			TestEqual(TEXT("con revisión"), C.Advance(630, Env, Flat).Ticks, 1);
		});

		It("un salto de tiempo se resuelve con 4 revisiones como máximo y el resto se descarta (08 §2.6)", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D::ZeroVector, 30000), Flat);
			TestEqual(TEXT("tope"), Model.Advance(MAX_int32 / 2, Dry(), Flat).Ticks, FSandModel::MaxTicksPerAdvance);
			TestEqual(TEXT("sin deuda acumulada"), Model.Advance(0, Dry(), Flat).Ticks, 0);
			TestEqual(TEXT("negativo"), Model.Advance(-500, Dry(), Flat).Ticks, 0);
			TestEqual(TEXT("sin desbordar"), Model.Advance(MAX_int32, Dry(), Flat).Ticks, FSandModel::MaxTicksPerAdvance);
		});

		It("pinceles degenerados no hacen nada", [this, Flat]()
		{
			FSandModel Model;
			FSandBrush B;
			B.Radius = 0.0f;
			TestFalse(TEXT("radio 0"), Model.Dig(B, Flat).Changed());
			B.Radius = 1.0f;
			B.Depth = -1.0f;
			TestFalse(TEXT("profundidad negativa"), Model.Dig(B, Flat).Changed());
			B.Depth = 0.1f;
			B.MassBudget = -5;
			TestFalse(TEXT("presupuesto negativo"), Model.Dig(B, Flat).Changed());
			TestTrue(TEXT("vacío"), Model.IsEmpty());
		});
	});

	Describe("relleno por oleaje (biblia 02 §5.2)", [this, Flat]()
	{
		// Un hoyo de una sola columna: pincel de 0,2 m, que no llega a las vecinas a 0,25 m.
		auto OneHole = [Flat](FSandModel& Model, const FVector2D& At, float Depth)
		{
			FSandBrush B;
			B.Center = At;
			B.Radius = 0.2f;
			B.Depth = Depth;
			Model.Dig(B, Flat);
		};

		It("en la pleamar devuelve un 20 % por medio ciclo, y un 35 % en marea viva", [this, Flat, OneHole]()
		{
			FSandModel Normal;
			FSandModel Spring;
			OneHole(Normal, FVector2D::ZeroVector, 1.0f);
			OneHole(Spring, FVector2D::ZeroVector, 1.0f);
			TestEqual(TEXT("hoyo de 1 m"), Normal.DeltaMm(FIntPoint(0, 0)), -1000);
			const FSandResult R = Normal.ApplyHalfTide(Tide(0.0, -1.0), Flat);
			Spring.ApplyHalfTide(Tide(0.0, -1.0, true), Flat);
			TestEqual(TEXT("20 %"), Normal.DeltaMm(FIntPoint(0, 0)), -800);
			TestEqual(TEXT("35 %"), Spring.DeltaMm(FIntPoint(0, 0)), -650);
			TestEqual(TEXT("el mar ha traído 200"), R.SeaMass, static_cast<int64>(200));
			TestEqual(TEXT("una columna"), R.ColumnsChanged, 1);
			TestTrue(TEXT("remalla su chunk"), R.DirtyChunks.Contains(FIntPoint(0, 0)));
			TestTrue(TEXT("y deja la columna para la revisión de pendiente"), Normal.IsDirty(FIntPoint(0, 0)));
		});

		It("rellena más cuanto más cerca del agua y nada por encima de la pleamar", [this, Flat, OneHole]()
		{
			FSandModel Above;
			FSandModel Middle;
			FSandModel Low;
			OneHole(Above, FVector2D::ZeroVector, 1.0f);
			OneHole(Middle, FVector2D::ZeroVector, 1.0f);
			OneHole(Low, FVector2D::ZeroVector, 1.0f);
			// Misma playa (base a 0 m) con tres mareas distintas.
			TestFalse(TEXT("sobre la pleamar"), Above.ApplyHalfTide(Tide(-0.01, -1.0), Flat).Changed());
			Middle.ApplyHalfTide(Tide(1.0, -1.0), Flat);
			Low.ApplyHalfTide(Tide(1.0, 0.0), Flat);
			TestEqual(TEXT("seco: igual"), Above.DeltaMm(FIntPoint(0, 0)), -1000);
			TestEqual(TEXT("a media franja: 40 %"), Middle.DeltaMm(FIntPoint(0, 0)), -600);
			TestEqual(TEXT("en la bajamar: 60 %"), Low.DeltaMm(FIntPoint(0, 0)), -400);
		});

		It("un hoyo bajo la bajamar se cierra del todo en 2–3 ciclos, no de golpe", [this, Flat, OneHole]()
		{
			for (const float Depth : { 0.3f, 1.0f, 1.5f })
			{
				FSandModel Model;
				OneHole(Model, FVector2D::ZeroVector, Depth);
				int32 Halves = 0;
				while (Model.DeltaMm(FIntPoint(0, 0)) != 0 && Halves < 20)
				{
					Model.ApplyHalfTide(Tide(1.0, 0.5), Flat);
					++Halves;
				}
				TestTrue(TEXT("cerrado en ≤ 3 ciclos (6 medios)"), Halves <= 6);
				TestTrue(TEXT("pero no en el primer medio ciclo"), Halves >= 2);
				TestTrue(TEXT("sin rastro"), Model.EditedChunks().Num() == 0);
			}
			// La capa entera (1,5 m) tarda de 2 a 3 ciclos, como dice la biblia.
			FSandModel Deep;
			OneHole(Deep, FVector2D::ZeroVector, 1.5f);
			for (int32 Half = 0; Half < 4; ++Half)
			{
				Deep.ApplyHalfTide(Tide(1.0, 0.5), Flat);
			}
			TestTrue(TEXT("tras 2 ciclos aún queda algo"), Deep.DeltaMm(FIntPoint(0, 0)) < 0);
			Deep.ApplyHalfTide(Tide(1.0, 0.5), Flat);
			Deep.ApplyHalfTide(Tide(1.0, 0.5), Flat);
			TestEqual(TEXT("tras 3, nada"), Deep.DeltaMm(FIntPoint(0, 0)), 0);
		});

		It("alisa un montón hacia la altura original y la arena se la lleva el mar", [this, Flat]()
		{
			FSandBrush B;
			B.Radius = 0.8f;
			B.Depth = 0.3f;
			B.MassBudget = 300000;
			FSandModel Model;
			const int64 Piled = Model.Pile(B, Flat).Mass;
			const int32 Start = Model.DeltaMm(FIntPoint(0, 0));
			const FSandResult R = Model.ApplyHalfTide(Tide(0.5, -0.5), Flat);
			TestTrue(TEXT("más bajo"), Model.DeltaMm(FIntPoint(0, 0)) < Start);
			TestTrue(TEXT("pero sigue ahí"), Model.DeltaMm(FIntPoint(0, 0)) > 0);
			TestTrue(TEXT("el mar se lleva arena"), R.SeaMass < 0);
			TestEqual(TEXT("al banco"), Model.SeaBankMass(), -R.SeaMass);
			TestEqual(TEXT("masa"), Model.TotalMass() + Model.SeaBankMass(), Piled);
		});

		It("no es continuo: entre medio ciclo y medio ciclo la revisión de pendiente no rellena nada", [this, Flat]()
		{
			// Hoyo de paredes suaves (por debajo del reposo) en plena franja: sin oleaje continuo, no se mueve.
			FSandModel Model;
			FSandBrush B;
			B.Radius = 0.8f;
			B.Depth = 0.3f;
			Model.Dig(B, Flat);
			const int32 Start = Model.DeltaMm(FIntPoint(0, 0));
			FSandEnvironment Env;
			Env.HighTide = 1.0;
			for (int32 T = 0; T < 600; ++T)
			{
				Model.Tick(Env, Flat);
			}
			TestEqual(TEXT("10 minutos después, igual"), Model.DeltaMm(FIntPoint(0, 0)), Start);
			TestEqual(TEXT("dormido"), Model.NumDirtyColumns(), 0);
		});

		It("llega a toda la isla: también a la arena lejos de los jugadores", [this, Flat, OneHole]()
		{
			FSandModel Model;
			OneHole(Model, FVector2D(500.0, -300.0), 0.5f);
			const FIntPoint Far = Model.ColumnOf(500.0, -300.0);
			TestTrue(TEXT("rellena"), Model.ApplyHalfTide(Tide(0.5, -0.5), Flat).Changed());
			TestTrue(TEXT("sube"), Model.DeltaMm(Far) > -500);
		});

		It("mareas no finitas no hacen nada", [this, Flat, OneHole]()
		{
			FSandModel Model;
			OneHole(Model, FVector2D::ZeroVector, 0.5f);
			TestFalse(TEXT("NaN"), Model.ApplyHalfTide(Tide(NAN, 0.0), Flat).Changed());
			TestFalse(TEXT("infinito"), Model.ApplyHalfTide(Tide(0.0, -INFINITY), Flat).Changed());
			TestEqual(TEXT("igual"), Model.DeltaMm(FIntPoint(0, 0)), -500);
		});
	});

	Describe("estructuras (biblia 02 §5.3)", [this, Flat]()
	{
		It("sujetan la arena a menos de 1 m de su huella, con las esquinas redondas", [this, Flat]()
		{
			FSandModel Model;
			// Un tablón de 1 m a lo largo del eje Y: huella en x ∈ [0,5; 0,75].
			Model.SetAnchor(FVector2D(0.5, -0.5), FVector2D(0.75, 0.5), true, Flat);
			TestTrue(TEXT("huella"), Model.IsAnchored(FIntPoint(2, 0)) && Model.IsAnchored(FIntPoint(3, 0)));
			TestFalse(TEXT("fuera de la huella"), Model.IsAnchored(FIntPoint(4, 0)));
			TestTrue(TEXT("la huella está sujeta"), Model.IsHeld(FIntPoint(2, 0)));
			TestTrue(TEXT("a 1 m justo, sujeta"), Model.IsHeld(FIntPoint(-2, 0)));
			TestFalse(TEXT("a 1,25 m, suelta"), Model.IsHeld(FIntPoint(-3, 0)));
			TestTrue(TEXT("al otro lado, a 1 m"), Model.IsHeld(FIntPoint(7, 0)));
			TestFalse(TEXT("al otro lado, a 1,25 m"), Model.IsHeld(FIntPoint(8, 0)));
			TestTrue(TEXT("junto a la esquina, a 0,79 m"), Model.IsHeld(FIntPoint(-1, -3)));
			TestFalse(TEXT("en diagonal de la esquina, a 1,12 m"), Model.IsHeld(FIntPoint(-2, -4)));
		});

		It("la arena sujeta no desliza y, al quitar la estructura, se derrumba a 34°", [this, Flat]()
		{
			FSandModel Free;
			FSandModel Held;
			Held.SetAnchor(FVector2D(0.5, -0.5), FVector2D(0.75, 0.5), true, Flat);
			// Montón pegado al tablón, entero dentro de la zona sujeta.
			Free.Pile(Spike(FVector2D::ZeroVector, 5000, 0.5f), Flat);
			Held.Pile(Spike(FVector2D::ZeroVector, 5000, 0.5f), Flat);
			const int32 Peak = Held.DeltaMm(FIntPoint(0, 0));
			TestTrue(TEXT("más empinado que el reposo"), Peak - Held.DeltaMm(FIntPoint(-1, 0)) > FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
			Settle(Free, Dry(), Flat);
			Settle(Held, Dry(), Flat);
			TestEqual(TEXT("sujeto: la cima sigue igual"), Held.DeltaMm(FIntPoint(0, 0)), Peak);
			TestTrue(TEXT("suelto: se ha derrumbado"), Free.DeltaMm(FIntPoint(0, 0)) < Peak);
			TestEqual(TEXT("masa"), Held.TotalMass(), static_cast<int64>(5000));

			Held.SetAnchor(FVector2D(0.5, -0.5), FVector2D(0.75, 0.5), false, Flat);
			TestTrue(TEXT("despierta"), Held.NumDirtyColumns() > 0);
			Settle(Held, Dry(), Flat);
			TestTrue(TEXT("vuelve a 34°"), MaxDrop(Held, FIntPoint(0, 0), 20) <= FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
			TestEqual(TEXT("masa"), Held.TotalMass(), static_cast<int64>(5000));
		});

		It("la arena de fuera sí puede caer contra la estructura", [this, Flat]()
		{
			FSandModel Model;
			Model.SetAnchor(FVector2D(0.5, -0.5), FVector2D(0.75, 0.5), true, Flat);
			// Montón justo fuera de la zona (x = −1,75 m), que se derrumba hacia el tablón.
			Model.Pile(Spike(FVector2D(-1.75, 0.0), 40000), Flat);
			const int32 Before = Model.DeltaMm(FIntPoint(-2, 0));
			Settle(Model, Dry(), Flat);
			TestTrue(TEXT("la arena sujeta ha recibido"), Model.DeltaMm(FIntPoint(-2, 0)) > Before);
			TestEqual(TEXT("masa"), Model.TotalMass(), static_cast<int64>(40000));
		});

		It("el oleaje no rellena la arena sujeta y sí la de más allá de 1 m", [this, Flat]()
		{
			FSandModel Model;
			// Pilote de 0,2 m en el origen.
			Model.SetAnchor(FVector2D(-0.1, -0.1), FVector2D(0.1, 0.1), true, Flat);
			FSandBrush B;
			B.Radius = 0.2f;
			B.Depth = 0.5f;
			B.Center = FVector2D(0.75, 0.0);
			Model.Dig(B, Flat);
			B.Center = FVector2D(2.0, 0.0);
			Model.Dig(B, Flat);
			Model.ApplyHalfTide(Tide(1.0, -1.0), Flat);
			TestEqual(TEXT("a 0,65 m: sujeto"), Model.DeltaMm(FIntPoint(3, 0)), -500);
			TestTrue(TEXT("a 1,9 m: se rellena"), Model.DeltaMm(FIntPoint(8, 0)) > -500);
		});

		It("la pala no cava bajo la huella, pero sí al lado", [this, Flat]()
		{
			FSandModel Model;
			Model.SetAnchor(FVector2D(-0.25, -0.25), FVector2D(0.25, 0.25), true, Flat);
			FSandBrush B;
			B.Radius = 1.0f;
			Model.Dig(B, Flat);
			TestEqual(TEXT("pilote intacto"), Model.DeltaMm(FIntPoint(0, 0)), 0);
			TestTrue(TEXT("alrededor sí"), Model.DeltaMm(FIntPoint(2, 0)) < 0);
		});

		It("las referencias se cuentan: dos estructuras que se solapan", [this, Flat]()
		{
			FSandModel Model;
			Model.SetAnchor(FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), true, Flat);
			Model.SetAnchor(FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), true, Flat);
			Model.SetAnchor(FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), false, Flat);
			TestTrue(TEXT("sigue anclada"), Model.IsAnchored(FIntPoint(0, 0)));
			Model.SetAnchor(FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), false, Flat);
			TestFalse(TEXT("suelta"), Model.IsAnchored(FIntPoint(0, 0)));
			TestFalse(TEXT("y sin sujeción"), Model.IsHeld(FIntPoint(0, 0)));

			// Dos pilotes a 1,5 m: la arena del medio la sujetan los dos.
			Model.SetAnchor(FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), true, Flat);
			Model.SetAnchor(FVector2D(1.5, 0.0), FVector2D(1.5, 0.0), true, Flat);
			Model.SetAnchor(FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), false, Flat);
			TestTrue(TEXT("el medio sigue sujeto por el otro"), Model.IsHeld(FIntPoint(3, 0)));
			TestFalse(TEXT("el lado lejano ya no"), Model.IsHeld(FIntPoint(-3, 0)));
			// Quitar lo que no está puesto no deja contadores negativos.
			Model.SetAnchor(FVector2D(-5.0, -5.0), FVector2D(-5.0, -5.0), false, Flat);
			Model.SetAnchor(FVector2D(1.5, 0.0), FVector2D(1.5, 0.0), false, Flat);
			TestFalse(TEXT("todo suelto"), Model.IsHeld(FIntPoint(3, 0)) || Model.IsHeld(FIntPoint(6, 0)));
		});

		It("la zona sujeta cruza el borde de chunk y las cajas no válidas no hacen nada", [this, Flat]()
		{
			FSandModel Model;
			// Pilote a 0,1 m del borde entre los chunks 0 y 1 (x = 8 m).
			Model.SetAnchor(FVector2D(7.9, 0.0), FVector2D(7.9, 0.0), true, Flat);
			TestEqual(TEXT("chunk vecino"), Model.ChunkOfColumn(FIntPoint(35, 0)).X, 1);
			TestTrue(TEXT("a 0,85 m en el chunk 1: sujeta"), Model.IsHeld(FIntPoint(35, 0)));
			TestFalse(TEXT("a 1,1 m: suelta"), Model.IsHeld(FIntPoint(36, 0)));
			// En negativo: el chunk −1 empieza en x = −0,25.
			Model.SetAnchor(FVector2D(0.1, 0.0), FVector2D(0.1, 0.0), true, Flat);
			TestTrue(TEXT("a 0,85 m en el chunk −1: sujeta"), Model.IsHeld(FIntPoint(-3, 0)));
			TestFalse(TEXT("a 1,1 m: suelta"), Model.IsHeld(FIntPoint(-4, 0)));

			FSandModel Empty;
			TestFalse(TEXT("caja al revés"), Empty.SetAnchor(FVector2D(1.0, 1.0), FVector2D(0.0, 0.0), true, Flat).Changed());
			TestFalse(TEXT("NaN"), Empty.SetAnchor(FVector2D(NAN, 0.0), FVector2D(1.0, 1.0), true, Flat).Changed());
			TestFalse(TEXT("nada sujeto"), Empty.IsHeld(FIntPoint(0, 0)));
		});
	});

	Describe("coste y red: solo chunks cerca de algún jugador (biblia 08 §2.6)", [this, Flat]()
	{
		It("la distancia a un chunk se mide a su borde, también en negativo", [this]()
		{
			const FSandModel Model;
			TestEqual(TEXT("dentro"), Model.DistanceToChunk(FVector2D(1.0, 1.0), FIntPoint(0, 0)), 0.0);
			TestEqual(TEXT("chunk 10: empieza a 80 m"), Model.DistanceToChunk(FVector2D::ZeroVector, FIntPoint(10, 0)), 80.0);
			TestEqual(TEXT("chunk 11: 88 m"), Model.DistanceToChunk(FVector2D::ZeroVector, FIntPoint(11, 0)), 88.0);
			TestEqual(TEXT("chunk −1: acaba en −0,25"), Model.DistanceToChunk(FVector2D::ZeroVector, FIntPoint(-1, 0)), 0.25);
		});

		It("un montón a más de 80 m queda congelado e intacto hasta que llega un jugador", [this, Flat]()
		{
			FSandModel Model;
			// Chunk 10 (a 80 m justos del jugador): activo. Chunk 11 (a 88 m): congelado.
			Model.Pile(Spike(FVector2D(84.0, 4.0), 20000), Flat);
			Model.Pile(Spike(FVector2D(92.0, 4.0), 20000), Flat);
			const FIntPoint Near = Model.ColumnOf(84.0, 4.0);
			const FIntPoint Far = Model.ColumnOf(92.0, 4.0);
			const int32 FarPeak = Model.DeltaMm(Far);
			const int32 NearPeak = Model.DeltaMm(Near);
			FSandEnvironment Env = Dry();
			int32 MaxChunks = 0;
			for (int32 T = 0; T < 400; ++T)
			{
				const FSandResult R = Model.Tick(Env, Flat);
				MaxChunks = FMath::Max(MaxChunks, R.ActiveChunks);
				if (R.ActiveColumns == 0)
				{
					TestTrue(TEXT("lo lejano sigue pendiente"), R.DormantColumns > 0);
					break;
				}
			}
			TestTrue(TEXT("el chunk a 80 m se ha asentado"), Model.DeltaMm(Near) < NearPeak && MaxDrop(Model, Near, 20) <= 168);
			TestEqual(TEXT("el de 88 m no se ha tocado"), Model.DeltaMm(Far), FarPeak);
			TestTrue(TEXT("y sigue sucio"), Model.IsDirty(Far));
			TestTrue(TEXT("solo se revisan los chunks del montón cercano"), MaxChunks <= 2);

			Env.Focus = FVector2D(92.0, 4.0);
			TestTrue(TEXT("se asienta al llegar"), Settle(Model, Env, Flat) > 0);
			TestEqual(TEXT("nada sucio"), Model.NumDirtyColumns(), 0);
			TestTrue(TEXT("el lejano ya a 34°"), MaxDrop(Model, Far, 20) <= 168);
		});

		It("al volver, un chunk congelado recupera de golpe las revisiones que se saltó, como mucho 4", [this, Flat]()
		{
			// Montón en el centro de un chunk a 200 m: todo lo que se mueve queda en ese chunk.
			const FVector2D At(204.0, 4.0);
			FSandEnvironment Far = Dry();
			FSandEnvironment Near = Dry();
			Near.Focus = FVector2D(203.0, 4.0);
			for (const int32 Missed : { 2, 4, 10 })
			{
				FSandModel Frozen;
				FSandModel Fresh;
				Frozen.Pile(Spike(At, 40000), Flat);
				Fresh.Pile(Spike(At, 40000), Flat);
				const int32 Start = Frozen.DeltaMm(Frozen.ColumnOf(At.X, At.Y));
				for (int32 T = 0; T < Missed; ++T)
				{
					const FSandResult R = Frozen.Tick(Far, Flat);
					TestEqual(TEXT("congelado: no revisa"), R.ActiveColumns, 0);
					TestTrue(TEXT("y lo cuenta como pendiente"), R.DormantColumns > 0);
				}
				TestEqual(TEXT("intacto"), Frozen.DeltaMm(Frozen.ColumnOf(At.X, At.Y)), Start);
				const int32 Expected = FMath::Min(Missed, FSandModel::MaxCatchUpRevisions);
				const FSandResult Burst = Frozen.Tick(Near, Flat);
				TestEqual(TEXT("recupera las que se saltó, como mucho 4"), Burst.CatchUpRevisions, Expected);
				for (int32 T = 0; T <= Expected; ++T)
				{
					Fresh.Tick(Near, Flat);
				}
				TestTrue(TEXT("igual que si hubiera revisado 1 + esas"), Frozen == Fresh);
				TestTrue(TEXT("pero no se asienta de golpe"), Frozen.NumDirtyColumns() > 0);
				TestEqual(TEXT("la ráfaga no se repite"), Frozen.Tick(Near, Flat).CatchUpRevisions, 0);
			}
		});

		It("la ráfaga solo es para los chunks que vuelven: los que ya estaban cerca no corren más", [this, Flat]()
		{
			FSandModel Model;
			FSandModel Reference;
			// Uno cerca desde el principio (centro del chunk (0, 0)) y otro lejos (centro del (25, 0)).
			Model.Pile(Spike(FVector2D(4.0, 4.0), 40000), Flat);
			Model.Pile(Spike(FVector2D(204.0, 4.0), 40000), Flat);
			Reference.Pile(Spike(FVector2D(4.0, 4.0), 40000), Flat);
			FSandEnvironment Env = Dry();
			Env.Focus = FVector2D(4.0, 4.0);
			for (int32 T = 0; T < 3; ++T)
			{
				Model.Tick(Env, Flat);
				Reference.Tick(Env, Flat);
			}
			Env.ExtraFoci.Add(FVector2D(203.0, 4.0));
			TestEqual(TEXT("llega el segundo jugador: 3 revisiones de más"), Model.Tick(Env, Flat).CatchUpRevisions, 3);
			Reference.Tick(Env, Flat);
			bool bSame = true;
			for (int32 Y = 0; Y < 32; ++Y)
			{
				for (int32 X = 0; X < 32; ++X)
				{
					bSame &= Model.DeltaMm(FIntPoint(X, Y)) == Reference.DeltaMm(FIntPoint(X, Y));
				}
			}
			TestTrue(TEXT("el montón cercano va a su ritmo"), bSame);
		});

		It("en cooperativo cada jugador activa su zona y el solape se revisa una sola vez", [this, Flat]()
		{
			FSandModel One;
			FSandModel Two;
			One.Pile(Spike(FVector2D::ZeroVector, 20000), Flat);
			Two.Pile(Spike(FVector2D::ZeroVector, 20000), Flat);
			// 160 m = 20 chunks justos: el segundo montón también está en una esquina de chunks.
			Two.Pile(Spike(FVector2D(160.0, 0.0), 20000), Flat);
			FSandEnvironment Env = Dry();
			Env.ExtraFoci.Add(FVector2D(1.0, 0.0));
			Env.ExtraFoci.Add(FVector2D(160.0, 0.0));
			for (int32 T = 0; T < 5; ++T)
			{
				One.Tick(Dry(), Flat);
				Two.Tick(Env, Flat);
			}
			bool bSame = true;
			bool bFarSame = true;
			for (int32 Y = -12; Y <= 12; ++Y)
			{
				for (int32 X = -12; X <= 12; ++X)
				{
					bSame &= One.DeltaMm(FIntPoint(X, Y)) == Two.DeltaMm(FIntPoint(X, Y));
					bFarSame &= Two.DeltaMm(FIntPoint(X, Y)) == Two.DeltaMm(FIntPoint(X + 640, Y));
				}
			}
			TestTrue(TEXT("dos focos encima no la hacen ir el doble de rápido"), bSame);
			TestTrue(TEXT("el segundo jugador mueve su montón igual"), bFarSame);
			TestTrue(TEXT("y se ha movido"), Two.DeltaMm(FIntPoint(640, 0)) < One.DeltaMm(FIntPoint(0, 0)) + 1 && Two.DeltaMm(FIntPoint(640, 0)) < FSandModel::MaxPileHeightMm);
		});

		It("una revisión cambia como mucho 64 columnas por chunk; el resto espera", [this, Flat]()
		{
			FSandModel Model;
			// Cono de 2 m de alto y 2 m de radio (45°) en el centro del chunk (0, 0): todo resbala.
			FSandBrush B;
			B.Center = FVector2D(4.0, 4.0);
			B.Radius = 2.0f;
			B.Depth = 2.0f;
			B.MassBudget = MAX_int32;
			const int64 Mass = Model.Pile(B, Flat).Mass;
			FSandEnvironment Env = Dry();
			Env.Focus = FVector2D(4.0, 4.0);
			int32 MaxChanged = 0;
			bool bDeferred = false;
			for (int32 T = 0; T < 40; ++T)
			{
				const TMap<FIntPoint, int32> Before = Snapshot(Model, FIntPoint(0, 0));
				const FSandResult R = Model.Tick(Env, Flat);
				const int32 Changed = ChangedInChunk(Model, Before, FIntPoint(0, 0));
				MaxChanged = FMath::Max(MaxChanged, Changed);
				TestTrue(TEXT("≤ 64 por chunk"), Changed <= FSandModel::MaxChangedColumnsPerChunk);
				TestEqual(TEXT("lo que dice el resultado"), R.ColumnsChanged, Changed);
				bDeferred |= R.DeferredColumns > 0;
			}
			TestEqual(TEXT("llega al tope"), MaxChanged, FSandModel::MaxChangedColumnsPerChunk);
			TestTrue(TEXT("y aplaza lo demás"), bDeferred);
			TestTrue(TEXT("acaba asentándose"), Settle(Model, Env, Flat) >= 0);
			TestTrue(TEXT("a 34°"), MaxDrop(Model, FIntPoint(16, 16), 20) <= 168);
			TestEqual(TEXT("masa"), Model.TotalMass(), Mass);
		});

		It("con todo asentado una revisión no toca ninguna columna", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D::ZeroVector, 30000), Flat);
			Settle(Model, Dry(), Flat);
			const FSandResult R = Model.Tick(Dry(), Flat);
			TestEqual(TEXT("activas"), R.ActiveColumns, 0);
			TestEqual(TEXT("cambiadas"), R.ColumnsChanged, 0);
		});
	});

	Describe("tope del montón", [this, Flat]()
	{
		It("la avalancha no llena una hondonada por encima del montón máximo y la capa se sigue cargando", [this]()
		{
			// Escalón de 3 m: la arena que cae por el borde se acumula al pie.
			auto Cliff = [](double X, double) { return X < 0.0 ? -3.0 : 0.0; };
			FSandModel Model;
			FSandEnvironment Env = Dry();
			for (int32 I = 0; I < 30; ++I)
			{
				Model.Pile(Spike(FVector2D(0.5, 0.0), 20000), Cliff);
				Settle(Model, Env, Cliff, 400);
			}
			int32 Max = 0;
			for (int32 Y = -30; Y <= 30; ++Y)
			{
				for (int32 X = -40; X <= 30; ++X)
				{
					Max = FMath::Max(Max, Model.DeltaMm(FIntPoint(X, Y)));
				}
			}
			TestTrue(TEXT("el pie del escalón llega al tope"), Max >= FSandModel::MaxPileHeightMm - 200);
			TestTrue(TEXT("pero no lo pasa"), Max <= FSandModel::MaxPileHeightMm);
			FSandModel Loaded;
			TestTrue(TEXT("se carga"), Loaded.FromValue(Model.ToValue()));
			TestTrue(TEXT("igual"), Loaded == Model);
		});
	});

	Describe("red: paquete de terreno versión 2, capa de arena (biblia 08 §2.2 y §2.6)", [this, Flat]()
	{
		It("el cliente que aplica los paquetes de cada revisión queda igual que el servidor", [this, Flat]()
		{
			FSandModel Server;
			FSandModel Client;
			FSandEnvironment Env = Dry();
			TArray<FIntPoint> Touched;
			auto Send = [&](const FSandResult& R)
			{
				TMap<FIntPoint, uint8> Chunks;
				for (const FIntPoint& Column : R.ChangedColumns)
				{
					Chunks.FindOrAdd(Server.ChunkOfColumn(Column));
				}
				for (const auto& Pair : Chunks)
				{
					for (const TArray<uint8>& Packet : Server.EncodePackets(Pair.Key, R.ChangedColumns))
					{
						TestTrue(TEXT("≤ 512 B"), Packet.Num() <= FSandModel::MaxPacketBytes);
						TestTrue(TEXT("el cliente lo acepta"), Client.ApplyPacket(Packet, Flat));
					}
					Touched.AddUnique(Pair.Key);
				}
			};
			// Pala, avalancha en la esquina de cuatro chunks (también en negativo) y medio ciclo.
			Send(Server.Pile(Spike(FVector2D::ZeroVector, 40000), Flat));
			FSandBrush B;
			B.Center = FVector2D(-3.0, 2.0);
			Send(Server.Dig(B, Flat));
			for (int32 T = 0; T < 60; ++T)
			{
				Send(Server.Tick(Env, Flat));
			}
			Send(Server.ApplyHalfTide(Tide(0.5, -0.5), Flat));
			TestTrue(TEXT("ha tocado varios chunks"), Touched.Num() >= 4);
			bool bSame = true;
			for (int32 Y = -40; Y <= 40; ++Y)
			{
				for (int32 X = -40; X <= 40; ++X)
				{
					bSame &= Server.DeltaMm(FIntPoint(X, Y)) == Client.DeltaMm(FIntPoint(X, Y));
				}
			}
			TestTrue(TEXT("mismos deltas"), bSame);
			for (const FIntPoint& Chunk : Touched)
			{
				TestEqual(TEXT("misma comprobación"), Client.ChunkChecksum(Chunk), Server.ChunkChecksum(Chunk));
			}
			TestEqual(TEXT("el cliente no simula"), Client.NumDirtyColumns(), 0);
		});

		It("el chunk completo reemplaza lo que tuviera el cliente, se parte en paquetes de 512 B y es idempotente", [this, Flat]()
		{
			FSandModel Server;
			// Chunk (0, 0) lleno de deltas distintos: 1 024 columnas → más de un paquete.
			FSandBrush B;
			B.Center = FVector2D(4.0, 4.0);
			B.Radius = 6.0f;
			B.Depth = 1.0f;
			B.MassBudget = MAX_int32;
			Server.Pile(B, Flat);
			const TArray<TArray<uint8>> Packets = Server.EncodeFullChunk(FIntPoint(0, 0));
			TestTrue(TEXT("varios paquetes"), Packets.Num() > 1);
			FSandModel Stale;
			FSandBrush Junk;
			Junk.Center = FVector2D(1.0, 7.0);
			Stale.Dig(Junk, Flat);
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				for (const TArray<uint8>& Packet : Packets)
				{
					TestTrue(TEXT("≤ 512 B"), Packet.Num() <= FSandModel::MaxPacketBytes);
					TestTrue(TEXT("aceptado"), Stale.ApplyPacket(Packet, Flat));
				}
				TestEqual(TEXT("comprobación igual (también al repetir)"), Stale.ChunkChecksum(FIntPoint(0, 0)), Server.ChunkChecksum(FIntPoint(0, 0)));
			}
			TestEqual(TEXT("el hoyo viejo ha desaparecido"), Stale.DeltaMm(Stale.ColumnOf(1.0, 7.0)), Server.DeltaMm(Server.ColumnOf(1.0, 7.0)));
			// Un chunk vacío también se sincroniza: un paquete sin tramos que lo vacía.
			const TArray<TArray<uint8>> Empty = FSandModel().EncodeFullChunk(FIntPoint(0, 0));
			TestEqual(TEXT("un paquete"), Empty.Num(), 1);
			TestTrue(TEXT("aceptado"), Stale.ApplyPacket(Empty[0], Flat));
			TestTrue(TEXT("vacío"), Stale.EditedChunks().Num() == 0);
			TestTrue(TEXT("checksum de un chunk vacío"), Stale.ChunkChecksum(FIntPoint(0, 0)) == FSandModel().ChunkChecksum(FIntPoint(0, 0)));
		});

		It("rechaza paquetes rotos sin tocar nada", [this, Flat]()
		{
			FSandModel Server;
			Server.Pile(Spike(FVector2D(4.0, 4.0), 5000), Flat);
			const TArray<uint8> Good = Server.EncodeFullChunk(FIntPoint(0, 0))[0];
			FSandModel Client;
			auto Broken = [&](TFunctionRef<void(TArray<uint8>&)> Edit)
			{
				TArray<uint8> P = Good;
				Edit(P);
				return P;
			};
			const TArray<TArray<uint8>> Bad = {
				Broken([](TArray<uint8>& P) { P[0] = 1; }),                  // versión volumétrica
				Broken([](TArray<uint8>& P) { P[1] = 0; }),                  // capa de densidad
				Broken([](TArray<uint8>& P) { P[2] = 0x80; }),               // bandera desconocida
				Broken([](TArray<uint8>& P) { P[7] = 1; }),                  // ChunkZ distinto de 0
				Broken([](TArray<uint8>& P) { P.SetNum(P.Num() - 1); }),     // truncado
				Broken([](TArray<uint8>& P) { P.Add(0); }),                  // bytes de más
				Broken([](TArray<uint8>& P) { P[11] = 0xFF; P[12] = 0x03; }), // primer índice 1023 con más de uno
				Broken([](TArray<uint8>& P) { P[13] = 0; }),                 // tramo vacío
				Broken([](TArray<uint8>& P) { P[14] = 0xFF; P[15] = 0x7F; }), // 32 767 mm: montón imposible
				Broken([](TArray<uint8>& P) { P.SetNum(5); }),               // sin cabecera
			};
			for (const TArray<uint8>& P : Bad)
			{
				TestFalse(TEXT("rechazado"), Client.ApplyPacket(P, Flat));
			}
			TestTrue(TEXT("nada aplicado"), Client.IsEmpty());
			TestTrue(TEXT("el bueno sí"), Client.ApplyPacket(Good, Flat));
		});
	});

	Describe("guardado", [this, Flat]()
	{
		It("ida y vuelta a mitad de derrumbe continúa exactamente igual, banco del mar incluido", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D(-4.1, 7.9), 30000), Flat);
			FSandBrush B;
			B.Center = FVector2D(-5.0, 9.0);
			Model.Dig(B, Flat);
			FSandEnvironment Env = Dry();
			Env.Focus = FVector2D(-4.0, 8.0);
			for (int32 T = 0; T < 3; ++T)
			{
				Model.Tick(Env, Flat);
			}
			Model.ApplyHalfTide(Tide(0.5, -0.5), Flat);
			TestTrue(TEXT("el mar tiene arena"), Model.SeaBankMass() != 0);
			FSandModel Loaded;
			TestTrue(TEXT("carga"), Loaded.FromValue(Model.ToValue()));
			TestTrue(TEXT("igual"), Loaded == Model);
			TestEqual(TEXT("banco"), Loaded.SeaBankMass(), Model.SeaBankMass());
			Settle(Model, Env, Flat);
			Settle(Loaded, Env, Flat);
			Model.ApplyHalfTide(Tide(0.5, -0.5), Flat);
			Loaded.ApplyHalfTide(Tide(0.5, -0.5), Flat);
			TestTrue(TEXT("sigue igual"), Loaded == Model);
			TestEqual(TEXT("masa"), Loaded.TotalMass(), Model.TotalMass());
		});

		It("rechaza datos de otra rejilla o manipulados", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D::ZeroVector, 5000), Flat);
			const FSaveValue Good = Model.ToValue();

			FSandModel Other(FSandSettings{ 0.5f, 32 });
			TestFalse(TEXT("otra celda"), Other.FromValue(Good));
			TestTrue(TEXT("queda vacío"), Other.IsEmpty());

			auto Tampered = [&Good](int32 Value)
			{
				FSaveValue V = Good;
				FSaveValue Runs = FSaveValue::MakeArray();
				Runs.Add(FSaveValue::MakeInt(0));
				Runs.Add(FSaveValue::MakeInt(1));
				Runs.Add(FSaveValue::MakeInt(Value));
				FSaveValue Entry = FSaveValue::MakeArray();
				Entry.Add(FSaveValue::MakeInt(5));
				Entry.Add(FSaveValue::MakeInt(5));
				Entry.Add(MoveTemp(Runs));
				FSaveValue List = FSaveValue::MakeArray();
				List.Add(MoveTemp(Entry));
				V.Set(TEXT("chunks"), MoveTemp(List));
				return V;
			};
			FSandModel Probe;
			TestTrue(TEXT("válido"), Probe.FromValue(Tampered(120)));
			TestFalse(TEXT("montón imposible"), Probe.FromValue(Tampered(FSandModel::MaxPileHeightMm + 1)));
			TestFalse(TEXT("más hondo que la capa"), Probe.FromValue(Tampered(-FSandModel::MaxDigDepthMm - 1)));
			TestFalse(TEXT("delta cero guardado"), Probe.FromValue(Tampered(0)));
			TestTrue(TEXT("vacío tras fallar"), Probe.IsEmpty());

			FSaveValue OddDirty = Good;
			FSaveValue List = FSaveValue::MakeArray();
			List.Add(FSaveValue::MakeInt(3));
			OddDirty.Set(TEXT("dirty"), MoveTemp(List));
			TestFalse(TEXT("sucias impares"), Probe.FromValue(OddDirty));

			FSaveValue BadSea = Good;
			BadSea.Set(TEXT("sea"), FSaveValue::MakeString(TEXT("mucha")));
			TestFalse(TEXT("banco que no es un entero"), Probe.FromValue(BadSea));
			TestEqual(TEXT("banco a cero tras fallar"), Probe.SeaBankMass(), static_cast<int64>(0));
		});
	});
}

#endif
