#include "WorldGen/IslandShelfModel.h"

#include "Core/ExploredNoise.h"
#include "WorldGen/IslandShapeModel.h"

namespace
{
	FORCEINLINE float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Anchuras (en T) y cotas extremas de la plataforma y del talud. */
	constexpr float MinWidth = 0.05f;
	constexpr float WidthRange = 0.36f;
	/**
	 * Borde hondo: la rampa de la plataforma ya baja con una pendiente parecida a la del talud,
	 * así que no queda la mesa turquesa con un escalón oscuro alrededor.
	 */
	constexpr float ShallowEdge = -7.0f;
	constexpr float EdgeRange = 11.0f;
	constexpr float MinSlope = 0.2f;
	constexpr float SlopeRange = 0.35f;
	/** Cota del borde en el lóbulo que recoge un cayo: somero, pero sin asomar como bajío suelto. */
	constexpr float CayLobeEdge = -6.0f;

	/** Ruido del perímetro llevado a [0, 1] con buen contraste (costas anchas y costas estrechas). */
	float Spread(const FExploredNoise& N, float Angle, float Frequency, float Offset)
	{
		return SmoothStep(-0.35f, 0.35f, FIslandShapeModel::AroundCoast(N, Angle, Frequency, Offset, 2));
	}
}

float FIslandShelf::Sample(const TArray<float>& Table, float Angle) const
{
	if (Table.Num() != Bins || !FMath::IsFinite(Angle))
	{
		return Table.IsEmpty() ? 0.0f : Table[0];
	}
	const float Unit = FMath::Frac((Angle + UE_PI) / UE_TWO_PI);
	const float Pos = Unit * Bins;
	const int32 I0 = FMath::Clamp(FMath::FloorToInt32(Pos), 0, Bins - 1);
	const int32 I1 = (I0 + 1) % Bins;
	return FMath::Lerp(Table[I0], Table[I1], Pos - I0);
}

FIslandShelf FIslandShelfModel::Build(uint32 Seed, const TArray<FShelfCay>& Cays)
{
	FIslandShelf Shelf;
	const FExploredNoise N(Seed ^ 0x51E1Fu);
	Shelf.Widths.SetNum(FIslandShelf::Bins);
	Shelf.Edges.SetNum(FIslandShelf::Bins);
	Shelf.Slopes.SetNum(FIslandShelf::Bins);
	for (int32 B = 0; B < FIslandShelf::Bins; ++B)
	{
		const float Angle = -UE_PI + UE_TWO_PI * B / FIslandShelf::Bins;
		float Width = MinWidth + WidthRange * Spread(N, Angle, 1.1f, 3.0f);
		float Edge = ShallowEdge - EdgeRange * Spread(N, Angle, 1.4f, -9.0f);
		for (const FShelfCay& Cay : Cays)
		{
			// Lóbulo redondeado de la plataforma que recoge el cayo (no un dedo estrecho).
			const float Bell = SmoothStep(FMath::Max(Cay.HalfWidth, 0.02f), 0.0f, FMath::Abs(FMath::FindDeltaAngleRadians(Angle, Cay.Angle)));
			Width = FMath::Max(Width, (Cay.T - 1.0f + 0.12f) * FMath::Sqrt(Bell));
			Edge = FMath::Lerp(Edge, FMath::Max(Edge, CayLobeEdge), Bell);
		}
		Width = FMath::Clamp(Width, MinWidth, MaxReach - 1.0f - 0.05f);
		Shelf.Widths[B] = Width;
		Shelf.Edges[B] = Edge;
		Shelf.Slopes[B] = FMath::Clamp(MinSlope + SlopeRange * Spread(N, Angle, 1.2f, 17.0f), 0.05f, MaxReach - 1.0f - Width);
	}
	return Shelf;
}

float FIslandShelfModel::Profile(const FIslandShelf& Shelf, float Angle, float T, float Floor, float FootDepth, float Detail)
{
	if (!FMath::IsFinite(T) || T <= 1.0f)
	{
		return FootDepth;
	}
	const float Width = FMath::Max(Shelf.Width(Angle), 0.01f);
	const float Edge = FMath::Min(Shelf.EdgeDepth(Angle), FootDepth - 0.5f);
	const float X = (T - 1.0f) / Width;
	if (X <= 1.0f)
	{
		// Rampa que se empina hacia el borde (nunca plana) con un rizado proporcional a la
		// profundidad: el fondo de la plataforma no forma bajíos sueltos.
		float H = FootDepth + (Edge - FootDepth) * X * (0.45f + 0.55f * X);
		H += (FMath::IsFinite(Detail) ? FMath::Clamp(Detail, -1.0f, 1.0f) : 0.0f) * 0.035f * FMath::Abs(H) * SmoothStep(0.0f, 0.3f, X);
		return H;
	}
	// Talud: Hermite desde el borde con la pendiente de la rampa hasta el fondo, sin arista.
	// Fondo no finito: el talud se queda en la cota del borde (FMath::Min no descarta NaN con /fp:fast).
	const float Target = FMath::IsFinite(Floor) ? FMath::Min(Floor, Edge) : Edge;
	const float Slope = FMath::Max(Shelf.SlopeWidth(Angle), 0.02f);
	const float Y = FMath::Min((X - 1.0f) * Width / Slope, 1.0f);
	const float Drop = Target - Edge;
	const float StartSlope = Drop < -0.1f ? FMath::Clamp((Edge - FootDepth) * 1.55f / Width * Slope / Drop, 0.0f, 2.0f) : 0.0f;
	const float Y2 = Y * Y;
	const float Y3 = Y2 * Y;
	const float Shape = (Y3 - 2.0f * Y2 + Y) * StartSlope + (3.0f * Y2 - 2.0f * Y3);
	return Edge + Drop * Shape;
}
