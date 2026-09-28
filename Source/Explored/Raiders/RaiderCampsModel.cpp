#include "Raiders/RaiderCampsModel.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/TerrainDensity.h"

namespace RaiderCampsDetail
{
	/** Sal de cada campamento en la semilla: cambiar el orden del enum no mueve los campamentos. */
	uint32 CampSalt(ERaiderCampId Camp)
	{
		return Camp == ERaiderCampId::BrokenCove ? 0xC0A1A207u : 0xF0DE0ADEu;
	}

	/** Altura mínima para considerar tierra firme un objeto del campamento (m). */
	constexpr float MinPropHeight = 0.5f;

	float Height(const FTerrainDensity& Density, const FVector2D& P)
	{
		return Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y)).Height;
	}

	/** Punto en tierra hacia dentro de la isla; prueba distancias cada vez más cortas y, si nada vale, el centro. */
	FVector PropOnLand(const FTerrainDensity& Density, const FVector& Center, const FVector2D& Inward, const FVector2D& Side,
		float Along, float Across)
	{
		for (const float Scale : { 1.0f, 0.6f, 0.3f })
		{
			const FVector2D P = FVector2D(Center.X, Center.Y) + Inward * (Along * Scale) + Side * (Across * Scale);
			const float H = Height(Density, P);
			if (H >= MinPropHeight)
			{
				return FVector(P.X, P.Y, H);
			}
		}
		return Center;
	}

	/** Agua para el barco en una dirección desde la playa: distancia o −1 si no hay a tiro. */
	float BoatSpotAlong(const FTerrainDensity& Density, const FVector& Beach, const FVector2D& Dir, EPirateBoatType Boat, FVector& Out)
	{
		for (float D = 2.0f; D <= FRaiderCampsModel::MaxBoatDistanceM; D += 2.0f)
		{
			const FVector2D P = FVector2D(Beach.X, Beach.Y) + Dir * D;
			const float H = Height(Density, P);
			if (Boat == EPirateBoatType::RaidingCanoe && H < MinPropHeight)
			{
				// Varada en la línea de agua: nunca por debajo del nivel del mar.
				Out = FVector(P.X, P.Y, FMath::Max(H, FArchipelagoLayout::SeaLevel));
				return D;
			}
			if (Boat == EPirateBoatType::BlackSloop && H <= FArchipelagoLayout::SeaLevel - FRaiderCampsModel::SloopDraftM)
			{
				Out = FVector(P.X, P.Y, FArchipelagoLayout::SeaLevel);
				return D;
			}
		}
		return -1.0f;
	}

	/** El agua más cercana para el barco en 16 rumbos; en empate gana el primer rumbo. */
	bool FindNearestWater(const FTerrainDensity& Density, const FVector& Beach, EPirateBoatType Boat, FVector& OutSpot, FVector2D& OutDir)
	{
		float Best = -1.0f;
		for (int32 I = 0; I < 16; ++I)
		{
			const float A = I * (2.0f * UE_PI / 16.0f);
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			FVector Spot;
			const float D = BoatSpotAlong(Density, Beach, Dir, Boat, Spot);
			if (D > 0.0f && (Best < 0.0f || D < Best))
			{
				Best = D;
				OutSpot = Spot;
				OutDir = Dir;
			}
		}
		return Best > 0.0f;
	}

	bool IsClear(const FVector& P, const TArray<FVector>& Avoid)
	{
		const float MinSq = FMath::Square(FRaiderCampsModel::MinClearanceM);
		for (const FVector& A : Avoid)
		{
			if (FVector2D::DistSquared(FVector2D(P.X, P.Y), FVector2D(A.X, A.Y)) < MinSq)
			{
				return false;
			}
		}
		return true;
	}
}

const TCHAR* LexToString(ERaiderCampId Camp)
{
	switch (Camp)
	{
	case ERaiderCampId::BrokenCove: return TEXT("cala_rota");
	case ERaiderCampId::RottenAnchorage: return TEXT("fondeadero_podrido");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EPirateBoatType Boat)
{
	switch (Boat)
	{
	case EPirateBoatType::RaidingCanoe: return TEXT("piragua_asalto");
	case EPirateBoatType::BlackSloop: return TEXT("balandra_negra");
	default: return TEXT("unknown");
	}
}

bool FRaiderCamp::operator==(const FRaiderCamp& Other) const
{
	return Id == Other.Id && Island == Other.Island && IslandIndex == Other.IslandIndex && Location == Other.Location &&
		Yaw == Other.Yaw && TentLocation == Other.TentLocation && FireLocation == Other.FireLocation &&
		ChestLocation == Other.ChestLocation && Boat == Other.Boat && BoatLocation == Other.BoatLocation &&
		Loot == Other.Loot && bOnBeach == Other.bOnBeach;
}

EIslandArchetype FRaiderCampsModel::IslandOf(ERaiderCampId Camp)
{
	return Camp == ERaiderCampId::RottenAnchorage ? EIslandArchetype::Mangrove : EIslandArchetype::Teeth;
}

EPirateBoatType FRaiderCampsModel::BoatOf(ERaiderCampId Camp)
{
	return Camp == ERaiderCampId::RottenAnchorage ? EPirateBoatType::BlackSloop : EPirateBoatType::RaidingCanoe;
}

void FRaiderCampsModel::CrewRange(EPirateBoatType Boat, int32& OutMin, int32& OutMax)
{
	if (Boat == EPirateBoatType::BlackSloop)
	{
		OutMin = 5;
		OutMax = 6;
	}
	else
	{
		OutMin = 2;
		OutMax = 3;
	}
}

TArray<FRaiderLoot> FRaiderCampsModel::RollLoot(uint32 WorldSeed, ERaiderCampId Camp)
{
	// Ids que ya existen en items.json. La brea, el alcohol destilado y la vela de lona de la
	// tabla de biblia 05 §2.6 aún no son objetos: entran cuando existan (DataCheck lo vigila
	// con el resto del borrador de fase 3).
	FExploredRandom Rng(ExploredHash::Hash32(WorldSeed ^ RaiderCampsDetail::CampSalt(Camp)), 0x4C4F4F54ULL);  // «LOOT»
	TArray<FRaiderLoot> Loot;
	auto Add = [&Loot](const TCHAR* Id, int32 Count)
	{
		if (Count > 0)
		{
			Loot.Add({ FName(Id), Count });
		}
	};
	Add(TEXT("chapa_fuselaje"), Rng.RangeInt(1, 3));
	Add(TEXT("tubo_aluminio"), Rng.RangeInt(0, 2));
	Add(TEXT("cable_electrico"), Rng.RangeInt(1, 2));
	Add(TEXT("cuerda"), Rng.RangeInt(2, 4));
	// La medicina solo está en el cofre de campamento (no en el del barco): siempre aquí, a veces.
	Add(TEXT("pasta_medicinal"), Rng.Chance(0.5f) ? 1 : 0);
	return Loot;
}

bool FRaiderCampsModel::Place(const FTerrainDensity& Density, ERaiderCampId Camp, const TArray<FVector>& Avoid, FRaiderCamp& Out)
{
	using namespace RaiderCampsDetail;
	const FArchipelagoLayout& Layout = Density.GetLayout();
	const EIslandArchetype Archetype = IslandOf(Camp);
	const int32 IslandIndex = Layout.Islands.IndexOfByPredicate([Archetype](const FIslandDesc& I) { return I.Archetype == Archetype; });
	if (IslandIndex == INDEX_NONE)
	{
		return false;
	}
	const FIslandDesc& Island = Layout.Islands[IslandIndex];

	Out = FRaiderCamp();
	Out.Id = Camp;
	Out.Island = Archetype;
	Out.IslandIndex = IslandIndex;
	Out.Boat = BoatOf(Camp);
	Out.Loot = RollLoot(Layout.Seed, Camp);

	// Playas candidatas: celdas de la rejilla con altura de playa que pertenecen a la isla. Se
	// recorre toda la isla y no solo rayos desde el centro porque Los Dientes son islotes y
	// su centro cae en el agua. El orden de prueba lo fija un hash de la semilla por celda:
	// el mismo mundo da siempre el mismo campamento, y otro mundo, otro sitio.
	const uint32 CampSeed = ExploredHash::Hash32(Layout.Seed ^ CampSalt(Camp));
	const float Reach = Island.Radius * SearchRadiusFactor;
	const int32 Cells = FMath::Max(1, FMath::CeilToInt(Reach / SearchCellM));
	struct FCandidate
	{
		FVector P;
		/** Franja de altura: 0 playa, 1 y 2 cornisas cada vez más altas. */
		int32 Band;
		uint32 Order;
		int32 Cell;
	};
	TArray<FCandidate> Candidates;
	for (int32 IY = -Cells; IY <= Cells; ++IY)
	{
		for (int32 IX = -Cells; IX <= Cells; ++IX)
		{
			const FVector2D Offset(IX * SearchCellM, IY * SearchCellM);
			if (Offset.SizeSquared() > FMath::Square(Reach))
			{
				continue;
			}
			const FVector2D P = Island.Center + Offset;
			const FTerrainColumn Column = Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y));
			if (Column.IslandIndex != IslandIndex || Column.Height <= BeachMinHeightM || Column.Height >= LedgeMaxHeightM)
			{
				continue;
			}
			const int32 Band = Column.Height < BeachMaxHeightM ? 0 : (Column.Height < LedgeMidHeightM ? 1 : 2);
			Candidates.Add({ FVector(P.X, P.Y, Column.Height), Band, ExploredHash::Hash2D(CampSeed, IX, IY),
				(IY + Cells) * (2 * Cells + 1) + (IX + Cells) });
		}
	}
	// Primero las playas; si la isla no tiene (Los Dientes pasan del agua al acantilado), la
	// cornisa más baja. Dentro de cada franja, el orden de la semilla.
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.Band != B.Band)
		{
			return A.Band < B.Band;
		}
		return A.Order != B.Order ? A.Order < B.Order : A.Cell < B.Cell;
	});

	// Primera pasada: playa con agua para el barco y lejos de los puntos de interés; segunda:
	// con agua aunque esté cerca; tercera: cualquier playa. Como mucho MaxAttempts búsquedas de
	// barco por pasada, que son lo caro.
	bool bFound = false;
	FVector2D Outward(1.0, 0.0);
	FVector Beach = FVector::ZeroVector;
	FVector BoatSpot = FVector::ZeroVector;
	for (int32 Pass = 0; Pass < 3 && !bFound; ++Pass)
	{
		int32 Tried = 0;
		for (const FCandidate& C : Candidates)
		{
			if (Pass == 0 && !IsClear(C.P, Avoid))
			{
				continue;
			}
			// En una cornisa, solo donde se puede plantar una tienda.
			if (C.Band > 0 && Density.Normal(FVector(C.P.X, C.P.Y, C.P.Z), 1.0f).Z < MinLedgeFlatness)
			{
				continue;
			}
			FVector Spot;
			FVector2D Dir;
			const bool bBoat = FindNearestWater(Density, C.P, Out.Boat, Spot, Dir);
			if (Pass < 2 && !bBoat)
			{
				if (++Tried >= MaxAttempts)
				{
					break;
				}
				continue;
			}
			bFound = true;
			Beach = C.P;
			Outward = bBoat ? Dir : (FVector2D(C.P.X, C.P.Y) - Island.Center).GetSafeNormal();
			if (Outward.IsNearlyZero())
			{
				Outward = FVector2D(1.0, 0.0);
			}
			BoatSpot = bBoat ? Spot : FVector(C.P.X + Outward.X * 6.0, C.P.Y + Outward.Y * 6.0, FArchipelagoLayout::SeaLevel);
			break;
		}
	}

	if (!bFound)
	{
		// Sin playa (isla toda acantilado): el borde nominal de la isla en un ángulo de la
		// semilla. No pasa con la semilla oficial, pero un campamento fijo tiene que existir.
		const float Angle = ExploredHash::ToUnitFloat(CampSeed) * 2.0f * UE_PI;
		Outward = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
		const FVector2D P = Island.Center + Outward * (Island.Radius * 0.9f);
		Beach = FVector(P.X, P.Y, FMath::Max(Height(Density, P), FArchipelagoLayout::SeaLevel));
		BoatSpot = FVector(P.X + Outward.X * 10.0, P.Y + Outward.Y * 10.0, FArchipelagoLayout::SeaLevel);
	}
	Out.bOnBeach = bFound;
	Out.Location = Beach;
	Out.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Outward.Y, Outward.X));
	Out.BoatLocation = BoatSpot;

	const FVector2D Inward = -Outward;
	const FVector2D Side(-Outward.Y, Outward.X);
	Out.FireLocation = PropOnLand(Density, Beach, Inward, Side, 6.0f, 0.0f);
	Out.TentLocation = PropOnLand(Density, Beach, Inward, Side, 10.0f, 4.0f);
	Out.ChestLocation = PropOnLand(Density, Beach, Inward, Side, 10.0f, -3.0f);
	return true;
}

TArray<FRaiderCamp> FRaiderCampsModel::Generate(const FTerrainDensity& Density, const TArray<FVector>& Avoid)
{
	TArray<FRaiderCamp> Camps;
	for (int32 I = 0; I < static_cast<int32>(ERaiderCampId::Count); ++I)
	{
		FRaiderCamp Camp;
		if (Place(Density, static_cast<ERaiderCampId>(I), Avoid, Camp))
		{
			Camps.Add(MoveTemp(Camp));
		}
	}
	return Camps;
}
