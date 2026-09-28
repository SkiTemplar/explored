#include "WorldGen/TerrainToolModel.h"

namespace TerrainToolModelDetail
{
	/**
	 * Profundidades (m desde la superficie sin editar) de `mining.json/strata` por isla.
	 * SandDepth: hasta dónde llega la arena bajo una superficie de playa. RockDepth: dónde
	 * empieza la roca de la isla. DeepDepth: dónde la caliza de La Meseta da paso al basalto.
	 */
	struct FStrataColumn
	{
		float SandDepth = 0.0f;
		float RockDepth = 4.0f;
		ETerrainMaterial Rock = ETerrainMaterial::Basalto;
		float DeepDepth = TNumericLimits<float>::Max();
		ETerrainMaterial DeepRock = ETerrainMaterial::Basalto;
	};

	FStrataColumn StrataFor(bool bHasIsland, EIslandArchetype Archetype)
	{
		FStrataColumn S;
		if (!bHasIsland)
		{
			// Fondo marino y cayos sin isla: arena sobre basalto, como la playa de Landing.
			S.SandDepth = 3.0f;
			return S;
		}
		switch (Archetype)
		{
		case EIslandArchetype::Landing: S.SandDepth = 3.0f; S.RockDepth = 4.0f; break;
		case EIslandArchetype::Emerald: S.SandDepth = 1.5f; S.RockDepth = 2.0f; break;
		case EIslandArchetype::Smoke: S.SandDepth = 0.0f; S.RockDepth = 0.5f; break;
		case EIslandArchetype::Teeth: S.SandDepth = 1.0f; S.RockDepth = 0.5f; break;
		case EIslandArchetype::Mangrove: S.SandDepth = 0.0f; S.RockDepth = 3.0f; break;
		case EIslandArchetype::WhiteSands: S.SandDepth = 4.0f; S.RockDepth = 5.0f; break;
		case EIslandArchetype::Mesa:
			S.SandDepth = 0.0f;
			S.RockDepth = 1.0f;
			S.Rock = ETerrainMaterial::Caliza;
			S.DeepDepth = 40.0f;
			S.DeepRock = ETerrainMaterial::Basalto;
			break;
		default: break;
		}
		return S;
	}

	bool IsFinite(const FVector& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
	}
}

bool FTerrainToolModel::ToolFromItem(const FName& ItemId, ETerrainDigTool& OutTool)
{
	struct FEntry
	{
		const TCHAR* Id;
		ETerrainDigTool Tool;
	};
	// «pico» y «pala» son las plantillas de items.json; hasta que el objeto lleve su cabeza,
	// un pico genérico es de piedra. Los ids de mining.json/tools van tal cual.
	static const FEntry Entries[] = {
		{TEXT("pala"), ETerrainDigTool::PalaTosca},
		{TEXT("pala_tosca"), ETerrainDigTool::PalaTosca},
		{TEXT("pico"), ETerrainDigTool::PicoPiedra},
		{TEXT("pico_piedra"), ETerrainDigTool::PicoPiedra},
		{TEXT("pico_tallado"), ETerrainDigTool::PicoTallado},
		{TEXT("pico_obsidiana"), ETerrainDigTool::PicoObsidiana},
		{TEXT("pico_rescatado"), ETerrainDigTool::PicoRescatado},
	};
	for (const FEntry& Entry : Entries)
	{
		if (ItemId == FName(Entry.Id))
		{
			OutTool = Entry.Tool;
			return true;
		}
	}
	return false;
}

ETerrainToolAction FTerrainToolModel::ActionFor(ETerrainDigTool Tool, bool bSecondary)
{
	if (!IsShovel(Tool))
	{
		return ETerrainToolAction::Pick;
	}
	return bSecondary ? ETerrainToolAction::PlaceSoil : ETerrainToolAction::ShovelFlatten;
}

ETerrainMaterial FTerrainToolModel::ClassifyMaterial(const FTerrainStrataQuery& Query)
{
	using namespace TerrainToolModelDetail;
	const FStrataColumn S = StrataFor(Query.bHasIsland, Query.Archetype);
	const float RawDepth = Query.ColumnHeight - Query.Z;
	if (!FMath::IsFinite(RawDepth))
	{
		return ETerrainMaterial::Tierra;
	}
	const float Depth = FMath::Max(0.0f, RawDepth);
	if (Depth >= S.DeepDepth)
	{
		return S.DeepRock;
	}
	if (Depth >= S.RockDepth)
	{
		return S.Rock;
	}
	if (Depth < ExposedRockDepth && Query.RockWeight >= ExposedRockWeight)
	{
		return S.Rock;
	}
	if (Query.SandWeight >= SandySurfaceWeight && Depth < S.SandDepth)
	{
		return ETerrainMaterial::Arena;
	}
	return ETerrainMaterial::Tierra;
}

float FTerrainToolModel::SecondsPerUse(ETerrainToolAction Action, ETerrainDigTool Tool)
{
	if (Action == ETerrainToolAction::Pick)
	{
		return FTerrainEdits::ToolInfo(Tool).SecondsPerHit;
	}
	return FTerrainEditModel::SecondsPerShovelStroke;
}

ETerrainToolVerdict FTerrainToolModel::Validate(const FTerrainToolRequest& Request)
{
	using namespace TerrainToolModelDetail;
	const bool bKnown = Request.Action < ETerrainToolAction::Count && Request.Tool < ETerrainDigTool::Count;
	if (!bKnown || !IsFinite(Request.ImpactPoint) || !IsFinite(Request.EyeLocation)
		|| !FMath::IsFinite(Request.SecondsSinceLastUse) || !FMath::IsFinite(Request.DensityAtImpact))
	{
		return ETerrainToolVerdict::Invalid;
	}
	const double Reach = static_cast<double>(ReachMeters + ServerReachTolerance);
	if (FVector::DistSquared(Request.ImpactPoint, Request.EyeLocation) > Reach * Reach)
	{
		return ETerrainToolVerdict::TooFar;
	}
	const float MinSeconds = SecondsPerUse(Request.Action, Request.Tool) * (1.0f - FTerrainEdits::CadenceTolerance);
	if (Request.SecondsSinceLastUse < MinSeconds)
	{
		return ETerrainToolVerdict::TooSoon;
	}
	if (FMath::Abs(Request.DensityAtImpact) > MaxSurfaceDistance)
	{
		return ETerrainToolVerdict::NotSurface;
	}
	return ETerrainToolVerdict::Accepted;
}

FShovelStroke FTerrainToolModel::MakeShovelStroke(const FVector& ImpactPoint, double FeetZ, ETerrainMaterial Material,
	int32 ToolTier, double CarriedSoil)
{
	FShovelStroke Stroke;
	Stroke.Center = FVector(ImpactPoint.X, ImpactPoint.Y, FeetZ);
	Stroke.PlaneNormal = FVector(0.0, 0.0, 1.0);
	Stroke.Radius = ShovelRadius;
	Stroke.EdgeWidth = ShovelEdge;
	Stroke.Material = Material;
	Stroke.ToolTier = ToolTier;
	Stroke.SoilBudget = FMath::IsFinite(CarriedSoil) ? FMath::Clamp(CarriedSoil, 0.0, MaxCarriedSoil) : 0.0;
	Stroke.bMarkPath = true;
	return Stroke;
}

FSoilPlacement FTerrainToolModel::MakeSoilPlacement(const FVector& ImpactPoint, double CarriedSoil)
{
	FSoilPlacement Placement;
	Placement.Center = ImpactPoint;
	Placement.Radius = SoilRadius;
	Placement.SoilBudget = FMath::IsFinite(CarriedSoil) ? FMath::Clamp(CarriedSoil, 0.0, MaxCarriedSoil) : 0.0;
	return Placement;
}

double FTerrainToolModel::UpdateCarriedSoil(double Carried, ETerrainMaterial Material, double VolumeRemoved, double VolumeAdded)
{
	if (!FMath::IsFinite(Carried) || !FMath::IsFinite(VolumeRemoved) || !FMath::IsFinite(VolumeAdded))
	{
		return FMath::IsFinite(Carried) ? FMath::Clamp(Carried, 0.0, MaxCarriedSoil) : 0.0;
	}
	const double Gained = IsLooseSoil(Material) ? FMath::Max(0.0, VolumeRemoved) : 0.0;
	return FMath::Clamp(Carried + Gained - FMath::Max(0.0, VolumeAdded), 0.0, MaxCarriedSoil);
}

EMineHitCue FTerrainToolModel::PredictCue(ETerrainToolAction Action, ETerrainDigTool Tool, ETerrainMaterial Material,
	double CarriedSoil)
{
	if (Action == ETerrainToolAction::PlaceSoil)
	{
		return CarriedSoil > 0.0 ? EMineHitCue::Hit : EMineHitCue::Miss;
	}
	if (Action == ETerrainToolAction::ShovelFlatten && !IsLooseSoil(Material))
	{
		return EMineHitCue::Rebound;
	}
	const int32 Tier = FTerrainEdits::ToolInfo(Tool).ToolTier;
	return FTerrainEditModel::ToolFactor(Material, Tier) > 0.0f ? EMineHitCue::Hit : EMineHitCue::Rebound;
}

EMineHitCue FTerrainToolModel::CueFromResult(const FTerrainEditResult& Result)
{
	if (Result.bRejected)
	{
		return EMineHitCue::Rebound;
	}
	return Result.Changed() ? EMineHitCue::Hit : EMineHitCue::Miss;
}
