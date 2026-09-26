#include "Fauna/FaunaSpawning.h"

#include "Core/ExploredRandom.h"

namespace FaunaSpawningDetail
{
	/** Canales de hash independientes por uso. */
	constexpr int32 RollChannel = 1;
	constexpr int32 PlaceXChannel = 2;
	constexpr int32 PlaceYChannel = 3;
	constexpr int32 SizeChannel = 4;
	constexpr int32 SeedChannel = 5;

	float Unit(uint32 WorldSeed, const FIntPoint& Cell, EFaunaSpecies Species, int32 Channel)
	{
		const uint32 Salted = ExploredHash::Hash32(WorldSeed ^ (static_cast<uint32>(Species) * 0x9E3779B9u) ^ static_cast<uint32>(Channel) * 0x85EBCA6Bu);
		return ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Salted, Cell.X, Cell.Y));
	}

	/** Altura de vuelo inicial de las bandadas (cm sobre el nivel del mar). */
	constexpr float BirdSpawnAltitudeCm = 1500.0f;
}

const TCHAR* LexToString(EMarineZone Zone)
{
	switch (Zone)
	{
	case EMarineZone::Land: return TEXT("Land");
	case EMarineZone::Shallows: return TEXT("Shallows");
	case EMarineZone::Lagoon: return TEXT("Lagoon");
	case EMarineZone::Reef: return TEXT("Reef");
	case EMarineZone::Slope: return TEXT("Slope");
	case EMarineZone::Deep: return TEXT("Deep");
	default: return TEXT("Unknown");
	}
}

EMarineZone FFaunaSpawnRules::ClassifyZone(float SeabedZCm, bool bLagoon)
{
	const float Depth = -SeabedZCm;
	if (Depth <= 0.0f)
	{
		return EMarineZone::Land;
	}
	if (bLagoon)
	{
		return EMarineZone::Lagoon;
	}
	if (Depth < ReefMinDepthCm)
	{
		return EMarineZone::Shallows;
	}
	if (Depth < SlopeMinDepthCm)
	{
		return EMarineZone::Reef;
	}
	if (Depth < DeepMinDepthCm)
	{
		return EMarineZone::Slope;
	}
	return EMarineZone::Deep;
}

float FFaunaSpawnRules::Chance(EFaunaSpecies Species, const FFaunaCellContext& Context)
{
	const EMarineZone Zone = ClassifyZone(Context.SeabedZCm, Context.bLagoon);
	const bool bCoast = Context.DistanceToCoastCm < BirdCoastRangeCm;
	float Base = 0.0f;
	switch (Species)
	{
	case EFaunaSpecies::ReefFish:
		Base = Zone == EMarineZone::Reef ? 0.85f : Zone == EMarineZone::Lagoon ? 0.55f : Zone == EMarineZone::Shallows ? 0.3f : 0.0f;
		break;
	case EFaunaSpecies::OpenSeaFish:
		Base = Zone == EMarineZone::Deep ? 0.35f : Zone == EMarineZone::Slope ? 0.25f : 0.0f;
		break;
	case EFaunaSpecies::Stingray:
		Base = Zone == EMarineZone::Shallows ? 0.35f : Zone == EMarineZone::Lagoon ? 0.3f : 0.0f;
		break;
	case EFaunaSpecies::Jellyfish:
		Base = Zone == EMarineZone::Deep ? 0.25f : Zone == EMarineZone::Slope ? 0.15f : 0.0f;
		break;
	case EFaunaSpecies::ReefShark:
		Base = Zone == EMarineZone::Slope ? 0.2f : Zone == EMarineZone::Reef ? 0.05f : 0.0f;
		break;
	case EFaunaSpecies::TigerShark:
		// Solo en aguas profundas: es el límite natural del mundo.
		Base = Zone == EMarineZone::Deep && -Context.SeabedZCm >= FFaunaSpeciesInfo::Get(Species).MinWaterDepthCm ? 0.08f : 0.0f;
		break;
	case EFaunaSpecies::Dolphin:
		Base = Zone == EMarineZone::Deep ? 0.06f : Zone == EMarineZone::Slope ? 0.03f : 0.0f;
		break;
	case EFaunaSpecies::SeaTurtle:
		Base = Zone == EMarineZone::Lagoon ? 0.4f : (Zone == EMarineZone::Shallows || Zone == EMarineZone::Reef) ? 0.05f : 0.0f;
		break;
	case EFaunaSpecies::HumpbackWhale:
		// Solo durante el paso de ballenas y lejos de la costa: se ven desde miradores y acantilados.
		Base = Zone == EMarineZone::Deep && Context.bWhalePassage && Context.DistanceToCoastCm >= WhaleMinCoastDistanceCm ? 0.15f : 0.0f;
		break;
	case EFaunaSpecies::Gull:
		Base = Context.bNearTeeth ? 0.6f : bCoast ? 0.35f : 0.0f;
		break;
	case EFaunaSpecies::Frigatebird:
		Base = Context.bNearTeeth ? 0.5f : bCoast ? 0.08f : 0.0f;
		break;
	default:
		break;
	}
	return Base * FFaunaActivity::Level(Species, Context.Hours);
}

void FFaunaSpawnRules::GroupSizeRange(EFaunaSpecies Species, int32& OutMin, int32& OutMax)
{
	switch (Species)
	{
	case EFaunaSpecies::ReefFish: OutMin = 20; OutMax = 60; break;
	case EFaunaSpecies::OpenSeaFish: OutMin = 60; OutMax = 150; break;
	case EFaunaSpecies::Jellyfish: OutMin = 3; OutMax = 8; break;
	case EFaunaSpecies::Dolphin: OutMin = 3; OutMax = 7; break;
	case EFaunaSpecies::SeaTurtle: OutMin = 1; OutMax = 2; break;
	case EFaunaSpecies::HumpbackWhale: OutMin = 1; OutMax = 2; break;
	case EFaunaSpecies::Gull: OutMin = 6; OutMax = 20; break;
	case EFaunaSpecies::Frigatebird: OutMin = 3; OutMax = 10; break;
	default: OutMin = 1; OutMax = 1; break;
	}
}

void FFaunaSpawnRules::SpawnsForCell(uint32 WorldSeed, const FIntPoint& Cell, const FFaunaCellContext& Context, TArray<FFaunaSpawn>& Out)
{
	using namespace FaunaSpawningDetail;
	const FVector2D Center = CellCenter(Cell);
	for (int32 Index = 0; Index < static_cast<int32>(EFaunaSpecies::Count); ++Index)
	{
		const EFaunaSpecies Species = static_cast<EFaunaSpecies>(Index);
		const float P = Chance(Species, Context);
		if (P <= 0.0f || Unit(WorldSeed, Cell, Species, RollChannel) >= P)
		{
			continue;
		}
		FFaunaSpawn Spawn;
		Spawn.Species = Species;
		const float OX = (Unit(WorldSeed, Cell, Species, PlaceXChannel) - 0.5f) * CellSizeCm * 0.8f;
		const float OY = (Unit(WorldSeed, Cell, Species, PlaceYChannel) - 0.5f) * CellSizeCm * 0.8f;
		const bool bBird = FFaunaSpeciesInfo::Get(Species).bBird;
		// Z orientativa (mitad de la columna de agua en el centro); el modelo la ajusta a la franja real.
		const float Z = bBird ? BirdSpawnAltitudeCm : Context.SeabedZCm * 0.5f;
		Spawn.PositionCm = FVector(Center.X + OX, Center.Y + OY, Z);
		int32 MinSize = 1;
		int32 MaxSize = 1;
		GroupSizeRange(Species, MinSize, MaxSize);
		Spawn.GroupSize = MinSize + FMath::Min(MaxSize - MinSize, FMath::FloorToInt(Unit(WorldSeed, Cell, Species, SizeChannel) * (MaxSize - MinSize + 1)));
		Spawn.Seed = ExploredHash::Hash3D(WorldSeed, Cell.X, Cell.Y, Index * 7919 + SeedChannel);
		Out.Add(Spawn);
	}
}

FIntPoint FFaunaSpawnRules::CellOf(const FVector2D& PointCm)
{
	return FIntPoint(FMath::FloorToInt(PointCm.X / CellSizeCm), FMath::FloorToInt(PointCm.Y / CellSizeCm));
}

FVector2D FFaunaSpawnRules::CellCenter(const FIntPoint& Cell)
{
	return FVector2D((Cell.X + 0.5) * CellSizeCm, (Cell.Y + 0.5) * CellSizeCm);
}

FFaunaLodSettings FFaunaLod::ForSpecies(EFaunaSpecies Species)
{
	FFaunaLodSettings Settings;
	switch (Species)
	{
	case EFaunaSpecies::HumpbackWhale:
		Settings.FullRadiusCm = 30000.0f;
		Settings.ReducedRadiusCm = 80000.0f;
		break;
	case EFaunaSpecies::Gull:
	case EFaunaSpecies::Frigatebird:
		Settings.FullRadiusCm = 8000.0f;
		Settings.ReducedRadiusCm = 30000.0f;
		break;
	case EFaunaSpecies::ReefShark:
	case EFaunaSpecies::TigerShark:
	case EFaunaSpecies::Dolphin:
		Settings.FullRadiusCm = 6000.0f;
		Settings.ReducedRadiusCm = 20000.0f;
		break;
	default:
		break;
	}
	return Settings;
}

EFaunaLodTier FFaunaLod::Tier(float DistanceCm, EFaunaLodTier Previous, const FFaunaLodSettings& Settings)
{
	const float Up = 1.0f - Settings.Hysteresis;    // para acercarse de nivel
	const float Down = 1.0f + Settings.Hysteresis;  // para alejarse de nivel
	switch (Previous)
	{
	case EFaunaLodTier::Full:
		if (DistanceCm <= Settings.FullRadiusCm * Down)
		{
			return EFaunaLodTier::Full;
		}
		return DistanceCm <= Settings.ReducedRadiusCm * Down ? EFaunaLodTier::Reduced : EFaunaLodTier::Frozen;
	case EFaunaLodTier::Reduced:
		if (DistanceCm < Settings.FullRadiusCm * Up)
		{
			return EFaunaLodTier::Full;
		}
		return DistanceCm <= Settings.ReducedRadiusCm * Down ? EFaunaLodTier::Reduced : EFaunaLodTier::Frozen;
	case EFaunaLodTier::Frozen:
	default:
		if (DistanceCm < Settings.FullRadiusCm * Up)
		{
			return EFaunaLodTier::Full;
		}
		return DistanceCm < Settings.ReducedRadiusCm * Up ? EFaunaLodTier::Reduced : EFaunaLodTier::Frozen;
	}
}

bool FFaunaLod::ShouldTick(EFaunaLodTier Tier, uint64 Frame, uint32 AgentId, int32 ReducedInterval)
{
	switch (Tier)
	{
	case EFaunaLodTier::Full:
		return true;
	case EFaunaLodTier::Reduced:
	{
		const uint64 Interval = static_cast<uint64>(FMath::Max(1, ReducedInterval));
		return (Frame + AgentId) % Interval == 0;
	}
	default:
		return false;
	}
}

float FFaunaLod::TickDelta(EFaunaLodTier Tier, float FrameDeltaSeconds, int32 ReducedInterval)
{
	switch (Tier)
	{
	case EFaunaLodTier::Full:
		return FrameDeltaSeconds;
	case EFaunaLodTier::Reduced:
		return FrameDeltaSeconds * FMath::Max(1, ReducedInterval);
	default:
		return 0.0f;
	}
}
