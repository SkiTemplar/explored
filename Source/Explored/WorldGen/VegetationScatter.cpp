#include "WorldGen/VegetationScatter.h"

#include "Async/ParallelFor.h"
#include "Core/ExploredNoise.h"
#include "Core/ExploredRandom.h"
#include "WorldGen/TerrainDensity.h"

int32 FScatterResult::Total() const
{
	int32 Sum = 0;
	for (const TArray<FScatterInstance>& Instances : PerRule)
	{
		Sum += Instances.Num();
	}
	return Sum;
}

namespace
{
	using E = EIslandArchetype;

	FScatterRule MakeRule(const TCHAR* Species, const TCHAR* Category, const TCHAR* Filter,
		std::initializer_list<TPair<EIslandArchetype, float>> Weights)
	{
		FScatterRule Rule;
		Rule.Species = FName(Species);
		Rule.ManifestCategory = Category;
		Rule.NameFilter = Filter;
		for (const auto& W : Weights)
		{
			Rule.IslandWeight.Add(W.Key, W.Value);
		}
		return Rule;
	}
}

TArray<FScatterRule> FVegetationScatter::DefaultRules()
{
	TArray<FScatterRule> Rules;

	// Palmeras: franja costera, inclinadas hacia el mar.
	{
		FScatterRule R = MakeRule(TEXT("Palm"), TEXT("palm"), TEXT(""),
			{{E::Landing, 1.0f}, {E::WhiteSands, 1.0f}, {E::Emerald, 0.6f}, {E::Mangrove, 0.25f}, {E::Teeth, 0.2f}, {E::Mesa, 0.3f}, {E::Smoke, 0.2f}});
		R.MinHeight = 1.4f;
		R.MaxHeight = 14.0f;
		R.MinNormalZ = 0.82f;
		R.Spacing = 10.0f;
		R.ClusterScale = 45.0f;
		R.ClusterThreshold = -0.15f;
		R.MinScale = 0.85f;
		R.MaxScale = 1.25f;
		R.Sink = 0.4f;
		R.LeanTowardsSea = 14.0f;
		// Palmera aislada y reconocible en la silueta de la costa: se deja ver de lejos.
		R.CullDistance = 600.0f;
		Rules.Add(R);
	}
	// Gigantes del dosel: pocos y dispersos, sobre todo en Esmeralda.
	{
		FScatterRule R = MakeRule(TEXT("JungleGiant"), TEXT("tree"), TEXT("Giant"),
			{{E::Emerald, 1.0f}, {E::Landing, 0.35f}, {E::Mesa, 0.25f}});
		R.MinHeight = 8.0f;
		R.MaxHeight = 170.0f;
		R.MinNormalZ = 0.8f;
		R.Spacing = 34.0f;
		R.ClusterScale = 160.0f;
		R.ClusterThreshold = -0.2f;
		R.MinScale = 0.8f;
		R.MaxScale = 1.15f;
		R.Sink = 0.8f;
		// El árbol más alto del dosel: define el perfil de la isla desde lejos.
		R.CullDistance = 900.0f;
		Rules.Add(R);
	}
	// Árboles de copa ancha: el grueso de la selva.
	{
		FScatterRule R = MakeRule(TEXT("JungleWide"), TEXT("tree"), TEXT("Wide"),
			{{E::Emerald, 1.0f}, {E::Landing, 0.6f}, {E::Mesa, 0.4f}, {E::Smoke, 0.3f}, {E::Teeth, 0.15f}});
		R.MinHeight = 4.0f;
		R.MaxHeight = 200.0f;
		R.MinNormalZ = 0.72f;
		R.Spacing = 16.0f;
		R.ClusterScale = 90.0f;
		R.ClusterThreshold = -0.35f;
		R.MinScale = 0.75f;
		R.MaxScale = 1.25f;
		R.Sink = 0.6f;
		// El grueso de la masa forestal: el mayor número de instancias entre los árboles, de
		// ahí que se corte antes que los gigantes o las palmeras.
		R.CullDistance = 700.0f;
		Rules.Add(R);
	}
	// Manglar: en la orilla y los canales del Manglar de las Voces.
	{
		FScatterRule R = MakeRule(TEXT("Mangrove"), TEXT("tree"), TEXT("Mangrove"),
			{{E::Mangrove, 1.0f}, {E::WhiteSands, 0.08f}, {E::Landing, 0.05f}});
		R.MinHeight = -0.6f;
		R.MaxHeight = 5.0f;
		R.MinNormalZ = 0.8f;
		R.Spacing = 9.0f;
		R.ClusterScale = 50.0f;
		R.ClusterThreshold = -0.4f;
		R.MinScale = 0.8f;
		R.MaxScale = 1.3f;
		R.Sink = 0.2f;
		R.CullDistance = 500.0f;
		Rules.Add(R);
	}
	// Árboles de sotobosque: rellenan bajo el dosel.
	{
		FScatterRule R = MakeRule(TEXT("Understory"), TEXT("tree"), TEXT("Understory"),
			{{E::Emerald, 1.0f}, {E::Landing, 0.7f}, {E::Mangrove, 0.35f}, {E::Mesa, 0.3f}});
		R.MinHeight = 3.0f;
		R.MaxHeight = 160.0f;
		R.MinNormalZ = 0.7f;
		R.Spacing = 12.0f;
		R.ClusterScale = 60.0f;
		R.ClusterThreshold = -0.25f;
		R.MinScale = 0.8f;
		R.MaxScale = 1.25f;
		R.Sink = 0.3f;
		R.CullDistance = 450.0f;
		Rules.Add(R);
	}
	// Restos por el suelo: troncos caídos, tocones, ramas y cocos.
	{
		FScatterRule R = MakeRule(TEXT("Debris"), TEXT("debris"), TEXT(""),
			{{E::Emerald, 1.0f}, {E::Landing, 0.9f}, {E::Mangrove, 0.6f}, {E::Mesa, 0.5f}, {E::WhiteSands, 0.5f}, {E::Smoke, 0.3f}});
		R.MinHeight = 1.2f;
		R.MaxHeight = 180.0f;
		R.MinNormalZ = 0.8f;
		R.Spacing = 16.0f;
		R.ClusterScale = 40.0f;
		R.ClusterThreshold = -0.3f;
		R.MinScale = 0.8f;
		R.MaxScale = 1.2f;
		R.AlignToNormal = 0.9f;
		R.Sink = 0.1f;
		// Clutter pequeño sin silueta propia: se corta pronto y no aporta a las sombras.
		R.CullDistance = 110.0f;
		R.bCastShadow = false;
		Rules.Add(R);
	}
	// Sotobosque: helechos, arbustos y hojas grandes, muy denso en la selva.
	{
		FScatterRule R = MakeRule(TEXT("Shrub"), TEXT("shrub"), TEXT(""),
			{{E::Emerald, 1.0f}, {E::Landing, 0.8f}, {E::Mangrove, 0.8f}, {E::Mesa, 0.5f}, {E::Smoke, 0.35f}, {E::Teeth, 0.3f}, {E::WhiteSands, 0.3f}});
		R.MinHeight = 2.2f;
		R.MaxHeight = 220.0f;
		R.MinNormalZ = 0.65f;
		R.Spacing = 4.2f;
		R.ClusterScale = 25.0f;
		R.ClusterThreshold = -0.3f;
		R.MinScale = 0.7f;
		R.MaxScale = 1.4f;
		R.AlignToNormal = 0.4f;
		R.Sink = 0.1f;
		R.CullDistance = 120.0f;
		Rules.Add(R);
	}
	// Hierba y flores: praderas y claros.
	{
		FScatterRule R = MakeRule(TEXT("Grass"), TEXT("grass"), TEXT(""),
			{{E::Mesa, 1.0f}, {E::Landing, 0.6f}, {E::Emerald, 0.4f}, {E::Smoke, 0.5f}, {E::Teeth, 0.5f}, {E::WhiteSands, 0.4f}, {E::Mangrove, 0.3f}});
		R.MinHeight = 2.5f;
		R.MaxHeight = 300.0f;
		R.MinNormalZ = 0.75f;
		R.Spacing = 1.6f;
		R.ClusterScale = 18.0f;
		R.ClusterThreshold = -0.1f;
		R.MinScale = 0.7f;
		R.MaxScale = 1.3f;
		R.AlignToNormal = 0.7f;
		R.Sink = 0.05f;
		R.bNoCollision = true;
		R.CullDistance = 90.0f;
		R.bCastShadow = false;
		Rules.Add(R);
	}
	// Rocas: laderas, playas y los islotes; siguen la pendiente.
	{
		FScatterRule R = MakeRule(TEXT("Rock"), TEXT("rock"), TEXT(""),
			{{E::Teeth, 1.0f}, {E::Smoke, 1.0f}, {E::Mesa, 0.7f}, {E::Emerald, 0.4f}, {E::Landing, 0.35f}, {E::WhiteSands, 0.15f}, {E::Mangrove, 0.1f}});
		R.MinHeight = -1.5f;
		R.MaxHeight = 450.0f;
		R.MinNormalZ = 0.35f;
		R.Spacing = 11.0f;
		R.ClusterScale = 70.0f;
		R.ClusterThreshold = -0.2f;
		R.MinScale = 0.5f;
		R.MaxScale = 2.2f;
		R.AlignToNormal = 0.8f;
		R.Sink = 0.5f;
		// Las rocas van de cantos rodados a peñascos de isla; sin cota se quedaban siempre
		// visibles (la regla no tenía CullDistance).
		R.CullDistance = 400.0f;
		Rules.Add(R);
	}
	return Rules;
}

bool FVegetationScatter::FindSurface(const FTerrainDensity& Density, float X, float Y, float& OutZ, FVector& OutNormal)
{
	const FTerrainColumn Column = Density.SampleColumn(X, Y);
	float Top = Column.Height + FTerrainDensity::OverhangAmplitude * 3.0f + 1.0f;
	if (Density.DensityWithColumn(FVector(X, Y, Top), Column) < 0.0f)
	{
		return false; // Bajo un voladizo o dentro de roca: no se coloca.
	}
	// Baja a pasos hasta entrar en sólido y luego biseca.
	float Bottom = Top;
	for (int32 I = 0; I < 40; ++I)
	{
		Bottom -= 0.5f;
		if (Density.DensityWithColumn(FVector(X, Y, Bottom), Column) < 0.0f)
		{
			break;
		}
		Top = Bottom;
		if (I == 39)
		{
			return false;
		}
	}
	for (int32 I = 0; I < 12; ++I)
	{
		const float Mid = 0.5f * (Top + Bottom);
		if (Density.DensityWithColumn(FVector(X, Y, Mid), Column) < 0.0f)
		{
			Bottom = Mid;
		}
		else
		{
			Top = Mid;
		}
	}
	OutZ = 0.5f * (Top + Bottom);
	OutNormal = Density.Normal(FVector(X, Y, OutZ), 0.5f);
	return true;
}

FScatterResult FVegetationScatter::Generate(const FTerrainDensity& Density, const TArray<FScatterRule>& Rules,
	const FBox2D& Region, uint32 Seed)
{
	FScatterResult Result;
	Result.PerRule.SetNum(Rules.Num());

	for (int32 RuleIndex = 0; RuleIndex < Rules.Num(); ++RuleIndex)
	{
		const FScatterRule& Rule = Rules[RuleIndex];
		const uint32 RuleSeed = ExploredHash::Hash32(Seed ^ GetTypeHash(Rule.Species));
		const FExploredNoise Cluster(RuleSeed);
		TArray<FScatterInstance>& Out = Result.PerRule[RuleIndex];

		// Rejilla con desplazamiento aleatorio: separación aproximadamente uniforme sin solapes.
		const int32 MinX = FMath::FloorToInt32(Region.Min.X / Rule.Spacing);
		const int32 MaxX = FMath::CeilToInt32(Region.Max.X / Rule.Spacing);
		const int32 MinY = FMath::FloorToInt32(Region.Min.Y / Rule.Spacing);
		const int32 MaxY = FMath::CeilToInt32(Region.Max.Y / Rule.Spacing);

		// Filas en paralelo; se concatenan en orden para que el resultado sea determinista.
		TArray<TArray<FScatterInstance>> Rows;
		Rows.SetNum(FMath::Max(0, MaxY - MinY));
		ParallelFor(Rows.Num(), [&](int32 RowIndex)
		{
			const int32 GY = MinY + RowIndex;
			TArray<FScatterInstance>& RowOut = Rows[RowIndex];
			for (int32 GX = MinX; GX < MaxX; ++GX)
			{
				const uint32 H = ExploredHash::Hash2D(RuleSeed, GX, GY);
				FExploredRandom Rng(H);
				const float X = (GX + Rng.RangeFloat(0.1f, 0.9f)) * Rule.Spacing;
				const float Y = (GY + Rng.RangeFloat(0.1f, 0.9f)) * Rule.Spacing;
				if (!Region.IsInside(FVector2D(X, Y)))
				{
					continue;
				}

				const FTerrainColumn Column = Density.SampleColumn(X, Y);
				if (Column.IslandIndex == INDEX_NONE || Column.Height < Rule.MinHeight - 3.0f || Column.Height > Rule.MaxHeight + 3.0f)
				{
					continue;
				}
				const EIslandArchetype Archetype = Density.GetLayout().Islands[Column.IslandIndex].Archetype;
				const float* Weight = Rule.IslandWeight.Find(Archetype);
				if (!Weight || *Weight <= 0.0f)
				{
					continue;
				}

				// Agrupación natural: ruido de baja frecuencia + probabilidad por isla.
				const float ClusterValue = Cluster.Fbm2D(X / Rule.ClusterScale, Y / Rule.ClusterScale, 3);
				if (ClusterValue < Rule.ClusterThreshold)
				{
					continue;
				}
				const float EdgeFade = FMath::Clamp((ClusterValue - Rule.ClusterThreshold) * 4.0f, 0.0f, 1.0f);
				if (!Rng.Chance(*Weight * EdgeFade))
				{
					continue;
				}

				float Z = 0.0f;
				FVector Normal;
				if (!FindSurface(Density, X, Y, Z, Normal))
				{
					continue;
				}
				if (Z < Rule.MinHeight || Z > Rule.MaxHeight || Normal.Z < Rule.MinNormalZ)
				{
					continue;
				}

				// Orientación: vertical mezclada con la normal, giro aleatorio en Z.
				const FVector Up = FMath::Lerp(FVector::UpVector, Normal, Rule.AlignToNormal).GetSafeNormal();
				FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Up) *
					FQuat(FVector::UpVector, Rng.RangeFloat(0.0f, UE_TWO_PI));

				if (Rule.LeanTowardsSea > 0.0f)
				{
					// Hacia el mar = en contra del centro de la isla.
					const FIslandDesc& Island = Density.GetLayout().Islands[Column.IslandIndex];
					const FVector2D Away = (FVector2D(X, Y) - Island.Center).GetSafeNormal();
					const FVector Axis = FVector::CrossProduct(FVector::UpVector, FVector(Away, 0.0)).GetSafeNormal();
					const float Lean = FMath::DegreesToRadians(Rng.RangeFloat(0.3f, 1.0f) * Rule.LeanTowardsSea);
					Rotation = FQuat(Axis, Lean) * Rotation;
				}

				const float Scale = Rng.RangeFloat(Rule.MinScale, Rule.MaxScale);
				FScatterInstance Instance;
				Instance.MeshIndex = static_cast<int32>(Rng.NextUInt32() % static_cast<uint32>(FMath::Max(1, Rule.Meshes.Num())));
				Instance.Transform = FTransform(Rotation, FVector(X, Y, Z - Rule.Sink * Scale) * 100.0, FVector(Scale));
				RowOut.Add(Instance);
			}
		});
		for (TArray<FScatterInstance>& Row : Rows)
		{
			Out.Append(MoveTemp(Row));
		}
	}
	return Result;
}
