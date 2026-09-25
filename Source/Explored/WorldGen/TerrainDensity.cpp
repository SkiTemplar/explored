#include "WorldGen/TerrainDensity.h"

#include "Core/ExploredRandom.h"

namespace
{
	/** Distancia normalizada máxima a la que una isla influye (fin de la plataforma). */
	constexpr float InfluenceLimit = 1.9f;
	/** Profundidad de la plataforma somera junto a la costa. */
	constexpr float ShelfDepth = -1.2f;

	FORCEINLINE float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Terrazas suaves: cuantiza V en Steps escalones con transiciones de anchura Softness. */
	float Terrace(float V, int32 Steps, float Softness)
	{
		const float Scaled = V * Steps;
		const float Base = FMath::FloorToFloat(Scaled);
		const float Frac = Scaled - Base;
		return (Base + SmoothStep(0.5f - Softness, 0.5f + Softness, Frac)) / Steps;
	}

	FVector2D ToLocal(const FIslandDesc& Island, float X, float Y)
	{
		const FVector2D D = (FVector2D(X, Y) - Island.Center) / Island.Radius;
		const float C = FMath::Cos(-Island.Rotation);
		const float S = FMath::Sin(-Island.Rotation);
		return FVector2D(D.X * C - D.Y * S, D.X * S + D.Y * C);
	}

	float DistanceToSegment(const FVector& P, const FVector& A, const FVector& B, float& OutT)
	{
		const FVector AB = B - A;
		const float LenSq = AB.SizeSquared();
		OutT = LenSq > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(P - A, AB) / LenSq, 0.0f, 1.0f) : 0.0f;
		return FVector::Dist(P, A + AB * OutT);
	}

	struct FPalette
	{
		FLinearColor Sand;
		FLinearColor Grass;
		FLinearColor Rock;
	};

	FPalette PaletteFor(EIslandArchetype Archetype)
	{
		// Colores en sRGB convertidos a lineal para el color de vértice.
		auto C = [](uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); };
		switch (Archetype)
		{
		case EIslandArchetype::Landing: return {C(236, 214, 170), C(96, 158, 62), C(128, 118, 104)};
		case EIslandArchetype::Emerald: return {C(222, 200, 158), C(58, 138, 52), C(92, 104, 88)};
		case EIslandArchetype::Smoke: return {C(70, 64, 62), C(104, 118, 58), C(58, 46, 44)};
		case EIslandArchetype::Teeth: return {C(206, 196, 178), C(118, 140, 88), C(150, 148, 142)};
		case EIslandArchetype::Mangrove: return {C(150, 134, 102), C(92, 116, 66), C(98, 96, 84)};
		case EIslandArchetype::WhiteSands: return {C(248, 240, 222), C(128, 176, 78), C(196, 190, 176)};
		case EIslandArchetype::Mesa: return {C(214, 190, 144), C(170, 164, 76), C(160, 124, 92)};
		default: return {C(230, 210, 170), C(90, 150, 60), C(120, 115, 105)};
		}
	}
}

FTerrainDensity::FTerrainDensity(const FArchipelagoLayout& InLayout)
	: Layout(InLayout)
	, FloorNoise(InLayout.Seed ^ 0x1F123BB5u)
	, DetailNoise(InLayout.Seed ^ 0x5F356495u)
	, OverhangNoise(InLayout.Seed ^ 0x2C1B3C6Du)
{
	BuildCaves();
}

float FTerrainDensity::IslandHeight(const FIslandDesc& Island, float X, float Y, float& OutT) const
{
	const FExploredNoise N(Island.Seed);
	FVector2D Q = ToLocal(Island, X, Y);

	// Costa irregular: deformación del dominio a dos escalas.
	const FVector2D Warped = N.Warp2D(Q.X * 2.2f, Q.Y * 2.2f, 0.35f, 3) / 2.2f;
	Q = FMath::Lerp(Q, Warped, 0.8f);
	const float Coast = 1.0f + 0.12f * N.Fbm2D(Q.X * 5.0f + 11.0f, Q.Y * 5.0f - 7.0f, 3);
	const float T = Q.Size() / Coast;
	OutT = T;

	if (T >= InfluenceLimit)
	{
		return FArchipelagoLayout::OceanFloor;
	}

	// Perfil submarino común: plataforma somera, cresta de arrecife y talud.
	const float Floor = FArchipelagoLayout::OceanFloor;
	float Underwater = FMath::Lerp(ShelfDepth, Floor, SmoothStep(1.05f, InfluenceLimit, T));
	Underwater += 1.2f * FMath::Exp(-FMath::Square((T - 1.28f) / 0.05f));

	const float U = 1.0f - T;
	const float Hmax = Island.MaxHeight;
	float Land = -1000.0f;

	switch (Island.Archetype)
	{
	case EIslandArchetype::Landing:
	{
		const float Rise = FMath::Pow(SmoothStep(0.06f, 1.0f, U), 1.6f);
		const float Hills = 0.7f + 0.3f * N.Fbm2D(Q.X * 3.0f, Q.Y * 3.0f, 4);
		Land = 1.8f * SmoothStep(-0.02f, 0.08f, U) + (Hmax - 2.0f) * Rise * Hills;
		// Laguna protegida (lugar del amaraje) con bocana hacia el mar.
		const float Lagoon = FVector2D::Distance(Q, FVector2D(0.58f, 0.0f)) / 0.26f;
		const float Inlet = FMath::Abs(Q.Y) / 0.07f + FMath::Max(0.0f, 0.62f - Q.X) * 10.0f;
		const float Water = FMath::Min(Lagoon, Inlet);
		if (Water < 1.3f)
		{
			const float LagoonFloor = -5.0f + 3.5f * SmoothStep(0.3f, 1.0f, Water);
			Land = FMath::Lerp(LagoonFloor, Land, SmoothStep(0.85f, 1.3f, Water));
		}
		break;
	}
	case EIslandArchetype::Emerald:
	{
		const float Rise = FMath::Pow(SmoothStep(0.04f, 1.0f, U), 1.15f);
		const float Ridges = N.Ridged2D(Q.X * 2.4f + 3.0f, Q.Y * 2.4f, 5);
		Land = 1.5f * SmoothStep(-0.02f, 0.06f, U) + Hmax * Rise * (0.45f + 0.55f * Ridges);
		break;
	}
	case EIslandArchetype::Smoke:
	{
		const float Cone = FMath::Pow(FMath::Max(U, 0.0f), 1.35f);
		const float Flows = N.Ridged2D(Q.X * 6.0f, Q.Y * 6.0f, 3);
		Land = 1.2f * SmoothStep(-0.02f, 0.05f, U) + Hmax * Cone * (0.92f + 0.08f * Flows);
		// Cráter con borde marcado.
		const float Crater = 0.14f;
		if (T < Crater * 1.3f)
		{
			const float Bowl = 1.0f - SmoothStep(0.0f, Crater, T);
			Land -= Bowl * Hmax * 0.28f;
		}
		break;
	}
	case EIslandArchetype::Mesa:
	{
		const float Rise = SmoothStep(0.05f, 0.7f, U);
		const float Stepped = Terrace(Rise * (0.9f + 0.1f * N.Fbm2D(Q.X * 2.0f, Q.Y * 2.0f, 3)), 3, 0.18f);
		const float Plateau = 4.0f * N.Fbm2D(Q.X * 6.0f, Q.Y * 6.0f, 4);
		Land = 1.6f * SmoothStep(-0.02f, 0.06f, U) + Hmax * Stepped + Plateau * Rise;
		break;
	}
	case EIslandArchetype::Mangrove:
	{
		Land = 0.4f + 4.5f * SmoothStep(0.0f, 0.7f, U) + 1.2f * N.Fbm2D(Q.X * 6.0f, Q.Y * 6.0f, 4);
		// Canales sinuosos por debajo del nivel del mar.
		const float Channel = FMath::Abs(N.Fbm2D(Q.X * 3.5f + 40.0f, Q.Y * 3.5f, 4));
		const float ChannelMask = 1.0f - SmoothStep(0.03f, 0.08f, Channel);
		Land = FMath::Lerp(Land, -1.6f, ChannelMask * SmoothStep(0.0f, 0.15f, U));
		break;
	}
	case EIslandArchetype::WhiteSands:
	{
		// Anillo de arena alrededor de una laguna somera.
		const float Ring = FMath::Abs(T - 0.78f) / 0.14f;
		const float RingLand = 0.6f + Hmax * FMath::Square(FMath::Max(0.0f, 1.0f - Ring));
		const float LagoonFloor = -3.5f - 1.5f * SmoothStep(0.64f, 0.2f, T);
		Land = Ring < 1.0f ? RingLand : (T < 0.78f ? FMath::Lerp(RingLand, LagoonFloor, SmoothStep(1.0f, 1.8f, Ring)) : -1000.0f);
		// Pasos entre el anillo y la laguna (motus).
		const float Gap = N.Fbm2D(Q.X * 4.0f, Q.Y * 4.0f, 2);
		if (Ring < 1.2f && Gap > 0.35f)
		{
			Land = FMath::Lerp(Land, -0.8f, SmoothStep(0.35f, 0.5f, Gap));
		}
		break;
	}
	case EIslandArchetype::Teeth:
	{
		float Best = -1000.0f;
		for (int32 I = 0; I < Island.Islets.Num(); ++I)
		{
			const FVector2D Center = Island.Islets[I] / Island.Radius;
			const float R = 0.13f + 0.07f * ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Island.Seed, I, 0));
			const float D = FVector2D::Distance(Q, Center) / R;
			if (D < 1.6f)
			{
				const float Stack = FMath::Pow(FMath::Max(0.0f, 1.0f - D), 0.45f);
				const float Height = Hmax * (0.5f + 0.5f * ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Island.Seed, I, 1)));
				Best = FMath::Max(Best, D < 1.0f ? 1.0f + Height * Stack : FMath::Lerp(-2.5f, 1.0f, 1.6f - D));
			}
		}
		Land = FMath::Max(Best, -3.0f + 1.5f * N.Fbm2D(Q.X * 5.0f, Q.Y * 5.0f, 3));
		// Entre islotes siempre hay agua somera, no tierra.
		Underwater = FMath::Max(Underwater, -3.0f);
		return T < 1.0f ? Land : FMath::Max(Underwater, Land * (1.0f - SmoothStep(1.0f, 1.2f, T)));
	}
	default:
		break;
	}

	if (T >= 1.0f)
	{
		// Transición suave entre la orilla y la plataforma.
		return FMath::Lerp(FMath::Min(Land, 0.0f) + ShelfDepth, Underwater, SmoothStep(1.0f, 1.12f, T));
	}
	if (Land < -500.0f)
	{
		return Underwater;
	}
	return FMath::Lerp(Underwater, Land, SmoothStep(-0.02f, 0.03f, U));
}

FTerrainColumn FTerrainDensity::SampleColumn(float X, float Y) const
{
	FTerrainColumn Column;
	Column.Height = FArchipelagoLayout::OceanFloor + 6.0f * FloorNoise.Fbm2D(X / 420.0f, Y / 420.0f, 4);

	for (int32 I = 0; I < Layout.Islands.Num(); ++I)
	{
		const FIslandDesc& Island = Layout.Islands[I];
		const float Reach = Island.Radius * (InfluenceLimit + 0.4f);
		if (FVector2D::DistSquared(Island.Center, FVector2D(X, Y)) > Reach * Reach)
		{
			continue;
		}

		float T = 0.0f;
		const float H = IslandHeight(Island, X, Y, T);
		if (H > Column.Height)
		{
			Column.Height = H;
		}
		if (T < Column.NormalizedDistance)
		{
			Column.NormalizedDistance = T;
			Column.IslandIndex = I;
		}
	}

	// Microrrelieve en tierra firme.
	if (Column.Height > 0.5f)
	{
		Column.Height += 0.6f * DetailNoise.Fbm2D(X / 14.0f, Y / 14.0f, 3);
	}
	return Column;
}

float FTerrainDensity::DensityWithColumn(const FVector& P, const FTerrainColumn& Column) const
{
	float D = P.Z - Column.Height;

	// Voladizos y roca irregular por encima de las playas.
	const float RockMask = SmoothStep(3.0f, 18.0f, Column.Height);
	if (RockMask > 0.0f && FMath::Abs(D) < OverhangAmplitude * 3.0f)
	{
		D += RockMask * OverhangAmplitude * OverhangNoise.Fbm3D(P.X / 16.0f, P.Y / 16.0f, P.Z / 10.0f, 3);
	}

	if (!Caves.IsEmpty())
	{
		D = FMath::Max(D, -CaveCarve(P));
	}
	return D;
}

float FTerrainDensity::Density(const FVector& P) const
{
	return DensityWithColumn(P, SampleColumn(P.X, P.Y));
}

FVector FTerrainDensity::Normal(const FVector& P, float Step) const
{
	const float Dx = Density(P + FVector(Step, 0, 0)) - Density(P - FVector(Step, 0, 0));
	const float Dy = Density(P + FVector(0, Step, 0)) - Density(P - FVector(0, Step, 0));
	const float Dz = Density(P + FVector(0, 0, Step)) - Density(P - FVector(0, 0, Step));
	return FVector(Dx, Dy, Dz).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
}

float FTerrainDensity::CaveCarve(const FVector& P) const
{
	float Best = TNumericLimits<float>::Max();
	for (const FCaveDesc& Cave : Caves)
	{
		// Descarte rápido por caja.
		const FBox Bounds = FBox(
			FVector::Min(Cave.Start, Cave.End) - FVector(Cave.Radius * 2.0f),
			FVector::Max(Cave.Start, Cave.End) + FVector(Cave.Radius * 2.0f));
		if (!Bounds.IsInside(P))
		{
			continue;
		}

		float T = 0.0f;
		const float Dist = DistanceToSegment(P, Cave.Start, Cave.End, T);
		const FExploredNoise N(Cave.Seed);
		// La boca es algo más ancha y el fondo se estrecha.
		const float Taper = FMath::Lerp(1.25f, 0.7f, T);
		const float Wobble = 0.3f * N.Fbm3D(P.X / 6.0f, P.Y / 6.0f, P.Z / 6.0f, 2);
		Best = FMath::Min(Best, Dist - Cave.Radius * (Taper + Wobble));
	}
	return Best;
}

void FTerrainDensity::BuildCaves()
{
	FExploredRandom Rng(static_cast<uint64>(Layout.Seed) ^ 0xCAFEF00DULL);

	for (const FIslandDesc& Island : Layout.Islands)
	{
		int32 Count = 0;
		float EntranceFraction = 0.45f;
		switch (Island.Archetype)
		{
		case EIslandArchetype::Emerald: Count = 3; EntranceFraction = 0.35f; break;
		case EIslandArchetype::Smoke: Count = 2; EntranceFraction = 0.25f; break;
		case EIslandArchetype::Mesa: Count = 3; EntranceFraction = 0.3f; break;
		case EIslandArchetype::Landing: Count = 1; EntranceFraction = 0.4f; break;
		default: break;
		}

		for (int32 I = 0; I < Count; ++I)
		{
			// Busca, en una dirección aleatoria desde el centro, la ladera a la altura deseada.
			const float Angle = Rng.RangeFloat(0.0f, UE_TWO_PI);
			const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			const float TargetHeight = Island.MaxHeight * EntranceFraction;
			FVector2D Entrance = Island.Center;
			for (float R = 0.0f; R < Island.Radius; R += 4.0f)
			{
				const FVector2D P = Island.Center + Dir * R;
				float T = 0.0f;
				if (IslandHeight(Island, P.X, P.Y, T) < TargetHeight)
				{
					Entrance = P;
					break;
				}
			}

			float T = 0.0f;
			const float EntranceHeight = IslandHeight(Island, Entrance.X, Entrance.Y, T);
			FCaveDesc Cave;
			Cave.Radius = Rng.RangeFloat(3.5f, 6.0f);
			Cave.Start = FVector(Entrance.X, Entrance.Y, EntranceHeight + Cave.Radius * 0.3f) - FVector(Dir, 0.0) * 2.0f;
			const float Length = Rng.RangeFloat(35.0f, 70.0f);
			Cave.End = Cave.Start - FVector(Dir, 0.0) * Length - FVector(0, 0, Rng.RangeFloat(2.0f, 8.0f));
			Cave.Seed = Rng.NextUInt32();
			Caves.Add(Cave);
		}

		// Arcos marinos en Los Dientes: túneles horizontales a nivel del mar.
		if (Island.Archetype == EIslandArchetype::Teeth)
		{
			for (int32 I = 0; I < FMath::Min(3, Island.Islets.Num()); ++I)
			{
				const FVector2D C = Island.Center + Island.Islets[I];
				const float Angle = Rng.RangeFloat(0.0f, UE_TWO_PI);
				const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
				FCaveDesc Arch;
				Arch.Radius = Rng.RangeFloat(5.0f, 8.0f);
				Arch.Start = FVector(C - Dir * 40.0f, 1.5);
				Arch.End = FVector(C + Dir * 40.0f, 1.5);
				Arch.Seed = Rng.NextUInt32();
				Caves.Add(Arch);
			}
		}
	}
}

FLinearColor FTerrainDensity::SurfaceColor(const FVector& P, const FVector& InNormal) const
{
	const FTerrainColumn Column = SampleColumn(P.X, P.Y);
	const EIslandArchetype Archetype = Column.IslandIndex != INDEX_NONE
		? Layout.Islands[Column.IslandIndex].Archetype
		: EIslandArchetype::Landing;
	const FPalette Palette = PaletteFor(Archetype);

	const float Variation = DetailNoise.Fbm2D(P.X / 30.0f + 100.0f, P.Y / 30.0f, 3);
	const float Z = P.Z;

	// Pesos: arena cerca del agua, roca en pendiente o bajo tierra, hierba en el resto.
	const float SandLine = 2.2f + 1.2f * Variation;
	float Sand = 1.0f - SmoothStep(SandLine - 0.8f, SandLine + 0.8f, Z);
	float Rock = 1.0f - SmoothStep(0.55f, 0.78f, InNormal.Z);
	// Interior de cuevas y voladizos: roca.
	if (Z < Column.Height - 2.0f)
	{
		Rock = 1.0f;
	}
	if (Archetype == EIslandArchetype::Smoke)
	{
		// Ladera alta volcánica: roca y ceniza.
		Rock = FMath::Max(Rock, SmoothStep(120.0f, 220.0f, Z));
	}
	Sand *= 1.0f - Rock;
	const float Grass = FMath::Max(0.0f, 1.0f - Sand - Rock);

	FLinearColor Color = Palette.Sand * Sand + Palette.Grass * Grass + Palette.Rock * Rock;

	// Arena mojada junto a la orilla y fondo marino más claro en someros.
	if (Z < 0.6f)
	{
		const float Wet = SmoothStep(0.6f, -0.2f, Z);
		Color = FMath::Lerp(Color, Color * 0.72f, Wet);
	}
	if (Z < -0.5f)
	{
		const float Depth = SmoothStep(-0.5f, -25.0f, Z);
		const FLinearColor Seabed = FMath::Lerp(Palette.Sand * 1.05f, Palette.Sand * 0.45f, Depth);
		Color = FMath::Lerp(Color, Seabed, SmoothStep(-0.5f, -2.0f, Z) * (1.0f - Rock * 0.5f));
	}

	// Variación suave de tono para romper la uniformidad.
	Color *= 0.92f + 0.16f * (Variation * 0.5f + 0.5f);
	Color.A = Rock;
	return Color;
}

void FTerrainDensity::HeightBounds(const FBox2D& Rect, float SampleSpacing, float& OutMin, float& OutMax) const
{
	OutMin = TNumericLimits<float>::Max();
	OutMax = TNumericLimits<float>::Lowest();
	for (float X = Rect.Min.X; X <= Rect.Max.X + KINDA_SMALL_NUMBER; X += SampleSpacing)
	{
		for (float Y = Rect.Min.Y; Y <= Rect.Max.Y + KINDA_SMALL_NUMBER; Y += SampleSpacing)
		{
			const float H = SampleColumn(X, Y).Height;
			OutMin = FMath::Min(OutMin, H);
			OutMax = FMath::Max(OutMax, H);
		}
	}

	// Margen por el muestreo discreto, el ruido 3D y las cuevas.
	OutMin -= 12.0f;
	OutMax += 6.0f;
	for (const FCaveDesc& Cave : Caves)
	{
		const FBox2D CaveRect(
			FVector2D(FMath::Min(Cave.Start.X, Cave.End.X), FMath::Min(Cave.Start.Y, Cave.End.Y)) - FVector2D(Cave.Radius * 2.0f),
			FVector2D(FMath::Max(Cave.Start.X, Cave.End.X), FMath::Max(Cave.Start.Y, Cave.End.Y)) + FVector2D(Cave.Radius * 2.0f));
		if (CaveRect.Intersect(Rect))
		{
			OutMin = FMath::Min(OutMin, static_cast<float>(FMath::Min(Cave.Start.Z, Cave.End.Z)) - Cave.Radius * 2.0f);
		}
	}
}
