#include "WorldGen/HarvestModel.h"

namespace
{
	FHarvestDrop MakeDrop(const TCHAR* ItemId, int32 MinCount, int32 MaxCount)
	{
		FHarvestDrop Drop;
		Drop.ItemId = FName(ItemId);
		Drop.MinCount = MinCount;
		Drop.MaxCount = MaxCount;
		return Drop;
	}
}

TArray<FHarvestSpeciesRule> FHarvestModel::DefaultRules()
{
	TArray<FHarvestSpeciesRule> Rules;

	{
		// Palmera: cocos y fibra al talar; hojas sueltas en cada golpe mientras se tala.
		FHarvestSpeciesRule R;
		R.Species = FName(TEXT("Palm"));
		R.HitsBareHands = 8;
		R.HitsWithTool = 4;
		R.RequiredToolTag = FName(TEXT("Filo"));
		R.PerHitDrops = { MakeDrop(TEXT("hoja_palma"), 0, 1) };
		R.FellDrops = {
			MakeDrop(TEXT("tronco_pequeno"), 1, 1),
			MakeDrop(TEXT("coco_maduro"), 1, 3),
			MakeDrop(TEXT("fibra_coco"), 0, 2),
			MakeDrop(TEXT("cascara_coco"), 0, 1),
		};
		R.RegrowHours = 18.0f * 24.0f; // con fruto: 18 días (biblia 02 §1.2), igual que FFellingModel::Palm.
		Rules.Add(R);
	}
	{
		// Árbol grande de la jungla: mucha madera, requiere herramienta de verdad.
		FHarvestSpeciesRule R;
		R.Species = FName(TEXT("JungleGiant"));
		R.HitsBareHands = 14;
		R.HitsWithTool = 6;
		R.RequiredToolTag = FName(TEXT("Filo"));
		R.PerHitDrops = { MakeDrop(TEXT("rama_seca"), 0, 1) };
		R.FellDrops = {
			MakeDrop(TEXT("tronco_pequeno"), 2, 3),
			MakeDrop(TEXT("corteza"), 1, 2),
			MakeDrop(TEXT("resina"), 0, 1),
		};
		R.RegrowHours = 24.0f * 24.0f; // madera sin fruto: 24 días.
		Rules.Add(R);
	}
	{
		FHarvestSpeciesRule R;
		R.Species = FName(TEXT("JungleWide"));
		R.HitsBareHands = 11;
		R.HitsWithTool = 5;
		R.RequiredToolTag = FName(TEXT("Filo"));
		R.PerHitDrops = { MakeDrop(TEXT("rama_verde"), 0, 1) };
		R.FellDrops = {
			MakeDrop(TEXT("tronco_pequeno"), 1, 2),
			MakeDrop(TEXT("corteza"), 1, 1),
		};
		R.RegrowHours = 24.0f * 24.0f;
		Rules.Add(R);
	}
	{
		// Manglar: junto al agua, da madera flotante además del tronco.
		FHarvestSpeciesRule R;
		R.Species = FName(TEXT("Mangrove"));
		R.HitsBareHands = 9;
		R.HitsWithTool = 4;
		R.RequiredToolTag = FName(TEXT("Filo"));
		R.PerHitDrops = { MakeDrop(TEXT("rama_seca"), 0, 1) };
		R.FellDrops = {
			MakeDrop(TEXT("tronco_pequeno"), 1, 2),
			MakeDrop(TEXT("madera_flotante"), 1, 1),
		};
		R.RegrowHours = 24.0f * 24.0f; // madera sin fruto: 24 días.
		Rules.Add(R);
	}
	{
		// Sotobosque: árbol joven y pequeño, se puede tumbar a mano.
		FHarvestSpeciesRule R;
		R.Species = FName(TEXT("Understory"));
		R.HitsBareHands = 5;
		R.HitsWithTool = 3;
		R.RequiredToolTag = FName(TEXT("Filo"));
		R.PerHitDrops = { MakeDrop(TEXT("palo_recto"), 0, 1) };
		R.FellDrops = {
			MakeDrop(TEXT("tronco_pequeno"), 1, 1),
			MakeDrop(TEXT("rama_seca"), 1, 2),
			MakeDrop(TEXT("hoja_platano"), 0, 2),
		};
		R.RegrowHours = 24.0f * 24.0f; // madera sin fruto: 24 días.
		Rules.Add(R);
	}
	{
		// Arbusto: un golpe basta, a mano o con cuchillo. Da corteza y liana.
		FHarvestSpeciesRule R;
		R.Species = FName(TEXT("Shrub"));
		R.HitsBareHands = 1;
		R.HitsWithTool = 1;
		R.RequiredToolTag = NAME_None;
		R.PerHitDrops = {};
		R.FellDrops = {
			MakeDrop(TEXT("corteza"), 0, 1),
			MakeDrop(TEXT("liana"), 0, 1),
			MakeDrop(TEXT("algodon_silvestre"), 0, 1),
		};
		R.RegrowHours = 4.0f * 24.0f; // arbustos: 4 días.
		Rules.Add(R);
	}
	{
		// Hierba: fibra rápida a mano, rebrota pronto.
		FHarvestSpeciesRule R;
		R.Species = FName(TEXT("Grass"));
		R.HitsBareHands = 1;
		R.HitsWithTool = 1;
		R.RequiredToolTag = NAME_None;
		R.PerHitDrops = {};
		R.FellDrops = {
			MakeDrop(TEXT("algodon_silvestre"), 1, 1),
			MakeDrop(TEXT("musgo"), 0, 1),
		};
		R.RegrowHours = 4.0f * 24.0f; // matas de hierba: 4 días (biblia 02 §1.2).
		Rules.Add(R);
	}
	{
		// Roca: picar con algo contundente. Se agota tras varios golpes y vuelve a aparecer mucho más tarde.
		FHarvestSpeciesRule R;
		R.Species = FName(TEXT("Rock"));
		R.HitsBareHands = 6;
		R.HitsWithTool = 3;
		R.RequiredToolTag = FName(TEXT("Contundente"));
		R.PerHitDrops = { MakeDrop(TEXT("canto_rodado"), 0, 1) };
		R.FellDrops = {
			MakeDrop(TEXT("piedra_plana"), 1, 1),
			MakeDrop(TEXT("pedernal"), 0, 1),
			MakeDrop(TEXT("lasca_tallada"), 1, 2),
		};
		R.RegrowHours = 720.0f; // 30 días: un afloramiento nuevo, no la misma roca.
		Rules.Add(R);
	}

	return Rules;
}

const FHarvestSpeciesRule* FHarvestModel::FindRule(const TArray<FHarvestSpeciesRule>& Rules, FName Species)
{
	for (const FHarvestSpeciesRule& Rule : Rules)
	{
		if (Rule.Species == Species)
		{
			return &Rule;
		}
	}
	return nullptr;
}

int32 FHarvestModel::HitsRequired(const FHarvestSpeciesRule& Rule, bool bHasRequiredTool)
{
	const bool bToolApplies = bHasRequiredTool && !Rule.RequiredToolTag.IsNone();
	return FMath::Max(1, bToolApplies ? Rule.HitsWithTool : Rule.HitsBareHands);
}

int32 FHarvestModel::ApplyHit(const FHarvestSpeciesRule& Rule, int32 CurrentHits, bool bHasRequiredTool, bool& bOutFelled)
{
	const int32 Required = HitsRequired(Rule, bHasRequiredTool);
	const int32 NewHits = FMath::Min(CurrentHits + 1, Required);
	bOutFelled = NewHits >= Required;
	return NewHits;
}

TArray<FHarvestDrop> FHarvestModel::RollDrops(const TArray<FHarvestDrop>& Drops, FExploredRandom& Random)
{
	TArray<FHarvestDrop> Rolled;
	Rolled.Reserve(Drops.Num());
	for (const FHarvestDrop& Drop : Drops)
	{
		const int32 Count = Drop.MaxCount > Drop.MinCount ? Random.RangeInt(Drop.MinCount, Drop.MaxCount) : Drop.MinCount;
		if (Count <= 0)
		{
			continue;
		}
		FHarvestDrop Result;
		Result.ItemId = Drop.ItemId;
		Result.MinCount = Count;
		Result.MaxCount = Count;
		Rolled.Add(Result);
	}
	return Rolled;
}
