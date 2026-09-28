#include "WorldGen/BeachProfileModel.h"

#include "Async/ParallelFor.h"

namespace
{
	FORCEINLINE float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Mínimo suave polinómico: exacto cuando A y B se separan más de K. */
	FORCEINLINE float SmoothMin(float A, float B, float K)
	{
		const float H = FMath::Max(K - FMath::Abs(A - B), 0.0f) / K;
		return FMath::Min(A, B) - H * H * K * 0.25f;
	}

	FORCEINLINE float SmoothMax(float A, float B, float K)
	{
		return -SmoothMin(-A, -B, K);
	}

	/** Integral de SmoothStep(A, B, t) entre A y X: rampa que arranca sin arista. */
	float SmoothStepIntegral(float A, float B, float X)
	{
		if (X <= A)
		{
			return 0.0f;
		}
		const float Length = B - A;
		if (X >= B)
		{
			return 0.5f * Length + (X - B);
		}
		const float U = (X - A) / Length;
		return Length * (U * U * U - 0.5f * U * U * U * U);
	}

	/** Tierra = altura estrictamente positiva; el nivel del mar exacto cuenta como agua. */
	/** Pasadas de suavizado del campo de distancia (ver Build, paso 6). */
	constexpr int32 SmoothingPasses = 2;

	FORCEINLINE bool IsLand(float H)
	{
		return H > 0.0f;
	}

	float DistSquaredToSegment(const FVector2f& P, const FVector2f& A, const FVector2f& B)
	{
		const FVector2f AB = B - A;
		const float LenSq = AB.SizeSquared();
		const float T = LenSq > 1e-12f ? FMath::Clamp(FVector2f::DotProduct(P - A, AB) / LenSq, 0.0f, 1.0f) : 0.0f;
		return FVector2f::DistSquared(P, A + AB * T);
	}
}

// --- FBeachShoreField ------------------------------------------------------------------

void FBeachShoreField::Build(const FVector2D& Min, const FVector2D& Max, float CellSize, int32 CoarseFactor, float ActiveBand,
	TFunctionRef<float(double X, double Y)> HeightAt)
{
	Width = 0;
	Height = 0;
	ShoreSegments = 0;
	Distances.Reset();
	if (!(CellSize > 0.0f) || !(Max.X > Min.X) || !(Max.Y > Min.Y))
	{
		return;
	}

	Origin = Min;
	Cell = CellSize;
	const int32 W = FMath::Max(2, FMath::CeilToInt32((Max.X - Min.X) / CellSize) + 1);
	const int32 H = FMath::Max(2, FMath::CeilToInt32((Max.Y - Min.Y) / CellSize) + 1);
	const int32 CF = FMath::Max(1, CoarseFactor);
	const int32 CW = (W - 1 + CF - 1) / CF + 1;
	const int32 CH = (H - 1 + CF - 1) / CF + 1;

	// 1) Rejilla gruesa: dónde hay costa (o puede haberla).
	TArray<float> Coarse;
	Coarse.SetNumUninitialized(CW * CH);
	ParallelFor(CH, [&](int32 CY)
	{
		for (int32 CX = 0; CX < CW; ++CX)
		{
			Coarse[CY * CW + CX] = HeightAt(Origin.X + static_cast<double>(CX * CF) * Cell, Origin.Y + static_cast<double>(CY * CF) * Cell);
		}
	});

	// 2) Nodos finos que se evalúan de verdad: los de celdas gruesas activas.
	TArray<uint8> Exact;
	Exact.SetNumZeroed(W * H);
	for (int32 CY = 0; CY + 1 < CH; ++CY)
	{
		for (int32 CX = 0; CX + 1 < CW; ++CX)
		{
			const float A = Coarse[CY * CW + CX];
			const float B = Coarse[CY * CW + CX + 1];
			const float C = Coarse[(CY + 1) * CW + CX];
			const float D = Coarse[(CY + 1) * CW + CX + 1];
			const bool bMixed = IsLand(A) != IsLand(B) || IsLand(A) != IsLand(C) || IsLand(A) != IsLand(D);
			const float Nearest = FMath::Min(FMath::Min(FMath::Abs(A), FMath::Abs(B)), FMath::Min(FMath::Abs(C), FMath::Abs(D)));
			if (!bMixed && !(Nearest < ActiveBand))
			{
				continue;
			}
			for (int32 Y = CY * CF; Y <= FMath::Min((CY + 1) * CF, H - 1); ++Y)
			{
				for (int32 X = CX * CF; X <= FMath::Min((CX + 1) * CF, W - 1); ++X)
				{
					Exact[Y * W + X] = 1;
				}
			}
		}
	}

	TArray<float> Values;
	Values.SetNumUninitialized(W * H);
	ParallelFor(H, [&](int32 Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			if (Exact[Y * W + X])
			{
				Values[Y * W + X] = HeightAt(Origin.X + static_cast<double>(X) * Cell, Origin.Y + static_cast<double>(Y) * Cell);
				continue;
			}
			// Lejos de la costa basta el signo: interpolación de la rejilla gruesa.
			const int32 CX = FMath::Min(X / CF, CW - 2);
			const int32 CY = FMath::Min(Y / CF, CH - 2);
			const float FX = FMath::Clamp(static_cast<float>(X - CX * CF) / CF, 0.0f, 1.0f);
			const float FY = FMath::Clamp(static_cast<float>(Y - CY * CF) / CF, 0.0f, 1.0f);
			const float Top = FMath::Lerp(Coarse[CY * CW + CX], Coarse[CY * CW + CX + 1], FX);
			const float Bottom = FMath::Lerp(Coarse[(CY + 1) * CW + CX], Coarse[(CY + 1) * CW + CX + 1], FX);
			Values[Y * W + X] = FMath::Lerp(Top, Bottom, FY);
		}
	});

	// 3) Cruces por cero en las aristas: por bisección sobre la altura exacta cuando los dos
	// extremos se evaluaron de verdad (la interpolación lineal de un hombro de costa desplaza
	// el cruce hasta medio metro y la playa sale rizada); si no, interpolación lineal.
	constexpr float NoCut = -1.0f;
	TArray<float> CutH;
	TArray<float> CutV;
	CutH.Init(NoCut, W * H);
	CutV.Init(NoCut, W * H);
	auto Bisect = [&](int32 X, int32 Y, int32 DX, int32 DY)
	{
		const int32 A = Y * W + X;
		const int32 B = (Y + DY) * W + X + DX;
		const bool bLandA = IsLand(Values[A]);
		if (!Exact[A] || !Exact[B])
		{
			return FMath::Clamp(Values[A] / (Values[A] - Values[B]), 0.0f, 1.0f);
		}
		float Lo = 0.0f;
		float Hi = 1.0f;
		for (int32 Step = 0; Step < 7; ++Step)
		{
			const float Mid = 0.5f * (Lo + Hi);
			const float V = HeightAt(Origin.X + (X + DX * Mid) * static_cast<double>(Cell), Origin.Y + (Y + DY * Mid) * static_cast<double>(Cell));
			(IsLand(V) == bLandA ? Lo : Hi) = Mid;
		}
		return 0.5f * (Lo + Hi);
	};
	ParallelFor(H, [&](int32 Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			if (X + 1 < W && IsLand(Values[Y * W + X]) != IsLand(Values[Y * W + X + 1]))
			{
				CutH[Y * W + X] = Bisect(X, Y, 1, 0);
			}
			if (Y + 1 < H && IsLand(Values[Y * W + X]) != IsLand(Values[(Y + 1) * W + X]))
			{
				CutV[Y * W + X] = Bisect(X, Y, 0, 1);
			}
		}
	});

	// 4) Costa como segmentos (marching squares), en coordenadas de celda. La distancia se
	// mide a los segmentos y no a los cruces sueltos: con puntos, a 2 m de una costa recta
	// el error llegaba al 40 % y la cara de la playa salía ondulada.
	TArray<FVector2f> SegA;
	TArray<FVector2f> SegB;
	TArray<int32> NodeSeg;
	TArray<float> Dist2;
	constexpr float Unreached = TNumericLimits<float>::Max();
	NodeSeg.Init(INDEX_NONE, W * H);
	Dist2.Init(Unreached, W * H);
	auto Offer = [&](int32 X, int32 Y, int32 Seg)
	{
		const int32 Index = Y * W + X;
		const float D2 = DistSquaredToSegment(FVector2f(static_cast<float>(X), static_cast<float>(Y)), SegA[Seg], SegB[Seg]);
		if (D2 < Dist2[Index])
		{
			Dist2[Index] = D2;
			NodeSeg[Index] = Seg;
		}
	};
	for (int32 Y = 0; Y + 1 < H; ++Y)
	{
		for (int32 X = 0; X + 1 < W; ++X)
		{
			// Esquinas en sentido antihorario y aristas: 0 abajo, 1 derecha, 2 arriba, 3 izquierda.
			const float X0 = static_cast<float>(X);
			const float Y0 = static_cast<float>(Y);
			const FVector2f P[4] = {FVector2f(X0, Y0), FVector2f(X0 + 1.0f, Y0), FVector2f(X0 + 1.0f, Y0 + 1.0f), FVector2f(X0, Y0 + 1.0f)};
			const float V[4] = {Values[Y * W + X], Values[Y * W + X + 1], Values[(Y + 1) * W + X + 1], Values[(Y + 1) * W + X]};
			// Parámetro del cruce medido siempre desde la esquina de menor coordenada.
			const float EdgeT[4] = {CutH[Y * W + X], CutV[Y * W + X + 1], CutH[(Y + 1) * W + X], CutV[Y * W + X]};
			const FVector2f EdgeFrom[4] = {P[0], P[1], P[3], P[0]};
			const FVector2f EdgeDir[4] = {FVector2f(1.0f, 0.0f), FVector2f(0.0f, 1.0f), FVector2f(1.0f, 0.0f), FVector2f(0.0f, 1.0f)};
			FVector2f Edge[4];
			bool bCut[4];
			int32 Cuts = 0;
			for (int32 E = 0; E < 4; ++E)
			{
				bCut[E] = EdgeT[E] != NoCut;
				if (bCut[E])
				{
					Edge[E] = EdgeFrom[E] + EdgeDir[E] * EdgeT[E];
					++Cuts;
				}
			}
			if (Cuts == 0)
			{
				continue;
			}
			const int32 First = SegA.Num();
			if (Cuts == 2)
			{
				int32 Found[2];
				int32 F = 0;
				for (int32 E = 0; E < 4; ++E)
				{
					if (bCut[E])
					{
						Found[F++] = E;
					}
				}
				SegA.Add(Edge[Found[0]]);
				SegB.Add(Edge[Found[1]]);
			}
			else
			{
				// Silla: el valor del centro decide qué par de esquinas opuestas queda unido.
				const bool bCenterLikeFirst = IsLand(0.25f * (V[0] + V[1] + V[2] + V[3])) == IsLand(V[0]);
				if (bCenterLikeFirst)
				{
					SegA.Add(Edge[0]); SegB.Add(Edge[1]);
					SegA.Add(Edge[2]); SegB.Add(Edge[3]);
				}
				else
				{
					SegA.Add(Edge[3]); SegB.Add(Edge[0]);
					SegA.Add(Edge[1]); SegB.Add(Edge[2]);
				}
			}
			for (int32 Seg = First; Seg < SegA.Num(); ++Seg)
			{
				Offer(X, Y, Seg);
				Offer(X + 1, Y, Seg);
				Offer(X, Y + 1, Seg);
				Offer(X + 1, Y + 1, Seg);
			}
		}
	}
	ShoreSegments = SegA.Num();
	if (ShoreSegments == 0)
	{
		// Sin costa no hay playa: el campo queda vacío y el terreno no se toca.
		return;
	}

	// 5) Propagación del segmento de costa más cercano (dos barridos, dos veces).
	auto Relax = [&](int32 X, int32 Y, int32 NX, int32 NY)
	{
		if (NX < 0 || NY < 0 || NX >= W || NY >= H)
		{
			return;
		}
		const int32 Seg = NodeSeg[NY * W + NX];
		if (Seg != INDEX_NONE && Seg != NodeSeg[Y * W + X])
		{
			Offer(X, Y, Seg);
		}
	};
	for (int32 Round = 0; Round < 2; ++Round)
	{
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				Relax(X, Y, X - 1, Y - 1);
				Relax(X, Y, X, Y - 1);
				Relax(X, Y, X + 1, Y - 1);
				Relax(X, Y, X - 1, Y);
			}
			for (int32 X = W - 1; X >= 0; --X)
			{
				Relax(X, Y, X + 1, Y);
			}
		}
		for (int32 Y = H - 1; Y >= 0; --Y)
		{
			for (int32 X = W - 1; X >= 0; --X)
			{
				Relax(X, Y, X + 1, Y);
				Relax(X, Y, X - 1, Y + 1);
				Relax(X, Y, X, Y + 1);
				Relax(X, Y, X + 1, Y + 1);
			}
			for (int32 X = 0; X < W; ++X)
			{
				Relax(X, Y, X - 1, Y);
			}
		}
	}

	Width = W;
	Height = H;
	Distances.SetNumUninitialized(W * H);
	for (int32 I = 0; I < W * H; ++I)
	{
		const float D = FMath::Sqrt(Dist2[I]) * Cell;
		Distances[I] = IsLand(Values[I]) ? D : -D;
	}

	// 6) Suavizado binomial (1-2-1) separable: deja intacto el campo de una costa recta y
	// alisa los meandros de menos de ~3 celdas, que la ola tampoco deja en una playa real.
	TArray<float> Temp;
	Temp.SetNumUninitialized(W * H);
	for (int32 Pass = 0; Pass < SmoothingPasses; ++Pass)
	{
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const float L = Distances[Y * W + FMath::Max(X - 1, 0)];
				const float R = Distances[Y * W + FMath::Min(X + 1, W - 1)];
				Temp[Y * W + X] = 0.25f * (L + R) + 0.5f * Distances[Y * W + X];
			}
		}
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const float B = Temp[FMath::Max(Y - 1, 0) * W + X];
				const float T = Temp[FMath::Min(Y + 1, H - 1) * W + X];
				Distances[Y * W + X] = 0.25f * (B + T) + 0.5f * Temp[Y * W + X];
			}
		}
	}
}

bool FBeachShoreField::Contains(double X, double Y) const
{
	if (!IsValid())
	{
		return false;
	}
	const double FX = (X - Origin.X) / Cell;
	const double FY = (Y - Origin.Y) / Cell;
	return FX >= 0.0 && FY >= 0.0 && FX <= Width - 1 && FY <= Height - 1;
}

bool FBeachShoreField::SignedDistance(double X, double Y, float& OutDistance) const
{
	if (!Contains(X, Y))
	{
		return false;
	}
	const double FX = (X - Origin.X) / Cell;
	const double FY = (Y - Origin.Y) / Cell;
	const int32 X0 = FMath::Min(static_cast<int32>(FX), Width - 2);
	const int32 Y0 = FMath::Min(static_cast<int32>(FY), Height - 2);
	const float TX = static_cast<float>(FX - X0);
	const float TY = static_cast<float>(FY - Y0);
	const float Top = FMath::Lerp(Distances[Y0 * Width + X0], Distances[Y0 * Width + X0 + 1], TX);
	const float Bottom = FMath::Lerp(Distances[(Y0 + 1) * Width + X0], Distances[(Y0 + 1) * Width + X0 + 1], TX);
	OutDistance = FMath::Lerp(Top, Bottom, TY);
	return true;
}

// --- FBeachProfileModel ----------------------------------------------------------------

float FBeachProfileModel::SlopeFromDegrees(float Degrees)
{
	return FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Degrees, 0.0f, 60.0f)));
}

float FBeachProfileModel::DegreesFromSlope(float Slope)
{
	return FMath::RadiansToDegrees(FMath::Atan(Slope));
}

float FBeachProfileModel::ComposeHeight(const FBeachProfileParams& Params, float ShoreDistance, float BaseHeight)
{
	const float Slope = FMath::Max(Params.Slope, 0.0f);
	const float K = FMath::Max(Params.JoinSoftness, 0.01f);
	const float D = ShoreDistance;
	const float Ramp = Slope * D;

	if (D >= 0.0f)
	{
		const float Face = SmoothMin(Ramp, FMath::Max(Params.BermHeight, K), K);
		return FMath::Lerp(Face, BaseHeight, SmoothStep(Params.BlendStart, Params.BlendEnd, D));
	}

	// En el agua la cara sigue con la misma pendiente y, ya con algo de fondo, un poco más
	// empinada, hasta el fondo cercano a la orilla, que nunca es más somero que
	// NearshoreDepth; más lejos manda el fondo del relieve base.
	const float Offshore = -D;
	const float SteepenStart = Slope > 0.0f ? Params.UnderwaterSteepenDepth / Slope : 0.0f;
	const float Extra = Slope * FMath::Max(Params.UnderwaterSlopeScale - 1.0f, 0.0f);
	const float Face = Ramp - Extra * SmoothStepIntegral(SteepenStart, SteepenStart + FMath::Max(Params.UnderwaterSteepenLength, 0.01f), Offshore);
	const float Seabed = SmoothMin(BaseHeight, -Params.NearshoreDepth, K);
	const float Nearshore = SmoothMax(Face, Seabed, K);
	return FMath::Lerp(Nearshore, BaseHeight, SmoothStep(Params.OffshoreFadeStart, Params.OffshoreFadeEnd, -D));
}

float FBeachProfileModel::BeachAmount(const FBeachProfileParams& Params, float ShoreDistance)
{
	return ShoreDistance >= 0.0f
		? 1.0f - SmoothStep(Params.BlendStart, Params.BlendEnd, ShoreDistance)
		: 1.0f - SmoothStep(Params.OffshoreFadeStart, Params.OffshoreFadeEnd, -ShoreDistance);
}

FBeachTransectReport FBeachProfileModel::AnalyzeTransect(const TArray<float>& Heights, float Step, const FBeachTransectBands& Bands)
{
	FBeachTransectReport Report;
	if (!(Step > 0.0f) || Heights.Num() < 2)
	{
		return Report;
	}
	for (int32 I = 1; I < Heights.Num(); ++I)
	{
		if (Heights[I - 1] > 0.0f && Heights[I] <= 0.0f)
		{
			Report.WaterlineIndex = I;
			break;
		}
	}
	if (Report.WaterlineIndex == INDEX_NONE)
	{
		return Report;
	}
	Report.bHasWaterline = true;

	auto InFace = [&Bands](float H) { return FMath::IsFinite(H) && H >= Bands.FaceBottom && H <= Bands.FaceTop; };
	auto InShore = [&Bands](float H) { return H >= Bands.ShoreBottom && H <= Bands.ShoreTop; };

	int32 Lo = Report.WaterlineIndex - 1;
	int32 Hi = Report.WaterlineIndex;
	if (!InFace(Heights[Lo]) || !InFace(Heights[Hi]))
	{
		// La orilla es un salto mayor que la propia franja: todo el salto es la pendiente.
		Report.FaceSamples = 2;
		Report.MaxFaceSlope = (Heights[Lo] - Heights[Hi]) / Step;
		Report.MinShoreSlope = Report.MaxFaceSlope;
		return Report;
	}
	while (Lo > 0 && InFace(Heights[Lo - 1]))
	{
		--Lo;
	}
	while (Hi + 1 < Heights.Num() && InFace(Heights[Hi + 1]))
	{
		++Hi;
	}
	Report.FaceSamples = Hi - Lo + 1;

	float MinShoreSlope = TNumericLimits<float>::Max();
	for (int32 I = Lo; I < Hi; ++I)
	{
		const float Drop = Heights[I] - Heights[I + 1];
		Report.MaxFaceSlope = FMath::Max(Report.MaxFaceSlope, FMath::Abs(Drop) / Step);
		Report.MaxSeawardRise = FMath::Max(Report.MaxSeawardRise, -Drop);
		if (InShore(Heights[I]) && InShore(Heights[I + 1]))
		{
			MinShoreSlope = FMath::Min(MinShoreSlope, Drop / Step);
		}
	}
	Report.MinShoreSlope = MinShoreSlope == TNumericLimits<float>::Max() ? 0.0f : MinShoreSlope;

	const int32 K = FMath::Max(1, FMath::RoundToInt32(Bands.CurvatureBaseline / Step));
	const float Span = K * Step;
	for (int32 I = FMath::Max(Lo, K); I <= Hi && I + K < Heights.Num(); ++I)
	{
		if (InShore(Heights[I]))
		{
			const float Curvature = (Heights[I - K] - 2.0f * Heights[I] + Heights[I + K]) / (Span * Span);
			Report.MinShoreCurvature = FMath::Min(Report.MinShoreCurvature, Curvature);
		}
	}
	return Report;
}
