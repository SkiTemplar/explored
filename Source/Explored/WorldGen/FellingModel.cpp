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

	/** Por debajo de este módulo, golpe + pendiente se consideran anulados. */
	constexpr double MinNetPush = 1.0e-3;

	/** Ramas del suelo de un árbol (biblia 02 §1.3): 2–4 rama_seca por ciclo de 6 h, tope de 6. */
	void SetTreeGroundBranches(FFellingProfile& P)
	{
		P.GroundBranchCapacity = 6;
		P.GroundBranchCycleMin = 2;
		P.GroundBranchCycleMax = 4;
		P.GroundBranchItem = FName(TEXT("rama_seca"));
	}

	/** Biblia 02 §1.2: 18 días con fruto, 24 madera sin fruto, 4 arbustos. */
	constexpr int32 RegrowDaysFruit = 18;
	constexpr int32 RegrowDaysTimber = 24;
	constexpr int32 RegrowDaysShrub = 4;

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
		// Palmera: tronco fino y alto, cocos maduros y hojas en la copa. Rebrota del estípite si no
		// se arranca. El coco_verde no cae al talar: se coge trepando (biblia 02 §13.1).
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
			MakeYield(TEXT("cascara_coco"), K::Fruit, 0, 1),
		};
		P.HeightMeters = 9.0f;
		P.CrownRadiusMeters = 3.0f;
		P.StumpRegrowDays = RegrowDaysFruit;
		P.SproutDays = 6;
		P.WindDeviationDeg = 15.0f;
		P.UprootShovelHits = 4;
		P.UprootDrops = { MakeUprootDrop(TEXT("fibra_coco"), 1, 2) };
		SetTreeGroundBranches(P);
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
		P.StumpRegrowDays = RegrowDaysTimber;
		P.SproutDays = 8;
		P.UprootShovelHits = 8;
		P.UprootDrops = { MakeUprootDrop(TEXT("madera_dura"), 0, 1), MakeUprootDrop(TEXT("rama_seca"), 1, 2) };
		SetTreeGroundBranches(P);
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
		P.StumpRegrowDays = RegrowDaysTimber;
		P.SproutDays = 8;
		P.UprootShovelHits = 6;
		P.UprootDrops = { MakeUprootDrop(TEXT("rama_seca"), 1, 2) };
		SetTreeGroundBranches(P);
		Profiles.Add(P);
	}
	{
		// Manglar: bajo y ancho, suelta madera flotante. Madera sin fruto: 24 días.
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
		P.StumpRegrowDays = RegrowDaysTimber;
		P.SproutDays = 8;
		P.UprootShovelHits = 5;
		P.UprootDrops = { MakeUprootDrop(TEXT("madera_flotante"), 0, 1) };
		SetTreeGroundBranches(P);
		Profiles.Add(P);
	}
	{
		// Sotobosque: árbol joven que cae corto. Madera sin fruto: 24 días, aunque el brote asoma antes.
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
		P.StumpRegrowDays = RegrowDaysTimber;
		P.SproutDays = 4;
		P.UprootShovelHits = 3;
		P.UprootDrops = { MakeUprootDrop(TEXT("rama_seca"), 1, 1) };
		SetTreeGroundBranches(P);
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
		P.StumpRegrowDays = RegrowDaysShrub;
		P.SproutDays = 1;
		P.WindDeviationDeg = 0.0f; // no cae
		P.UprootShovelHits = 1;
		P.UprootDrops = {};
		// No es un árbol: sin ramas del suelo propias (biblia 02 §1.3 «bajo cada árbol»).
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
	const FVector2D Dir = HitDirection.GetSafeNormal();
	if (!Dir.IsZero())
	{
		// Un golpe sin dirección (NaN, cero) cuenta como trabajo pero no cambia hacia dónde caerá.
		Progress.LastHit = Dir;
	}
	Progress.Work = FMath::Min(Progress.Work + Work, WorkToFell);
	return Progress.Work >= WorkToFell;
}

FVector2D FFellingModel::ResolveFallDirection(const FFellingProgress& Progress, const FVector2D& Downhill, uint32 InstanceSeed)
{
	const FVector2D PushDir = Progress.LastHit.GetSafeNormal();
	// Una pendiente NaN o infinita (muestra de terreno corrupta) se ignora.
	const FVector2D Slope = (FMath::IsFinite(Downhill.X) && FMath::IsFinite(Downhill.Y)) ? Downhill : FVector2D::ZeroVector;
	const FVector2D Combined = PushDir + Slope * SlopeWeight;
	if (Combined.Size() >= MinNetPush)
	{
		return Combined.GetSafeNormal();
	}
	const double Angle = (double)ExploredHash::ToUnitFloat(ExploredHash::Hash32(InstanceSeed)) * 2.0 * UE_DOUBLE_PI;
	return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
}

TArray<FFellingDrop> FFellingModel::ComputeFellDrops(const FFellingProfile& Profile, const FVector2D& Base, const FVector2D& FallDirection, FExploredRandom& Random, double ReachFraction)
{
	FVector2D Dir = FallDirection.GetSafeNormal();
	if (Dir.IsZero())
	{
		Dir = FVector2D(1.0, 0.0);
	}
	const FVector2D Perp(-Dir.Y, Dir.X);
	// Apoyado en algo, el tronco solo se proyecta en parte sobre el suelo (NaN → cae entero).
	const double Reach = FMath::IsFinite(ReachFraction) ? FMath::Clamp(ReachFraction, 0.0, 1.0) : 1.0;
	const double HeightCm = FMath::Max(0.0f, Profile.HeightMeters) * 100.0 * Reach;
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

int64 FFellingModel::RegrowMinutes(const FFellingProfile& Profile)
{
	return Profile.StumpRegrowDays > 0 ? (int64)Profile.StumpRegrowDays * MinutesPerDay : 0;
}

int64 FFellingModel::SproutMinutes(const FFellingProfile& Profile)
{
	const int64 Regrow = RegrowMinutes(Profile);
	if (Regrow <= 0)
	{
		return 0;
	}
	return FMath::Clamp((int64)Profile.SproutDays * MinutesPerDay, (int64)1, Regrow - 1);
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
	const int64 SproutAt = SproutMinutes(Profile);
	const int64 MatureAt = RegrowMinutes(Profile);
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
		const int64 SproutAt = SproutMinutes(Profile);
		const int64 Span = FMath::Max((int64)1, RegrowMinutes(Profile) - SproutAt);
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
	Source.CycleMin = Profile.GroundBranchCycleMin;
	Source.CycleMax = Profile.GroundBranchCycleMax;
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
