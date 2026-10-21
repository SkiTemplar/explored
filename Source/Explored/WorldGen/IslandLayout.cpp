#include "WorldGen/IslandLayout.h"

namespace IslandLayoutDetail
{
	/** Fracción de la distancia normalizada donde empieza a bajar la meseta hacia la playa. */
	constexpr float PlateauEdge = 0.55f;
	/** Una zona contiene el punto si está a menos de su radio por este factor (holgura de borde). */
	constexpr float ZoneContainSlack = 1.0f;
	/** Tope inferior de la escala armónica de la costa (evita divisiones por casi cero). */
	constexpr float MinCoastScale = 0.2f;

	FCoastHarmonic Harmonic(int32 K, float Amplitude, float PhaseDeg)
	{
		FCoastHarmonic Out;
		Out.K = K;
		Out.Amplitude = Amplitude;
		Out.Phase = FMath::DegreesToRadians(PhaseDeg);
		return Out;
	}

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
	const float UX = static_cast<float>(Local.X) / A;
	const float UY = static_cast<float>(Local.Y) / B;
	const float Ellipse = FMath::Sqrt(UX * UX + UY * UY);
	if (Coast.IsEmpty())
	{
		return Ellipse;
	}
	const float Theta = FMath::Atan2(UY, UX);
	float Scale = 1.0f;
	for (const FCoastHarmonic& Harmonic : Coast)
	{
		Scale += Harmonic.Amplitude * FMath::Cos(static_cast<float>(Harmonic.K) * Theta + Harmonic.Phase);
	}
	return Ellipse / FMath::Max(Scale, IslandLayoutDetail::MinCoastScale);
}

float FIslandRock::HeightAt(const FVector2D& Point) const
{
	const float Dist = static_cast<float>(FVector2D::Distance(Point, Center));
	return Height * (1.0f - FMath::SmoothStep(0.0f, FMath::Max(Radius, KINDA_SMALL_NUMBER), Dist));
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
	Main.Coast = { IslandLayoutDetail::Harmonic(2, 0.03f, 20.0f), IslandLayoutDetail::Harmonic(3, 0.05f, 70.0f), IslandLayoutDetail::Harmonic(5, 0.035f, 200.0f), IslandLayoutDetail::Harmonic(7, 0.02f, 130.0f) };
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
	Islet.Coast = { IslandLayoutDetail::Harmonic(3, 0.07f, 40.0f), IslandLayoutDetail::Harmonic(5, 0.04f, 160.0f) };
	Layout.Islands.Add(Islet);

	auto AddZone = [&Layout](const TCHAR* Id, const TCHAR* Name, EIslandArchetype Biome, FVector2D Center, float Radius, float Hill,
		float HillSigma = 0.0f)
	{
		FIslandZone Zone;
		Zone.Id = Id;
		Zone.Name = Name;
		Zone.Biome = Biome;
		Zone.Center = Center;
		Zone.Radius = Radius;
		Zone.Hill = Hill;
		Zone.HillSigma = HillSigma;
		Layout.Zones.Add(Zone);
	};
	AddZone(TEXT("cala_varada"), TEXT("Cala Varada"), EIslandArchetype::Landing, FVector2D(-100.0f, 10.0f), 35.0f, 0.0f);
	AddZone(TEXT("palmar"), TEXT("Palmar"), EIslandArchetype::Landing, FVector2D(-30.0f, -5.0f), 48.0f, 0.0f);
	AddZone(TEXT("rocas_pescador"), TEXT("Rocas del Pescador"), EIslandArchetype::Teeth, FVector2D(-15.0f, -140.0f), 30.0f, 2.0f);
	AddZone(TEXT("bosque_bambu"), TEXT("Bosque de Bambú"), EIslandArchetype::Emerald, FVector2D(55.0f, -85.0f), 42.0f, 5.0f);
	AddZone(TEXT("playa_norte"), TEXT("Playa Norte"), EIslandArchetype::WhiteSands, FVector2D(110.0f, 55.0f), 28.0f, 0.0f);
	AddZone(TEXT("colina_faro"), TEXT("Colina del Faro"), EIslandArchetype::Mesa, FVector2D(45.0f, 15.0f), 47.0f, 32.5f, 42.0f);
	AddZone(TEXT("isla_carguero"), TEXT("Isla del Carguero"), EIslandArchetype::Teeth, FVector2D(232.0f, 150.0f), 32.0f, 0.0f);

	// Rocas, repisas de pesca, farallones y la hondonada de la Cala Varada (island.json/rocks).
	auto AddRock = [&Layout](const TCHAR* Id, FVector2D Center, float Radius, float Height)
	{
		FIslandRock Rock;
		Rock.Id = Id;
		Rock.Center = Center;
		Rock.Radius = Radius;
		Rock.Height = Height;
		Layout.Rocks.Add(Rock);
	};
	AddRock(TEXT("roca_pescador_grande"), FVector2D(-20.0f, -140.0f), 9.0f, 5.0f);
	AddRock(TEXT("roca_pescador_sur"), FVector2D(-34.0f, -150.0f), 7.0f, 3.5f);
	AddRock(TEXT("roca_pescador_centro"), FVector2D(-10.0f, -153.0f), 7.0f, 3.2f);
	AddRock(TEXT("roca_pescador_norte"), FVector2D(8.0f, -149.0f), 6.0f, 3.0f);
	AddRock(TEXT("repisa_pescador_sur"), FVector2D(-30.0f, -162.0f), 6.0f, 1.6f);
	AddRock(TEXT("repisa_pescador_centro"), FVector2D(-14.0f, -163.0f), 6.0f, 1.5f);
	AddRock(TEXT("repisa_pescador_norte"), FVector2D(2.0f, -161.0f), 6.0f, 1.5f);
	AddRock(TEXT("farallon_pescador_a"), FVector2D(-26.0f, -173.0f), 4.5f, 7.0f);
	AddRock(TEXT("farallon_pescador_b"), FVector2D(2.0f, -172.0f), 3.5f, 5.0f);
	AddRock(TEXT("risco_colina_este"), FVector2D(52.0f, 40.0f), 8.0f, 6.0f);
	AddRock(TEXT("risco_colina_noroeste"), FVector2D(74.0f, -12.0f), 8.0f, 5.0f);
	AddRock(TEXT("risco_colina_sureste"), FVector2D(18.0f, 44.0f), 7.0f, 5.0f);
	AddRock(TEXT("risco_islote_norte"), FVector2D(258.0f, 172.0f), 9.0f, 7.0f);
	AddRock(TEXT("risco_islote_este"), FVector2D(244.0f, 182.0f), 6.0f, 5.0f);
	AddRock(TEXT("risco_islote_oeste"), FVector2D(262.0f, 146.0f), 6.0f, 4.0f);
	AddRock(TEXT("farallon_islote"), FVector2D(276.0f, 186.0f), 4.0f, 6.0f);
	AddRock(TEXT("roca_cala_este"), FVector2D(-118.0f, 52.0f), 6.0f, 3.0f);
	AddRock(TEXT("roca_cala_oeste"), FVector2D(-120.0f, -38.0f), 5.0f, 2.5f);
	AddRock(TEXT("hondonada_cala_varada"), FVector2D(-138.0f, 8.0f), 32.0f, -3.5f);

	auto AddPath = [&Layout](const TCHAR* Id, std::initializer_list<FVector2D> Points)
	{
		FIslandPath Path;
		Path.Id = Id;
		Path.Width = 3.0f;
		Path.Points = Points;
		Layout.Paths.Add(Path);
	};
	AddPath(TEXT("camino_cala_palmar"), { FVector2D(-112, 10), FVector2D(-88, 6), FVector2D(-62, 0), FVector2D(-30, -5) });
	AddPath(TEXT("camino_palmar_faro"), { FVector2D(-30, -5), FVector2D(-2, -5), FVector2D(20, -5), FVector2D(30, 0), FVector2D(40, 10), FVector2D(45, 15) });
	AddPath(TEXT("camino_playa_faro"), { FVector2D(109, 55), FVector2D(100, 53), FVector2D(84, 54), FVector2D(72, 61), FVector2D(60, 67),
		FVector2D(48, 67), FVector2D(36, 62), FVector2D(32, 48), FVector2D(34, 36), FVector2D(40, 24), FVector2D(45, 15) });

	auto AddPoint = [&Layout](const TCHAR* Id, FVector2D Location, float Yaw)
	{
		FIslandPoint Point;
		Point.Id = Id;
		Point.Location = Location;
		Point.Yaw = Yaw;
		Layout.Points.Add(Point);
	};
	AddPoint(TEXT("player_start"), FVector2D(-112.0f, 10.0f), 0.0f);
	AddPoint(TEXT("raft_site"), FVector2D(109.0f, 55.0f), 40.0f);
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

float FIslandPath::Length() const
{
	float Total = 0.0f;
	for (int32 I = 1; I < Points.Num(); ++I)
	{
		Total += static_cast<float>(FVector2D::Distance(Points[I - 1], Points[I]));
	}
	return Total;
}

const FIslandPath* FIslandLayout::FindPath(FName Id) const
{
	return Paths.FindByPredicate([Id](const FIslandPath& P) { return P.Id == Id; });
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
		// Las colinas solo levantan tierra firme y se desvanecen hacia la costa en una franja ancha
		// (HillFadeStart..1): así la ladera que baja al mar no se empina más que la propia colina.
		const float LandFactor = 1.0f - FMath::SmoothStep(HillFadeStart, 1.0f, Column.NormalizedDistance);
		for (const FIslandZone& Zone : Zones)
		{
			if (Zone.Hill <= 0.0f)
			{
				continue;
			}
			const float Sigma = Zone.GetHillSigma();
			const float DistSq = static_cast<float>(FVector2D::DistSquared(Point, Zone.Center));
			Column.Height += Zone.Hill * LandFactor * FMath::Exp(-DistSq / (2.0f * Sigma * Sigma));
		}
	}
	for (const FIslandRock& Rock : Rocks)
	{
		Column.Height += Rock.HeightAt(Point);
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
		if (!IsValidName(Zone.Id) || Ids.Contains(Zone.Id) || Zone.Radius <= 0.0f || Zone.Hill < 0.0f || Zone.HillSigma < 0.0f)
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
	for (const FIslandRock& Rock : Rocks)
	{
		if (!IsValidName(Rock.Id) || Ids.Contains(Rock.Id) || Rock.Radius <= 0.0f)
		{
			OutError = FString::Printf(TEXT("roca inválida o con id repetido: %s"), *Rock.Id.ToString());
			return false;
		}
		Ids.Add(Rock.Id);
	}
	for (const FIslandPath& Path : Paths)
	{
		if (!IsValidName(Path.Id) || Ids.Contains(Path.Id) || Path.Points.Num() < 2 || Path.Width <= 0.0f)
		{
			OutError = FString::Printf(TEXT("camino inválido o con id repetido: %s"), *Path.Id.ToString());
			return false;
		}
		Ids.Add(Path.Id);
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
