#include "WorldGen/FellingModel.h"

namespace
{
	FFellingYield MakeYield(const TCHAR* ItemId, EFellingYieldKind Kind, int32 MinCount, int32 MaxCount)
	{
		FFellingYield Yield;
		Yield.ItemId = FName(ItemId);
		Yield.Kind = Kind;
		Yield.MinCount = MinCount;
		Yield.MaxCount = MaxCount;
		return Yield;
	}

	FHarvestDrop MakeUprootDrop(const TCHAR* ItemId, int32 MinCount, int32 MaxCount)
	{
		FHarvestDrop Drop;
		Drop.ItemId = FName(ItemId);
		Drop.MinCount = MinCount;
		Drop.MaxCount = MaxCount;
		return Drop;
	}

	/** Golpes acotados a [1, 512]: con más, ceil(W/N)·(N-1) podría alcanzar W un golpe antes. */
	int32 ClampHits(int32 Hits)
	{
		return FMath::Clamp(Hits, 1, 512);
	}

	/** Trabajo de un golpe redondeado hacia arriba: N golpes siempre bastan y N-1 nunca. */
	int32 WorkPerHit(int32 Hits)
	{
		const int32 N = ClampHits(Hits);
		return (FFellingModel::WorkToFell + N - 1) / N;
	}

	/** Por debajo de este empuje neto (fracción del trabajo total) los golpes se consideran anulados. */
	constexpr double MinNetPush = 1.0e-3;

	constexpr int32 HandsIdx = (int32)EFellingTool::Hands;
	constexpr int32 BluntIdx = (int32)EFellingTool::Blunt;
	constexpr int32 EdgeIdx = (int32)EFellingTool::Edge;
	constexpr int32 ShovelIdx = (int32)EFellingTool::Shovel;
}

TArray<FFellingProfile> FFellingModel::DefaultProfiles()
{
	using K = EFellingYieldKind;
	TArray<FFellingProfile> Profiles;

	{
		// Palmera: tronco fino y alto, cocos y hojas en la copa. Rebrota del estípite si no se arranca.
		FFellingProfile P;
		P.Species = FName(TEXT("Palm"));
		P.HitsByTool[HandsIdx] = 8;
		P.HitsByTool[BluntIdx] = 7;
		P.HitsByTool[EdgeIdx] = 4;
		P.Yields = {
			MakeYield(TEXT("tronco_pequeno"), K::Log, 1, 1),
			MakeYield(TEXT("hoja_palma"), K::Leaf, 2, 4),
			MakeYield(TEXT("fibra_coco"), K::Leaf, 0, 2),
			MakeYield(TEXT("coco_maduro"), K::Fruit, 1, 3),
			MakeYield(TEXT("coco_verde"), K::Fruit, 0, 2),
			MakeYield(TEXT("cascara_coco"), K::Fruit, 0, 1),
		};
		P.HeightMeters = 9.0f;
		P.CrownRadiusMeters = 3.0f;
		P.StumpRegrowDays = 12;
		P.SaplingToMatureDays = 30;
		P.UprootShovelHits = 4;
		P.UprootDrops = { MakeUprootDrop(TEXT("fibra_coco"), 1, 2) };
		P.GroundBranchCapacity = 2;
		P.GroundBranchPerDayMilli = 500; // una hoja seca cada dos días
		P.GroundBranchItem = FName(TEXT("hoja_palma"));
		Profiles.Add(P);
	}
	{
		// Gigante de la jungla: el que más madera da y el que más tarda en volver.
		FFellingProfile P;
		P.Species = FName(TEXT("JungleGiant"));
		P.HitsByTool[HandsIdx] = 14;
		P.HitsByTool[BluntIdx] = 12;
		P.HitsByTool[EdgeIdx] = 6;
		P.Yields = {
			MakeYield(TEXT("tronco_pequeno"), K::Log, 2, 3),
			MakeYield(TEXT("madera_dura"), K::Log, 1, 2),
			MakeYield(TEXT("corteza"), K::Log, 1, 2),
			MakeYield(TEXT("resina"), K::Log, 0, 1),
			MakeYield(TEXT("rama_seca"), K::Branch, 2, 4),
		};
		P.HeightMeters = 22.0f;
		P.CrownRadiusMeters = 6.0f;
		P.StumpRegrowDays = 20;
		P.SaplingToMatureDays = 60;
		P.UprootShovelHits = 8;
		P.UprootDrops = { MakeUprootDrop(TEXT("madera_dura"), 0, 1), MakeUprootDrop(TEXT("rama_seca"), 1, 2) };
		P.GroundBranchCapacity = 4;
		P.GroundBranchPerDayMilli = 1500;
		P.GroundBranchItem = FName(TEXT("rama_seca"));
		Profiles.Add(P);
	}
	{
		FFellingProfile P;
		P.Species = FName(TEXT("JungleWide"));
		P.HitsByTool[HandsIdx] = 11;
		P.HitsByTool[BluntIdx] = 10;
		P.HitsByTool[EdgeIdx] = 5;
		P.Yields = {
			MakeYield(TEXT("tronco_pequeno"), K::Log, 1, 2),
			MakeYield(TEXT("madera_blanda"), K::Log, 1, 1),
			MakeYield(TEXT("corteza"), K::Log, 1, 1),
			MakeYield(TEXT("rama_verde"), K::Branch, 2, 3),
			MakeYield(TEXT("rama_seca"), K::Branch, 1, 2),
		};
		P.HeightMeters = 14.0f;
		P.CrownRadiusMeters = 7.0f;
		P.StumpRegrowDays = 15;
		P.SaplingToMatureDays = 45;
		P.UprootShovelHits = 6;
		P.UprootDrops = { MakeUprootDrop(TEXT("rama_seca"), 1, 2) };
		P.GroundBranchCapacity = 4;
		P.GroundBranchPerDayMilli = 1200;
		P.GroundBranchItem = FName(TEXT("rama_seca"));
		Profiles.Add(P);
	}
	{
		// Manglar: bajo y ancho, suelta madera flotante. Rebrota a los 20 días (480 h, como en FHarvestModel).
		FFellingProfile P;
		P.Species = FName(TEXT("Mangrove"));
		P.HitsByTool[HandsIdx] = 9;
		P.HitsByTool[BluntIdx] = 8;
		P.HitsByTool[EdgeIdx] = 4;
		P.Yields = {
			MakeYield(TEXT("tronco_pequeno"), K::Log, 1, 2),
			MakeYield(TEXT("madera_flotante"), K::Log, 1, 1),
			MakeYield(TEXT("rama_seca"), K::Branch, 1, 3),
		};
		P.HeightMeters = 7.0f;
		P.CrownRadiusMeters = 4.0f;
		P.StumpRegrowDays = 20;
		P.SaplingToMatureDays = 30;
		P.UprootShovelHits = 5;
		P.UprootDrops = { MakeUprootDrop(TEXT("madera_flotante"), 0, 1) };
		P.GroundBranchCapacity = 2;
		P.GroundBranchPerDayMilli = 600;
		P.GroundBranchItem = FName(TEXT("rama_seca"));
		Profiles.Add(P);
	}
	{
		// Sotobosque: árbol joven, cae corto y vuelve pronto (10 días = 240 h de FHarvestModel).
		FFellingProfile P;
		P.Species = FName(TEXT("Understory"));
		P.HitsByTool[HandsIdx] = 5;
		P.HitsByTool[BluntIdx] = 4;
		P.HitsByTool[EdgeIdx] = 3;
		P.Yields = {
			MakeYield(TEXT("tronco_pequeno"), K::Log, 1, 1),
			MakeYield(TEXT("palo_recto"), K::Branch, 1, 2),
			MakeYield(TEXT("rama_seca"), K::Branch, 1, 2),
			MakeYield(TEXT("hoja_platano"), K::Leaf, 0, 2),
		};
		P.HeightMeters = 5.0f;
		P.CrownRadiusMeters = 2.0f;
		P.StumpRegrowDays = 10;
		P.SaplingToMatureDays = 12;
		P.UprootShovelHits = 3;
		P.UprootDrops = { MakeUprootDrop(TEXT("rama_seca"), 1, 1) };
		P.GroundBranchCapacity = 2;
		P.GroundBranchPerDayMilli = 800;
		P.GroundBranchItem = FName(TEXT("rama_seca"));
		Profiles.Add(P);
	}
	{
		// Arbusto: no cae, se desbroza en el sitio. Un golpe con cualquier cosa; la pala lo arranca de raíz.
		FFellingProfile P;
		P.Species = FName(TEXT("Shrub"));
		P.HitsByTool[HandsIdx] = 1;
		P.HitsByTool[BluntIdx] = 1;
		P.HitsByTool[EdgeIdx] = 1;
		P.HitsByTool[ShovelIdx] = 1;
		P.Yields = {
			MakeYield(TEXT("corteza"), K::Branch, 0, 1),
			MakeYield(TEXT("liana"), K::Branch, 0, 1),
			MakeYield(TEXT("vara_flexible"), K::Branch, 0, 1),
			MakeYield(TEXT("algodon_silvestre"), K::Leaf, 0, 1),
		};
		P.HeightMeters = 0.0f;
		P.CrownRadiusMeters = 1.0f;
		P.StumpRegrowDays = 3; // 72 h de FHarvestModel
		P.SaplingToMatureDays = 2;
		P.UprootShovelHits = 1;
		P.UprootDrops = {};
		P.GroundBranchCapacity = 1;
		P.GroundBranchPerDayMilli = 300;
		P.GroundBranchItem = FName(TEXT("rama_seca"));
		Profiles.Add(P);
	}

	return Profiles;
}

const FFellingProfile* FFellingModel::FindProfile(const TArray<FFellingProfile>& Profiles, FName Species)
{
	for (const FFellingProfile& Profile : Profiles)
	{
		if (Profile.Species == Species)
		{
			return &Profile;
		}
	}
	return nullptr;
}

int32 FFellingModel::HitsRequired(const FFellingProfile& Profile, EFellingTool Tool)
{
	const int32 Index = (int32)Tool;
	if (Index < 0 || Index >= (int32)EFellingTool::Count)
	{
		return 0;
	}
	const int32 Hits = Profile.HitsByTool[Index];
	return Hits > 0 ? ClampHits(Hits) : 0;
}

bool FFellingModel::ApplyHit(const FFellingProfile& Profile, FFellingProgress& Progress, EFellingTool Tool, const FVector2D& HitDirection)
{
	const int32 Hits = HitsRequired(Profile, Tool);
	if (Hits <= 0 || Progress.Work >= WorkToFell)
	{
		return false;
	}
	const int32 Work = WorkPerHit(Hits);
	Progress.Push += HitDirection.GetSafeNormal() * ((double)Work / (double)WorkToFell);
	Progress.Work = FMath::Min(Progress.Work + Work, WorkToFell);
	return Progress.Work >= WorkToFell;
}

FVector2D FFellingModel::ResolveFallDirection(const FFellingProgress& Progress, const FVector2D& Downhill, uint32 InstanceSeed)
{
	const FVector2D PushDir = Progress.Push.Size() >= MinNetPush ? Progress.Push.GetSafeNormal() : FVector2D::ZeroVector;
	const FVector2D Combined = PushDir + Downhill * SlopeWeight;
	if (Combined.Size() >= MinNetPush)
	{
		return Combined.GetSafeNormal();
	}
	const double Angle = (double)ExploredHash::ToUnitFloat(ExploredHash::Hash32(InstanceSeed)) * 2.0 * UE_DOUBLE_PI;
	return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
}

TArray<FFellingDrop> FFellingModel::ComputeFellDrops(const FFellingProfile& Profile, const FVector2D& Base, const FVector2D& FallDirection, FExploredRandom& Random)
{
	FVector2D Dir = FallDirection.GetSafeNormal();
	if (Dir.IsZero())
	{
		Dir = FVector2D(1.0, 0.0);
	}
	const FVector2D Perp(-Dir.Y, Dir.X);
	const double HeightCm = FMath::Max(0.0f, Profile.HeightMeters) * 100.0;
	const double CrownCm = FMath::Max(0.0f, Profile.CrownRadiusMeters) * 100.0;
	// La copa empieza al 85 % del tronco; los troncos se reparten por el 80 % inferior.
	const FVector2D Crown = Base + Dir * (HeightCm * 0.85);

	// Primero se tiran todas las cantidades (orden fijo de Yields) y luego se colocan:
	// así el número de troncos se conoce antes de repartirlos a lo largo del tronco.
	TArray<int32> Counts;
	Counts.Reserve(Profile.Yields.Num());
	int32 NumLogs = 0;
	for (const FFellingYield& Yield : Profile.Yields)
	{
		const int32 Lo = FMath::Max(0, Yield.MinCount);
		const int32 Hi = FMath::Max(Lo, Yield.MaxCount);
		const int32 Count = Hi > Lo ? Random.RangeInt(Lo, Hi) : Lo;
		Counts.Add(Count);
		if (Yield.Kind == EFellingYieldKind::Log)
		{
			NumLogs += Count;
		}
	}

	TArray<FFellingDrop> Drops;
	int32 LogIndex = 0;
	for (int32 i = 0; i < Profile.Yields.Num(); ++i)
	{
		const FFellingYield& Yield = Profile.Yields[i];
		for (int32 n = 0; n < Counts[i]; ++n)
		{
			FFellingDrop Drop;
			Drop.ItemId = Yield.ItemId;
			Drop.Kind = Yield.Kind;
			switch (Yield.Kind)
			{
			case EFellingYieldKind::Log:
			{
				const double T = ((double)LogIndex + 0.5) / (double)NumLogs;
				Drop.Position = Base + Dir * (HeightCm * 0.8 * T);
				++LogIndex;
				break;
			}
			case EFellingYieldKind::Branch:
			case EFellingYieldKind::Leaf:
			{
				const double Across = (double)Random.RangeFloat(-0.5f, 0.5f) * CrownCm;
				const double Along = (double)Random.RangeFloat(-0.25f, 0.25f) * CrownCm;
				Drop.Position = Crown + Perp * Across + Dir * Along;
				break;
			}
			case EFellingYieldKind::Fruit:
			default:
				Drop.Position = Crown + Random.InsideUnitDisc() * CrownCm;
				break;
			}
			Drops.Add(Drop);
		}
	}
	return Drops;
}

EStumpStage FFellingModel::StageAt(const FFellingProfile& Profile, const FStumpState& Stump, int64 NowMinute)
{
	if (Stump.bUprooted)
	{
		return EStumpStage::Uprooted;
	}
	if (Profile.StumpRegrowDays <= 0)
	{
		return EStumpStage::Stump;
	}
	const int64 Elapsed = NowMinute - Stump.FelledAtMinute;
	const int64 SproutAt = (int64)Profile.StumpRegrowDays * MinutesPerDay;
	const int64 MatureAt = SproutAt + (int64)FMath::Max(1, Profile.SaplingToMatureDays) * MinutesPerDay;
	if (Elapsed < SproutAt)
	{
		return EStumpStage::Stump;
	}
	return Elapsed < MatureAt ? EStumpStage::Sapling : EStumpStage::Mature;
}

float FFellingModel::GrowthScaleAt(const FFellingProfile& Profile, const FStumpState& Stump, int64 NowMinute)
{
	switch (StageAt(Profile, Stump, NowMinute))
	{
	case EStumpStage::Mature:
		return 1.0f;
	case EStumpStage::Sapling:
	{
		const int64 SproutAt = (int64)Profile.StumpRegrowDays * MinutesPerDay;
		const int64 Span = (int64)FMath::Max(1, Profile.SaplingToMatureDays) * MinutesPerDay;
		const double Alpha = (double)(NowMinute - Stump.FelledAtMinute - SproutAt) / (double)Span;
		return (float)FMath::Lerp((double)SaplingStartScale, 1.0, FMath::Clamp(Alpha, 0.0, 1.0));
	}
	default:
		return 0.0f;
	}
}

bool FFellingModel::ApplyUprootHit(const FFellingProfile& Profile, FStumpState& Stump, EFellingTool Tool, int64 NowMinute)
{
	if (Tool != EFellingTool::Shovel)
	{
		return false;
	}
	const EStumpStage Stage = StageAt(Profile, Stump, NowMinute);
	if (Stage != EStumpStage::Stump && Stage != EStumpStage::Sapling)
	{
		return false;
	}
	Stump.UprootWork = FMath::Min(Stump.UprootWork + WorkPerHit(Profile.UprootShovelHits), WorkToFell);
	Stump.bUprooted = Stump.UprootWork >= WorkToFell;
	return Stump.bUprooted;
}

FGroundBranchSource FFellingModel::MakeBranchSource(const FFellingProfile& Profile, const FVector2D& Position)
{
	FGroundBranchSource Source;
	Source.Position = Position;
	Source.CrownRadiusMeters = Profile.CrownRadiusMeters;
	Source.Capacity = Profile.GroundBranchCapacity;
	Source.PerDayMilli = Profile.GroundBranchPerDayMilli;
	Source.ItemId = Profile.GroundBranchItem;
	return Source;
}

FIntPoint FFellingModel::CellOf(const FVector2D& Position, double CellSize)
{
	if (!(CellSize > 0.0))
	{
		return FIntPoint(0, 0);
	}
	return FIntPoint(FMath::FloorToInt32(Position.X / CellSize), FMath::FloorToInt32(Position.Y / CellSize));
}
