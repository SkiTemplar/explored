#include "WorldGen/PointsOfInterest.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/TerrainDensity.h"
#include "WorldGen/VegetationScatter.h"

const TCHAR* LexToString(EPoiType Type)
{
	switch (Type)
	{
	case EPoiType::WreckFuselage: return TEXT("WreckFuselage");
	case EPoiType::WreckWing: return TEXT("WreckWing");
	case EPoiType::WreckTail: return TEXT("WreckTail");
	case EPoiType::WreckEngine: return TEXT("WreckEngine");
	case EPoiType::HaldenCamp: return TEXT("HaldenCamp");
	case EPoiType::RadioStation: return TEXT("RadioStation");
	case EPoiType::TideObservatory: return TEXT("TideObservatory");
	case EPoiType::Lighthouse: return TEXT("Lighthouse");
	case EPoiType::StarCompass: return TEXT("StarCompass");
	case EPoiType::Waterfall: return TEXT("Waterfall");
	case EPoiType::Viewpoint: return TEXT("Viewpoint");
	case EPoiType::Shipwreck: return TEXT("Shipwreck");
	case EPoiType::TurtleBeach: return TEXT("TurtleBeach");
	case EPoiType::HotSpring: return TEXT("HotSpring");
	case EPoiType::TidePool: return TEXT("TidePool");
	case EPoiType::Note: return TEXT("Note");
	case EPoiType::HaldenPage: return TEXT("HaldenPage");
	case EPoiType::Bottle: return TEXT("Bottle");
	case EPoiType::Petroglyph: return TEXT("Petroglyph");
	case EPoiType::BeaconSite: return TEXT("BeaconSite");
	default: return TEXT("Unknown");
	}
}

namespace
{
	FVector OnSurface(const FTerrainDensity& Density, float X, float Y)
	{
		float Z = 0.0f;
		FVector Normal;
		if (FVegetationScatter::FindSurface(Density, X, Y, Z, Normal))
		{
			return FVector(X, Y, Z);
		}
		return FVector(X, Y, Density.SampleColumn(X, Y).Height);
	}

	float YawTowards(const FVector& From, const FVector2D& To)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X));
	}

	int32 IndexOf(const FArchipelagoLayout& Layout, EIslandArchetype Archetype)
	{
		return Layout.Islands.IndexOfByPredicate([Archetype](const FIslandDesc& I) { return I.Archetype == Archetype; });
	}
}

FVector FPoiLayout::FindSummit(const FTerrainDensity& Density, const FIslandDesc& Island)
{
	FVector2D Best = Island.Center;
	float BestH = -1000.0f;
	const float Step = Island.Radius / 24.0f;
	for (float Y = -Island.Radius; Y <= Island.Radius; Y += Step)
	{
		for (float X = -Island.Radius; X <= Island.Radius; X += Step)
		{
			const FVector2D P = Island.Center + FVector2D(X, Y);
			const float H = Density.SampleColumn(P.X, P.Y).Height;
			if (H > BestH)
			{
				BestH = H;
				Best = P;
			}
		}
	}
	// Refinado local.
	const FVector2D Coarse = Best;
	for (float Y = -Step; Y <= Step; Y += Step / 6.0f)
	{
		for (float X = -Step; X <= Step; X += Step / 6.0f)
		{
			const FVector2D P = Coarse + FVector2D(X, Y);
			const float H = Density.SampleColumn(P.X, P.Y).Height;
			if (H > BestH)
			{
				BestH = H;
				Best = P;
			}
		}
	}
	return OnSurface(Density, Best.X, Best.Y);
}

bool FPoiLayout::FindBeach(const FTerrainDensity& Density, const FIslandDesc& Island, float Angle, FVector& Out)
{
	const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
	// De fuera hacia dentro: el primer punto con altura de playa.
	for (float R = Island.Radius * 1.4f; R > 0.0f; R -= 2.0f)
	{
		const FVector2D P = Island.Center + Dir * R;
		const float H = Density.SampleColumn(P.X, P.Y).Height;
		if (H > 1.2f && H < 3.0f)
		{
			Out = OnSurface(Density, P.X, P.Y);
			return true;
		}
		if (H >= 3.0f)
		{
			break;
		}
	}
	return false;
}

bool FPoiLayout::FindInland(const FTerrainDensity& Density, const FIslandDesc& Island, float Angle, float Fraction, FVector& Out)
{
	const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
	// Busca a lo largo del radio el punto más cercano a la fracción pedida que esté en tierra firme y no muy empinado.
	for (int32 Attempt = 0; Attempt < 24; ++Attempt)
	{
		const float Offset = static_cast<float>(Attempt / 2) * 0.03f * ((Attempt % 2) ? 1.0f : -1.0f);
		const FVector2D P = Island.Center + Dir * Island.Radius * FMath::Clamp(Fraction + Offset, 0.02f, 1.2f);
		const float H = Density.SampleColumn(P.X, P.Y).Height;
		if (H < 2.5f)
		{
			continue;
		}
		const FVector S = OnSurface(Density, P.X, P.Y);
		if (Density.Normal(S, 1.0f).Z > 0.8f)
		{
			Out = S;
			return true;
		}
	}
	return false;
}

TArray<FPointOfInterest> FPoiLayout::Generate(const FTerrainDensity& Density)
{
	const FArchipelagoLayout& Layout = Density.GetLayout();
	TArray<FPointOfInterest> Out;
	FExploredRandom Rng(static_cast<uint64>(Layout.Seed) ^ 0x901CAFEULL);

	auto Add = [&Out](EPoiType Type, int32 Island, const FVector& Location, float Yaw, FName Content = NAME_None, bool bUnderwater = false)
	{
		FPointOfInterest Poi;
		Poi.Type = Type;
		Poi.IslandIndex = Island;
		Poi.Location = Location;
		Poi.Yaw = Yaw;
		Poi.ContentId = Content;
		Poi.bUnderwater = bUnderwater;
		Out.Add(Poi);
	};

	auto BeachOrInland = [&](const FIslandDesc& Island, float Angle, FVector& Location)
	{
		return FindBeach(Density, Island, Angle, Location) || FindInland(Density, Island, Angle, 0.7f, Location);
	};

	// Miradores: cima de cada isla (en Los Dientes, el islote más alto).
	for (int32 I = 0; I < Layout.Islands.Num(); ++I)
	{
		const FIslandDesc& Island = Layout.Islands[I];
		const FVector Summit = FindSummit(Density, Island);
		Add(EPoiType::Viewpoint, I, Summit, 0.0f, FName(*FString::Printf(TEXT("view_%s"), LexToString(Island.Archetype))));
	}

	// Isla del Amaraje: laguna con el fuselaje, ala en la playa y notas 1–4.
	const int32 LandingIndex = IndexOf(Layout, EIslandArchetype::Landing);
	if (LandingIndex != INDEX_NONE)
	{
		const FIslandDesc& Island = Layout.Islands[LandingIndex];
		const float C = FMath::Cos(Island.Rotation);
		const float S = FMath::Sin(Island.Rotation);
		// Centro de la laguna en coordenadas locales (0.58, 0) → mundo.
		const FVector2D LagoonLocal(0.58f * Island.Radius, 0.0f);
		const FVector2D Lagoon = Island.Center + FVector2D(LagoonLocal.X * C - LagoonLocal.Y * S, LagoonLocal.X * S + LagoonLocal.Y * C);
		const FVector LagoonFloor(Lagoon.X, Lagoon.Y, Density.SampleColumn(Lagoon.X, Lagoon.Y).Height);
		Add(EPoiType::WreckFuselage, LandingIndex, LagoonFloor, Rng.RangeFloat(0.0f, 360.0f), FName(TEXT("ines_01")), true);

		FVector Beach;
		if (FindBeach(Density, Island, Island.Rotation + UE_PI, Beach))
		{
			Add(EPoiType::WreckWing, LandingIndex, Beach, YawTowards(Beach, Island.Center), FName(TEXT("ines_02")));
		}
		FVector Hill = FindSummit(Density, Island);
		Add(EPoiType::Note, LandingIndex, Hill + FVector(2, 2, 0), 0.0f, FName(TEXT("ines_03")));
		if (FindBeach(Density, Island, Island.Rotation + 0.35f, Beach))
		{
			Add(EPoiType::Note, LandingIndex, Beach, YawTowards(Beach, Island.Center), FName(TEXT("ines_04")));
		}
		if (FindBeach(Density, Island, Island.Rotation + UE_HALF_PI, Beach))
		{
			Add(EPoiType::TidePool, LandingIndex, Beach, 0.0f);
		}
	}

	// Esmeralda: cascada con cueva, campamento I, motor, notas 5–8.
	const int32 EmeraldIndex = IndexOf(Layout, EIslandArchetype::Emerald);
	if (EmeraldIndex != INDEX_NONE)
	{
		const FIslandDesc& Island = Layout.Islands[EmeraldIndex];
		FVector P;
		// La primera cueva de Esmeralda es la de la cascada.
		for (const FCaveDesc& Cave : Density.GetCaves())
		{
			if (FVector2D::Distance(FVector2D(Cave.Start), Island.Center) < Island.Radius)
			{
				Add(EPoiType::Waterfall, EmeraldIndex, Cave.Start, YawTowards(Cave.Start, Island.Center) + 180.0f, FName(TEXT("ines_07")));
				Add(EPoiType::Petroglyph, EmeraldIndex, FMath::Lerp(Cave.Start, Cave.End, 0.7f), 0.0f, FName(TEXT("petro_01")));
				break;
			}
		}
		if (FindBeach(Density, Island, Island.Rotation + UE_PI, P)) { Add(EPoiType::Note, EmeraldIndex, P, 0.0f, FName(TEXT("ines_05"))); }
		if (FindInland(Density, Island, Island.Rotation + 2.2f, 0.55f, P))
		{
			Add(EPoiType::HaldenCamp, EmeraldIndex, P, Rng.RangeFloat(0.0f, 360.0f), FName(TEXT("camp_halden_1")));
			Add(EPoiType::Note, EmeraldIndex, P + FVector(3, -2, 0), 0.0f, FName(TEXT("ines_06")));
		}
		if (FindInland(Density, Island, Island.Rotation + UE_HALF_PI, 0.65f, P)) { Add(EPoiType::Note, EmeraldIndex, P, 0.0f, FName(TEXT("ines_08"))); }
		if (FindInland(Density, Island, Island.Rotation - 0.8f, 0.8f, P)) { Add(EPoiType::WreckEngine, EmeraldIndex, P, Rng.RangeFloat(0.0f, 360.0f)); }
	}

	// Manglar: estación de radio y notas 9–10.
	const int32 MangroveIndex = IndexOf(Layout, EIslandArchetype::Mangrove);
	if (MangroveIndex != INDEX_NONE)
	{
		const FIslandDesc& Island = Layout.Islands[MangroveIndex];
		FVector P;
		if (BeachOrInland(Island, Island.Rotation, P)) { Add(EPoiType::Note, MangroveIndex, P, 0.0f, FName(TEXT("ines_09"))); }
		if (FindInland(Density, Island, Island.Rotation + 1.8f, 0.35f, P))
		{
			Add(EPoiType::RadioStation, MangroveIndex, P, Rng.RangeFloat(0.0f, 360.0f), FName(TEXT("radio")));
			Add(EPoiType::Note, MangroveIndex, P + FVector(2, 2, 0), 0.0f, FName(TEXT("ines_10")));
		}
	}

	// Arenas Blancas: pecio, playa de tortugas y campamento III, nota 11.
	const int32 SandsIndex = IndexOf(Layout, EIslandArchetype::WhiteSands);
	if (SandsIndex != INDEX_NONE)
	{
		const FIslandDesc& Island = Layout.Islands[SandsIndex];
		FVector P;
		if (FindBeach(Density, Island, Island.Rotation + 0.5f, P))
		{
			const FVector2D Out2D = FVector2D(P) + (FVector2D(P) - Island.Center).GetSafeNormal() * 60.0f;
			const FVector Wreck(Out2D.X, Out2D.Y, Density.SampleColumn(Out2D.X, Out2D.Y).Height);
			Add(EPoiType::Shipwreck, SandsIndex, Wreck, YawTowards(Wreck, Island.Center), FName(TEXT("ines_11")), Wreck.Z < -1.0f);
		}
		if (FindBeach(Density, Island, Island.Rotation + 2.5f, P)) { Add(EPoiType::TurtleBeach, SandsIndex, P, 0.0f); }
		if (FindBeach(Density, Island, Island.Rotation - 1.6f, P)) { Add(EPoiType::HaldenCamp, SandsIndex, P, 0.0f, FName(TEXT("camp_halden_3"))); }
	}

	// Humo: brújula estelar en la cumbre, aguas termales, campamento II, notas 12–13 y lugar de la baliza.
	const int32 SmokeIndex = IndexOf(Layout, EIslandArchetype::Smoke);
	if (SmokeIndex != INDEX_NONE)
	{
		const FIslandDesc& Island = Layout.Islands[SmokeIndex];
		const FVector Summit = FindSummit(Density, Island);
		Add(EPoiType::StarCompass, SmokeIndex, Summit, 0.0f, FName(TEXT("ines_13")));
		Add(EPoiType::BeaconSite, SmokeIndex, Summit + FVector(6, 0, 0), 0.0f);
		FVector P;
		if (FindInland(Density, Island, Island.Rotation + 0.6f, 0.75f, P))
		{
			Add(EPoiType::HaldenCamp, SmokeIndex, P, 0.0f, FName(TEXT("camp_halden_2")));
			Add(EPoiType::Note, SmokeIndex, P + FVector(-2, 3, 0), 0.0f, FName(TEXT("ines_12")));
		}
		if (FindInland(Density, Island, Island.Rotation - 1.2f, 0.85f, P)) { Add(EPoiType::HotSpring, SmokeIndex, P, 0.0f); }
	}

	// Meseta: observatorio de mareas, campamento IV y notas 14–15.
	const int32 MesaIndex = IndexOf(Layout, EIslandArchetype::Mesa);
	if (MesaIndex != INDEX_NONE)
	{
		const FIslandDesc& Island = Layout.Islands[MesaIndex];
		FVector P;
		if (FindInland(Density, Island, Island.Rotation + 0.3f, 0.3f, P))
		{
			Add(EPoiType::TideObservatory, MesaIndex, P, 0.0f, FName(TEXT("ines_14")));
		}
		if (FindInland(Density, Island, Island.Rotation + 2.6f, 0.6f, P))
		{
			Add(EPoiType::HaldenCamp, MesaIndex, P, 0.0f, FName(TEXT("camp_halden_4")));
			Add(EPoiType::Note, MesaIndex, P + FVector(2, -3, 0), 0.0f, FName(TEXT("ines_15")));
		}
	}

	// Los Dientes: faro en el islote más alto, colonia de aves y notas 16–18.
	const int32 TeethIndex = IndexOf(Layout, EIslandArchetype::Teeth);
	if (TeethIndex != INDEX_NONE)
	{
		const FIslandDesc& Island = Layout.Islands[TeethIndex];
		const FVector Summit = FindSummit(Density, Island);
		Add(EPoiType::Lighthouse, TeethIndex, Summit, 0.0f, FName(TEXT("ines_18")));
		Add(EPoiType::Note, TeethIndex, Summit + FVector(-4, 3, -2), 0.0f, FName(TEXT("ines_17")));
		FVector P;
		if (BeachOrInland(Island, Island.Rotation + 1.0f, P)) { Add(EPoiType::Note, TeethIndex, P, 0.0f, FName(TEXT("ines_16"))); }
	}

	// Cola del Albatros: en el fondo del canal entre la isla de inicio y la más cercana.
	if (LandingIndex != INDEX_NONE)
	{
		const FIslandDesc& Landing = Layout.Islands[LandingIndex];
		const FIslandDesc* Nearest = nullptr;
		for (const FIslandDesc& Other : Layout.Islands)
		{
			if (&Other != &Landing && (!Nearest || FVector2D::Distance(Other.Center, Landing.Center) < FVector2D::Distance(Nearest->Center, Landing.Center)))
			{
				Nearest = &Other;
			}
		}
		if (Nearest)
		{
			const FVector2D Mid = FMath::Lerp(Landing.Center, Nearest->Center, Landing.Radius / (Landing.Radius + Nearest->Radius));
			Add(EPoiType::WreckTail, LandingIndex, FVector(Mid.X, Mid.Y, Density.SampleColumn(Mid.X, Mid.Y).Height), Rng.RangeFloat(0.0f, 360.0f), NAME_None, true);
		}
	}

	// Páginas del diario Halden: repartidas entre los campamentos, la estación y el observatorio.
	{
		TArray<const FPointOfInterest*> Anchors;
		for (const FPointOfInterest& Poi : Out)
		{
			if (Poi.Type == EPoiType::HaldenCamp || Poi.Type == EPoiType::RadioStation || Poi.Type == EPoiType::TideObservatory || Poi.Type == EPoiType::StarCompass)
			{
				Anchors.Add(&Poi);
			}
		}
		TArray<FPointOfInterest> Pages;
		for (int32 Page = 1; Page <= 24 && Anchors.Num() > 0; ++Page)
		{
			const FPointOfInterest& Anchor = *Anchors[(Page - 1) % Anchors.Num()];
			const FVector2D Offset = Rng.InsideUnitDisc() * 12.0f;
			FPointOfInterest P;
			P.Type = EPoiType::HaldenPage;
			P.IslandIndex = Anchor.IslandIndex;
			P.Location = OnSurface(Density, Anchor.Location.X + Offset.X, Anchor.Location.Y + Offset.Y);
			P.ContentId = FName(*FString::Printf(TEXT("halden_%02d"), Page));
			Pages.Add(P);
		}
		Out.Append(Pages);
	}

	// Botellas en playas al azar y petroglifos en cuevas y cumbres.
	for (int32 B = 1; B <= 8; ++B)
	{
		const int32 IslandIndex = Rng.RangeInt(0, Layout.Islands.Num() - 1);
		FVector P;
		if (FindBeach(Density, Layout.Islands[IslandIndex], Rng.RangeFloat(0.0f, UE_TWO_PI), P))
		{
			Add(EPoiType::Bottle, IslandIndex, P, Rng.RangeFloat(0.0f, 360.0f), FName(*FString::Printf(TEXT("bottle_%02d"), B)));
		}
	}
	int32 Petro = 2;
	for (const FCaveDesc& Cave : Density.GetCaves())
	{
		if (Petro > 30)
		{
			break;
		}
		const FVector Mid = FMath::Lerp(Cave.Start, Cave.End, 0.5f);
		Add(EPoiType::Petroglyph, INDEX_NONE, Mid, 0.0f, FName(*FString::Printf(TEXT("petro_%02d"), Petro++)));
	}
	for (int32 I = 0; I < Layout.Islands.Num() && Petro <= 30; ++I)
	{
		for (int32 K = 0; K < 3 && Petro <= 30; ++K)
		{
			FVector P;
			if (FindInland(Density, Layout.Islands[I], Rng.RangeFloat(0.0f, UE_TWO_PI), Rng.RangeFloat(0.3f, 0.8f), P))
			{
				Add(EPoiType::Petroglyph, I, P, Rng.RangeFloat(0.0f, 360.0f), FName(*FString::Printf(TEXT("petro_%02d"), Petro++)));
			}
		}
	}
	return Out;
}
