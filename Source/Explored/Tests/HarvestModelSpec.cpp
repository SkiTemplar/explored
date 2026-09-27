#include "Misc/AutomationTest.h"

#include "WorldGen/HarvestModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FHarvestModelSpec, "Explored.Harvest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FHarvestModelSpec)

void FHarvestModelSpec::Define()
{
	Describe("las reglas por defecto", [this]()
	{
		It("traen una regla para cada especie de FVegetationScatter::DefaultRules", [this]()
		{
			const TArray<FHarvestSpeciesRule> Rules = FHarvestModel::DefaultRules();
			for (const TCHAR* Species : { TEXT("Palm"), TEXT("JungleGiant"), TEXT("JungleWide"),
				TEXT("Mangrove"), TEXT("Understory"), TEXT("Shrub"), TEXT("Grass"), TEXT("Rock") })
			{
				TestNotNull(*FString::Printf(TEXT("hay regla para %s"), Species), FHarvestModel::FindRule(Rules, FName(Species)));
			}
		});

		It("no tienen una especie repetida", [this]()
		{
			const TArray<FHarvestSpeciesRule> Rules = FHarvestModel::DefaultRules();
			TSet<FName> Seen;
			for (const FHarvestSpeciesRule& Rule : Rules)
			{
				TestFalse(TEXT("especie repetida"), Seen.Contains(Rule.Species));
				Seen.Add(Rule.Species);
			}
		});
	});

	Describe("HitsRequired", [this]()
	{
		It("exige menos golpes con la herramienta adecuada que a mano", [this]()
		{
			const TArray<FHarvestSpeciesRule> Rules = FHarvestModel::DefaultRules();
			const FHarvestSpeciesRule* Palm = FHarvestModel::FindRule(Rules, FName(TEXT("Palm")));
			TestNotNull(TEXT("Palm existe"), Palm);
			if (Palm)
			{
				TestTrue(TEXT("con hacha hacen falta menos golpes"), FHarvestModel::HitsRequired(*Palm, true) < FHarvestModel::HitsRequired(*Palm, false));
			}
		});

		It("ignora la herramienta si la regla no exige ninguna (hierba)", [this]()
		{
			const TArray<FHarvestSpeciesRule> Rules = FHarvestModel::DefaultRules();
			const FHarvestSpeciesRule* Grass = FHarvestModel::FindRule(Rules, FName(TEXT("Grass")));
			TestNotNull(TEXT("Grass existe"), Grass);
			if (Grass)
			{
				TestEqual(TEXT("mismos golpes con o sin herramienta"),
					FHarvestModel::HitsRequired(*Grass, true), FHarvestModel::HitsRequired(*Grass, false));
			}
		});
	});

	Describe("ApplyHit", [this]()
	{
		It("tala en el último golpe y no antes", [this]()
		{
			FHarvestSpeciesRule Rule;
			Rule.HitsBareHands = 3;
			Rule.HitsWithTool = 3;

			int32 Hits = 0;
			bool bFelled = false;
			Hits = FHarvestModel::ApplyHit(Rule, Hits, false, bFelled);
			TestFalse(TEXT("golpe 1: no tala"), bFelled);
			Hits = FHarvestModel::ApplyHit(Rule, Hits, false, bFelled);
			TestFalse(TEXT("golpe 2: no tala"), bFelled);
			Hits = FHarvestModel::ApplyHit(Rule, Hits, false, bFelled);
			TestTrue(TEXT("golpe 3: tala"), bFelled);
			TestEqual(TEXT("golpes acumulados"), Hits, 3);
		});

		It("no se pasa del umbral aunque se siga golpeando", [this]()
		{
			FHarvestSpeciesRule Rule;
			Rule.HitsBareHands = 1;
			Rule.HitsWithTool = 1;
			bool bFelled = false;
			int32 Hits = FHarvestModel::ApplyHit(Rule, 0, false, bFelled);
			Hits = FHarvestModel::ApplyHit(Rule, Hits, false, bFelled);
			TestEqual(TEXT("saturado en el umbral"), Hits, 1);
		});
	});

	Describe("RollDrops", [this]()
	{
		It("nunca tira menos de MinCount ni más de MaxCount", [this]()
		{
			TArray<FHarvestDrop> Drops;
			FHarvestDrop Drop;
			Drop.ItemId = FName(TEXT("coco_maduro"));
			Drop.MinCount = 1;
			Drop.MaxCount = 3;
			Drops.Add(Drop);

			FExploredRandom Random(1234);
			for (int32 Attempt = 0; Attempt < 50; ++Attempt)
			{
				const TArray<FHarvestDrop> Rolled = FHarvestModel::RollDrops(Drops, Random);
				TestEqual(TEXT("un objeto tirado"), Rolled.Num(), 1);
				if (Rolled.Num() == 1)
				{
					TestTrue(TEXT("dentro del rango"), Rolled[0].MinCount >= 1 && Rolled[0].MinCount <= 3);
				}
			}
		});

		It("omite lo que sale con MinCount y MaxCount a 0 (drop opcional que no ha tocado)", [this]()
		{
			TArray<FHarvestDrop> Drops;
			FHarvestDrop Drop;
			Drop.ItemId = FName(TEXT("fibra_coco"));
			Drop.MinCount = 0;
			Drop.MaxCount = 0;
			Drops.Add(Drop);

			FExploredRandom Random(7);
			const TArray<FHarvestDrop> Rolled = FHarvestModel::RollDrops(Drops, Random);
			TestEqual(TEXT("nada que soltar"), Rolled.Num(), 0);
		});
	});
}

#endif
