#include "Cartography/MapStroke.h"

namespace MapStroke
{
	double DistanceToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double LenSq = AB.SizeSquared();
		if (LenSq <= UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector2D::Distance(P, A);
		}
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / LenSq, 0.0, 1.0);
		return FVector2D::Distance(P, A + AB * T);
	}

	double DistanceToPolyline(const FVector2D& P, const TArray<FVector2D>& Points, bool bClosed)
	{
		if (Points.Num() == 0)
		{
			return TNumericLimits<double>::Max();
		}
		if (Points.Num() == 1)
		{
			return FVector2D::Distance(P, Points[0]);
		}
		double Best = TNumericLimits<double>::Max();
		const int32 Segments = bClosed ? Points.Num() : Points.Num() - 1;
		for (int32 I = 0; I < Segments; ++I)
		{
			Best = FMath::Min(Best, DistanceToSegment(P, Points[I], Points[(I + 1) % Points.Num()]));
		}
		return Best;
	}

	double Length(const TArray<FVector2D>& Points, bool bClosed)
	{
		double Total = 0.0;
		for (int32 I = 1; I < Points.Num(); ++I)
		{
			Total += FVector2D::Distance(Points[I - 1], Points[I]);
		}
		if (bClosed && Points.Num() > 2)
		{
			Total += FVector2D::Distance(Points.Last(), Points[0]);
		}
		return Total;
	}

	TArray<FVector2D> Simplify(const TArray<FVector2D>& Points, double Tolerance)
	{
		if (Points.Num() <= 2 || Tolerance <= 0.0)
		{
			return Points;
		}

		// Marca de puntos conservados (uint8 y no bool: TArray<bool> no es contiguo en todas partes).
		TArray<uint8> Keep;
		Keep.Init(0, Points.Num());
		Keep[0] = 1;
		Keep.Last() = 1;

		TArray<TPair<int32, int32>> Stack;
		Stack.Add(TPair<int32, int32>(0, Points.Num() - 1));
		while (Stack.Num() > 0)
		{
			const TPair<int32, int32> Range = Stack.Pop();
			const int32 First = Range.Key;
			const int32 Last = Range.Value;
			double WorstDistance = -1.0;
			int32 Worst = INDEX_NONE;
			for (int32 I = First + 1; I < Last; ++I)
			{
				const double D = DistanceToSegment(Points[I], Points[First], Points[Last]);
				if (D > WorstDistance)
				{
					WorstDistance = D;
					Worst = I;
				}
			}
			if (Worst != INDEX_NONE && WorstDistance > Tolerance)
			{
				Keep[Worst] = 1;
				Stack.Add(TPair<int32, int32>(First, Worst));
				Stack.Add(TPair<int32, int32>(Worst, Last));
			}
		}

		TArray<FVector2D> Result;
		for (int32 I = 0; I < Points.Num(); ++I)
		{
			if (Keep[I] != 0)
			{
				Result.Add(Points[I]);
			}
		}
		return Result;
	}

	TArray<FVector2D> SimplifyClosed(const TArray<FVector2D>& Points, double Tolerance)
	{
		if (Points.Num() <= 3)
		{
			return Points;
		}
		// Se parte el anillo por el punto más alejado del primero: así ninguna de las dos
		// mitades degenera en un segmento de longitud cero.
		int32 Far = 0;
		double FarDistance = -1.0;
		for (int32 I = 1; I < Points.Num(); ++I)
		{
			const double D = FVector2D::DistSquared(Points[0], Points[I]);
			if (D > FarDistance)
			{
				FarDistance = D;
				Far = I;
			}
		}
		TArray<FVector2D> FirstHalf;
		TArray<FVector2D> SecondHalf;
		for (int32 I = 0; I <= Far; ++I)
		{
			FirstHalf.Add(Points[I]);
		}
		for (int32 I = Far; I < Points.Num(); ++I)
		{
			SecondHalf.Add(Points[I]);
		}
		SecondHalf.Add(Points[0]);

		TArray<FVector2D> Result = Simplify(FirstHalf, Tolerance);
		const TArray<FVector2D> Tail = Simplify(SecondHalf, Tolerance);
		// La cola empieza en Far (ya incluido) y acaba en el primer punto (no se repite).
		for (int32 I = 1; I < Tail.Num() - 1; ++I)
		{
			Result.Add(Tail[I]);
		}
		return Result;
	}

	void Smooth(TArray<FVector2D>& Points, double Alpha, int32 Iterations)
	{
		if (Points.Num() <= 2)
		{
			return;
		}
		TArray<FVector2D> Previous;
		for (int32 Pass = 0; Pass < Iterations; ++Pass)
		{
			Previous = Points;
			for (int32 I = 1; I < Points.Num() - 1; ++I)
			{
				const FVector2D Mean = (Previous[I - 1] + Previous[I + 1]) * 0.5;
				Points[I] = Previous[I] + (Mean - Previous[I]) * Alpha;
			}
		}
	}

	TArray<FVector2D> Resample(const TArray<FVector2D>& Points, double Spacing, bool bClosed)
	{
		if (Points.Num() < 2 || Spacing <= 0.0)
		{
			return Points;
		}
		TArray<FVector2D> Path = Points;
		if (bClosed)
		{
			Path.Add(Points[0]);
		}

		TArray<FVector2D> Result;
		Result.Add(Path[0]);
		double Carry = 0.0;
		for (int32 I = 1; I < Path.Num(); ++I)
		{
			const FVector2D A = Path[I - 1];
			const FVector2D B = Path[I];
			const double SegmentLength = FVector2D::Distance(A, B);
			double Along = Spacing - Carry;
			while (Along <= SegmentLength)
			{
				Result.Add(A + (B - A) * (Along / SegmentLength));
				Along += Spacing;
			}
			Carry = SegmentLength - (Along - Spacing);
		}
		if (bClosed)
		{
			// El último punto remuestreado puede coincidir con el primero al cerrar el anillo.
			if (Result.Num() > 1 && FVector2D::Distance(Result.Last(), Result[0]) < Spacing * 0.5)
			{
				Result.Pop();
			}
		}
		else if (FVector2D::Distance(Result.Last(), Path.Last()) > Spacing * 0.01)
		{
			Result.Add(Path.Last());
		}
		return Result;
	}
}
