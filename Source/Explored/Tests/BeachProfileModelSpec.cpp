#include "Misc/AutomationTest.h"

#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/BeachProfileModel.h"
#include "WorldGen/TerrainDensity.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BeachProfileTest
{
	/** Límites del encargo del director: cara de 2° a 6°, sin subidas ni curva hacia abajo. */
	constexpr float MaxFaceDeg = 6.0f;
	constexpr float MinShoreDeg = 2.0f;
	/** Subida máxima tolerada entre dos muestras seguidas yendo mar adentro (m). */
	constexpr float MaxRise = 0.005f;
	/**
	 * Segunda derivada mínima (1/m) con base de 2 m: un cambio de pendiente de 0,016 en 2 m
	 * (menos de 1°). El hombro antiguo pasaba de 0,1 a 0,4 en 5 m: diez veces más.
	 */
	constexpr float MinCurvature = -0.008f;
	constexpr float Step = 0.5f;
	constexpr int32 RaysPerIsland = 48;

	/** Un transecto perpendicular a la costa, de tierra a mar. */
	struct FTransect
	{
		/** Distancia a lo largo del transecto: < 0 tierra adentro, 0 en la línea de agua. */
		TArray<float> S;
		TArray<FTerrainColumn> Columns;
		/** Tramo de playa (BeachAmount ~ 1) alrededor de la línea de agua: [Lo, Hi]. */
		int32 Lo = INDEX_NONE;
		int32 Hi = INDEX_NONE;
		/**
		 * Playa abierta: el transecto es perpendicular en la orilla, no cruza el eje de una
		 * bahía o un canal (la distancia a la orilla crece siempre) y no lo corta otra isla.
		 */
		bool bOpen = false;
		/** Además la costa es recta a lo largo de todo el tramo: la distancia crece como s. */
		bool bStraight = false;
		/** Motivo por el que no es abierta, para los mensajes de la spec. */
		const TCHAR* Why = TEXT("");

		TArray<float> BeachHeights() const
		{
			TArray<float> Out;
			for (int32 I = Lo; I <= Hi; ++I)
			{
				Out.Add(Columns[I].Height);
			}
			return Out;
		}
	};

	/**
	 * Busca la costa exterior de la isla por el rayo de ángulo Angle y traza un transecto
	 * perpendicular (según el gradiente de ShoreDistance) de 80 m en tierra a 140 m en el mar.
	 */
	bool TraceTransect(const FTerrainDensity& Density, const FIslandDesc& Island, float Angle, FTransect& Out)
	{
		const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		float Crossing = -1.0f;
		float Previous = 1.0f;
		for (float R = Island.Radius * 0.3f; R < Island.Radius * 1.9f; R += 1.0f)
		{
			const FVector2D P = Island.Center + Dir * R;
			const float H = Density.SampleColumn(P.X, P.Y).Height;
			if (Previous > 0.0f && H <= 0.0f)
			{
				Crossing = R;
			}
			Previous = H;
		}
		if (Crossing < 0.0f)
		{
			return false;
		}
		const FVector2D C = Island.Center + Dir * Crossing;
		if (Density.SampleColumn(C.X, C.Y).BeachAmount <= 0.0f)
		{
			return false;
		}
		const float Gx = Density.SampleColumn(C.X + 1.0f, C.Y).ShoreDistance - Density.SampleColumn(C.X - 1.0f, C.Y).ShoreDistance;
		const float Gy = Density.SampleColumn(C.X, C.Y + 1.0f).ShoreDistance - Density.SampleColumn(C.X, C.Y - 1.0f).ShoreDistance;
		FVector2D Seaward(-Gx, -Gy);
		if (Seaward.Size() < 1e-3)
		{
			return false;
		}
		Seaward.Normalize();

		Out = FTransect();
		for (float S = -80.0f; S <= 140.0f; S += Step)
		{
			const FVector2D P = C + Seaward * S;
			Out.S.Add(S);
			Out.Columns.Add(Density.SampleColumn(P.X, P.Y));
		}
		int32 Water = INDEX_NONE;
		for (int32 I = 1; I < Out.Columns.Num(); ++I)
		{
			if (Out.Columns[I - 1].Height > 0.0f && Out.Columns[I].Height <= 0.0f)
			{
				Water = I;
				break;
			}
		}
		if (Water == INDEX_NONE)
		{
			return false;
		}
		Out.Lo = Water - 1;
		Out.Hi = Water;
		while (Out.Lo > 0 && Out.Columns[Out.Lo - 1].BeachAmount >= 0.999f)
		{
			--Out.Lo;
		}
		while (Out.Hi + 1 < Out.Columns.Num() && Out.Columns[Out.Hi + 1].BeachAmount >= 0.999f)
		{
			++Out.Hi;
		}
		Out.bOpen = true;
		Out.bStraight = true;
		// Donde la playa acaba por su cuenta BeachAmount baja poco a poco; si cae de golpe es
		// que otra isla o un cayo da la altura de la columna.
		auto Abrupt = [&Out](int32 Inner, int32 Outer)
		{
			if (!Out.Columns.IsValidIndex(Outer))
			{
				return false;
			}
			const FTerrainColumn& A = Out.Columns[Inner];
			const FTerrainColumn& B = Out.Columns[Outer];
			const bool bDistanceJumps = Outer > Inner ? B.ShoreDistance >= A.ShoreDistance : B.ShoreDistance <= A.ShoreDistance;
			return bDistanceJumps || A.BeachAmount - B.BeachAmount > 0.01f;
		};
		if (Abrupt(Out.Lo, Out.Lo - 1) || Abrupt(Out.Hi, Out.Hi + 1))
		{
			Out.bOpen = false;
			Out.Why = TEXT("cortado por otra isla o un cayo");
		}
		for (int32 I = Out.Lo; I <= Out.Hi; ++I)
		{
			const FTerrainColumn& Col = Out.Columns[I];
			const float Expected = -Out.S[I];
			if (FMath::Abs(Out.S[I]) <= 10.0f && FMath::Abs(Col.ShoreDistance - Expected) > 1.0f)
			{
				Out.bOpen = false;
				Out.Why = TEXT("no es perpendicular a la orilla");
			}
			if (I > Out.Lo && Col.ShoreDistance >= Out.Columns[I - 1].ShoreDistance)
			{
				Out.bOpen = false;
				Out.Why = TEXT("cruza el eje de una bahía, un canal o una laguna");
			}
			if (FMath::Abs(Col.ShoreDistance - Expected) > FMath::Max(1.0f, 0.05f * FMath::Abs(Out.S[I])))
			{
				Out.bStraight = false;
			}
		}
		return true;
	}

	/** Pendiente media (tangente) entre las muestras con altura en [Bottom, Top]. */
	float MeanSlope(const TArray<float>& H, float Bottom, float Top)
	{
		float Drop = 0.0f;
		int32 Pairs = 0;
		for (int32 I = 0; I + 1 < H.Num(); ++I)
		{
			if (H[I] <= Top && H[I] >= Bottom && H[I + 1] <= Top && H[I + 1] >= Bottom)
			{
				Drop += H[I] - H[I + 1];
				++Pairs;
			}
		}
		return Pairs > 0 ? Drop / (Pairs * Step) : 0.0f;
	}

	/** Perfil sintético equiespaciado de tierra a mar. */
	TArray<float> Profile(int32 Num, TFunctionRef<float(float S)> Height)
	{
		TArray<float> Out;
		for (int32 I = 0; I < Num; ++I)
		{
			Out.Add(Height(I * Step));
		}
		return Out;
	}
}

BEGIN_DEFINE_SPEC(FBeachProfileModelSpec, "Explored.BeachProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	/** Comprueba todos los transectos abiertos de las islas con playa de una semilla. */
	void CheckSeed(uint32 Seed, int32 MinOpenPerIsland)
	{
		using namespace BeachProfileTest;
		const FTerrainDensity Density(FArchipelagoLayout::Generate(Seed));
		const FBeachTransectBands Bands;
		const float MaxSlope = FBeachProfileModel::SlopeFromDegrees(MaxFaceDeg);
		const float MinSlope = FBeachProfileModel::SlopeFromDegrees(MinShoreDeg);
		for (const FIslandDesc& Island : Density.GetLayout().Islands)
		{
			if (!FTerrainDensity::HasBeach(Island.Archetype))
			{
				continue;
			}
			int32 Open = 0;
			int32 Straight = 0;
			for (int32 K = 0; K < RaysPerIsland; ++K)
			{
				FTransect T;
				if (!TraceTransect(Density, Island, K * UE_TWO_PI / RaysPerIsland, T) || !T.bOpen)
				{
					continue;
				}
				++Open;
				const TArray<float> H = T.BeachHeights();
				const FBeachTransectReport R = FBeachProfileModel::AnalyzeTransect(H, Step, Bands);
				const FString Where = FString::Printf(TEXT("semilla %u, %s, rayo %d"), Seed, LexToString(Island.Archetype), K);
				if (!R.bHasWaterline)
				{
					AddError(FString::Printf(TEXT("%s: sin línea de agua"), *Where));
					continue;
				}
				if (R.MaxFaceSlope > MaxSlope)
				{
					AddError(FString::Printf(TEXT("%s: pendiente de %.2f° en la franja de ±5 m"), *Where, FBeachProfileModel::DegreesFromSlope(R.MaxFaceSlope)));
				}
				if (R.MinShoreSlope < MinSlope)
				{
					AddError(FString::Printf(TEXT("%s: la orilla se queda en %.2f°"), *Where, FBeachProfileModel::DegreesFromSlope(R.MinShoreSlope)));
				}
				if (R.MaxSeawardRise > MaxRise)
				{
					AddError(FString::Printf(TEXT("%s: el perfil vuelve a subir %.3f m"), *Where, R.MaxSeawardRise));
				}
				if (R.MinShoreCurvature < MinCurvature)
				{
					AddError(FString::Printf(TEXT("%s: curva hacia abajo en la orilla (%.4f 1/m)"), *Where, R.MinShoreCurvature));
				}
				// La cara cubre toda la franja: de la berma hasta 5 m de fondo.
				if (H[0] < 1.0f || H.Last() > Bands.FaceBottom)
				{
					AddError(FString::Printf(TEXT("%s: el tramo de playa va de %.2f a %.2f m"), *Where, H[0], H.Last()));
				}
				// Bajo el agua sigue la misma pendiente, o algo mayor, que en la arena seca. Solo en
				// costas rectas: dentro de una bahía la distancia a la orilla crece más despacio
				// que el transecto y el fondo sale más tendido, como en una bahía de verdad.
				if (T.bStraight)
				{
					++Straight;
					const float Dry = MeanSlope(H, 0.2f, 1.0f);
					const float Wet = MeanSlope(H, -4.0f, -0.2f);
					if (Dry <= 0.0f || Wet < Dry * 0.98f || Wet > Dry * 1.5f)
					{
						AddError(FString::Printf(TEXT("%s: pendiente seca %.3f y mojada %.3f"), *Where, Dry, Wet));
					}
				}
			}
			if (Straight == 0)
			{
				AddError(FString::Printf(TEXT("semilla %u, %s: ningún tramo de costa recta"), Seed, LexToString(Island.Archetype)));
			}
			if (Open < MinOpenPerIsland)
			{
				AddError(FString::Printf(TEXT("semilla %u, %s: solo %d transectos de playa abierta de %d"),
					Seed, LexToString(Island.Archetype), Open, RaysPerIsland));
			}
		}
	}
END_DEFINE_SPEC(FBeachProfileModelSpec)

void FBeachProfileModelSpec::Define()
{
	using namespace BeachProfileTest;

	Describe("ComposeHeight", [this]()
	{
		It("vale 0 en la orilla y baja recta con la pendiente pedida a los dos lados", [this]()
		{
			FBeachProfileParams P;
			P.Slope = FBeachProfileModel::SlopeFromDegrees(4.0f);
			// Relieve base con el hombro antiguo: llano a 1,5 m y caída a -1,2 m en la orilla.
			auto OldShoulder = [](float D) { return D > 0.0f ? FMath::Min(1.5f, D * 0.4f) : -1.2f; };
			TestEqual(TEXT("Orilla"), FBeachProfileModel::ComposeHeight(P, 0.0f, OldShoulder(0.0f)), 0.0f);
			// Recta desde la berma hasta donde empieza a empinarse, ya con fondo.
			for (float D = -P.UnderwaterSteepenDepth / P.Slope; D <= 15.0f; D += 0.25f)
			{
				const float H = FBeachProfileModel::ComposeHeight(P, D, OldShoulder(D));
				if (!FMath::IsNearlyEqual(H, P.Slope * D, 1e-4f))
				{
					AddError(FString::Printf(TEXT("d = %.2f: %.4f en vez de %.4f"), D, H, P.Slope * D));
					break;
				}
			}
		});

		It("bajo el agua se empina poco a poco hasta la pendiente sumergida, sin aristas", [this]()
		{
			FBeachProfileParams P;
			P.Slope = FBeachProfileModel::SlopeFromDegrees(3.0f);
			const float Base = -40.0f;
			float PreviousSlope = P.Slope;
			for (float D = 0.0f; D > -80.0f; D -= 0.5f)
			{
				const float Local = (FBeachProfileModel::ComposeHeight(P, D, Base) - FBeachProfileModel::ComposeHeight(P, D - 0.5f, Base)) / 0.5f;
				if (Local < PreviousSlope - 1e-4f || Local - PreviousSlope > 0.001f)
				{
					AddError(FString::Printf(TEXT("d = %.1f: la pendiente pasa de %.4f a %.4f"), D, PreviousSlope, Local));
					return;
				}
				PreviousSlope = Local;
			}
			TestTrue(TEXT("Llega a la pendiente sumergida"), FMath::IsNearlyEqual(PreviousSlope, P.Slope * P.UnderwaterSlopeScale, 1e-3f));
		});

		It("se aplana en la berma y tierra adentro devuelve el relieve base", [this]()
		{
			FBeachProfileParams P;
			P.Slope = 0.07f;
			P.BermHeight = 1.8f;
			const float Berm = FBeachProfileModel::ComposeHeight(P, P.BlendStart - 0.1f, 40.0f);
			TestTrue(TEXT("Berma a su altura"), FMath::IsNearlyEqual(Berm, P.BermHeight, 0.01f));
			TestEqual(TEXT("Relieve base"), FBeachProfileModel::ComposeHeight(P, P.BlendEnd + 1.0f, 40.0f), 40.0f);
			TestEqual(TEXT("Nada de playa tierra adentro"), FBeachProfileModel::BeachAmount(P, P.BlendEnd + 1.0f), 0.0f);
			TestEqual(TEXT("Toda la playa en la orilla"), FBeachProfileModel::BeachAmount(P, 0.0f), 1.0f);
		});

		It("bajo el agua baja hasta NearshoreDepth y mar adentro devuelve el fondo base", [this]()
		{
			FBeachProfileParams P;
			P.Slope = 0.06f;
			// Fondo base somero (la plataforma antigua a -1,2 m) y cresta de arrecife.
			const float Face = FBeachProfileModel::ComposeHeight(P, -P.NearshoreDepth / P.Slope, -1.2f);
			TestTrue(TEXT("La plataforma somera queda bajo la cara"), Face <= -P.NearshoreDepth);
			const float Steep = FBeachProfileModel::ComposeHeight(P, -60.0f, -30.0f);
			TestTrue(TEXT("La cara no se corta aunque el fondo base caiga en picado"), Steep < P.Slope * -60.0f && Steep > P.Slope * P.UnderwaterSlopeScale * -60.0f);
			TestEqual(TEXT("Mar adentro manda el fondo base"), FBeachProfileModel::ComposeHeight(P, -P.OffshoreFadeEnd - 1.0f, -2.8f), -2.8f);
		});

		It("nunca sube mar adentro si el relieve base no lo hace", [this]()
		{
			FBeachProfileParams P;
			P.Slope = FBeachProfileModel::SlopeFromDegrees(3.0f);
			// Relieve base monótono pero con escalones y mesetas, a los dos lados de la orilla.
			auto Base = [](float D)
			{
				return D >= 0.0f ? 0.2f * D + 3.0f * FMath::FloorToFloat(D / 17.0f) : FMath::Max(-80.0f, -1.2f + 0.05f * D - 2.0f * FMath::FloorToFloat(-D / 23.0f));
			};
			float Previous = TNumericLimits<float>::Max();
			for (float D = 150.0f; D >= -250.0f; D -= 0.25f)
			{
				const float H = FBeachProfileModel::ComposeHeight(P, D, Base(D));
				if (H > Previous + 1e-4f)
				{
					AddError(FString::Printf(TEXT("Sube en d = %.2f (%.4f > %.4f)"), D, H, Previous));
					break;
				}
				Previous = H;
			}
		});

		It("tolera parámetros rotos sin NaN", [this]()
		{
			FBeachProfileParams P;
			P.Slope = -1.0f;
			P.JoinSoftness = 0.0f;
			P.BermHeight = -3.0f;
			for (float D : {-500.0f, -1.0f, 0.0f, 1.0f, 500.0f})
			{
				TestTrue(TEXT("Finito"), FMath::IsFinite(FBeachProfileModel::ComposeHeight(P, D, 2.0f)));
			}
			TestEqual(TEXT("Pendiente recortada"), FBeachProfileModel::SlopeFromDegrees(200.0f), FBeachProfileModel::SlopeFromDegrees(60.0f));
		});
	});

	Describe("AnalyzeTransect", [this]()
	{
		It("da por buena una playa recta de 4°", [this]()
		{
			const float S = FBeachProfileModel::SlopeFromDegrees(4.0f);
			const FBeachTransectReport R = FBeachProfileModel::AnalyzeTransect(Profile(400, [S](float X) { return 2.0f - S * X; }), Step);
			TestTrue(TEXT("Línea de agua"), R.bHasWaterline);
			TestTrue(TEXT("Pendiente"), FMath::IsNearlyEqual(R.MaxFaceSlope, S, 1e-3f) && FMath::IsNearlyEqual(R.MinShoreSlope, S, 1e-3f));
			TestTrue(TEXT("Sin subidas"), R.MaxSeawardRise <= 1e-5f);
			TestTrue(TEXT("Sin curvatura"), R.MinShoreCurvature > -1e-3f);
			// De 2 m sobre el agua a 5 m bajo ella.
			TestTrue(TEXT("Cubre la franja"), R.FaceSamples >= FMath::FloorToInt32(7.0f / S / Step));
		});

		It("caza el hombro convexo antiguo: llano a 1,5 m y caída a la plataforma", [this]()
		{
			// Perfil medido en el juego antes del arreglo (ver el PR): berma plana que cae de
			// golpe a -1,2 m justo en la orilla.
			const TArray<float> H = Profile(200, [](float X)
			{
				const float Shoulder = 1.5f * (1.0f - FMath::Square(FMath::Clamp((X - 40.0f) / 8.0f, 0.0f, 1.0f)));
				return X < 48.0f ? Shoulder : FMath::Max(-1.2f, -0.4f * (X - 48.0f));
			});
			const FBeachTransectReport R = FBeachProfileModel::AnalyzeTransect(H, Step);
			TestTrue(TEXT("Demasiado empinado"), R.MaxFaceSlope > FBeachProfileModel::SlopeFromDegrees(MaxFaceDeg));
			TestTrue(TEXT("Curva hacia abajo en la orilla"), R.MinShoreCurvature < MinCurvature);
		});

		It("caza un perfil que vuelve a subir mar adentro", [this]()
		{
			const TArray<float> H = Profile(300, [](float X) { return 1.0f - 0.05f * X + (X > 60.0f && X < 70.0f ? 0.3f : 0.0f); });
			TestTrue(TEXT("Subida"), FBeachProfileModel::AnalyzeTransect(H, Step).MaxSeawardRise > 0.25f);
		});

		It("no se rompe con entradas corruptas", [this]()
		{
			TestFalse(TEXT("Vacío"), FBeachProfileModel::AnalyzeTransect(TArray<float>(), Step).bHasWaterline);
			TestFalse(TEXT("Paso nulo"), FBeachProfileModel::AnalyzeTransect({1.0f, -1.0f}, 0.0f).bHasWaterline);
			TestFalse(TEXT("Paso NaN"), FBeachProfileModel::AnalyzeTransect({1.0f, -1.0f}, NAN).bHasWaterline);
			TestFalse(TEXT("Todo tierra"), FBeachProfileModel::AnalyzeTransect({3.0f, 2.0f, 1.0f}, Step).bHasWaterline);
			const FBeachTransectReport Cliff = FBeachProfileModel::AnalyzeTransect({30.0f, 20.0f, -10.0f}, Step);
			TestTrue(TEXT("Acantilado: el salto es la pendiente"), Cliff.bHasWaterline && Cliff.MaxFaceSlope > 10.0f);
			const FBeachTransectReport Nan = FBeachProfileModel::AnalyzeTransect({2.0f, 1.0f, 0.5f, -0.5f, NAN, -2.0f}, Step);
			TestTrue(TEXT("Un NaN corta el tramo sin contaminarlo"), Nan.bHasWaterline && FMath::IsFinite(Nan.MaxFaceSlope) && Nan.FaceSamples == 4);
		});
	});

	Describe("FBeachShoreField", [this]()
	{
		It("da la distancia exacta a una costa recta, también tras el suavizado", [this]()
		{
			FBeachShoreField Field;
			Field.Build(FVector2D(-100.0, -100.0), FVector2D(100.0, 100.0), 4.0f, 4, 6.0f, [](double X, double) { return static_cast<float>(0.3 * X + 1.7); });
			TestTrue(TEXT("Válido"), Field.IsValid());
			for (double X = -90.0; X <= 90.0; X += 7.3)
			{
				float D = 0.0f;
				TestTrue(TEXT("Dentro"), Field.SignedDistance(X, 13.0, D));
				const float Expected = static_cast<float>(X + 1.7 / 0.3);
				if (!FMath::IsNearlyEqual(D, Expected, 0.02f))
				{
					AddError(FString::Printf(TEXT("x = %.1f: %.3f en vez de %.3f"), X, D, Expected));
				}
			}
		});

		It("mide la distancia a la orilla de una isla redonda con error de centímetros", [this]()
		{
			FBeachShoreField Field;
			Field.Build(FVector2D(-200.0, -200.0), FVector2D(200.0, 200.0), 4.0f, 4, 6.0f,
				[](double X, double Y) { return static_cast<float>(0.1 * (120.0 - FMath::Sqrt(X * X + Y * Y))); });
			for (int32 K = 0; K < 32; ++K)
			{
				const double A = K * UE_TWO_PI / 32.0;
				for (double R : {100.0, 115.0, 120.0, 125.0, 140.0})
				{
					float D = 0.0f;
					Field.SignedDistance(R * FMath::Cos(A), R * FMath::Sin(A), D);
					if (!FMath::IsNearlyEqual(D, static_cast<float>(120.0 - R), 0.15f))
					{
						AddError(FString::Printf(TEXT("r = %.0f, ángulo %d: %.3f en vez de %.3f"), R, K, D, 120.0 - R));
					}
				}
			}
		});

		It("encuentra una charca más pequeña que la celda gruesa si está cerca del nivel del mar", [this]()
		{
			FBeachShoreField Field;
			// Llano a 2 m con un hoyo de 5 m de radio en (10, 10): cabe dentro de una celda gruesa.
			Field.Build(FVector2D(-64.0, -64.0), FVector2D(64.0, 64.0), 2.0f, 8, 6.0f,
				[](double X, double Y) { return static_cast<float>(FMath::Min(2.0, 0.8 * (FMath::Sqrt(FMath::Square(X - 10.0) + FMath::Square(Y - 10.0)) - 5.0))); });
			float D = 0.0f;
			TestTrue(TEXT("Hay costa"), Field.IsValid() && Field.SignedDistance(10.0, 10.0, D));
			// El suavizado redondea el fondo de una charca de apenas dos celdas de radio.
			TestTrue(TEXT("El centro de la charca es agua"), D < -1.5f);
		});

		It("queda vacío sin costa o con parámetros rotos", [this]()
		{
			FBeachShoreField Field;
			Field.Build(FVector2D(0.0, 0.0), FVector2D(100.0, 100.0), 4.0f, 4, 6.0f, [](double, double) { return 5.0f; });
			TestFalse(TEXT("Todo tierra"), Field.IsValid());
			float D = 123.0f;
			TestFalse(TEXT("Sin muestra"), Field.SignedDistance(50.0, 50.0, D));
			TestEqual(TEXT("No toca la salida"), D, 123.0f);
			Field.Build(FVector2D(0.0, 0.0), FVector2D(100.0, 100.0), 0.0f, 4, 6.0f, [](double X, double) { return static_cast<float>(X - 50.0); });
			TestFalse(TEXT("Celda nula"), Field.IsValid());
			Field.Build(FVector2D(0.0, 0.0), FVector2D(100.0, 100.0), NAN, 4, 6.0f, [](double X, double) { return static_cast<float>(X - 50.0); });
			TestFalse(TEXT("Celda NaN"), Field.IsValid());
			Field.Build(FVector2D(100.0, 0.0), FVector2D(0.0, 100.0), 4.0f, 4, 6.0f, [](double X, double) { return static_cast<float>(X - 50.0); });
			TestFalse(TEXT("Caja al revés"), Field.IsValid());
		});
	});

	Describe("Playas del archipiélago", [this]()
	{
		It("bajan rectas al agua en todas las islas con playa de la semilla oficial", [this]()
		{
			CheckSeed(FArchipelagoLayout::OfficialSeed, 12);
		});

		It("bajan rectas al agua también con otras semillas", [this]()
		{
			// Con otras semillas el atolón puede salir casi todo de motus y pasos: hay menos
			// tramos de costa abierta, pero los que hay cumplen igual.
			CheckSeed(7u, 6);
			CheckSeed(424242u, 6);
		});

		It("ponen la orilla justo donde el terreno cruza el nivel del mar", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
			int32 Checked = 0;
			for (const FIslandDesc& Island : Density.GetLayout().Islands)
			{
				if (!FTerrainDensity::HasBeach(Island.Archetype))
				{
					continue;
				}
				for (int32 K = 0; K < 16; ++K)
				{
					FTransect T;
					if (!TraceTransect(Density, Island, K * UE_TWO_PI / 16.0f, T) || !T.bOpen)
					{
						continue;
					}
					for (int32 I = T.Lo + 1; I <= T.Hi; ++I)
					{
						if (T.Columns[I - 1].Height > 0.0f && T.Columns[I].Height <= 0.0f)
						{
							++Checked;
							TestTrue(TEXT("Distancia a la orilla ~0 en la línea de agua"), FMath::Abs(T.Columns[I].ShoreDistance) < Step + 0.1f);
						}
					}
				}
			}
			TestTrue(TEXT("Hay orillas que comprobar"), Checked > 20);
		});

		It("dejan sin playa los acantilados de Los Dientes y el fango del manglar", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
			for (int32 I = 0; I < Density.GetLayout().Islands.Num(); ++I)
			{
				const FIslandDesc& Island = Density.GetLayout().Islands[I];
				if (FTerrainDensity::HasBeach(Island.Archetype))
				{
					continue;
				}
				for (int32 K = 0; K < 64; ++K)
				{
					const float A = K * UE_TWO_PI / 64.0f;
					const FVector2D P = Island.Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Island.Radius * (0.6f + 0.02f * K);
					const FTerrainColumn Col = Density.SampleColumn(P.X, P.Y);
					if (Col.IslandIndex == I && Col.BeachAmount != 0.0f)
					{
						AddError(FString::Printf(TEXT("%s con playa en el rayo %d"), LexToString(Island.Archetype), K));
						break;
					}
				}
			}
		});

		It("son deterministas por semilla", [this]()
		{
			const FTerrainDensity A(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
			const FTerrainDensity B(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
			const FTerrainDensity Other(FArchipelagoLayout::Generate(7u));
			int32 Differ = 0;
			for (const FIslandDesc& Island : A.GetLayout().Islands)
			{
				for (int32 K = 0; K < 24; ++K)
				{
					const float Angle = K * UE_TWO_PI / 24.0f;
					for (float R : {0.9f, 1.0f, 1.1f})
					{
						const FVector2D P = Island.Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Island.Radius * R;
						const FTerrainColumn CA = A.SampleColumn(P.X, P.Y);
						const FTerrainColumn CB = B.SampleColumn(P.X, P.Y);
						if (CA.Height != CB.Height || CA.ShoreDistance != CB.ShoreDistance || CA.BeachAmount != CB.BeachAmount)
						{
							AddError(TEXT("La misma semilla da otra costa"));
							return;
						}
						Differ += CA.Height != Other.SampleColumn(P.X, P.Y).Height ? 1 : 0;
					}
				}
			}
			TestTrue(TEXT("Otra semilla, otra costa"), Differ > 100);
		});
	});
}

#endif
