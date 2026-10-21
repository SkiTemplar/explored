#include "WorldGen/IslandLayout.h"

namespace IslandLayoutDetail
{
	/** Fracción de la distancia normalizada donde empieza a bajar la meseta hacia la playa. */
	constexpr float PlateauEdge = 0.55f;
	/** Una zona contiene el punto si está a menos de su radio por este factor (holgura de borde). */
	constexpr float ZoneContainSlack = 1.0f;

	FVector2D ToLocal(const FIslandDesc& Island, const FVector2D& Point)
	{
		const FVector2D Delta = Point - Island.Center;
		const float C = FMath::Cos(Island.Rotation);
		const float S = FMath::Sin(Island.Rotation);
		return FVector2D(Delta.X * C + Delta.Y * S, -Delta.X * S + Delta.Y * C);
	}

	float MinorRadius(const FIslandDesc& Island)
	{
		return Island.Radius * FMath::Clamp(Island.Aspect, KINDA_SMALL_NUMBER, 1.0f);
	}

	float LandHeight(const FIslandDesc& Island, float NormalizedDistance)
	{
		if (NormalizedDistance >= 1.0f)
		{
			// Plataforma: baja con pendiente fija desde la costa hasta el fondo oceánico.
			const float Beyond = (NormalizedDistance - 1.0f) * MinorRadius(Island);
			return FMath::Max(FIslandLayout::OceanFloor, -Beyond * FIslandLayout::ShelfSlope);
		}
		const float Rise = 1.0f - FMath::SmoothStep(PlateauEdge, 1.0f, NormalizedDistance);
		return Island.MaxHeight * Rise;
	}

	bool IsValidName(FName Id)
	{
		return !Id.IsNone();
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

bool IslandArchetypeFromName(const FString& Name, EIslandArchetype& OutArchetype)
{
	for (int32 I = 0; I < static_cast<int32>(EIslandArchetype::Count); ++I)
	{
		const EIslandArchetype Candidate = static_cast<EIslandArchetype>(I);
		if (Name.Equals(LexToString(Candidate), ESearchCase::IgnoreCase))
		{
			OutArchetype = Candidate;
			return true;
		}
	}
	return false;
}

float FIslandDesc::NormalizedDistance(const FVector2D& Point) const
{
	const FVector2D Local = IslandLayoutDetail::ToLocal(*this, Point);
	const float A = FMath::Max(Radius, KINDA_SMALL_NUMBER);
	const float B = IslandLayoutDetail::MinorRadius(*this);
	return static_cast<float>(FMath::Sqrt(FMath::Square(Local.X / A) + FMath::Square(Local.Y / B)));
}

float FIslandDesc::RadiusAlong(const FVector2D& Direction) const
{
	const FVector2D Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return Radius;
	}
	const float Normalized = NormalizedDistance(Center + Dir);
	return Normalized > KINDA_SMALL_NUMBER ? 1.0f / Normalized : Radius;
}

float FIslandDesc::DistanceToCoast(const FVector2D& Point) const
{
	const FVector2D Delta = Point - Center;
	const float Dist = static_cast<float>(Delta.Size());
	return FMath::Max(0.0f, Dist - RadiusAlong(Delta));
}

FIslandLayout FIslandLayout::Provisional()
{
	FIslandLayout Layout;
	Layout.Seed = OfficialSeed;

	// Isla principal: 330 m de este a oeste por 260 m de norte (+X) a sur, centrada en el origen.
	FIslandDesc Main;
	Main.Id = TEXT("isla_sal");
	Main.Archetype = EIslandArchetype::Landing;
	Main.Center = FVector2D::ZeroVector;
	Main.Radius = 165.0f;
	Main.Aspect = 130.0f / 165.0f;
	Main.Rotation = UE_HALF_PI;
	Main.MaxHeight = 4.0f;
	Layout.Islands.Add(Main);

	// Islote del carguero: unos 150 m al nordeste de Playa Norte.
	FIslandDesc Islet;
	Islet.Id = TEXT("islote_carguero");
	Islet.Archetype = EIslandArchetype::Teeth;
	Islet.Center = FVector2D(232.0f, 150.0f);
	Islet.Radius = 35.0f;
	Islet.Aspect = 25.0f / 35.0f;
	Islet.Rotation = UE_PI * 0.25f;
	Islet.MaxHeight = 6.0f;
	Layout.Islands.Add(Islet);

	auto AddZone = [&Layout](const TCHAR* Id, const TCHAR* Name, EIslandArchetype Biome, FVector2D Center, float Radius, float Hill)
	{
		FIslandZone Zone;
		Zone.Id = Id;
		Zone.Name = Name;
		Zone.Biome = Biome;
		Zone.Center = Center;
		Zone.Radius = Radius;
		Zone.Hill = Hill;
		Layout.Zones.Add(Zone);
	};
	AddZone(TEXT("cala_varada"), TEXT("Cala Varada"), EIslandArchetype::Landing, FVector2D(-100.0f, 10.0f), 35.0f, 0.0f);
	AddZone(TEXT("palmar"), TEXT("Palmar"), EIslandArchetype::Landing, FVector2D(-30.0f, -5.0f), 48.0f, 0.0f);
	AddZone(TEXT("rocas_pescador"), TEXT("Rocas del Pescador"), EIslandArchetype::Teeth, FVector2D(-15.0f, -128.0f), 30.0f, 2.0f);
	AddZone(TEXT("bosque_bambu"), TEXT("Bosque de Bambú"), EIslandArchetype::Emerald, FVector2D(55.0f, -85.0f), 42.0f, 5.0f);
	AddZone(TEXT("playa_norte"), TEXT("Playa Norte"), EIslandArchetype::WhiteSands, FVector2D(110.0f, 55.0f), 28.0f, 0.0f);
	AddZone(TEXT("colina_faro"), TEXT("Colina del Faro"), EIslandArchetype::Mesa, FVector2D(45.0f, 15.0f), 47.0f, 31.0f);
	AddZone(TEXT("isla_carguero"), TEXT("Isla del Carguero"), EIslandArchetype::Teeth, FVector2D(232.0f, 150.0f), 32.0f, 0.0f);

	auto AddPoint = [&Layout](const TCHAR* Id, FVector2D Location, float Yaw)
	{
		FIslandPoint Point;
		Point.Id = Id;
		Point.Location = Location;
		Point.Yaw = Yaw;
		Layout.Points.Add(Point);
	};
	AddPoint(TEXT("player_start"), FVector2D(-112.0f, 10.0f), 0.0f);
	AddPoint(TEXT("raft_site"), FVector2D(120.0f, 62.0f), 40.0f);
	AddPoint(TEXT("lighthouse"), FVector2D(45.0f, 15.0f), 0.0f);
	return Layout;
}

const FIslandDesc* FIslandLayout::FindIsland(EIslandArchetype Archetype) const
{
	return Islands.FindByPredicate([Archetype](const FIslandDesc& I) { return I.Archetype == Archetype; });
}

const FIslandDesc* FIslandLayout::FindIslandById(FName Id) const
{
	return Islands.FindByPredicate([Id](const FIslandDesc& I) { return I.Id == Id; });
}

const FIslandZone* FIslandLayout::FindZone(FName Id) const
{
	return Zones.FindByPredicate([Id](const FIslandZone& Z) { return Z.Id == Id; });
}

const FIslandPoint* FIslandLayout::FindPoint(FName Id) const
{
	return Points.FindByPredicate([Id](const FIslandPoint& P) { return P.Id == Id; });
}

int32 FIslandLayout::FindZoneAt(const FVector2D& Point) const
{
	int32 Best = INDEX_NONE;
	double BestDist = 0.0;
	for (int32 I = 0; I < Zones.Num(); ++I)
	{
		const double Dist = FVector2D::Distance(Point, Zones[I].Center);
		if (Dist > Zones[I].Radius * IslandLayoutDetail::ZoneContainSlack)
		{
			continue;
		}
		if (Best == INDEX_NONE || Dist < BestDist)
		{
			Best = I;
			BestDist = Dist;
		}
	}
	return Best;
}

EIslandArchetype FIslandLayout::BiomeAt(const FIslandColumn& Column) const
{
	if (Zones.IsValidIndex(Column.ZoneIndex))
	{
		return Zones[Column.ZoneIndex].Biome;
	}
	return Islands.IsValidIndex(Column.IslandIndex) ? Islands[Column.IslandIndex].Archetype : EIslandArchetype::Count;
}

FIslandColumn FIslandLayout::SampleColumn(float X, float Y) const
{
	using namespace IslandLayoutDetail;
	const FVector2D Point(X, Y);
	FIslandColumn Column;
	Column.Height = OceanFloor;
	int32 Dominant = INDEX_NONE;
	for (int32 I = 0; I < Islands.Num(); ++I)
	{
		const float Normalized = Islands[I].NormalizedDistance(Point);
		Column.Height = FMath::Max(Column.Height, LandHeight(Islands[I], Normalized));
		if (Dominant == INDEX_NONE || Normalized < Column.NormalizedDistance)
		{
			Dominant = I;
			Column.NormalizedDistance = Normalized;
		}
	}
	Column.IslandIndex = Column.NormalizedDistance < OpenSeaNormalizedDistance ? Dominant : INDEX_NONE;

	if (Column.Height > SeaLevel)
	{
		// Las colinas solo levantan tierra firme: no crean islotes en mitad del mar.
		const float LandFactor = FMath::Clamp(Column.Height / FMath::Max(Islands[Dominant].MaxHeight, 1.0f), 0.0f, 1.0f);
		for (const FIslandZone& Zone : Zones)
		{
			if (Zone.Hill <= 0.0f)
			{
				continue;
			}
			const float Sigma = FMath::Max(Zone.Radius * 0.5f, 1.0f);
			const float DistSq = static_cast<float>(FVector2D::DistSquared(Point, Zone.Center));
			Column.Height += Zone.Hill * LandFactor * FMath::Exp(-DistSq / (2.0f * Sigma * Sigma));
		}
	}
	Column.ZoneIndex = FindZoneAt(Point);
	return Column;
}

bool FIslandLayout::Validate(FString& OutError) const
{
	using namespace IslandLayoutDetail;
	if (Islands.IsEmpty())
	{
		OutError = TEXT("no hay ninguna tierra");
		return false;
	}
	TSet<FName> Ids;
	for (const FIslandDesc& Island : Islands)
	{
		if (!IsValidName(Island.Id) || Ids.Contains(Island.Id))
		{
			OutError = FString::Printf(TEXT("id de tierra vacío o repetido: %s"), *Island.Id.ToString());
			return false;
		}
		if (Island.Radius <= 0.0f || Island.Aspect <= 0.0f || Island.Aspect > 1.0f || Island.MaxHeight <= 0.0f)
		{
			OutError = FString::Printf(TEXT("tierra %s con radio, proporción o altura fuera de rango"), *Island.Id.ToString());
			return false;
		}
		Ids.Add(Island.Id);
	}
	for (const FIslandZone& Zone : Zones)
	{
		if (!IsValidName(Zone.Id) || Ids.Contains(Zone.Id) || Zone.Radius <= 0.0f || Zone.Hill < 0.0f)
		{
			OutError = FString::Printf(TEXT("zona inválida o con id repetido: %s"), *Zone.Id.ToString());
			return false;
		}
		if (SampleColumn(Zone.Center.X, Zone.Center.Y).Height <= SeaLevel)
		{
			OutError = FString::Printf(TEXT("el centro de la zona %s queda en el agua"), *Zone.Id.ToString());
			return false;
		}
		Ids.Add(Zone.Id);
	}
	for (const FIslandPoint& Point : Points)
	{
		if (!IsValidName(Point.Id) || Ids.Contains(Point.Id))
		{
			OutError = FString::Printf(TEXT("punto con id vacío o repetido: %s"), *Point.Id.ToString());
			return false;
		}
		Ids.Add(Point.Id);
	}
	return true;
}
