#include "WorldGen/SeabedScatterModel.h"

#include "Core/ExploredNoise.h"
#include "Core/ExploredRandom.h"

namespace
{
	using E = EIslandArchetype;

	FSeabedScatterRule MakeRule(const TCHAR* Species, ESeabedCategory Category, ESeabedZone Zone,
		std::initializer_list<TPair<EIslandArchetype, float>> Weights)
	{
		FSeabedScatterRule Rule;
		Rule.Species = FName(Species);
		Rule.Category = Category;
		Rule.Zone = Zone;
		for (const auto& W : Weights)
		{
			Rule.IslandWeight.Add(W.Key, W.Value);
		}
		return Rule;
	}

	// Franja de agrupación del ruido de manchas (coral y algas, no praderas ni conchas).
	constexpr float ClusterScale = 6.0f;
}

TArray<FSeabedScatterRule> FSeabedScatterModel::DefaultRules()
{
	TArray<FSeabedScatterRule> Rules;

	// Arrecife somero (1-4 m): coral denso y variado. Tres especies para que no se vea
	// repetitivo; el horneado reparte los mesh de cada una según Content/Data/seabed_scatter.json.
	{
		FSeabedScatterRule R = MakeRule(TEXT("CoralCerebro"), ESeabedCategory::ReefCoral, ESeabedZone::Reef,
			{{E::WhiteSands, 1.0f}, {E::Landing, 0.7f}, {E::Emerald, 0.5f}, {E::Teeth, 0.3f}, {E::Mangrove, 0.1f}, {E::Smoke, 0.1f}});
		R.MinDepthM = 1.0f;
		R.MaxDepthM = 3.0f;
		R.MaxSlopeDeg = 25.0f;
		R.Spacing = 1.4f;
		R.ClusterThreshold = 0.15f;
		R.MinScale = 0.8f;
		R.MaxScale = 1.4f;
		R.CullEndDistanceM = 35.0f;
		Rules.Add(R);
	}
	{
		FSeabedScatterRule R = MakeRule(TEXT("CoralCuernoCiervo"), ESeabedCategory::ReefCoral, ESeabedZone::Reef,
			{{E::WhiteSands, 0.9f}, {E::Landing, 0.6f}, {E::Emerald, 0.4f}, {E::Teeth, 0.3f}});
		R.MinDepthM = 1.0f;
		R.MaxDepthM = 3.0f;
		R.MaxSlopeDeg = 25.0f;
		R.Spacing = 1.6f;
		R.ClusterThreshold = 0.2f;
		R.MinScale = 0.7f;
		R.MaxScale = 1.3f;
		R.CullEndDistanceM = 35.0f;
		Rules.Add(R);
	}
	{
		FSeabedScatterRule R = MakeRule(TEXT("CoralMesa"), ESeabedCategory::ReefCoral, ESeabedZone::Reef,
			{{E::WhiteSands, 0.7f}, {E::Landing, 0.5f}, {E::Emerald, 0.35f}});
		R.MinDepthM = 1.5f;
		R.MaxDepthM = 3.0f;
		R.MaxSlopeDeg = 20.0f;
		R.Spacing = 2.2f;
		R.ClusterThreshold = 0.25f;
		R.MinScale = 0.9f;
		R.MaxScale = 1.5f;
		R.CullEndDistanceM = 40.0f;
		Rules.Add(R);
	}

	// Borde del arrecife: más hondo y con más pendiente que el coral denso (la caída hacia
	// mar abierto), abanicos y esponjas grandes, más espaciados.
	{
		FSeabedScatterRule R = MakeRule(TEXT("CoralAbanico"), ESeabedCategory::ReefEdge, ESeabedZone::Reef,
			{{E::WhiteSands, 0.8f}, {E::Landing, 0.4f}, {E::Emerald, 0.3f}, {E::Teeth, 0.4f}});
		R.MinDepthM = 3.0f;
		R.MaxDepthM = 8.0f;
		R.MinSlopeDeg = 10.0f;
		R.Spacing = 3.0f;
		R.ClusterThreshold = 0.1f;
		R.MinScale = 1.0f;
		R.MaxScale = 1.6f;
		R.CullEndDistanceM = 45.0f;
		Rules.Add(R);
	}
	{
		FSeabedScatterRule R = MakeRule(TEXT("EsponjaBarril"), ESeabedCategory::ReefEdge, ESeabedZone::Reef,
			{{E::WhiteSands, 0.6f}, {E::Landing, 0.3f}, {E::Emerald, 0.25f}, {E::Teeth, 0.3f}});
		R.MinDepthM = 3.0f;
		R.MaxDepthM = 8.0f;
		R.MinSlopeDeg = 5.0f;
		R.Spacing = 3.4f;
		R.MinScale = 0.9f;
		R.MaxScale = 1.5f;
		R.CullEndDistanceM = 40.0f;
		Rules.Add(R);
	}
	{
		// Anémonas: mismo borde del arrecife, sin depender de la pendiente (viven en la
		// grieta y en el llano al pie de la caída por igual).
		FSeabedScatterRule R = MakeRule(TEXT("Anemona"), ESeabedCategory::ReefEdge, ESeabedZone::Reef,
			{{E::WhiteSands, 0.5f}, {E::Landing, 0.3f}, {E::Emerald, 0.2f}});
		R.MinDepthM = 3.0f;
		R.MaxDepthM = 8.0f;
		R.Spacing = 3.6f;
		R.MinScale = 0.6f;
		R.MaxScale = 1.1f;
		R.CullEndDistanceM = 30.0f;
		Rules.Add(R);
	}

	// Arena: praderas marinas (casi uniformes, matas) y conchas (dispersas).
	{
		FSeabedScatterRule R = MakeRule(TEXT("PraderaMarina"), ESeabedCategory::SandMeadow, ESeabedZone::Sand,
			{{E::Landing, 1.0f}, {E::WhiteSands, 1.0f}, {E::Mangrove, 0.6f}, {E::Emerald, 0.5f}, {E::Teeth, 0.4f}, {E::Smoke, 0.3f}});
		R.MinDepthM = 0.8f;
		R.MaxDepthM = 12.0f;
		R.MaxSlopeDeg = 15.0f;
		R.Spacing = 1.2f;
		R.ClusterThreshold = 0.0f; // uniforme: una pradera no sale en manchas puntuales.
		R.MinScale = 0.7f;
		R.MaxScale = 1.2f;
		R.CullEndDistanceM = 30.0f;
		Rules.Add(R);
	}
	{
		FSeabedScatterRule R = MakeRule(TEXT("ConchaGrande"), ESeabedCategory::SandShell, ESeabedZone::Sand,
			{{E::Landing, 0.6f}, {E::WhiteSands, 0.7f}, {E::Mangrove, 0.3f}, {E::Emerald, 0.3f}});
		R.MinDepthM = 0.8f;
		R.MaxDepthM = 15.0f;
		R.Spacing = 6.0f;
		R.MinScale = 0.8f;
		R.MaxScale = 1.3f;
		R.CullEndDistanceM = 25.0f;
		Rules.Add(R);
	}
	{
		FSeabedScatterRule R = MakeRule(TEXT("ConchaPequena"), ESeabedCategory::SandShell, ESeabedZone::Sand,
			{{E::Landing, 0.8f}, {E::WhiteSands, 0.9f}, {E::Mangrove, 0.4f}, {E::Emerald, 0.4f}, {E::Teeth, 0.3f}});
		R.MinDepthM = 0.8f;
		R.MaxDepthM = 15.0f;
		R.Spacing = 3.0f;
		R.MinScale = 0.4f;
		R.MaxScale = 0.8f;
		R.CullEndDistanceM = 18.0f;
		Rules.Add(R);
	}

	// Roca: algas (agrupadas, se agarran a las grietas) y erizos/estrellas (dispersos).
	{
		FSeabedScatterRule R = MakeRule(TEXT("AlgaKelp"), ESeabedCategory::RockKelp, ESeabedZone::Rock,
			{{E::Teeth, 1.0f}, {E::Mesa, 0.8f}, {E::Smoke, 0.6f}, {E::Emerald, 0.4f}, {E::Landing, 0.3f}, {E::WhiteSands, 0.2f}});
		R.MinDepthM = 1.0f;
		R.MaxDepthM = 20.0f;
		R.Spacing = 2.0f;
		R.ClusterThreshold = 0.1f;
		R.MinScale = 0.8f;
		R.MaxScale = 1.6f;
		R.CullEndDistanceM = 35.0f;
		Rules.Add(R);
	}
	{
		FSeabedScatterRule R = MakeRule(TEXT("ErizoMar"), ESeabedCategory::RockUrchin, ESeabedZone::Rock,
			{{E::Teeth, 0.8f}, {E::Mesa, 0.6f}, {E::Smoke, 0.4f}, {E::Emerald, 0.3f}});
		R.MinDepthM = 1.0f;
		R.MaxDepthM = 20.0f;
		R.Spacing = 3.5f;
		R.MinScale = 0.5f;
		R.MaxScale = 0.9f;
		R.CullEndDistanceM = 20.0f;
		Rules.Add(R);
	}
	{
		FSeabedScatterRule R = MakeRule(TEXT("EstrellaMar"), ESeabedCategory::RockUrchin, ESeabedZone::Rock,
			{{E::Teeth, 0.6f}, {E::Mesa, 0.4f}, {E::Smoke, 0.3f}, {E::Landing, 0.2f}, {E::WhiteSands, 0.2f}});
		R.MinDepthM = 1.0f;
		R.MaxDepthM = 20.0f;
		R.Spacing = 4.0f;
		R.MinScale = 0.5f;
		R.MaxScale = 0.9f;
		R.CullEndDistanceM = 20.0f;
		Rules.Add(R);
	}

	// Bloques y cantos submarinos: estructura, no organismo. A diferencia del resto, sí
	// llevan colisión (son sólidos, no decorado atravesable) y más espaciados.
	{
		FSeabedScatterRule R = MakeRule(TEXT("RocaSubmarina"), ESeabedCategory::RockFormation, ESeabedZone::Rock,
			{{E::Teeth, 1.0f}, {E::Mesa, 0.9f}, {E::Smoke, 0.7f}, {E::Emerald, 0.4f}, {E::Landing, 0.3f}, {E::WhiteSands, 0.2f}});
		R.MinDepthM = 1.0f;
		R.MaxDepthM = 25.0f;
		R.Spacing = 5.0f;
		R.MinScale = 0.6f;
		R.MaxScale = 1.8f;
		R.bCollisionEnabled = true;
		R.CullEndDistanceM = 45.0f;
		Rules.Add(R);
	}

	return Rules;
}

TArray<FSeabedScatterInstance> FSeabedScatterModel::Generate(const FBox2D& CellBoundsM, FSampleColumn SampleColumn,
	const TArray<FSeabedScatterRule>& Rules, uint32 Seed, EIslandArchetype Archetype,
	const TArray<FVector2D>& AvoidPoints, float AvoidRadiusM, int32 InstanceBudget)
{
	TArray<FSeabedScatterInstance> Out;
	if (InstanceBudget <= 0)
	{
		return Out;
	}
	const float AvoidRadiusSq = AvoidRadiusM * AvoidRadiusM;

	for (const FSeabedScatterRule& Rule : Rules)
	{
		if (Out.Num() >= InstanceBudget)
		{
			break;
		}
		const float* Weight = Rule.IslandWeight.Find(Archetype);
		if (!Weight || *Weight <= 0.0f || Rule.Spacing <= 0.0f)
		{
			continue;
		}

		const uint32 RuleSeed = ExploredHash::Hash32(Seed ^ GetTypeHash(Rule.Species) ^ 0x5EABED00u);
		const FExploredNoise Cluster(RuleSeed);

		const int32 MinX = FMath::FloorToInt32(CellBoundsM.Min.X / Rule.Spacing);
		const int32 MaxX = FMath::CeilToInt32(CellBoundsM.Max.X / Rule.Spacing);
		const int32 MinY = FMath::FloorToInt32(CellBoundsM.Min.Y / Rule.Spacing);
		const int32 MaxY = FMath::CeilToInt32(CellBoundsM.Max.Y / Rule.Spacing);

		for (int32 GY = MinY; GY <= MaxY; ++GY)
		{
			for (int32 GX = MinX; GX <= MaxX; ++GX)
			{
				if (Out.Num() >= InstanceBudget)
				{
					return Out;
				}

				const uint32 H = ExploredHash::Hash2D(RuleSeed, GX, GY);
				FExploredRandom Rng(H);
				const float X = (GX + Rng.RangeFloat(0.15f, 0.85f)) * Rule.Spacing;
				const float Y = (GY + Rng.RangeFloat(0.15f, 0.85f)) * Rule.Spacing;
				const FVector2D P(X, Y);
				if (!CellBoundsM.IsInside(P))
				{
					continue;
				}

				const FSeabedColumn Column = SampleColumn(X, Y);
				// Nunca por encima del nivel del mar ni en la rompiente: se aplica a toda regla.
				if (Column.DepthM < SurfBreakDepthM)
				{
					continue;
				}
				if (Column.Zone != Rule.Zone ||
					Column.DepthM < Rule.MinDepthM || Column.DepthM > Rule.MaxDepthM ||
					Column.SlopeDeg < Rule.MinSlopeDeg || Column.SlopeDeg > Rule.MaxSlopeDeg)
				{
					continue;
				}

				if (Rule.ClusterThreshold > 0.0f)
				{
					const float ClusterValue = Cluster.Fbm2D(X / ClusterScale, Y / ClusterScale, 3);
					if (ClusterValue < Rule.ClusterThreshold)
					{
						continue;
					}
				}

				if (!Rng.Chance(*Weight))
				{
					continue;
				}

				bool bAvoided = false;
				for (const FVector2D& Avoid : AvoidPoints)
				{
					if (FVector2D::DistSquared(P, Avoid) < AvoidRadiusSq)
					{
						bAvoided = true;
						break;
					}
				}
				if (bAvoided)
				{
					continue;
				}

				const float Scale = Rng.RangeFloat(Rule.MinScale, Rule.MaxScale);
				const FQuat Rotation(FVector::UpVector, Rng.RangeFloat(0.0f, UE_TWO_PI));

				FSeabedScatterInstance Instance;
				Instance.Species = Rule.Species;
				Instance.Category = Rule.Category;
				Instance.MeshIndex = static_cast<int32>(Rng.NextUInt32() % static_cast<uint32>(FMath::Max(1, Rule.Meshes.Num())));
				// Z en metros bajo el nivel del mar (0), pasado a centímetros como el resto de scatters.
				Instance.Transform = FTransform(Rotation, FVector(X, Y, -Column.DepthM) * 100.0, FVector(Scale));
				Out.Add(Instance);
			}
		}
	}
	return Out;
}
