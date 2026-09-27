#include "Misc/AutomationTest.h"

#include "WorldGen/SandModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FSandModelSpec, "Explored.Sand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	/** Entorno seco: el agua muy por debajo y el foco en el origen. */
	FSandEnvironment Dry()
	{
		FSandEnvironment Env;
		Env.SeaLevel = -10.0;
		return Env;
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
	FSandBrush Spike(const FVector2D& Center, int64 Mass)
	{
		FSandBrush B;
		B.Center = Center;
		B.Radius = 1.2f;
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
			TestEqual(TEXT("húmedo"), FSandModel::ReposeDropMm(FSandModel::WetReposeDeg, 0.25f), 249);
		});

		It("las olas pesan más cuanto más cerca del agua y nada fuera de la franja", [this]()
		{
			TestEqual(TEXT("en la línea"), FSandModel::WaveWeightMilli(0, 0), 1000);
			TestEqual(TEXT("bajo el agua somera"), FSandModel::WaveWeightMilli(-1000, 0), 1000);
			TestEqual(TEXT("demasiado hondo"), FSandModel::WaveWeightMilli(-1501, 0), 0);
			TestEqual(TEXT("donde ya no llega"), FSandModel::WaveWeightMilli(1000, 0), 0);
			int32 Previous = 1001;
			for (int32 H = 0; H <= 1000; H += 50)
			{
				const int32 W = FSandModel::WaveWeightMilli(H, 0);
				TestTrue(TEXT("decrece al subir"), W < Previous);
				Previous = W;
			}
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

		It("las olas tampoco: un hoyo en la orilla se rellena con arena de alrededor", [this, Flat]()
		{
			FSandModel Model;
			FSandBrush B;
			B.Radius = 0.8f;
			B.Depth = 0.3f;
			const int64 Dug = Model.Dig(B, Flat).Mass;
			FSandEnvironment Env;
			Env.SeaLevel = 0.0;
			for (int32 T = 0; T < 600; ++T)
			{
				Model.Tick(Env, Flat);
			}
			TestEqual(TEXT("masa"), Model.TotalMass(), -Dug);
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
			FSandModel Corner;
			FSandModel Inside;
			// Sin tope de masa: el reparto del resto de un tope rompería la simetría del pincel, no de la física.
			Corner.Pile(Spike(FVector2D(0.0, 0.0), MAX_int32), Flat);
			Inside.Pile(Spike(FVector2D(4.0, 4.0), MAX_int32), Flat);
			FSandEnvironment EnvCorner = Dry();
			FSandEnvironment EnvInside = Dry();
			EnvInside.Focus = FVector2D(4.0, 4.0);
			Settle(Corner, EnvCorner, Flat);
			Settle(Inside, EnvInside, Flat);
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

		It("un segundo de golpe da lo mismo que diez pasos de 100 ms", [this, Flat]()
		{
			FSandModel A;
			FSandModel B;
			A.Pile(Spike(FVector2D(0.3, 0.2), 40000), Flat);
			B.Pile(Spike(FVector2D(0.3, 0.2), 40000), Flat);
			FSandEnvironment Env;
			Env.SeaLevel = -0.3;
			for (int32 S = 0; S < 5; ++S)
			{
				A.Advance(1000, Env, Flat);
				for (int32 I = 0; I < 10; ++I)
				{
					B.Advance(100, Env, Flat);
				}
			}
			TestTrue(TEXT("iguales"), A == B);
			// Y un paso de 37 ms no avanza nada hasta completar 100.
			FSandModel C;
			C.Pile(Spike(FVector2D::ZeroVector, 1000), Flat);
			TestEqual(TEXT("sin paso"), C.Advance(37, Env, Flat).Ticks, 0);
			TestEqual(TEXT("con paso"), C.Advance(63, Env, Flat).Ticks, 1);
		});

		It("un paso gigantesco o negativo no dispara el coste", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D::ZeroVector, 30000), Flat);
			TestEqual(TEXT("tope de pasos"), Model.Advance(MAX_int32 / 2, Dry(), Flat).Ticks, FSandModel::MaxTicksPerAdvance);
			TestEqual(TEXT("negativo"), Model.Advance(-500, Dry(), Flat).Ticks, FSandModel::MaxTicksPerAdvance);
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

	Describe("olas en la franja intermareal", [this, Flat]()
	{
		It("rellenan un hoyo más deprisa cuanto más cerca del agua y no tocan la arena seca", [this, Flat]()
		{
			// Hoyo poco profundo: sus paredes están por debajo del reposo, así que solo lo mueven las olas.
			FSandBrush B;
			B.Radius = 0.8f;
			B.Depth = 0.3f;
			FSandModel Shore;
			FSandModel High;
			FSandModel Inland;
			Shore.Dig(B, Flat);
			High.Dig(B, Flat);
			Inland.Dig(B, Flat);
			const int32 Start = Shore.DeltaMm(FIntPoint(0, 0));
			FSandEnvironment AtShore;
			AtShore.SeaLevel = 0.0;
			FSandEnvironment HighSwash;
			HighSwash.SeaLevel = -0.7;
			for (int32 T = 0; T < 60; ++T)
			{
				Shore.Tick(AtShore, Flat);
				High.Tick(HighSwash, Flat);
			}
			TestEqual(TEXT("seco: se duerme sin moverse"), Settle(Inland, Dry(), Flat), 1);
			TestEqual(TEXT("seco: igual"), Inland.DeltaMm(FIntPoint(0, 0)), Start);
			const int32 ShoreDepth = -Shore.DeltaMm(FIntPoint(0, 0));
			const int32 HighDepth = -High.DeltaMm(FIntPoint(0, 0));
			TestTrue(TEXT("arriba también se rellena algo"), HighDepth < -Start);
			TestTrue(TEXT("en la orilla, bastante más"), ShoreDepth * 3 < HighDepth * 2);
			TestTrue(TEXT("en 6 s la orilla ha borrado más de la mitad"), ShoreDepth * 2 < -Start);
		});

		It("alisan un montón que en seco se quedaría en pie", [this, Flat]()
		{
			FSandBrush B;
			B.Radius = 0.8f;
			B.Depth = 0.3f;
			B.MassBudget = 300000;
			FSandModel Model;
			Model.Pile(B, Flat);
			const int32 Start = Model.DeltaMm(FIntPoint(0, 0));
			FSandEnvironment Env;
			Env.SeaLevel = 0.0;
			for (int32 T = 0; T < 100; ++T)
			{
				Model.Tick(Env, Flat);
			}
			TestTrue(TEXT("mucho más bajo"), Model.DeltaMm(FIntPoint(0, 0)) * 3 < Start);
		});

		It("al subir la marea despiertan la arena editada que antes estaba seca", [this, Flat]()
		{
			FSandModel Model;
			FSandBrush B;
			B.Radius = 0.8f;
			B.Depth = 0.3f;
			Model.Dig(B, Flat);
			FSandEnvironment Low;
			Low.SeaLevel = -3.0;
			Settle(Model, Low, Flat);
			TestEqual(TEXT("dormido en bajamar"), Model.NumDirtyColumns(), 0);
			const int32 Before = Model.DeltaMm(FIntPoint(0, 0));
			FSandEnvironment HighTide;
			HighTide.SeaLevel = 0.0;
			const FSandResult R = Model.Tick(HighTide, Flat);
			TestTrue(TEXT("despierta"), R.ActiveColumns > 0);
			for (int32 T = 0; T < 30; ++T)
			{
				Model.Tick(HighTide, Flat);
			}
			TestTrue(TEXT("la pleamar empieza a rellenar"), Model.DeltaMm(FIntPoint(0, 0)) > Before);
		});
	});

	Describe("estructuras", [this, Flat]()
	{
		It("las columnas ancladas no se mueven y la arena pegada aguanta más pendiente", [this, Flat]()
		{
			FSandModel Free;
			FSandModel Held;
			// Un tablón de 1 m a lo largo del eje Y justo al este del montón.
			Held.SetAnchor(FVector2D(0.5, -0.5), FVector2D(0.75, 0.5), true, Flat);
			TestTrue(TEXT("anclada"), Held.IsAnchored(FIntPoint(2, 0)));
			TestFalse(TEXT("fuera de la caja"), Held.IsAnchored(FIntPoint(4, 0)));
			Free.Pile(Spike(FVector2D::ZeroVector, 40000), Flat);
			Held.Pile(Spike(FVector2D::ZeroVector, 40000), Flat);
			Settle(Free, Dry(), Flat);
			Settle(Held, Dry(), Flat);
			for (int32 Y = -2; Y <= 2; ++Y)
			{
				TestEqual(TEXT("bajo el tablón no cambia"), Held.DeltaMm(FIntPoint(2, Y)), 0);
			}
			TestTrue(TEXT("el montón anclado queda más alto"), Held.DeltaMm(FIntPoint(0, 0)) > Free.DeltaMm(FIntPoint(0, 0)));
			int32 HeldDrop = 0;
			for (int32 Y = -2; Y <= 2; ++Y)
			{
				HeldDrop = FMath::Max(HeldDrop, FMath::Abs(Held.DeltaMm(FIntPoint(1, Y)) - Held.DeltaMm(FIntPoint(0, Y))));
			}
			TestTrue(TEXT("la arena pegada al tablón aguanta más de 34°"), HeldDrop > FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
			TestTrue(TEXT("pero no más de 60°"), HeldDrop <= FSandModel::ReposeDropMm(FSandModel::AnchoredReposeDeg, 0.25f));
			TestEqual(TEXT("masa"), Held.TotalMass(), static_cast<int64>(40000));

			// Quitar el tablón suelta la arena que sujetaba.
			Held.SetAnchor(FVector2D(0.5, -0.5), FVector2D(0.75, 0.5), false, Flat);
			TestTrue(TEXT("despierta"), Held.NumDirtyColumns() > 0);
			Settle(Held, Dry(), Flat);
			TestTrue(TEXT("vuelve a 34°"), MaxDrop(Held, FIntPoint(0, 0), 20) <= FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));
		});

		It("la pala no cava bajo una estructura y las olas apenas la descalzan", [this, Flat]()
		{
			FSandModel Model;
			Model.SetAnchor(FVector2D(-0.25, -0.25), FVector2D(0.25, 0.25), true, Flat);
			FSandBrush B;
			B.Radius = 1.0f;
			Model.Dig(B, Flat);
			TestEqual(TEXT("pilote intacto"), Model.DeltaMm(FIntPoint(0, 0)), 0);
			TestTrue(TEXT("alrededor sí"), Model.DeltaMm(FIntPoint(2, 0)) < 0);
		});

		It("las referencias se cuentan: dos estructuras sobre la misma columna", [this, Flat]()
		{
			FSandModel Model;
			Model.SetAnchor(FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), true, Flat);
			Model.SetAnchor(FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), true, Flat);
			Model.SetAnchor(FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), false, Flat);
			TestTrue(TEXT("sigue anclada"), Model.IsAnchored(FIntPoint(0, 0)));
			Model.SetAnchor(FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), false, Flat);
			TestFalse(TEXT("suelta"), Model.IsAnchored(FIntPoint(0, 0)));
		});
	});

	Describe("coste: solo celdas sucias cerca del jugador", [this, Flat]()
	{
		It("un montón lejano queda dormido e intacto hasta que el jugador se acerca", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D::ZeroVector, 20000), Flat);
			Model.Pile(Spike(FVector2D(100.0, 0.0), 20000), Flat);
			const int32 FarPeak = Model.DeltaMm(Model.ColumnOf(100.0, 0.0));
			FSandEnvironment Env = Dry();
			int32 MaxActive = 0;
			for (int32 T = 0; T < 400; ++T)
			{
				const FSandResult R = Model.Tick(Env, Flat);
				MaxActive = FMath::Max(MaxActive, R.ActiveColumns);
				if (R.ActiveColumns == 0)
				{
					TestTrue(TEXT("lo lejano sigue pendiente"), R.DormantColumns > 0);
					break;
				}
			}
			TestTrue(TEXT("el montón cercano se ha asentado"), MaxDrop(Model, FIntPoint(0, 0), 20) <= 168);
			TestEqual(TEXT("el lejano no se ha tocado"), Model.DeltaMm(Model.ColumnOf(100.0, 0.0)), FarPeak);
			TestTrue(TEXT("el lejano sigue sucio"), Model.IsDirty(Model.ColumnOf(100.0, 0.0)));
			TestTrue(TEXT("nunca más de ~ lo que cubre el montón cercano"), MaxActive < 2000);

			Env.Focus = FVector2D(100.0, 0.0);
			TestTrue(TEXT("se asienta al llegar"), Settle(Model, Env, Flat) > 0);
			TestEqual(TEXT("nada sucio"), Model.NumDirtyColumns(), 0);
			TestTrue(TEXT("el lejano ya a 34°"), MaxDrop(Model, Model.ColumnOf(100.0, 0.0), 20) <= 168);
		});

		It("en cooperativo cada jugador activa su zona y el solape se simula una sola vez", [this, Flat]()
		{
			FSandModel One;
			FSandModel Two;
			One.Pile(Spike(FVector2D::ZeroVector, 20000), Flat);
			Two.Pile(Spike(FVector2D::ZeroVector, 20000), Flat);
			Two.Pile(Spike(FVector2D(60.0, 0.0), 20000), Flat);
			FSandEnvironment Env = Dry();
			Env.ExtraFoci.Add(FVector2D(1.0, 0.0));
			Env.ExtraFoci.Add(FVector2D(60.0, 0.0));
			for (int32 T = 0; T < 5; ++T)
			{
				One.Tick(Dry(), Flat);
				Two.Tick(Env, Flat);
			}
			bool bSame = true;
			for (int32 Y = -12; Y <= 12; ++Y)
			{
				for (int32 X = -12; X <= 12; ++X)
				{
					bSame &= One.DeltaMm(FIntPoint(X, Y)) == Two.DeltaMm(FIntPoint(X, Y));
				}
			}
			TestTrue(TEXT("dos focos encima no la hacen ir el doble de rápido"), bSame);
			const int32 FarPeak = Two.DeltaMm(Two.ColumnOf(60.0, 0.0));
			TestTrue(TEXT("el segundo jugador también mueve su montón"), FarPeak < FSandModel::MaxPileHeightMm);
			TestEqual(TEXT("y al mismo ritmo"), FarPeak, Two.DeltaMm(FIntPoint(0, 0)));
		});

		It("un paso nunca simula más del tope de columnas y empieza por las más cercanas", [this, Flat]()
		{
			FSandModel Model;
			FSandBrush B;
			B.Center = FVector2D(10.0, 0.0);
			B.Radius = 11.0f;
			B.Depth = 0.05f;
			B.MassBudget = MAX_int32;
			Model.Pile(B, Flat);
			TestTrue(TEXT("hay más sucias que el tope"), Model.NumDirtyColumns() > FSandModel::MaxActiveColumnsPerTick);
			FSandEnvironment Env = Dry();
			Env.ActiveRadius = 30.0;
			const FSandResult R = Model.Tick(Env, Flat);
			TestEqual(TEXT("tope"), R.ActiveColumns, FSandModel::MaxActiveColumnsPerTick);
			// Un montón de 5 cm no resbala: lo simulado se duerme y lo no simulado sigue pendiente.
			TestFalse(TEXT("la columna del foco ya se ha simulado"), Model.IsDirty(FIntPoint(0, 0)));
			TestTrue(TEXT("la más lejana sigue pendiente"), Model.IsDirty(Model.ColumnOf(20.9, 0.0)));
		});

		It("con todo asentado un paso no simula ninguna columna", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D::ZeroVector, 30000), Flat);
			Settle(Model, Dry(), Flat);
			const FSandResult R = Model.Tick(Dry(), Flat);
			TestEqual(TEXT("activas"), R.ActiveColumns, 0);
			TestEqual(TEXT("cambiadas"), R.ColumnsChanged, 0);
		});
	});

	Describe("guardado", [this, Flat]()
	{
		It("ida y vuelta a mitad de derrumbe continúa exactamente igual", [this, Flat]()
		{
			FSandModel Model;
			Model.Pile(Spike(FVector2D(-4.1, 7.9), 30000), Flat);
			FSandBrush B;
			B.Center = FVector2D(-5.0, 9.0);
			Model.Dig(B, Flat);
			FSandEnvironment Env = Dry();
			Env.Focus = FVector2D(-4.0, 8.0);
			for (int32 T = 0; T < 7; ++T)
			{
				Model.Tick(Env, Flat);
			}
			FSandModel Loaded;
			TestTrue(TEXT("carga"), Loaded.FromValue(Model.ToValue()));
			TestTrue(TEXT("igual"), Loaded == Model);
			Settle(Model, Env, Flat);
			Settle(Loaded, Env, Flat);
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
		});
	});
}

#endif
