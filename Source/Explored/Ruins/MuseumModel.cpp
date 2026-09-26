#include "Ruins/MuseumModel.h"

#include "Core/ExploredRandom.h"

const TCHAR* LexToString(EArtifactProvenance Provenance)
{
	switch (Provenance)
	{
	case EArtifactProvenance::Marae: return TEXT("marae");
	case EArtifactProvenance::RitualCave: return TEXT("cueva_ritual");
	case EArtifactProvenance::Shipwreck: return TEXT("pecio");
	case EArtifactProvenance::SunkenRuin: return TEXT("ruina_sumergida");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EArtifactRarity Rarity)
{
	switch (Rarity)
	{
	case EArtifactRarity::Common: return TEXT("comun");
	case EArtifactRarity::Rare: return TEXT("raro");
	case EArtifactRarity::Unique: return TEXT("unico");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EArtifactSize Size)
{
	switch (Size)
	{
	case EArtifactSize::Small: return TEXT("Pequeno");
	case EArtifactSize::Medium: return TEXT("Mediano");
	case EArtifactSize::Large: return TEXT("Grande");
	default: return TEXT("unknown");
	}
}

const TCHAR* LexToString(EMuseumResult Result)
{
	switch (Result)
	{
	case EMuseumResult::Ok: return TEXT("Ok");
	case EMuseumResult::UnknownArtifact: return TEXT("UnknownArtifact");
	case EMuseumResult::NotFound: return TEXT("NotFound");
	case EMuseumResult::AlreadyExhibited: return TEXT("AlreadyExhibited");
	case EMuseumResult::UnknownDisplay: return TEXT("UnknownDisplay");
	case EMuseumResult::DuplicateDisplay: return TEXT("DuplicateDisplay");
	case EMuseumResult::InvalidSlot: return TEXT("InvalidSlot");
	case EMuseumResult::SlotOccupied: return TEXT("SlotOccupied");
	case EMuseumResult::SlotEmpty: return TEXT("SlotEmpty");
	case EMuseumResult::TooLarge: return TEXT("TooLarge");
	default: return TEXT("Unknown");
	}
}

namespace MuseumModelDetail
{
	constexpr uint64 PlacementSalt = 0x54524541535552ULL;
	/** Separación entre tesoros que comparten lugar (metros). */
	constexpr float SpreadMeters = 1.5f;
	/** Las ruinas sumergidas quedan un poco más allá de la costa nominal. */
	constexpr float SunkenCoastFactor = 1.15f;
	constexpr float SunkenDepth = -1.0f;

	template <typename TEnum>
	bool ParseEnum(const FString& Text, TEnum& Out)
	{
		for (uint8 V = 0; V < static_cast<uint8>(TEnum::Count); ++V)
		{
			if (Text.Equals(LexToString(static_cast<TEnum>(V)), ESearchCase::IgnoreCase))
			{
				Out = static_cast<TEnum>(V);
				return true;
			}
		}
		return false;
	}

	/** Busca un elemento de un tipo en una ruina. */
	const FRuinElement* FindKind(const FRuinSite& Site, ERuinElementKind Kind)
	{
		return Site.Elements.FindByPredicate([Kind](const FRuinElement& E) { return E.Kind == Kind; });
	}

	/** Reparto en rueda: el n-ésimo tesoro de un lugar se separa del anterior en espiral. */
	FVector Spread(const FVector& Base, int32 Nth)
	{
		if (Nth == 0)
		{
			return Base;
		}
		const float Angle = 2.39996f * Nth;  // Ángulo áureo: no se amontonan.
		const float Radius = SpreadMeters * FMath::Sqrt(static_cast<float>(Nth));
		return Base + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
	}
}

// ---------------------------------------------------------------------------
// FTreasureCatalog
// ---------------------------------------------------------------------------

bool FTreasureCatalog::ParseProvenance(const FString& Text, EArtifactProvenance& Out)
{
	return MuseumModelDetail::ParseEnum(Text, Out);
}

bool FTreasureCatalog::ParseRarity(const FString& Text, EArtifactRarity& Out)
{
	return MuseumModelDetail::ParseEnum(Text, Out);
}

bool FTreasureCatalog::ParseSize(const FString& Text, EArtifactSize& Out)
{
	return MuseumModelDetail::ParseEnum(Text, Out);
}

const FArtifactDef* FTreasureCatalog::FindArtifact(FName Id) const
{
	return Id.IsNone() ? nullptr : Artifacts.FindByPredicate([Id](const FArtifactDef& A) { return A.Id == Id; });
}

const FDisplayDef* FTreasureCatalog::FindDisplay(FName Id) const
{
	return Id.IsNone() ? nullptr : Displays.FindByPredicate([Id](const FDisplayDef& D) { return D.Id == Id; });
}

bool FTreasureCatalog::CanEverDisplay(const FArtifactDef& Artifact) const
{
	for (const FDisplayDef& D : Displays)
	{
		for (const FDisplaySlotDef& S : D.Slots)
		{
			if (Artifact.Size <= S.MaxSize)
			{
				return true;
			}
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// FTreasurePlacement
// ---------------------------------------------------------------------------

TArray<FArtifactPlacement> FTreasurePlacement::Generate(const FRuinsLayout& Ruins, const TArray<FPointOfInterest>& Pois,
	const FTreasureCatalog& Catalog)
{
	using namespace MuseumModelDetail;

	// Lugares candidatos por procedencia, barajados con la semilla.
	TArray<int32> MaraeSites;
	TArray<int32> CaveSites;
	for (int32 I = 0; I < Ruins.Sites.Num(); ++I)
	{
		const FRuinSite& Site = Ruins.Sites[I];
		if (FindKind(Site, ERuinElementKind::StatueAlignment) && Ruins.Islands.IsValidIndex(Site.IslandIndex))
		{
			MaraeSites.Add(I);
		}
		if (FindKind(Site, ERuinElementKind::RitualCave))
		{
			CaveSites.Add(I);
		}
	}
	FExploredRandom Rng(static_cast<uint64>(Ruins.Seed) ^ PlacementSalt);
	auto Shuffle = [&Rng](TArray<int32>& Values)
	{
		for (int32 I = Values.Num() - 1; I > 0; --I)
		{
			Values.Swap(I, Rng.RangeInt(0, I));
		}
	};
	Shuffle(MaraeSites);
	Shuffle(CaveSites);
	const FPointOfInterest* Wreck = Pois.FindByPredicate([](const FPointOfInterest& P) { return P.Type == EPoiType::Shipwreck; });

	TArray<FArtifactPlacement> Out;
	TMap<FName, int32> PerPlace;
	int32 NextMarae = 0;
	int32 NextCave = 0;
	int32 NextSunken = 0;
	for (const FArtifactDef& Artifact : Catalog.Artifacts)
	{
		FArtifactPlacement P;
		P.ArtifactId = Artifact.Id;
		EArtifactProvenance Provenance = Artifact.Provenance;
		// Si falta el lugar de su procedencia, el tesoro queda en un marae.
		if ((Provenance == EArtifactProvenance::RitualCave && CaveSites.IsEmpty()) || (Provenance == EArtifactProvenance::Shipwreck && !Wreck))
		{
			Provenance = EArtifactProvenance::Marae;
		}
		switch (Provenance)
		{
		case EArtifactProvenance::Shipwreck:
			P.PlaceId = FName(TEXT("shipwreck"));
			P.IslandIndex = Wreck->IslandIndex;
			P.Location = Wreck->Location;
			P.bUnderwater = Wreck->bUnderwater;
			break;
		case EArtifactProvenance::RitualCave:
		{
			const FRuinSite& Site = Ruins.Sites[CaveSites[NextCave++ % CaveSites.Num()]];
			P.PlaceId = Site.Id;
			P.IslandIndex = Site.IslandIndex;
			P.Location = FindKind(Site, ERuinElementKind::RitualCave)->Location;
			break;
		}
		case EArtifactProvenance::SunkenRuin:
			if (!MaraeSites.IsEmpty())
			{
				const FRuinSite& Site = Ruins.Sites[MaraeSites[NextSunken++ % MaraeSites.Num()]];
				const FIslandDesc& Island = Ruins.Islands[Site.IslandIndex];
				FVector2D Dir = (FVector2D(Site.Anchor) - Island.Center).GetSafeNormal();
				if (Dir.IsNearlyZero())
				{
					Dir = FVector2D(1.0, 0.0);
				}
				const FVector2D At = Island.Center + Dir * (Island.Radius * SunkenCoastFactor);
				P.PlaceId = Site.Id;
				P.IslandIndex = Site.IslandIndex;
				P.Location = FVector(At.X, At.Y, SunkenDepth);
				P.bUnderwater = true;
				P.bNeedsSpringTide = true;
				break;
			}
			// Sin ruinas de isla no hay costa donde hundirlo: se trata como marae.
			[[fallthrough]];
		default:
			if (!MaraeSites.IsEmpty())
			{
				const FRuinSite& Site = Ruins.Sites[MaraeSites[NextMarae++ % MaraeSites.Num()]];
				const FRuinElement* Altar = FindKind(Site, ERuinElementKind::AltarOffering);
				P.PlaceId = Site.Id;
				P.IslandIndex = Site.IslandIndex;
				P.Location = Altar ? Altar->Location : Site.Anchor;
			}
			break;
		}
		if (!P.PlaceId.IsNone())
		{
			// Los que comparten sitio exacto se separan un poco (dos en el mismo altar).
			int32& Nth = PerPlace.FindOrAdd(FName(*FString::Printf(TEXT("%s_%d"), *P.PlaceId.ToString(), static_cast<int32>(Provenance))));
			P.Location = Spread(P.Location, Nth++);
		}
		Out.Add(P);
	}
	return Out;
}

// ---------------------------------------------------------------------------
// FMuseumModel
// ---------------------------------------------------------------------------

FMuseumModel::FMuseumModel(FTreasureCatalog InCatalog)
	: Catalog(MoveTemp(InCatalog))
{
}

FArtifactRecord* FMuseumModel::FindOrAddRecord(FName ArtifactId)
{
	if (!Catalog.FindArtifact(ArtifactId))
	{
		return nullptr;
	}
	FArtifactRecord* Record = State.Records.FindByPredicate([ArtifactId](const FArtifactRecord& R) { return R.ArtifactId == ArtifactId; });
	if (!Record)
	{
		Record = &State.Records.AddDefaulted_GetRef();
		Record->ArtifactId = ArtifactId;
	}
	return Record;
}

const FArtifactRecord* FMuseumModel::FindRecord(FName ArtifactId) const
{
	return State.Records.FindByPredicate([ArtifactId](const FArtifactRecord& R) { return R.ArtifactId == ArtifactId; });
}

bool FMuseumModel::MarkFound(FName ArtifactId, FName PlaceId, int32 IslandIndex)
{
	FArtifactRecord* Record = FindOrAddRecord(ArtifactId);
	if (!Record || Record->bFound)
	{
		return false;
	}
	Record->bFound = true;
	Record->FoundAt = PlaceId;
	Record->FoundIsland = IslandIndex;
	return true;
}

bool FMuseumModel::MarkPhotographed(FName ArtifactId)
{
	FArtifactRecord* Record = FindOrAddRecord(ArtifactId);
	if (!Record || Record->bPhotographed)
	{
		return false;
	}
	Record->bPhotographed = true;
	return true;
}

bool FMuseumModel::IsFound(FName ArtifactId) const
{
	const FArtifactRecord* R = FindRecord(ArtifactId);
	return R && R->bFound;
}

bool FMuseumModel::IsPhotographed(FName ArtifactId) const
{
	const FArtifactRecord* R = FindRecord(ArtifactId);
	return R && R->bPhotographed;
}

const FDisplayState* FMuseumModel::FindDisplay(int32 Key) const
{
	return State.Displays.FindByPredicate([Key](const FDisplayState& D) { return D.Key == Key; });
}

FDisplayState* FMuseumModel::FindDisplayMutable(int32 Key)
{
	return State.Displays.FindByPredicate([Key](const FDisplayState& D) { return D.Key == Key; });
}

EMuseumResult FMuseumModel::AddDisplay(int32 Key, FName DisplayId)
{
	const FDisplayDef* Def = Catalog.FindDisplay(DisplayId);
	if (!Def || Key == INDEX_NONE)
	{
		return EMuseumResult::UnknownDisplay;
	}
	if (FindDisplay(Key))
	{
		return EMuseumResult::DuplicateDisplay;
	}
	FDisplayState& D = State.Displays.AddDefaulted_GetRef();
	D.Key = Key;
	D.DisplayId = DisplayId;
	D.Slots.Init(NAME_None, Def->Slots.Num());
	return EMuseumResult::Ok;
}

EMuseumResult FMuseumModel::RemoveDisplay(int32 Key, TArray<FName>& OutReturned)
{
	OutReturned.Reset();
	const int32 Index = State.Displays.IndexOfByPredicate([Key](const FDisplayState& D) { return D.Key == Key; });
	if (Index == INDEX_NONE)
	{
		return EMuseumResult::UnknownDisplay;
	}
	for (const FName& Id : State.Displays[Index].Slots)
	{
		if (!Id.IsNone())
		{
			OutReturned.Add(Id);
		}
	}
	State.Displays.RemoveAt(Index);
	return EMuseumResult::Ok;
}

EMuseumResult FMuseumModel::CanPlace(FName ArtifactId, int32 Key, int32 Slot) const
{
	const FArtifactDef* Artifact = Catalog.FindArtifact(ArtifactId);
	if (!Artifact)
	{
		return EMuseumResult::UnknownArtifact;
	}
	if (!IsFound(ArtifactId))
	{
		return EMuseumResult::NotFound;
	}
	if (IsExhibited(ArtifactId))
	{
		return EMuseumResult::AlreadyExhibited;
	}
	const FDisplayState* Display = FindDisplay(Key);
	const FDisplayDef* Def = Display ? Catalog.FindDisplay(Display->DisplayId) : nullptr;
	if (!Def)
	{
		return EMuseumResult::UnknownDisplay;
	}
	if (!Def->Slots.IsValidIndex(Slot) || !Display->Slots.IsValidIndex(Slot))
	{
		return EMuseumResult::InvalidSlot;
	}
	if (!Display->Slots[Slot].IsNone())
	{
		return EMuseumResult::SlotOccupied;
	}
	if (Artifact->Size > Def->Slots[Slot].MaxSize)
	{
		return EMuseumResult::TooLarge;
	}
	return EMuseumResult::Ok;
}

EMuseumResult FMuseumModel::Place(FName ArtifactId, int32 Key, int32 Slot)
{
	const EMuseumResult Result = CanPlace(ArtifactId, Key, Slot);
	if (Result == EMuseumResult::Ok)
	{
		FindDisplayMutable(Key)->Slots[Slot] = ArtifactId;
	}
	return Result;
}

EMuseumResult FMuseumModel::Remove(int32 Key, int32 Slot, FName& OutArtifact)
{
	OutArtifact = NAME_None;
	FDisplayState* Display = FindDisplayMutable(Key);
	if (!Display)
	{
		return EMuseumResult::UnknownDisplay;
	}
	if (!Display->Slots.IsValidIndex(Slot))
	{
		return EMuseumResult::InvalidSlot;
	}
	if (Display->Slots[Slot].IsNone())
	{
		return EMuseumResult::SlotEmpty;
	}
	OutArtifact = Display->Slots[Slot];
	Display->Slots[Slot] = NAME_None;
	return EMuseumResult::Ok;
}

bool FMuseumModel::FindFreeSlot(FName ArtifactId, int32& OutKey, int32& OutSlot) const
{
	for (const FDisplayState& D : State.Displays)
	{
		for (int32 S = 0; S < D.Slots.Num(); ++S)
		{
			if (CanPlace(ArtifactId, D.Key, S) == EMuseumResult::Ok)
			{
				OutKey = D.Key;
				OutSlot = S;
				return true;
			}
		}
	}
	return false;
}

bool FMuseumModel::IsExhibited(FName ArtifactId) const
{
	if (ArtifactId.IsNone())
	{
		return false;
	}
	for (const FDisplayState& D : State.Displays)
	{
		if (D.Slots.Contains(ArtifactId))
		{
			return true;
		}
	}
	return false;
}

int32 FMuseumModel::CountExhibited() const
{
	int32 Count = 0;
	for (const FDisplayState& D : State.Displays)
	{
		for (const FName& Id : D.Slots)
		{
			Count += Id.IsNone() ? 0 : 1;
		}
	}
	return Count;
}

int32 FMuseumModel::CountFound() const
{
	int32 Count = 0;
	for (const FArtifactRecord& R : State.Records)
	{
		Count += R.bFound ? 1 : 0;
	}
	return Count;
}

int32 FMuseumModel::CountRegistered() const
{
	int32 Count = 0;
	for (const FArtifactRecord& R : State.Records)
	{
		Count += (R.bFound || R.bPhotographed) ? 1 : 0;
	}
	return Count;
}

float FMuseumModel::CatalogCompletion() const
{
	const int32 Total = Catalog.Artifacts.Num();
	return Total > 0 ? static_cast<float>(CountRegistered()) / Total : 0.0f;
}

bool FMuseumModel::IsCatalogComplete() const
{
	return Catalog.Artifacts.Num() > 0 && CountRegistered() == Catalog.Artifacts.Num();
}

void FMuseumModel::LoadState(const FMuseumState& Saved)
{
	State = FMuseumState();
	for (const FArtifactRecord& R : Saved.Records)
	{
		if (Catalog.FindArtifact(R.ArtifactId) && !FindRecord(R.ArtifactId) && (R.bFound || R.bPhotographed))
		{
			State.Records.Add(R);
		}
	}
	for (const FDisplayState& SavedDisplay : Saved.Displays)
	{
		if (AddDisplay(SavedDisplay.Key, SavedDisplay.DisplayId) != EMuseumResult::Ok)
		{
			continue;
		}
		// Recoloca hueco a hueco con las mismas reglas: nada duplicado, sin hallar ni demasiado grande.
		for (int32 S = 0; S < SavedDisplay.Slots.Num(); ++S)
		{
			if (!SavedDisplay.Slots[S].IsNone())
			{
				Place(SavedDisplay.Slots[S], SavedDisplay.Key, S);
			}
		}
	}
}
