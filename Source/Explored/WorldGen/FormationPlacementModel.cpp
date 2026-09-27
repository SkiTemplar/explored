#include "WorldGen/FormationPlacementModel.h"

#include "Core/ExploredNoise.h"
#include "Core/ExploredRandom.h"
#include "WorldGen/TerrainDensity.h"

const TCHAR* LexToString(EFormationKind Kind)
{
	switch (Kind)
	{
	case EFormationKind::CliffWall: return TEXT("CliffWall");
	case EFormationKind::CliffSpur: return TEXT("CliffSpur");
	case EFormationKind::SeaStack: return TEXT("SeaStack");
	case EFormationKind::SeaArch: return TEXT("SeaArch");
	case EFormationKind::Boulder: return TEXT("Boulder");
	case EFormationKind::Cobble: return TEXT("Cobble");
	case EFormationKind::LimestoneSlab: return TEXT("LimestoneSlab");
	default: return TEXT("Unknown");
	}
}

namespace
{
	const TCHAR* FormationsGroup = TEXT("AcantiladoFormaciones");
	const TCHAR* BlocksGroup = TEXT("AcantiladoBloques");

	float SlopeDegFromNormalZ(float NormalZ)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(NormalZ, -1.0f, 1.0f)));
	}

	bool IsAvoided(const TArray<FVector>& AvoidPoints, float AvoidRadius, const FVector2D& P)
	{
		const float RadiusSq = AvoidRadius * AvoidRadius;
		for (const FVector& Avoid : AvoidPoints)
		{
			if (FVector2D::DistSquared(P, FVector2D(Avoid)) < RadiusSq)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Pared u espolón hundido en la ladera, con el frente hacia fuera siguiendo la normal en el
	 * punto ya candidato (P, con pendiente >= MinCliffSlopeDeg comprobada por el que llama). El
	 * hundimiento se recorta si cruza a una ladera más suave: en una cresta estrecha, hundir a
	 * ciegas puede caer al otro lado, mucho menos empinado que el punto de partida.
	 */
	FFormationInstance MakeWallInstance(const FTerrainDensity& Density, EFormationKind Kind, FName MeshFilter,
		const FVector2D& P, float Z, const FVector& Normal, uint32 VariantSeed, const FFormationPlacementParams& Params)
	{
		const FVector2D InwardHorizontal = FVector2D(Normal.X, Normal.Y).GetSafeNormal();
		FVector2D Embedded = P;
		float EmbeddedZ = Z;
		FVector EmbeddedNormal = Normal;
		for (const float EmbedDist : {Params.WallEmbed, Params.WallEmbed * 0.5f, 0.0f})
		{
			const FVector2D Candidate = P - InwardHorizontal * EmbedDist;
			const FTerrainColumn Column = Density.SampleColumn(Candidate.X, Candidate.Y);
			const FVector CandidateNormal = Density.Normal(FVector(Candidate, Column.Height), 1.0f);
			if (SlopeDegFromNormalZ(CandidateNormal.Z) >= Params.MinCliffSlopeDeg || EmbedDist == 0.0f)
			{
				Embedded = Candidate;
				EmbeddedZ = Column.Height;
				EmbeddedNormal = CandidateNormal;
				break;
			}
		}

		const float Yaw = FMath::Atan2(EmbeddedNormal.Y, EmbeddedNormal.X);
		const FVector Up = FMath::Lerp(FVector::UpVector, EmbeddedNormal, 0.35f).GetSafeNormal();
		const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Up) * FQuat(FVector::UpVector, Yaw);

		FFormationInstance Instance;
		Instance.Kind = Kind;
		Instance.MeshFilter = MeshFilter;
		Instance.ManifestGroup = FName(FormationsGroup);
		Instance.VariantIndex = static_cast<int32>(VariantSeed);
		Instance.Transform = FTransform(Rotation, FVector(Embedded, EmbeddedZ) * 100.0, FVector(1.0f));
		return Instance;
	}

	/**
	 * Paredes y espolones: laderas de más de Params.MinCliffSlopeDeg, en rejilla dispersa con
	 * ruido de agrupación para que sean escasos («menos es más», exploracion.md §4.3). El estilo
	 * (basalto/caliza) sale de FTerrainDensity::SurfaceLayers, sin inventar una señal nueva.
	 */
	void GenerateWallsAndSpurs(const FTerrainDensity& Density, uint32 Seed, const TArray<FVector>& AvoidPoints,
		const FFormationPlacementParams& Params, TArray<FFormationInstance>& Out, TArray<FVector2D>& OutWallPositions)
	{
		const FArchipelagoLayout& Layout = Density.GetLayout();
		const uint32 WallSeed = ExploredHash::Hash32(Seed ^ 0x00575A11u);
		const FExploredNoise Cluster(WallSeed);

		for (int32 IslandIndex = 0; IslandIndex < Layout.Islands.Num(); ++IslandIndex)
		{
			const FIslandDesc& Island = Layout.Islands[IslandIndex];
			const float Extent = Island.Radius * 1.2f;
			const int32 MinX = FMath::FloorToInt32((Island.Center.X - Extent) / Params.WallSpacing);
			const int32 MaxX = FMath::CeilToInt32((Island.Center.X + Extent) / Params.WallSpacing);
			const int32 MinY = FMath::FloorToInt32((Island.Center.Y - Extent) / Params.WallSpacing);
			const int32 MaxY = FMath::CeilToInt32((Island.Center.Y + Extent) / Params.WallSpacing);

			int32 Accepted = 0;
			for (int32 GY = MinY; GY <= MaxY && Accepted < Params.MaxWallsPerIsland; ++GY)
			{
				for (int32 GX = MinX; GX <= MaxX && Accepted < Params.MaxWallsPerIsland; ++GX)
				{
					const uint32 H = ExploredHash::Hash2D(WallSeed, GX, GY);
					FExploredRandom Rng(H);
					const float X = (GX + Rng.RangeFloat(0.15f, 0.85f)) * Params.WallSpacing;
					const float Y = (GY + Rng.RangeFloat(0.15f, 0.85f)) * Params.WallSpacing;
					const FVector2D P(X, Y);
					if (FVector2D::DistSquared(P, Island.Center) > Extent * Extent)
					{
						continue;
					}
					const FTerrainColumn Column = Density.SampleColumn(X, Y);
					if (Column.IslandIndex != IslandIndex || Column.Height < 2.0f)
					{
						continue;
					}
					const FVector Normal = Density.Normal(FVector(X, Y, Column.Height), 1.0f);
					if (SlopeDegFromNormalZ(Normal.Z) < Params.MinCliffSlopeDeg)
					{
						continue;
					}
					const float ClusterValue = Cluster.Fbm2D(X / Params.WallClusterScale, Y / Params.WallClusterScale, 3);
					if (ClusterValue < Params.WallClusterThreshold)
					{
						continue;
					}
					if (IsAvoided(AvoidPoints, Params.AvoidRadius, P))
					{
						continue;
					}

					const EFormationKind Kind = Rng.Chance(0.7f) ? EFormationKind::CliffWall : EFormationKind::CliffSpur;
					FName MeshFilter;
					if (Kind == EFormationKind::CliffWall)
					{
						const float Volcanic = Density.SurfaceLayers(FVector(X, Y, Column.Height), Normal).W;
						MeshFilter = Volcanic > 0.5f ? FName(TEXT("CliffWall_Basalt")) : FName(TEXT("CliffWall_Sandstone"));
					}
					else
					{
						MeshFilter = FName(TEXT("CliffSpur"));
					}

					Out.Add(MakeWallInstance(Density, Kind, MeshFilter, P, Column.Height, Normal, H, Params));
					OutWallPositions.Add(P);
					++Accepted;
				}
			}
		}
	}

	/** Farallones en agua somera junto a los cayos rocosos (FCayDesc.bRocky) de Mesa y Teeth. */
	void GenerateSeaStacks(const FTerrainDensity& Density, uint32 Seed, const TArray<FVector>& AvoidPoints,
		const FFormationPlacementParams& Params, TArray<FFormationInstance>& Out)
	{
		const uint32 StackSeed = ExploredHash::Hash32(Seed ^ 0x57AC4000u);

		for (const FIslandDesc& Island : Density.GetLayout().Islands)
		{
			for (int32 CayIndex = 0; CayIndex < Island.Cays.Num(); ++CayIndex)
			{
				const FCayDesc& Cay = Island.Cays[CayIndex];
				if (!Cay.bRocky)
				{
					continue;
				}
				const uint32 H0 = ExploredHash::Hash2D(StackSeed, FMath::RoundToInt32(Island.Center.X), CayIndex);
				FExploredRandom Rng(H0);
				const int32 Count = Rng.RangeInt(Params.StackMinPerCay, Params.StackMaxPerCay);
				int32 Placed = 0;
				for (int32 Attempt = 0; Attempt < Count * 6 && Placed < Count; ++Attempt)
				{
					// Más allá del propio cayo (que suele estar emergido): la orla de agua somera que lo rodea.
					const FVector2D Offset = Rng.InsideUnitDisc() * Cay.Radius * Rng.RangeFloat(0.9f, 2.4f);
					const FVector2D P = Cay.Center + Offset;
					const FTerrainColumn Column = Density.SampleColumn(P.X, P.Y);
					if (Column.Height < Params.StackMinDepth || Column.Height > Params.StackMaxDepth)
					{
						continue;
					}
					if (IsAvoided(AvoidPoints, Params.AvoidRadius, P))
					{
						continue;
					}
					FFormationInstance Instance;
					Instance.Kind = EFormationKind::SeaStack;
					Instance.MeshFilter = FName(TEXT("SeaStack"));
					Instance.ManifestGroup = FName(FormationsGroup);
					Instance.VariantIndex = static_cast<int32>(ExploredHash::Hash2D(H0, Attempt, Placed));
					const float Scale = Rng.RangeFloat(0.7f, 1.6f); // alturas distintas dentro del grupo
					const float Yaw = Rng.RangeFloat(0.0f, UE_TWO_PI);
					Instance.Transform = FTransform(FQuat(FVector::UpVector, Yaw), FVector(P, Column.Height) * 100.0,
						FVector(Scale));
					Out.Add(Instance);
					++Placed;
				}
			}
		}
	}

	/**
	 * Arco marino de Los Dientes: reutiliza la primera cueva-arco que FTerrainDensity::BuildCaves ya
	 * genera para esa isla (los dos extremos a nivel del mar la distinguen de una cueva normal, cuya
	 * entrada está en la ladera). Un solo arco, «en un cabo de Teeth» (exploracion.md §1.4).
	 */
	void GenerateSeaArch(const FTerrainDensity& Density, const TArray<FVector>& AvoidPoints,
		const FFormationPlacementParams& Params, TArray<FFormationInstance>& Out)
	{
		const FIslandDesc* Teeth = Density.GetLayout().FindIsland(EIslandArchetype::Teeth);
		if (!Teeth)
		{
			return;
		}
		for (const FCaveDesc& Cave : Density.GetCaves())
		{
			if (FMath::Abs(Cave.Start.Z) > 5.0f || FMath::Abs(Cave.End.Z) > 5.0f)
			{
				continue;
			}
			if (FVector2D::DistSquared(FVector2D(Cave.Start), Teeth->Center) > FMath::Square(Teeth->Radius * 1.4f))
			{
				continue;
			}
			const FVector Mid = FMath::Lerp(Cave.Start, Cave.End, 0.5f);
			if (IsAvoided(AvoidPoints, Params.AvoidRadius, FVector2D(Mid)))
			{
				continue;
			}
			const FVector2D Dir = (FVector2D(Cave.End) - FVector2D(Cave.Start)).GetSafeNormal();
			const float Yaw = FMath::Atan2(Dir.Y, Dir.X);

			FFormationInstance Instance;
			Instance.Kind = EFormationKind::SeaArch;
			Instance.MeshFilter = FName(TEXT("SeaArch"));
			Instance.ManifestGroup = FName(FormationsGroup);
			Instance.VariantIndex = 0;
			Instance.Transform = FTransform(FQuat(FVector::UpVector, Yaw), Mid * 100.0, FVector(1.0f));
			Out.Add(Instance);
			break;
		}
	}

	/** Bloques y cantos al pie de cada pared o espolón aceptado: el cono de derrubios de exploracion.md §4.3. */
	void GenerateDebrisAroundWalls(const FTerrainDensity& Density, uint32 Seed, const TArray<FVector2D>& WallPositions,
		const TArray<FVector>& AvoidPoints, const FFormationPlacementParams& Params, TArray<FFormationInstance>& Out)
	{
		const uint32 DebrisSeed = ExploredHash::Hash32(Seed ^ 0x0DEB2151u);
		for (int32 W = 0; W < WallPositions.Num(); ++W)
		{
			const FVector2D& WallP = WallPositions[W];
			FExploredRandom Rng(ExploredHash::Hash2D(DebrisSeed, W, 0));
			int32 Placed = 0;
			for (int32 Attempt = 0; Attempt < Params.DebrisPerWall * 5 && Placed < Params.DebrisPerWall; ++Attempt)
			{
				const FVector2D Offset = Rng.InsideUnitDisc() * Params.DebrisSpreadRadius;
				const FVector2D P = WallP + Offset;
				const FTerrainColumn Column = Density.SampleColumn(P.X, P.Y);
				if (Column.IslandIndex == INDEX_NONE || Column.Height < 0.5f)
				{
					continue;
				}
				const FVector Normal = Density.Normal(FVector(P, Column.Height), 1.0f);
				// Al pie de la pared, más llano que la pared misma: es el derrubio, no la ladera entera.
				if (SlopeDegFromNormalZ(Normal.Z) > 48.0f)
				{
					continue;
				}
				if (IsAvoided(AvoidPoints, Params.AvoidRadius, P))
				{
					continue;
				}
				const bool bBoulder = Rng.Chance(0.4f);
				FFormationInstance Instance;
				Instance.Kind = bBoulder ? EFormationKind::Boulder : EFormationKind::Cobble;
				Instance.MeshFilter = FName(bBoulder ? TEXT("RockBoulder") : TEXT("RockCobble"));
				Instance.ManifestGroup = FName(BlocksGroup);
				Instance.VariantIndex = static_cast<int32>(ExploredHash::Hash2D(DebrisSeed, W, Attempt));
				const float Scale = Rng.RangeFloat(0.6f, 1.3f);
				const float Yaw = Rng.RangeFloat(0.0f, UE_TWO_PI);
				Instance.Transform = FTransform(FQuat(FVector::UpVector, Yaw), FVector(P, Column.Height) * 100.0,
					FVector(Scale));
				Out.Add(Instance);
				++Placed;
			}
		}
	}

	/** Bloques y cantos en los extremos rocosos de las playas: puntos con pendiente moderada a la altura de marea. */
	void GenerateBeachRockPoints(const FTerrainDensity& Density, uint32 Seed, const TArray<FVector>& AvoidPoints,
		const FFormationPlacementParams& Params, TArray<FFormationInstance>& Out)
	{
		const FArchipelagoLayout& Layout = Density.GetLayout();
		const uint32 BeachSeed = ExploredHash::Hash32(Seed ^ 0x00BEAC44u);
		const FExploredNoise Cluster(BeachSeed);

		for (int32 IslandIndex = 0; IslandIndex < Layout.Islands.Num(); ++IslandIndex)
		{
			const FIslandDesc& Island = Layout.Islands[IslandIndex];
			const float Extent = Island.Radius * 1.15f;
			const int32 MinX = FMath::FloorToInt32((Island.Center.X - Extent) / Params.BeachRockSpacing);
			const int32 MaxX = FMath::CeilToInt32((Island.Center.X + Extent) / Params.BeachRockSpacing);
			const int32 MinY = FMath::FloorToInt32((Island.Center.Y - Extent) / Params.BeachRockSpacing);
			const int32 MaxY = FMath::CeilToInt32((Island.Center.Y + Extent) / Params.BeachRockSpacing);

			for (int32 GY = MinY; GY <= MaxY; ++GY)
			{
				for (int32 GX = MinX; GX <= MaxX; ++GX)
				{
					const uint32 H = ExploredHash::Hash2D(BeachSeed, GX, GY);
					FExploredRandom Rng(H);
					const float X = (GX + Rng.RangeFloat(0.15f, 0.85f)) * Params.BeachRockSpacing;
					const float Y = (GY + Rng.RangeFloat(0.15f, 0.85f)) * Params.BeachRockSpacing;
					const FVector2D P(X, Y);
					const FTerrainColumn Column = Density.SampleColumn(X, Y);
					if (Column.IslandIndex != IslandIndex || Column.Height < 0.8f || Column.Height > 4.5f)
					{
						continue; // franja de playa, no el interior
					}
					const FVector Normal = Density.Normal(FVector(X, Y, Column.Height), 1.0f);
					const float SlopeDeg = SlopeDegFromNormalZ(Normal.Z);
					if (SlopeDeg < Params.MinBeachRockSlopeDeg || SlopeDeg > Params.MaxBeachRockSlopeDeg)
					{
						continue;
					}
					const float ClusterValue = Cluster.Fbm2D(X / 40.0f, Y / 40.0f, 2);
					if (ClusterValue < Params.BeachRockClusterThreshold)
					{
						continue;
					}
					if (IsAvoided(AvoidPoints, Params.AvoidRadius, P))
					{
						continue;
					}

					FExploredRandom Local(ExploredHash::Hash2D(H, 1, 1));
					for (int32 D = 0; D < Params.DebrisPerBeachPoint; ++D)
					{
						const FVector2D Offset = Local.InsideUnitDisc() * 4.0f;
						const FVector2D BP = P + Offset;
						const FTerrainColumn BColumn = Density.SampleColumn(BP.X, BP.Y);
						// El punto base ya está en la franja de playa, pero el salpicado de 4 m puede
						// caer justo al otro lado de un borde de costa cortado: se descarta el agua honda.
						if (BColumn.IslandIndex != IslandIndex || BColumn.Height < -0.5f)
						{
							continue;
						}
						if (IsAvoided(AvoidPoints, Params.AvoidRadius, BP))
						{
							continue;
						}
						const bool bBoulder = Local.Chance(0.5f);
						FFormationInstance Instance;
						Instance.Kind = bBoulder ? EFormationKind::Boulder : EFormationKind::Cobble;
						Instance.MeshFilter = FName(bBoulder ? TEXT("RockBoulder") : TEXT("RockCobble"));
						Instance.ManifestGroup = FName(BlocksGroup);
						Instance.VariantIndex = static_cast<int32>(ExploredHash::Hash2D(H, 2, D));
						const float Scale = Local.RangeFloat(0.5f, 1.1f);
						const float Yaw = Local.RangeFloat(0.0f, UE_TWO_PI);
						Instance.Transform = FTransform(FQuat(FVector::UpVector, Yaw), FVector(BP, BColumn.Height) * 100.0,
							FVector(Scale));
						Out.Add(Instance);
					}
				}
			}
		}
	}

	/** Losas de caliza sueltas en el llano alto de la meseta kárstica. */
	void GenerateLimestoneSlabs(const FTerrainDensity& Density, uint32 Seed, const TArray<FVector>& AvoidPoints,
		const FFormationPlacementParams& Params, TArray<FFormationInstance>& Out)
	{
		const FIslandDesc* Mesa = Density.GetLayout().FindIsland(EIslandArchetype::Mesa);
		if (!Mesa)
		{
			return;
		}
		const uint32 SlabSeed = ExploredHash::Hash32(Seed ^ 0x51AB5000u);
		const FExploredNoise Cluster(SlabSeed);
		const float MinH = Mesa->MaxHeight * Params.SlabMinHeightFrac;
		const float MaxH = Mesa->MaxHeight * Params.SlabMaxHeightFrac;

		const float Extent = Mesa->Radius * 1.05f;
		const int32 MinX = FMath::FloorToInt32((Mesa->Center.X - Extent) / Params.SlabSpacing);
		const int32 MaxX = FMath::CeilToInt32((Mesa->Center.X + Extent) / Params.SlabSpacing);
		const int32 MinY = FMath::FloorToInt32((Mesa->Center.Y - Extent) / Params.SlabSpacing);
		const int32 MaxY = FMath::CeilToInt32((Mesa->Center.Y + Extent) / Params.SlabSpacing);

		int32 Placed = 0;
		for (int32 GY = MinY; GY <= MaxY && Placed < Params.MaxSlabs; ++GY)
		{
			for (int32 GX = MinX; GX <= MaxX && Placed < Params.MaxSlabs; ++GX)
			{
				const uint32 H = ExploredHash::Hash2D(SlabSeed, GX, GY);
				FExploredRandom Rng(H);
				const float X = (GX + Rng.RangeFloat(0.15f, 0.85f)) * Params.SlabSpacing;
				const float Y = (GY + Rng.RangeFloat(0.15f, 0.85f)) * Params.SlabSpacing;
				const FVector2D P(X, Y);
				const FTerrainColumn Column = Density.SampleColumn(X, Y);
				if (Column.IslandIndex == INDEX_NONE ||
					Density.GetLayout().Islands[Column.IslandIndex].Archetype != EIslandArchetype::Mesa)
				{
					continue;
				}
				if (Column.Height < MinH || Column.Height > MaxH)
				{
					continue;
				}
				const FVector Normal = Density.Normal(FVector(X, Y, Column.Height), 1.0f);
				if (Normal.Z < Params.SlabMinNormalZ)
				{
					continue;
				}
				const float ClusterValue = Cluster.Fbm2D(X / 26.0f, Y / 26.0f, 2);
				if (ClusterValue < Params.SlabClusterThreshold)
				{
					continue;
				}
				if (IsAvoided(AvoidPoints, Params.AvoidRadius, P))
				{
					continue;
				}

				FFormationInstance Instance;
				Instance.Kind = EFormationKind::LimestoneSlab;
				Instance.MeshFilter = FName(TEXT("LimestoneSlab"));
				Instance.ManifestGroup = FName(BlocksGroup);
				Instance.VariantIndex = static_cast<int32>(H);
				const float Scale = Rng.RangeFloat(0.8f, 1.4f);
				const float Yaw = Rng.RangeFloat(0.0f, UE_TWO_PI);
				Instance.Transform = FTransform(FQuat(FVector::UpVector, Yaw), FVector(P, Column.Height) * 100.0,
					FVector(Scale));
				Out.Add(Instance);
				++Placed;
			}
		}
	}
}

TArray<FFormationInstance> FFormationPlacementModel::Generate(const FTerrainDensity& Density, uint32 Seed,
	const TArray<FVector>& AvoidPoints, const FFormationPlacementParams& Params)
{
	TArray<FFormationInstance> Out;
	TArray<FVector2D> WallPositions;
	GenerateWallsAndSpurs(Density, Seed, AvoidPoints, Params, Out, WallPositions);
	GenerateSeaStacks(Density, Seed, AvoidPoints, Params, Out);
	GenerateSeaArch(Density, AvoidPoints, Params, Out);
	GenerateDebrisAroundWalls(Density, Seed, WallPositions, AvoidPoints, Params, Out);
	GenerateBeachRockPoints(Density, Seed, AvoidPoints, Params, Out);
	GenerateLimestoneSlabs(Density, Seed, AvoidPoints, Params, Out);
	return Out;
}
