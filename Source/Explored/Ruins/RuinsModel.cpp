#include "Ruins/RuinsModel.h"

#include "Core/ExploredRandom.h"

const TCHAR* LexToString(EWayfindingTechnique Technique)
{
	switch (Technique)
	{
	case EWayfindingTechnique::StarPath: return TEXT("star_path");
	case EWayfindingTechnique::SwellReading: return TEXT("swell_reading");
	case EWayfindingTechnique::BirdsAtDusk: return TEXT("birds_at_dusk");
	case EWayfindingTechnique::FixedClouds: return TEXT("fixed_clouds");
	case EWayfindingTechnique::WaterColour: return TEXT("water_colour");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(ERuinElementKind Kind)
{
	switch (Kind)
	{
	case ERuinElementKind::Petroglyph: return TEXT("petroglyph");
	case ERuinElementKind::StatueAlignment: return TEXT("statue");
	case ERuinElementKind::AltarOffering: return TEXT("altar");
	case ERuinElementKind::StarCompass: return TEXT("compass");
	case ERuinElementKind::DoubleCanoe: return TEXT("canoe");
	case ERuinElementKind::RitualCave: return TEXT("cave");
	default: return TEXT("unknown");
	}
}

namespace RuinsModelDetail
{
	// Flujos de azar separados: cambiar cómo se colocan los elementos no altera qué enseña cada ruina.
	constexpr uint64 PlacementSalt = 0x52554E41504C4143ULL;
	constexpr uint64 AssignmentSalt = 0x52554E4154454348ULL;
	constexpr uint64 HiddenIslandSalt = 0x52554E4148494445ULL;

	/** Distancias del ancla de la ruina a la estatua y al altar (metros). */
	constexpr float StatueDistance = 6.0f;
	constexpr float AltarDistance = 4.0f;
	constexpr float CanoeDistance = 12.0f;
	/** Candidatos de dirección para la isla oculta. */
	constexpr int32 HiddenIslandCandidates = 32;

	/** Distancia de un punto a la costa nominal de una isla (negativa dentro). */
	float CoastDistance(const FIslandDesc& Island, const FVector2D& P)
	{
		return static_cast<float>(FVector2D::Distance(Island.Center, P)) - Island.Radius;
	}

	int32 NearestIsland(const TArray<FIslandDesc>& Islands, const FVector2D& P)
	{
		int32 Best = INDEX_NONE;
		float BestDistance = TNumericLimits<float>::Max();
		for (int32 I = 0; I < Islands.Num(); ++I)
		{
			const float D = CoastDistance(Islands[I], P);
			if (D < BestDistance)
			{
				BestDistance = D;
				Best = I;
			}
		}
		return Best;
	}

	FVector Offset(const FVector& Anchor, float Angle, float Distance)
	{
		return Anchor + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
	}

	/** Rasgo propio de la ruina de cada isla (GDD §4.2 y §6.1). */
	ERuinElementKind FeatureOf(EIslandArchetype Archetype)
	{
		switch (Archetype)
		{
		case EIslandArchetype::Emerald: return ERuinElementKind::RitualCave;  // Cueva tras la cascada.
		case EIslandArchetype::Smoke: return ERuinElementKind::RitualCave;    // Tubo de lava.
		case EIslandArchetype::Teeth: return ERuinElementKind::RitualCave;    // Cueva marina.
		case EIslandArchetype::WhiteSands: return ERuinElementKind::DoubleCanoe;
		default: return ERuinElementKind::Count;                              // Solo marae, estatua y altar.
		}
	}

	FName ElementId(FName SiteId, const TCHAR* Suffix)
	{
		return FName(*FString::Printf(TEXT("%s_%s"), *SiteId.ToString(), Suffix));
	}

	FWayfindingAnnotation MakeAnnotation(EWayfindingTechnique Technique, FName Source, int32 FromIsland, int32 TargetIsland,
		const FVector2D& From, const FVector2D& To)
	{
		FWayfindingAnnotation A;
		A.Technique = Technique;
		A.SourceSite = Source;
		A.FromIsland = FromIsland;
		A.TargetIsland = TargetIsland;
		A.From = From;
		A.To = To;
		A.BearingDegrees = FRuinsModel::BearingDegrees(From, To);
		A.DistanceMeters = static_cast<float>(FVector2D::Distance(From, To));
		return A;
	}

	/** Marca sin rumbo (nube sobre una cumbre, bajío): From = To y la distancia es el radio. */
	FWayfindingAnnotation MakeMark(EWayfindingTechnique Technique, FName Source, int32 TargetIsland, const FVector2D& At,
		float Radius, float Bearing)
	{
		FWayfindingAnnotation A = MakeAnnotation(Technique, Source, INDEX_NONE, TargetIsland, At, At);
		A.BearingDegrees = Bearing;
		A.DistanceMeters = Radius;
		return A;
	}
}

// ---------------------------------------------------------------------------
// FRuinSite
// ---------------------------------------------------------------------------

int32 FRuinSite::CountPetroglyphs() const
{
	int32 Count = 0;
	for (const FRuinElement& E : Elements)
	{
		Count += E.Kind == ERuinElementKind::Petroglyph ? 1 : 0;
	}
	return Count;
}

int32 FRuinSite::FindElement(FName ElementId) const
{
	return Elements.IndexOfByPredicate([ElementId](const FRuinElement& E) { return E.Id == ElementId; });
}

// ---------------------------------------------------------------------------
// FRuinsLayout
// ---------------------------------------------------------------------------

FRuinsLayout FRuinsLayout::Generate(const FArchipelagoLayout& Layout, const TArray<FPointOfInterest>& Pois)
{
	using namespace RuinsModelDetail;

	FRuinsLayout Out;
	Out.Seed = Layout.Seed;
	Out.Islands = Layout.Islands;
	const int32 NumIslands = Layout.Islands.Num();

	// Petroglifos por isla (los de cueva no tienen isla: van a la de costa más cercana).
	TArray<TArray<const FPointOfInterest*>> Petroglyphs;
	Petroglyphs.SetNum(NumIslands);
	const FPointOfInterest* Compass = nullptr;
	const FPointOfInterest* Waterfall = nullptr;
	const FPointOfInterest* TurtleBeach = nullptr;
	for (const FPointOfInterest& Poi : Pois)
	{
		if (Poi.Type == EPoiType::Petroglyph && !Poi.ContentId.IsNone())
		{
			const int32 Island = Layout.Islands.IsValidIndex(Poi.IslandIndex) ? Poi.IslandIndex
				: NearestIsland(Layout.Islands, FVector2D(Poi.Location));
			if (Petroglyphs.IsValidIndex(Island))
			{
				Petroglyphs[Island].Add(&Poi);
			}
		}
		else if (Poi.Type == EPoiType::StarCompass && !Compass)
		{
			Compass = &Poi;
		}
		else if (Poi.Type == EPoiType::Waterfall && !Waterfall)
		{
			Waterfall = &Poi;
		}
		else if (Poi.Type == EPoiType::TurtleBeach && !TurtleBeach)
		{
			TurtleBeach = &Poi;
		}
	}

	// Una ruina (marae) por isla.
	FExploredRandom PlaceRng(static_cast<uint64>(Layout.Seed) ^ PlacementSalt);
	for (int32 I = 0; I < NumIslands; ++I)
	{
		const FIslandDesc& Island = Layout.Islands[I];
		FRuinSite Site;
		Site.Id = FName(*(FString(TEXT("ruin_")) + FString(LexToString(Island.Archetype)).ToLower()));
		Site.IslandIndex = I;

		// Ancla: el primer petroglifo en tierra de la isla; si no hay, su mirador; si no, el centro.
		const FPointOfInterest* const* Inland = Petroglyphs[I].FindByPredicate([I](const FPointOfInterest* P) { return P->IslandIndex == I; });
		const FPointOfInterest* View = Pois.FindByPredicate([I](const FPointOfInterest& P) { return P.Type == EPoiType::Viewpoint && P.IslandIndex == I; });
		Site.Anchor = Inland ? (*Inland)->Location : View ? View->Location : FVector(Island.Center.X, Island.Center.Y, 0.0);

		const float Angle = PlaceRng.RangeFloat(0.0f, UE_TWO_PI);
		FRuinElement Statue;
		Statue.Id = ElementId(Site.Id, TEXT("statue"));
		Statue.Kind = ERuinElementKind::StatueAlignment;
		Statue.Location = Offset(Site.Anchor, Angle, StatueDistance);
		Site.Elements.Add(Statue);

		FRuinElement Altar;
		Altar.Id = ElementId(Site.Id, TEXT("altar"));
		Altar.Kind = ERuinElementKind::AltarOffering;
		Altar.Location = Offset(Site.Anchor, Angle + 2.4f, AltarDistance);
		Site.Elements.Add(Altar);

		const ERuinElementKind Feature = FeatureOf(Island.Archetype);
		if (Feature != ERuinElementKind::Count)
		{
			FRuinElement E;
			E.Kind = Feature;
			E.Id = ElementId(Site.Id, LexToString(Feature));
			E.Location = Offset(Site.Anchor, Angle - 2.4f, CanoeDistance);
			if (Feature == ERuinElementKind::RitualCave)
			{
				const FPointOfInterest* const* CavePetroglyph = Petroglyphs[I].FindByPredicate([](const FPointOfInterest* P) { return P->IslandIndex == INDEX_NONE; });
				if (Island.Archetype == EIslandArchetype::Emerald && Waterfall)
				{
					E.Location = Waterfall->Location;
				}
				else if (CavePetroglyph)
				{
					E.Location = (*CavePetroglyph)->Location;
				}
			}
			else if (Feature == ERuinElementKind::DoubleCanoe && TurtleBeach && TurtleBeach->IslandIndex == I)
			{
				E.Location = Offset(TurtleBeach->Location, Angle, CanoeDistance);
			}
			Site.Elements.Add(E);
		}

		for (const FPointOfInterest* P : Petroglyphs[I])
		{
			FRuinElement E;
			E.Id = P->ContentId;
			E.Kind = ERuinElementKind::Petroglyph;
			E.Location = P->Location;
			E.Yaw = P->Yaw;
			E.bRequired = false;
			Site.Elements.Add(E);
		}
		Site.RequiredPetroglyphs = FMath::Min(PetroglyphsPerSite, Petroglyphs[I].Num());
		Out.Sites.Add(MoveTemp(Site));
	}

	// La brújula estelar de la cumbre del Humo.
	int32 CompassSite = INDEX_NONE;
	if (Compass)
	{
		FRuinSite Site;
		Site.Id = FName(TEXT("ruin_compass"));
		Site.IslandIndex = Layout.Islands.IsValidIndex(Compass->IslandIndex) ? Compass->IslandIndex : NearestIsland(Layout.Islands, FVector2D(Compass->Location));
		Site.Anchor = Compass->Location;

		FRuinElement Monument;
		Monument.Id = ElementId(Site.Id, TEXT("monument"));
		Monument.Kind = ERuinElementKind::StarCompass;
		Monument.Location = Compass->Location;
		Site.Elements.Add(Monument);

		FRuinElement Altar;
		Altar.Id = ElementId(Site.Id, TEXT("altar"));
		Altar.Kind = ERuinElementKind::AltarOffering;
		Altar.Location = Offset(Site.Anchor, PlaceRng.RangeFloat(0.0f, UE_TWO_PI), AltarDistance);
		Site.Elements.Add(Altar);

		Site.Teaches = EWayfindingTechnique::StarPath;
		Site.StarPathTarget = HiddenIslandIndex;
		CompassSite = Out.Sites.Add(MoveTemp(Site));
	}

	// Isla oculta: fuera del mapa, en la dirección más alejada de todas las costas.
	FExploredRandom HiddenRng(static_cast<uint64>(Layout.Seed) ^ HiddenIslandSalt);
	const float StartAngle = HiddenRng.RangeFloat(0.0f, UE_TWO_PI);
	float BestClearance = -TNumericLimits<float>::Max();
	for (int32 K = 0; K < HiddenIslandCandidates; ++K)
	{
		const float A = StartAngle + static_cast<float>(UE_TWO_PI) * K / HiddenIslandCandidates;
		const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
		// Sobre el cuadrado de lado 2·HiddenIslandDistance: siempre fuera del mapa de 6 × 6 km.
		const FVector2D P = Dir * (HiddenIslandDistance / FMath::Max(FMath::Abs(Dir.X), FMath::Abs(Dir.Y)));
		float Clearance = TNumericLimits<float>::Max();
		for (const FIslandDesc& Island : Layout.Islands)
		{
			Clearance = FMath::Min(Clearance, CoastDistance(Island, P));
		}
		if (Clearance > BestClearance)
		{
			BestClearance = Clearance;
			Out.HiddenIslandCenter = P;
		}
	}
	Out.DepartureIsland = NearestIsland(Layout.Islands, Out.HiddenIslandCenter);

	// Técnicas: las cuatro que no son caminos de estrellas van a ruinas de isla barajadas;
	// el resto enseña caminos de estrellas hacia otra isla concreta.
	FExploredRandom AssignRng(static_cast<uint64>(Layout.Seed) ^ AssignmentSalt);
	TArray<int32> Order;
	for (int32 I = 0; I < NumIslands; ++I)
	{
		Order.Add(I);
	}
	for (int32 I = Order.Num() - 1; I > 0; --I)
	{
		Order.Swap(I, AssignRng.RangeInt(0, I));
	}
	const EWayfindingTechnique NonStar[] = {
		EWayfindingTechnique::SwellReading, EWayfindingTechnique::BirdsAtDusk,
		EWayfindingTechnique::FixedClouds, EWayfindingTechnique::WaterColour,
	};
	const int32 CompassPaths = CompassSite != INDEX_NONE ? 1 : 0;
	const int32 StarSitesNeeded = FMath::Max(1, RequiredStarPaths - CompassPaths);
	const int32 NonStarSlots = FMath::Clamp(NumIslands - StarSitesNeeded, 0, static_cast<int32>(UE_ARRAY_COUNT(NonStar)));
	// Mismo salto para todas: cada ruina apunta a una isla distinta de la suya y de las demás.
	const int32 TargetStep = NumIslands > 1 ? AssignRng.RangeInt(1, NumIslands - 1) : 0;
	for (int32 K = 0; K < Order.Num(); ++K)
	{
		FRuinSite& Site = Out.Sites[Order[K]];
		if (K < NonStarSlots)
		{
			Site.Teaches = NonStar[K];
			Site.StarPathTarget = INDEX_NONE;
		}
		else
		{
			Site.Teaches = EWayfindingTechnique::StarPath;
			Site.StarPathTarget = NumIslands > 1 ? (Site.IslandIndex + TargetStep) % NumIslands : HiddenIslandIndex;
		}
	}

	// Las estatuas miran a su camino de estrellas o, si no, al mar.
	for (FRuinSite& Site : Out.Sites)
	{
		const FVector2D From(Site.Anchor);
		const FVector2D Sea = From + (From - Out.TargetCenter(Site.IslandIndex));
		const FVector2D Look = Site.Teaches == EWayfindingTechnique::StarPath ? Out.TargetCenter(Site.StarPathTarget) : Sea;
		for (FRuinElement& E : Site.Elements)
		{
			if (E.Kind == ERuinElementKind::StatueAlignment)
			{
				E.Yaw = FRuinsModel::BearingDegrees(From, Look);
			}
		}
	}
	return Out;
}

int32 FRuinsLayout::FindSite(FName SiteId) const
{
	return Sites.IndexOfByPredicate([SiteId](const FRuinSite& S) { return S.Id == SiteId; });
}

int32 FRuinsLayout::FindSiteOfElement(FName ElementId) const
{
	if (ElementId.IsNone())
	{
		return INDEX_NONE;
	}
	return Sites.IndexOfByPredicate([ElementId](const FRuinSite& S) { return S.FindElement(ElementId) != INDEX_NONE; });
}

int32 FRuinsLayout::CountStarPathSites() const
{
	return CountSitesTeaching(EWayfindingTechnique::StarPath);
}

int32 FRuinsLayout::CountSitesTeaching(EWayfindingTechnique Technique) const
{
	int32 Count = 0;
	for (const FRuinSite& S : Sites)
	{
		Count += S.Teaches == Technique ? 1 : 0;
	}
	return Count;
}

FVector2D FRuinsLayout::TargetCenter(int32 Target) const
{
	if (Target == HiddenIslandIndex)
	{
		return HiddenIslandCenter;
	}
	return Islands.IsValidIndex(Target) ? Islands[Target].Center : FVector2D::ZeroVector;
}

// ---------------------------------------------------------------------------
// FRuinsModel
// ---------------------------------------------------------------------------

FRuinsModel::FRuinsModel(FRuinsLayout InLayout)
	: Layout(MoveTemp(InLayout))
{
}

float FRuinsModel::BearingDegrees(const FVector2D& From, const FVector2D& To)
{
	const FVector2D D = To - From;
	if (D.IsNearlyZero())
	{
		return 0.0f;
	}
	float Degrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)));
	Degrees = FMath::Fmod(Degrees + 360.0f, 360.0f);
	return Degrees >= 360.0f ? 0.0f : Degrees;
}

FRuinDiscovery FRuinsModel::Discover(FName ElementId)
{
	FRuinDiscovery Result;
	const int32 SiteIndex = Layout.FindSiteOfElement(ElementId);
	if (SiteIndex == INDEX_NONE || IsDiscovered(ElementId))
	{
		return Result;
	}
	State.DiscoveredElements.Add(ElementId);
	Result.bNew = true;
	Result.SiteIndex = SiteIndex;

	const FRuinSite& Site = Layout.Sites[SiteIndex];
	if (!State.CompletedSites.Contains(Site.Id) && IsSiteCompleteFromDiscoveries(SiteIndex))
	{
		const bool bKnew = KnowsTechnique(Site.Teaches);
		State.CompletedSites.Add(Site.Id);
		Result.bSiteCompleted = true;
		Result.Technique = Site.Teaches;
		Result.bTechniqueLearned = !bKnew;
		Result.StarPathTarget = Site.Teaches == EWayfindingTechnique::StarPath ? Site.StarPathTarget : INDEX_NONE;
	}
	return Result;
}

bool FRuinsModel::IsSiteCompleteFromDiscoveries(int32 SiteIndex) const
{
	int32 Done = 0;
	int32 Required = 0;
	GetSiteProgress(SiteIndex, Done, Required);
	return Required > 0 && Done >= Required;
}

void FRuinsModel::GetSiteProgress(int32 SiteIndex, int32& OutDone, int32& OutRequired) const
{
	OutDone = 0;
	OutRequired = 0;
	if (!Layout.Sites.IsValidIndex(SiteIndex))
	{
		return;
	}
	const FRuinSite& Site = Layout.Sites[SiteIndex];
	int32 Petroglyphs = 0;
	for (const FRuinElement& E : Site.Elements)
	{
		if (E.bRequired)
		{
			++OutRequired;
			OutDone += IsDiscovered(E.Id) ? 1 : 0;
		}
		else if (E.Kind == ERuinElementKind::Petroglyph && IsDiscovered(E.Id))
		{
			++Petroglyphs;
		}
	}
	OutRequired += Site.RequiredPetroglyphs;
	OutDone += FMath::Min(Petroglyphs, Site.RequiredPetroglyphs);
}

bool FRuinsModel::IsSiteComplete(int32 SiteIndex) const
{
	return Layout.Sites.IsValidIndex(SiteIndex) && State.CompletedSites.Contains(Layout.Sites[SiteIndex].Id);
}

bool FRuinsModel::KnowsTechnique(EWayfindingTechnique Technique) const
{
	for (const FName& Id : State.CompletedSites)
	{
		const int32 I = Layout.FindSite(Id);
		if (I != INDEX_NONE && Layout.Sites[I].Teaches == Technique)
		{
			return true;
		}
	}
	return false;
}

int32 FRuinsModel::CountKnownTechniques() const
{
	int32 Count = 0;
	for (uint8 T = 0; T < static_cast<uint8>(EWayfindingTechnique::Count); ++T)
	{
		Count += KnowsTechnique(static_cast<EWayfindingTechnique>(T)) ? 1 : 0;
	}
	return Count;
}

int32 FRuinsModel::CountStarPaths() const
{
	// Dos ruinas que enseñen el mismo destino cuentan como un solo camino.
	TArray<int32> Targets;
	for (const FName& Id : State.CompletedSites)
	{
		const int32 I = Layout.FindSite(Id);
		if (I != INDEX_NONE && Layout.Sites[I].Teaches == EWayfindingTechnique::StarPath)
		{
			Targets.AddUnique(Layout.Sites[I].StarPathTarget);
		}
	}
	return Targets.Num();
}

bool FRuinsModel::HasStarPathTo(int32 Target) const
{
	for (const FName& Id : State.CompletedSites)
	{
		const int32 I = Layout.FindSite(Id);
		if (I != INDEX_NONE && Layout.Sites[I].Teaches == EWayfindingTechnique::StarPath && Layout.Sites[I].StarPathTarget == Target)
		{
			return true;
		}
	}
	return false;
}

TArray<FWayfindingAnnotation> FRuinsModel::AnnotationsForSite(int32 SiteIndex) const
{
	using namespace RuinsModelDetail;

	TArray<FWayfindingAnnotation> Out;
	if (!Layout.Sites.IsValidIndex(SiteIndex))
	{
		return Out;
	}
	const FRuinSite& Site = Layout.Sites[SiteIndex];
	const TArray<FIslandDesc>& Islands = Layout.Islands;
	switch (Site.Teaches)
	{
	case EWayfindingTechnique::StarPath:
	{
		// La brújula marca el rumbo desde la propia cumbre; las ruinas de isla, desde el centro.
		const FVector2D From = Site.Elements.ContainsByPredicate([](const FRuinElement& E) { return E.Kind == ERuinElementKind::StarCompass; })
			? FVector2D(Site.Anchor)
			: Layout.TargetCenter(Site.IslandIndex);
		Out.Add(MakeAnnotation(Site.Teaches, Site.Id, Site.IslandIndex, Site.StarPathTarget, From, Layout.TargetCenter(Site.StarPathTarget)));
		break;
	}
	case EWayfindingTechnique::SwellReading:
		// El mar de fondo se deforma entre islas vecinas de la cadena: rumbo y distancia en ambos sentidos.
		for (int32 I = 0; I + 1 < Islands.Num(); ++I)
		{
			Out.Add(MakeAnnotation(Site.Teaches, Site.Id, I, I + 1, Islands[I].Center, Islands[I + 1].Center));
			Out.Add(MakeAnnotation(Site.Teaches, Site.Id, I + 1, I, Islands[I + 1].Center, Islands[I].Center));
		}
		break;
	case EWayfindingTechnique::BirdsAtDusk:
		// Al anochecer las bandadas vuelven a la tierra más cercana: de cada isla a su vecina más próxima.
		for (int32 I = 0; I < Islands.Num(); ++I)
		{
			int32 Nearest = INDEX_NONE;
			float Best = TNumericLimits<float>::Max();
			for (int32 J = 0; J < Islands.Num(); ++J)
			{
				const float Gap = CoastDistance(Islands[J], Islands[I].Center) - Islands[I].Radius;
				if (J != I && Gap < Best)
				{
					Best = Gap;
					Nearest = J;
				}
			}
			if (Nearest != INDEX_NONE)
			{
				Out.Add(MakeAnnotation(Site.Teaches, Site.Id, I, Nearest, Islands[I].Center, Islands[Nearest].Center));
			}
		}
		break;
	case EWayfindingTechnique::FixedClouds:
		// Una nube quieta sobre cada cumbre alta la delata desde lejos.
		for (int32 I = 0; I < Islands.Num(); ++I)
		{
			if (Islands[I].MaxHeight >= FRuinsLayout::FixedCloudMinHeight)
			{
				Out.Add(MakeMark(Site.Teaches, Site.Id, I, Islands[I].Center, Islands[I].Radius * 0.8f, 0.0f));
			}
		}
		break;
	case EWayfindingTechnique::WaterColour:
		// El tono del agua marca el arrecife de cada isla y los bajíos de sus cayos.
		for (int32 I = 0; I < Islands.Num(); ++I)
		{
			Out.Add(MakeMark(Site.Teaches, Site.Id, I, Islands[I].Center, Islands[I].Radius * 1.25f, 0.0f));
			for (const FCayDesc& Cay : Islands[I].Cays)
			{
				Out.Add(MakeMark(Site.Teaches, Site.Id, I, Cay.Center, Cay.Radius * 2.0f, BearingDegrees(Islands[I].Center, Cay.Center)));
			}
		}
		break;
	default:
		break;
	}
	return Out;
}

TArray<FWayfindingAnnotation> FRuinsModel::BuildAnnotations() const
{
	TArray<FWayfindingAnnotation> Out;
	TArray<EWayfindingTechnique> GlobalDone;
	for (int32 I = 0; I < Layout.Sites.Num(); ++I)
	{
		if (!IsSiteComplete(I))
		{
			continue;
		}
		const EWayfindingTechnique T = Layout.Sites[I].Teaches;
		// Las técnicas que no son caminos de estrellas valen para todo el archipiélago: una vez.
		if (T != EWayfindingTechnique::StarPath)
		{
			if (GlobalDone.Contains(T))
			{
				continue;
			}
			GlobalDone.Add(T);
		}
		Out.Append(AnnotationsForSite(I));
	}
	return Out;
}

void FRuinsModel::LoadState(const FRuinsState& Saved)
{
	State = FRuinsState();
	for (const FName& Id : Saved.DiscoveredElements)
	{
		if (Layout.FindSiteOfElement(Id) != INDEX_NONE && !State.DiscoveredElements.Contains(Id))
		{
			State.DiscoveredElements.Add(Id);
		}
	}
	// Respeta el orden guardado de las ruinas que siguen completas y añade las que falten.
	for (const FName& Id : Saved.CompletedSites)
	{
		const int32 I = Layout.FindSite(Id);
		if (I != INDEX_NONE && !State.CompletedSites.Contains(Id) && IsSiteCompleteFromDiscoveries(I))
		{
			State.CompletedSites.Add(Id);
		}
	}
	for (int32 I = 0; I < Layout.Sites.Num(); ++I)
	{
		if (!State.CompletedSites.Contains(Layout.Sites[I].Id) && IsSiteCompleteFromDiscoveries(I))
		{
			State.CompletedSites.Add(Layout.Sites[I].Id);
		}
	}
}
