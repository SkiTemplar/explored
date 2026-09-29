#include "WorldGen/ArchipelagoLayout.h"

#include "Core/ExploredNoise.h"
#include "Core/ExploredRandom.h"
#include "WorldGen/IslandShapeModel.h"

namespace
{
	struct FArchetypeTemplate
	{
		EIslandArchetype Archetype;
		float MinRadius;
		float MaxRadius;
		float MinHeight;
		float MaxHeight;
		/** Número de penínsulas [Min, Max]. */
		int32 MinLobes;
		int32 MaxLobes;
		/** Número de cayos satélite [Min, Max]. */
		int32 MinCays;
		int32 MaxCays;
	};

	// Orden de la cadena volcánica, como en un rastro de punto caliente: el volcán activo
	// es la isla más joven; las siguientes están cada vez más erosionadas y hundidas, y
	// el extremo viejo es ya un atolón. El Amaraje queda en el centro de la cadena.
	const FArchetypeTemplate ChainTemplates[] = {
		{EIslandArchetype::Smoke, 600.0f, 680.0f, 380.0f, 440.0f, 0, 0, 1, 2},
		{EIslandArchetype::Emerald, 640.0f, 720.0f, 160.0f, 200.0f, 2, 3, 1, 2},
		// El macizo kárstico lleva más cayos que el resto: torres y farallones sueltos
		// alrededor, como en El Nido / la bahía de Ha Long.
		{EIslandArchetype::Mesa, 580.0f, 660.0f, 230.0f, 280.0f, 1, 2, 2, 5},
		{EIslandArchetype::Landing, 480.0f, 540.0f, 45.0f, 65.0f, 1, 2, 2, 3},
		{EIslandArchetype::Mangrove, 460.0f, 540.0f, 10.0f, 16.0f, 2, 3, 2, 4},
		{EIslandArchetype::Teeth, 380.0f, 440.0f, 70.0f, 100.0f, 0, 0, 0, 0},
		{EIslandArchetype::WhiteSands, 440.0f, 520.0f, 7.0f, 10.0f, 0, 0, 1, 2},
	};
	constexpr int32 ChainLength = UE_ARRAY_COUNT(ChainTemplates);
	static_assert(ChainLength == static_cast<int32>(EIslandArchetype::Count), "Cada arquetipo aparece una vez en la cadena");

	/** Margen entre la costa nominal y el borde del mundo. */
	constexpr float WorldMargin = 250.0f;

	FVector2D QuadraticBezier(const FVector2D& A, const FVector2D& C, const FVector2D& B, float T)
	{
		const float U = 1.0f - T;
		return A * (U * U) + C * (2.0f * U * T) + B * (T * T);
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

	void GenerateLobes(FIslandDesc& Island, const FArchetypeTemplate& T, FExploredRandom& Rng)
	{
		const int32 Count = Rng.RangeInt(T.MinLobes, T.MaxLobes);
		for (int32 I = 0; I < Count; ++I)
		{
			float Angle = Rng.RangeFloat(-UE_PI, UE_PI);
			if (Island.Archetype == EIslandArchetype::Landing)
			{
				// La laguna del amaraje está en +X local: las penínsulas la rodean sin taparla.
				Angle = (Rng.Chance(0.5f) ? 1.0f : -1.0f) * Rng.RangeFloat(1.2f, 2.9f);
			}
			FIslandLobe Lobe;
			const float Distance = Rng.RangeFloat(0.38f, 0.62f);
			Lobe.Offset = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
			// La punta del lóbulo nunca pasa de 1,1 radios para respetar los canales.
			Lobe.Radius = Rng.RangeFloat(0.28f, 1.1f - Distance);
			Lobe.Aspect = Rng.RangeFloat(0.45f, 0.85f);
			// Orientado aproximadamente hacia fuera, como una península.
			Lobe.Angle = Angle + Rng.RangeFloat(-0.5f, 0.5f);
			Island.Lobes.Add(Lobe);
		}
	}

	/** Distancia (m) desde el centro hasta la costa más exterior de la isla en la dirección Dir. */
	float OuterCoastDistance(const FIslandDesc& Island, const FVector2D& Dir)
	{
		const FExploredNoise N(Island.Seed);
		auto CoastTAt = [&](float D)
		{
			const FVector2D P = Island.Center + Dir * D;
			const FVector2D Q = FIslandShapeModel::WarpedLocal(Island, N, static_cast<float>(P.X), static_cast<float>(P.Y));
			return FIslandShapeModel::CoastT(Island, N, Q);
		};
		const float Step = Island.Radius * 0.02f;
		for (float D = Island.Radius * 2.5f; D > 0.0f; D -= Step)
		{
			if (CoastTAt(D) < 1.0f)
			{
				return D + Step * 0.5f;
			}
		}
		return Island.Radius;
	}

	void GenerateCays(FIslandDesc& Island, const FArchetypeTemplate& T, const TArray<FIslandDesc>& All, FExploredRandom& Rng)
	{
		const int32 Count = Rng.RangeInt(T.MinCays, T.MaxCays);
		const bool bRockyIsland = Island.Archetype == EIslandArchetype::Smoke || Island.Archetype == EIslandArchetype::Mesa;
		for (int32 I = 0, Tries = 0; I < Count && Tries < 40; ++Tries)
		{
			const float Angle = Rng.RangeFloat(0.0f, UE_TWO_PI);
			const float Offshore = Rng.RangeFloat(0.0f, 1.0f);
			FCayDesc Cay;
			Cay.bRocky = bRockyIsland && Rng.Chance(0.7f);
			Cay.Radius = Cay.bRocky ? Rng.RangeFloat(18.0f, 40.0f) : Rng.RangeFloat(22.0f, 60.0f);
			// Sobre la plataforma, a poca distancia de la costa real (no de la nominal): a 1,45-1,75
			// radios del centro quedaban al borde del alcance de la isla, sueltos en aguas hondas.
			const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			const float Coast = OuterCoastDistance(Island, Dir);
			Cay.Center = Island.Center + Dir * (Coast + Cay.Radius * 1.8f + Offshore * Island.Radius * 0.12f);
			Cay.Height = Cay.bRocky ? Rng.RangeFloat(7.0f, 18.0f) : Rng.RangeFloat(1.4f, 3.2f);
			Cay.Aspect = Rng.RangeFloat(0.4f, 0.9f);
			Cay.Angle = Rng.RangeFloat(0.0f, UE_PI);

			const float E = FArchipelagoLayout::WorldHalfExtent - WorldMargin;
			const bool bInWorld = FMath::Abs(Cay.Center.X) < E && FMath::Abs(Cay.Center.Y) < E;
			const bool bFree = !All.ContainsByPredicate([&](const FIslandDesc& Other)
			{
				return &Other != &Island && FVector2D::Distance(Other.Center, Cay.Center) < Other.Radius * 1.35f + Cay.Radius * 3.0f;
			});
			const bool bApart = !Island.Cays.ContainsByPredicate([&](const FCayDesc& C)
			{
				return FVector2D::Distance(C.Center, Cay.Center) < (C.Radius + Cay.Radius) * 3.0f;
			});
			if (bInWorld && bFree && bApart)
			{
				Island.Cays.Add(Cay);
				++I;
			}
		}
	}

	/**
	 * Separa las islas por relajación: empuja los pares demasiado juntos, mantiene
	 * unidas las vecinas de la cadena y atrae cada isla hacia su ancla en el arco.
	 */
	void Relax(TArray<FIslandDesc>& Islands, const TArray<FVector2D>& Anchors, const TArray<float>& ChainChannels)
	{
		const int32 N = Islands.Num();
		for (int32 Iteration = 0; Iteration < 400; ++Iteration)
		{
			for (int32 A = 0; A < N; ++A)
			{
				for (int32 B = A + 1; B < N; ++B)
				{
					FIslandDesc& IA = Islands[A];
					FIslandDesc& IB = Islands[B];
					const bool bNeighbours = B == A + 1;
					const float Channel = bNeighbours ? ChainChannels[A] : FArchipelagoLayout::MinChannel * 1.3f;
					const float Wanted = IA.Radius + IB.Radius + Channel;
					FVector2D Delta = IB.Center - IA.Center;
					const float Distance = FMath::Max(Delta.Size(), 1.0f);
					Delta /= Distance;

					float Push = 0.0f;
					if (Distance < Wanted)
					{
						Push = Wanted - Distance;
					}
					else if (bNeighbours && Distance > Wanted * 1.08f)
					{
						// Las vecinas de la cadena se atraen para que los estrechos no se abran.
						Push = -(Distance - Wanted) * 0.25f;
					}
					if (Push != 0.0f)
					{
						// La isla pequeña se mueve más que la grande.
						const float WA = IB.Radius / (IA.Radius + IB.Radius);
						IA.Center -= Delta * Push * WA * 0.5f;
						IB.Center += Delta * Push * (1.0f - WA) * 0.5f;
					}
				}
			}

			const float Pull = Iteration < 300 ? 0.02f : 0.0f;
			for (int32 I = 0; I < N; ++I)
			{
				FIslandDesc& Island = Islands[I];
				Island.Center = FMath::Lerp(Island.Center, Anchors[I], Pull);
				const float Limit = FArchipelagoLayout::WorldHalfExtent - Island.Radius - WorldMargin;
				Island.Center.X = FMath::Clamp(Island.Center.X, -Limit, Limit);
				Island.Center.Y = FMath::Clamp(Island.Center.Y, -Limit, Limit);
			}
		}
	}

	bool IsValid(const TArray<FIslandDesc>& Islands)
	{
		for (int32 A = 0; A < Islands.Num(); ++A)
		{
			const FIslandDesc& IA = Islands[A];
			const float Limit = FArchipelagoLayout::WorldHalfExtent - IA.Radius - WorldMargin + 1.0f;
			if (FMath::Abs(IA.Center.X) > Limit || FMath::Abs(IA.Center.Y) > Limit)
			{
				return false;
			}
			for (int32 B = A + 1; B < Islands.Num(); ++B)
			{
				const FIslandDesc& IB = Islands[B];
				const float Gap = FVector2D::Distance(IA.Center, IB.Center) - IA.Radius - IB.Radius;
				if (Gap < FArchipelagoLayout::MinChannel)
				{
					return false;
				}
				if (B == A + 1 && Gap > FArchipelagoLayout::MaxChainChannel)
				{
					return false;
				}
			}
		}
		return true;
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

		// Arco de la cadena: una curva suave cruzando el mapa en diagonal, con flecha variable.
		const float Heading = Rng.RangeFloat(0.0f, UE_TWO_PI);
		const FVector2D Along(FMath::Cos(Heading), FMath::Sin(Heading));
		const FVector2D Side(-Along.Y, Along.X);
		const float HalfChord = Rng.RangeFloat(2500.0f, 2800.0f);
		const float Bulge = Rng.RangeFloat(0.45f, 0.75f) * HalfChord * (Rng.Chance(0.5f) ? 1.0f : -1.0f);
		const FVector2D ArcStart = -Along * HalfChord - Side * Bulge * 0.5f;
		const FVector2D ArcEnd = Along * HalfChord - Side * Bulge * 0.5f;
		const FVector2D ArcControl = Side * Bulge * 1.5f;

		TArray<FVector2D> Anchors;
		TArray<float> ChainChannels;
		for (int32 I = 0; I < ChainLength; ++I)
		{
			const FArchetypeTemplate& T = ChainTemplates[I];
			FIslandDesc Island;
			Island.Archetype = T.Archetype;
			Island.Radius = Rng.RangeFloat(T.MinRadius, T.MaxRadius);
			Island.MaxHeight = Rng.RangeFloat(T.MinHeight, T.MaxHeight);
			Island.Seed = Rng.NextUInt32();
			Island.Rotation = Rng.RangeFloat(0.0f, UE_TWO_PI);

			// Posición en el arco con desplazamiento lateral alterno (islas en escalón).
			const float Param = (I + 0.5f) / ChainLength + Rng.RangeFloat(-0.03f, 0.03f);
			const float Lateral = (I % 2 == 0 ? 1.0f : -1.0f) * Rng.RangeFloat(0.3f, 0.9f) * Island.Radius;
			const FVector2D Anchor = QuadraticBezier(ArcStart, ArcControl, ArcEnd, Param) + Side * Lateral;
			Island.Center = Anchor;
			Anchors.Add(Anchor);
			// Estrechos variados: algunos se cruzan a nado, otros piden balsa.
			ChainChannels.Add(Rng.Chance(0.35f) ? Rng.RangeFloat(160.0f, 240.0f) : Rng.RangeFloat(280.0f, 480.0f));
			Layout.Islands.Add(MoveTemp(Island));
		}

		Relax(Layout.Islands, Anchors, ChainChannels);
		if (!IsValid(Layout.Islands))
		{
			continue;
		}

		for (int32 I = 0; I < ChainLength; ++I)
		{
			FIslandDesc& Island = Layout.Islands[I];
			if (Island.Archetype == EIslandArchetype::Teeth)
			{
				GenerateIslets(Island, Rng);
			}
			GenerateLobes(Island, ChainTemplates[I], Rng);
		}
		for (int32 I = 0; I < ChainLength; ++I)
		{
			GenerateCays(Layout.Islands[I], ChainTemplates[I], Layout.Islands, Rng);
		}

		// La dorsal pasa por los centros; los extremos se prolongan un poco mar adentro.
		const FVector2D First = Layout.Islands[0].Center;
		const FVector2D Last = Layout.Islands.Last().Center;
		Layout.Spine.Add(First + (First - Layout.Islands[1].Center).GetSafeNormal() * Layout.Islands[0].Radius * 1.5f);
		for (const FIslandDesc& Island : Layout.Islands)
		{
			Layout.Spine.Add(Island.Center);
		}
		Layout.Spine.Add(Last + (Last - Layout.Islands[ChainLength - 2].Center).GetSafeNormal() * Layout.Islands.Last().Radius * 1.5f);
		return Layout;
	}

	checkf(false, TEXT("No se encontró una disposición válida para la semilla %u"), InSeed);
	return FArchipelagoLayout();
}

const FIslandDesc* FArchipelagoLayout::FindIsland(EIslandArchetype Archetype) const
{
	return Islands.FindByPredicate([Archetype](const FIslandDesc& I) { return I.Archetype == Archetype; });
}
