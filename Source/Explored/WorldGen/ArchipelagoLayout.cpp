#include "WorldGen/ArchipelagoLayout.h"

#include "Core/ExploredRandom.h"

namespace
{
	struct FArchetypeTemplate
	{
		EIslandArchetype Archetype;
		float MinRadius;
		float MaxRadius;
		float MinHeight;
		float MaxHeight;
	};

	// Orden de colocación: primero las grandes, que son las más difíciles de encajar.
	const FArchetypeTemplate Templates[] = {
		{EIslandArchetype::Mesa, 720.0f, 820.0f, 230.0f, 280.0f},
		{EIslandArchetype::Emerald, 680.0f, 780.0f, 160.0f, 200.0f},
		{EIslandArchetype::Smoke, 600.0f, 680.0f, 380.0f, 440.0f},
		{EIslandArchetype::Mangrove, 560.0f, 640.0f, 10.0f, 16.0f},
		{EIslandArchetype::WhiteSands, 480.0f, 560.0f, 7.0f, 10.0f},
		{EIslandArchetype::Teeth, 380.0f, 440.0f, 70.0f, 100.0f},
	};

	bool Overlaps(const FIslandDesc& A, FVector2D Center, float Radius)
	{
		const float MinDistance = A.Radius + Radius + FArchipelagoLayout::MinChannel;
		return FVector2D::DistSquared(A.Center, Center) < FMath::Square(MinDistance);
	}

	void GenerateIslets(FIslandDesc& Island, FExploredRandom& Rng)
	{
		const int32 Count = Rng.RangeInt(5, 8);
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector2D P = Rng.InsideUnitDisc() * Island.Radius * 0.7f;
			Island.Islets.Add(P);
		}
	}
}

const TCHAR* LexToString(EIslandArchetype Archetype)
{
	switch (Archetype)
	{
	case EIslandArchetype::Landing: return TEXT("Landing");
	case EIslandArchetype::Emerald: return TEXT("Emerald");
	case EIslandArchetype::Smoke: return TEXT("Smoke");
	case EIslandArchetype::Teeth: return TEXT("Teeth");
	case EIslandArchetype::Mangrove: return TEXT("Mangrove");
	case EIslandArchetype::WhiteSands: return TEXT("WhiteSands");
	case EIslandArchetype::Mesa: return TEXT("Mesa");
	default: return TEXT("Unknown");
	}
}

FArchipelagoLayout FArchipelagoLayout::Generate(uint32 InSeed)
{
	// Reintenta con semillas derivadas hasta encontrar una disposición válida.
	for (uint32 Attempt = 0; Attempt < 64; ++Attempt)
	{
		FExploredRandom Rng(static_cast<uint64>(InSeed) * 0x9E3779B97F4A7C15ULL + Attempt);
		FArchipelagoLayout Layout;
		Layout.Seed = InSeed;

		// La isla de inicio va cerca del centro, algo desplazada al sur.
		FIslandDesc Landing;
		Landing.Archetype = EIslandArchetype::Landing;
		Landing.Radius = Rng.RangeFloat(520.0f, 580.0f);
		Landing.MaxHeight = Rng.RangeFloat(45.0f, 65.0f);
		Landing.Center = FVector2D(Rng.RangeFloat(-250.0f, 250.0f), Rng.RangeFloat(-700.0f, -400.0f));
		Landing.Seed = Rng.NextUInt32();
		Landing.Rotation = Rng.RangeFloat(0.0f, UE_TWO_PI);
		Layout.Islands.Add(Landing);

		bool bFailed = false;
		for (const FArchetypeTemplate& T : Templates)
		{
			FIslandDesc Island;
			Island.Archetype = T.Archetype;
			Island.Radius = Rng.RangeFloat(T.MinRadius, T.MaxRadius);
			Island.MaxHeight = Rng.RangeFloat(T.MinHeight, T.MaxHeight);
			Island.Seed = Rng.NextUInt32();
			Island.Rotation = Rng.RangeFloat(0.0f, UE_TWO_PI);

			bool bPlaced = false;
			const float Limit = WorldHalfExtent - Island.Radius - 250.0f;
			for (int32 Try = 0; Try < 400 && !bPlaced; ++Try)
			{
				const FVector2D Candidate(Rng.RangeFloat(-Limit, Limit), Rng.RangeFloat(-Limit, Limit));
				bPlaced = !Layout.Islands.ContainsByPredicate([&](const FIslandDesc& Other)
				{
					return Overlaps(Other, Candidate, Island.Radius);
				});
				if (bPlaced)
				{
					Island.Center = Candidate;
				}
			}

			if (!bPlaced)
			{
				bFailed = true;
				break;
			}

			if (Island.Archetype == EIslandArchetype::Teeth)
			{
				GenerateIslets(Island, Rng);
			}
			Layout.Islands.Add(MoveTemp(Island));
		}

		if (!bFailed)
		{
			return Layout;
		}
	}

	checkf(false, TEXT("No se encontró una disposición válida para la semilla %u"), InSeed);
	return FArchipelagoLayout();
}

const FIslandDesc* FArchipelagoLayout::FindIsland(EIslandArchetype Archetype) const
{
	return Islands.FindByPredicate([Archetype](const FIslandDesc& I) { return I.Archetype == Archetype; });
}
