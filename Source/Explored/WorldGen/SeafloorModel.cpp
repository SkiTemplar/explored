#include "WorldGen/SeafloorModel.h"

#include "Core/ExploredRandom.h"

namespace
{
	/** Profundidad media de la cresta de la dorsal que une la cadena. */
	constexpr float RidgeDepth = -26.0f;
	/** Anchura característica (m) de la dorsal: caída gaussiana, sin meseta en la cresta. */
	constexpr float RidgeWidth = 650.0f;
	/**
	 * La llanura abisal sigue bajando desde AbyssStart hasta AbyssEnd (más allá del borde del
	 * mundo, así que dentro nunca se aplana), como mucho AbyssExtraDepth metros: fuera del
	 * mundo el fondo queda acotado y FindCandidateChunks no ve alturas absurdas.
	 */
	constexpr float AbyssStart = 1100.0f;
	constexpr float AbyssEnd = 5000.0f;
	constexpr float AbyssExtraDepth = 25.0f;

	float DistanceToPolyline(const TArray<FVector2D>& Points, const FVector2D& P)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 I = 0; I + 1 < Points.Num(); ++I)
		{
			const FVector2D A = Points[I];
			const FVector2D AB = Points[I + 1] - A;
			const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0), 0.0, 1.0);
			Best = FMath::Min(Best, static_cast<float>(FVector2D::Distance(P, A + AB * T)));
		}
		return Best;
	}

	bool IsFinite2D(float X, float Y)
	{
		return FMath::IsFinite(X) && FMath::IsFinite(Y);
	}
}

FSeafloorModel::FSeafloorModel(const FArchipelagoLayout& Layout)
	: Spine(Layout.Spine)
	, Seamounts(GenerateSeamounts(Layout))
	// Misma semilla que el fondo de antes: la llanura abisal conserva su carácter.
	, FloorNoise(Layout.Seed ^ 0x1F123BB5u)
	, ReliefNoise(Layout.Seed ^ 0x6A09E667u)
{
}

float FSeafloorModel::BaseFloorHeight(float X, float Y) const
{
	if (!IsFinite2D(X, Y))
	{
		return FArchipelagoLayout::OceanFloor;
	}
	float H = FArchipelagoLayout::OceanFloor + 6.0f * FloorNoise.Fbm2D(X / 420.0f, Y / 420.0f, 4);
	if (Spine.Num() >= 2)
	{
		const float Distance = DistanceToPolyline(Spine, FVector2D(X, Y)) * (1.0f + 0.35f * FloorNoise.Fbm2D(X / 300.0f + 50.0f, Y / 300.0f, 3));
		// Cresta con altibajos propios: antes se recortaba a -26 m en 250 m a cada lado.
		const float Crest = RidgeDepth + 5.0f * FloorNoise.Fbm2D(X / 260.0f - 17.0f, Y / 260.0f + 9.0f, 3);
		H = FMath::Lerp(H, Crest, FMath::Exp(-FMath::Square(Distance / RidgeWidth)));
		H -= AbyssExtraDepth * FMath::SmoothStep(AbyssStart, AbyssEnd, Distance);
	}
	// Ondulaciones de arena y lomos suaves: el fondo nunca es una lámina.
	H += 0.8f * (ReliefNoise.Ridged2D(X / 150.0f, Y / 150.0f, 3) - 0.5f) + 0.5f * ReliefNoise.Fbm2D(X / 55.0f + 31.0f, Y / 55.0f, 2);
	return H;
}

float FSeafloorModel::FloorHeight(float X, float Y) const
{
	float H = BaseFloorHeight(X, Y);
	if (!IsFinite2D(X, Y))
	{
		return H;
	}
	for (const FSeamountDesc& Mount : Seamounts)
	{
		const float Bound = Mount.Radius * 1.3f / FMath::Min(Mount.Aspect, 1.0f);
		if (FVector2D::DistSquared(Mount.Center, FVector2D(X, Y)) < Bound * Bound)
		{
			H = SeamountHeight(Mount, X, Y, H);
		}
	}
	return H;
}

float FSeafloorModel::SeamountHeight(const FSeamountDesc& Mount, float X, float Y, float Base) const
{
	const FVector2D D = FVector2D(X, Y) - Mount.Center;
	const float C = FMath::Cos(-Mount.Angle);
	const float S = FMath::Sin(-Mount.Angle);
	const FVector2D Local(D.X * C - D.Y * S, (D.X * S + D.Y * C) / Mount.Aspect);
	const FExploredNoise N(Mount.Seed);
	const float R = Local.Size() / Mount.Radius * FMath::Max(0.8f, 1.0f + 0.25f * N.Fbm2D(Local.X / 70.0f, Local.Y / 70.0f, 3));
	if (R >= 1.0f)
	{
		return Base;
	}
	const float Shape = 1.0f - FMath::SmoothStep(0.0f, 1.0f, R);
	float Top = Mount.Peak;
	if (Mount.IsIslet())
	{
		// Farallón: roca quebrada en lo alto, que se apaga hacia el pedestal.
		Top += 2.5f * N.Ridged2D(Local.X / 18.0f, Local.Y / 18.0f, 3) * FMath::Square(Shape);
	}
	return Base + (Top - Base) * Shape;
}

float FSeafloorModel::BlendIslandToFloor(float IslandHeight, float Floor, float ReachFraction)
{
	return FMath::Lerp(IslandHeight, Floor, FMath::SmoothStep(0.8f, 1.0f, ReachFraction));
}

float FSeafloorModel::CayHeight(const FCayDesc& Cay, float X, float Y, float Wobble, float Floor)
{
	const FVector2D D = FVector2D(X, Y) - Cay.Center;
	const float C = FMath::Cos(-Cay.Angle);
	const float S = FMath::Sin(-Cay.Angle);
	const FVector2D L(D.X * C - D.Y * S, (D.X * S + D.Y * C) / Cay.Aspect);
	const float R = L.Size() * (1.0f + 0.3f * Wobble) / Cay.Radius;
	if (!(R < 3.0f))
	{
		return Floor;
	}
	// Falda sumergida: arena somera que baja hasta el fondo que ya hay y se funde con él.
	const float Skirt = FMath::Lerp(-0.8f, FMath::Min(Floor, -0.8f), FMath::SmoothStep(1.0f, 3.0f, R));
	if (R >= 1.0f)
	{
		return FMath::Max(Skirt, Floor);
	}
	const float Land = Cay.bRocky
		? -2.0f + (Cay.Height + 2.0f) * FMath::Pow(FMath::Max(0.0f, 1.0f - R * R), 0.35f)
		: -0.6f + (Cay.Height + 0.6f) * (1.0f - R * R);
	return FMath::Max(Land, Skirt);
}

namespace
{
	bool IsClearOfLayout(const FArchipelagoLayout& Layout, const TArray<FSeamountDesc>& Placed, const FVector2D& Center, float Radius)
	{
		const float Edge = FArchipelagoLayout::WorldHalfExtent - Radius - 150.0f;
		if (FMath::Abs(Center.X) > Edge || FMath::Abs(Center.Y) > Edge)
		{
			return false;
		}
		for (const FIslandDesc& Island : Layout.Islands)
		{
			if (FVector2D::Distance(Island.Center, Center) < Island.Radius * FSeafloorModel::IslandClearance + Radius)
			{
				return false;
			}
			for (const FCayDesc& Cay : Island.Cays)
			{
				if (FVector2D::Distance(Cay.Center, Center) < Cay.Radius * 3.0f + Radius + 60.0f)
				{
					return false;
				}
			}
		}
		return !Placed.ContainsByPredicate([&](const FSeamountDesc& M)
		{
			return FVector2D::Distance(M.Center, Center) < M.Radius + Radius + 400.0f;
		});
	}

	void PlaceSeamounts(const FArchipelagoLayout& Layout, FExploredRandom& Rng, int32 Wanted, bool bIslets, TArray<FSeamountDesc>& Placed)
	{
		const float E = FArchipelagoLayout::WorldHalfExtent;
		int32 Count = 0;
		for (int32 Attempt = 0; Attempt < 400 && Count < Wanted; ++Attempt)
		{
			FSeamountDesc Mount;
			Mount.Center = FVector2D(Rng.RangeFloat(-E, E), Rng.RangeFloat(-E, E));
			Mount.Radius = bIslets ? Rng.RangeFloat(110.0f, 160.0f) : Rng.RangeFloat(140.0f, 260.0f);
			Mount.Peak = bIslets ? Rng.RangeFloat(6.0f, 14.0f) : Rng.RangeFloat(-24.0f, -9.0f);
			Mount.Aspect = Rng.RangeFloat(bIslets ? 0.7f : 0.35f, bIslets ? 1.0f : 0.8f);
			Mount.Angle = Rng.RangeFloat(0.0f, UE_PI);
			Mount.Seed = Rng.NextUInt32();
			if (IsClearOfLayout(Layout, Placed, Mount.Center, Mount.Radius / Mount.Aspect))
			{
				Placed.Add(Mount);
				++Count;
			}
		}
	}
}

TArray<FSeamountDesc> FSeafloorModel::GenerateSeamounts(const FArchipelagoLayout& Layout)
{
	TArray<FSeamountDesc> Placed;
	if (Layout.Islands.IsEmpty())
	{
		return Placed;
	}
	FExploredRandom Rng(static_cast<uint64>(Layout.Seed) ^ 0x5EA3047ULL);
	PlaceSeamounts(Layout, Rng, 2, true, Placed);
	PlaceSeamounts(Layout, Rng, 5, false, Placed);
	return Placed;
}
