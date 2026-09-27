#include "WorldGen/BeachDebrisModel.h"

#include "Core/ExploredNoise.h"
#include "Core/ExploredRandom.h"
#include "WorldGen/TerrainDensity.h"

namespace
{
	using E = EIslandArchetype;

	FBeachDebrisRule MakeRule(const TCHAR* Species, const TCHAR* Category, const TCHAR* Filter,
		std::initializer_list<TPair<EIslandArchetype, float>> Weights)
	{
		FBeachDebrisRule Rule;
		Rule.Species = FName(Species);
		Rule.ManifestCategory = Category;
		Rule.NameFilter = Filter;
		for (const auto& W : Weights)
		{
			Rule.IslandWeight.Add(W.Key, W.Value);
		}
		return Rule;
	}

	/** Franja de marea donde vive el microdetalle: de justo bajo el agua a la arena seca alta. */
	constexpr float TideMinHeight = -0.4f;
	constexpr float TideMaxHeight = 1.9f;
	constexpr float TideMinNormalZ = 0.8f;
}

TArray<FBeachDebrisRule> FBeachDebrisModel::DefaultRules()
{
	TArray<FBeachDebrisRule> Rules;

	// Cocos sueltos, como los que caen de las palmeras de la costa.
	{
		FBeachDebrisRule R = MakeRule(TEXT("BeachCoconut"), TEXT("palm"), TEXT("Coconut"),
			{{E::Landing, 1.0f}, {E::WhiteSands, 1.0f}, {E::Emerald, 0.5f}, {E::Mesa, 0.3f}, {E::Teeth, 0.2f}, {E::Smoke, 0.15f}, {E::Mangrove, 0.15f}});
		R.MinScale = 0.7f;
		R.MaxScale = 1.1f;
		Rules.Add(R);
	}
	// Montoncitos de cocos ya varados (malla de detrito agrupada).
	{
		FBeachDebrisRule R = MakeRule(TEXT("BeachCoconutPile"), TEXT("debris"), TEXT("Coconuts"),
			{{E::Landing, 0.8f}, {E::WhiteSands, 0.9f}, {E::Emerald, 0.4f}, {E::Mesa, 0.25f}});
		R.MinScale = 0.8f;
		R.MaxScale = 1.2f;
		Rules.Add(R);
	}
	// Troncos a la deriva, tumbados en la arena.
	{
		FBeachDebrisRule R = MakeRule(TEXT("Driftwood"), TEXT("debris"), TEXT("LogMoss"),
			{{E::Landing, 0.7f}, {E::WhiteSands, 0.6f}, {E::Mangrove, 0.7f}, {E::Emerald, 0.4f}, {E::Mesa, 0.3f}, {E::Teeth, 0.35f}, {E::Smoke, 0.2f}});
		R.AlignToNormal = 1.0f;
		R.MinScale = 0.6f;
		R.MaxScale = 1.3f;
		Rules.Add(R);
	}
	// Ramas menores, mezcladas con los troncos.
	{
		FBeachDebrisRule R = MakeRule(TEXT("DriftBranch"), TEXT("debris"), TEXT("Branch"),
			{{E::Landing, 0.5f}, {E::WhiteSands, 0.4f}, {E::Mangrove, 0.5f}, {E::Emerald, 0.3f}, {E::Teeth, 0.25f}});
		R.AlignToNormal = 0.8f;
		R.MinScale = 0.6f;
		R.MaxScale = 1.2f;
		Rules.Add(R);
	}
	// Conchas: sin malla todavía (Tools/Blender/props no tiene ninguna, exploracion.md §4.4).
	// La regla queda declarada para que añadir el asset el día que exista sea solo tocar el
	// manifiesto; el horneado la descarta mientras Meshes esté vacío (igual que cualquier
	// FScatterRule sin candidatos).
	{
		FBeachDebrisRule R = MakeRule(TEXT("Shell"), TEXT("shell"), TEXT(""),
			{{E::Landing, 1.0f}, {E::WhiteSands, 1.0f}, {E::Emerald, 0.5f}, {E::Mesa, 0.4f}, {E::Teeth, 0.4f}, {E::Smoke, 0.2f}, {E::Mangrove, 0.2f}});
		R.MinScale = 0.5f;
		R.MaxScale = 1.0f;
		Rules.Add(R);
	}
	// Algas: sin malla todavía, mismo motivo que las conchas.
	{
		FBeachDebrisRule R = MakeRule(TEXT("Seaweed"), TEXT("seaweed"), TEXT(""),
			{{E::Landing, 0.7f}, {E::WhiteSands, 0.6f}, {E::Mangrove, 0.8f}, {E::Emerald, 0.4f}, {E::Mesa, 0.3f}, {E::Teeth, 0.5f}});
		R.AlignToNormal = 1.0f;
		R.MinScale = 0.6f;
		R.MaxScale = 1.3f;
		Rules.Add(R);
	}
	return Rules;
}

TArray<FBeachDebrisInstance> FBeachDebrisModel::Generate(const FTerrainDensity& Density,
	const TArray<FBeachDebrisRule>& Rules, uint32 Seed, const TArray<FVector>& AvoidPoints, float AvoidRadius)
{
	TArray<FBeachDebrisInstance> Out;
	const FArchipelagoLayout& Layout = Density.GetLayout();
	const float AvoidRadiusSq = AvoidRadius * AvoidRadius;

	for (const FBeachDebrisRule& Rule : Rules)
	{
		// La posición no depende de si hay mallas resueltas (igual que FVegetationScatter::Generate):
		// quien hornea filtra antes las reglas sin candidatos (conchas y algas, por ahora).
		const uint32 RuleSeed = ExploredHash::Hash32(Seed ^ GetTypeHash(Rule.Species) ^ 0xBEAC4000u);
		// Agrupado, no uniforme: rejilla poco densa con umbral de ruido alto (solo los picos del
		// ruido producen instancias, como manchas dispersas de restos varados).
		constexpr float Spacing = 13.0f;
		constexpr float ClusterScale = 22.0f;
		constexpr float ClusterThreshold = 0.35f;
		const FExploredNoise Cluster(RuleSeed);

		for (int32 IslandIndex = 0; IslandIndex < Layout.Islands.Num(); ++IslandIndex)
		{
			const FIslandDesc& Island = Layout.Islands[IslandIndex];
			const float* Weight = Rule.IslandWeight.Find(Island.Archetype);
			if (!Weight || *Weight <= 0.0f)
			{
				continue;
			}
			const float Extent = Island.Radius * 1.1f;
			const int32 MinX = FMath::FloorToInt32((Island.Center.X - Extent) / Spacing);
			const int32 MaxX = FMath::CeilToInt32((Island.Center.X + Extent) / Spacing);
			const int32 MinY = FMath::FloorToInt32((Island.Center.Y - Extent) / Spacing);
			const int32 MaxY = FMath::CeilToInt32((Island.Center.Y + Extent) / Spacing);

			for (int32 GY = MinY; GY <= MaxY; ++GY)
			{
				for (int32 GX = MinX; GX <= MaxX; ++GX)
				{
					const uint32 H = ExploredHash::Hash2D(RuleSeed, GX, GY);
					FExploredRandom Rng(H);
					const float X = (GX + Rng.RangeFloat(0.1f, 0.9f)) * Spacing;
					const float Y = (GY + Rng.RangeFloat(0.1f, 0.9f)) * Spacing;
					const FVector2D P(X, Y);

					const FTerrainColumn Column = Density.SampleColumn(X, Y);
					if (Column.IslandIndex != IslandIndex ||
						Column.Height < TideMinHeight || Column.Height > TideMaxHeight)
					{
						continue;
					}

					const float ClusterValue = Cluster.Fbm2D(X / ClusterScale, Y / ClusterScale, 3);
					if (ClusterValue < ClusterThreshold)
					{
						continue;
					}
					const float EdgeFade = FMath::Clamp((ClusterValue - ClusterThreshold) * 5.0f, 0.0f, 1.0f);
					if (!Rng.Chance(*Weight * EdgeFade))
					{
						continue;
					}

					const FVector Normal = Density.Normal(FVector(X, Y, Column.Height), 0.5f);
					if (Normal.Z < TideMinNormalZ)
					{
						continue;
					}

					bool bAvoided = false;
					for (const FVector& Avoid : AvoidPoints)
					{
						if (FVector2D::DistSquared(P, FVector2D(Avoid)) < AvoidRadiusSq)
						{
							bAvoided = true;
							break;
						}
					}
					if (bAvoided)
					{
						continue;
					}

					const FVector Up = FMath::Lerp(FVector::UpVector, Normal, Rule.AlignToNormal).GetSafeNormal();
					const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Up) *
						FQuat(FVector::UpVector, Rng.RangeFloat(0.0f, UE_TWO_PI));
					const float Scale = Rng.RangeFloat(Rule.MinScale, Rule.MaxScale);

					FBeachDebrisInstance Instance;
					Instance.Species = Rule.Species;
					Instance.MeshIndex = static_cast<int32>(Rng.NextUInt32() % static_cast<uint32>(FMath::Max(1, Rule.Meshes.Num())));
					Instance.Transform = FTransform(Rotation, FVector(X, Y, Column.Height) * 100.0, FVector(Scale));
					Out.Add(Instance);
				}
			}
		}
	}
	return Out;
}
