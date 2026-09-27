#include "Exploration/ExplorationContentModel.h"

namespace ExplorationContentModelDetail
{
	/** Las siete islas jugables (GDD §4.2); la isla oculta no lleva lugares de interés con nombre. */
	const TArray<FName>& KnownIslandIds()
	{
		static const TArray<FName> Ids = {
			TEXT("landing"), TEXT("emerald"), TEXT("smoke"), TEXT("teeth"),
			TEXT("mangrove"), TEXT("whitesands"), TEXT("mesa"),
		};
		return Ids;
	}

	const TArray<FName>& KnownKinds()
	{
		static const TArray<FName> Kinds = {
			TEXT("cala"), TEXT("cueva"), TEXT("mirador"), TEXT("naufragio"), TEXT("ruina"),
			TEXT("cascada"), TEXT("arco_marino"), TEXT("poza"), TEXT("jardin_coral"),
			TEXT("cumbre"), TEXT("campamento"), TEXT("recurso"), TEXT("vista"),
		};
		return Kinds;
	}

	const TArray<FName>& KnownMapMarks()
	{
		static const TArray<FName> Marks = {
			TEXT("water"), TEXT("cave"), TEXT("danger"), TEXT("resource"), TEXT("ruin"), TEXT("wreck"),
		};
		return Marks;
	}
}

bool FExplorationCatalog::IsValidIslandId(FName IslandId)
{
	return ExplorationContentModelDetail::KnownIslandIds().Contains(IslandId);
}

bool FExplorationCatalog::IsValidKind(FName Kind)
{
	return ExplorationContentModelDetail::KnownKinds().Contains(Kind);
}

bool FExplorationCatalog::IsValidMapMark(FName MapMark)
{
	return MapMark.IsNone() || ExplorationContentModelDetail::KnownMapMarks().Contains(MapMark);
}

const FExplorationLandmark* FExplorationCatalog::FindLandmark(FName Id) const
{
	return Id.IsNone() ? nullptr : Landmarks.FindByPredicate([Id](const FExplorationLandmark& L) { return L.Id == Id; });
}

int32 FExplorationCatalog::CountForIsland(FName IslandId) const
{
	int32 Count = 0;
	for (const FExplorationLandmark& L : Landmarks)
	{
		if (L.IslandId == IslandId)
		{
			++Count;
		}
	}
	return Count;
}

bool FExplorationCatalog::Validate(TArray<FString>& OutErrors) const
{
	OutErrors.Reset();
	TSet<FName> SeenIds;
	for (const FExplorationLandmark& L : Landmarks)
	{
		const FString Tag = L.Id.IsNone() ? TEXT("<sin id>") : L.Id.ToString();
		if (L.Id.IsNone())
		{
			OutErrors.Add(TEXT("Lugar sin «id»"));
			continue;
		}
		if (SeenIds.Contains(L.Id))
		{
			OutErrors.Add(FString::Printf(TEXT("«%s»: id repetido"), *Tag));
		}
		SeenIds.Add(L.Id);

		if (!IsValidIslandId(L.IslandId))
		{
			OutErrors.Add(FString::Printf(TEXT("«%s»: isla «%s» desconocida"), *Tag, *L.IslandId.ToString()));
		}
		if (!IsValidKind(L.Kind))
		{
			OutErrors.Add(FString::Printf(TEXT("«%s»: tipo «%s» desconocido"), *Tag, *L.Kind.ToString()));
		}
		if (!IsValidMapMark(L.MapMark))
		{
			OutErrors.Add(FString::Printf(TEXT("«%s»: sello de mapa «%s» desconocido"), *Tag, *L.MapMark.ToString()));
		}
		if (L.NameEs.IsEmpty() || L.NameEn.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("«%s»: falta nombre en español o en inglés"), *Tag));
		}
		if (L.AccessEs.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("«%s»: falta cómo se llega"), *Tag));
		}
		if (L.RewardEs.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("«%s»: falta qué recompensa da"), *Tag));
		}
		if (L.AngleDeg < 0.0f || L.AngleDeg >= 360.0f)
		{
			OutErrors.Add(FString::Printf(TEXT("«%s»: ángulo fuera de [0, 360)"), *Tag));
		}
		// 1.2: un poco más allá del radio nominal, como los cayos satélite (FCayDesc) de ArchipelagoLayout.
		if (L.DistanceFrac < 0.0f || L.DistanceFrac > 1.2f)
		{
			OutErrors.Add(FString::Printf(TEXT("«%s»: distancia fuera de [0, 1.2] radios"), *Tag));
		}
	}
	return OutErrors.IsEmpty();
}
