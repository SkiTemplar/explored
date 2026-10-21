#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Items/IslandDataLoader.h"
#include "Items/ItemRegistrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ItemsSpecDetail
{
	FString ReadData(const TCHAR* File)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), File));
		return Text;
	}
}

/**
 * Los datos reales de Content/Data (GDD v3 §5 y §7) se leen sin errores y coinciden con los
 * valores por defecto de los modelos puros (los que usan los Specs del host).
 */
BEGIN_DEFINE_SPEC(FItemsSpec, "Explored.Items",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FIslandData Loaded;
	TArray<FString> Errors;
END_DEFINE_SPEC(FItemsSpec)

void FItemsSpec::Define()
{
	using namespace ItemsSpecDetail;

	BeforeEach([this]()
	{
		Loaded = FIslandData::Defaults();
		Errors = FIslandData::ParseInto(Loaded, ReadData(TEXT("items.json")), ReadData(TEXT("recipes.json")), ReadData(TEXT("verbs.json")),
			ReadData(TEXT("fuels.json")), ReadData(TEXT("survival_needs.json")), ReadData(TEXT("fish.json")));
	});

	It(TEXT("lee todos los ficheros sin errores"), [this]()
	{
		for (const FString& Error : Errors)
		{
			AddError(Error);
		}
		TestEqual(TEXT("Nombres de objetos"), Loaded.ItemNames.Num(), 17);
	});

	It(TEXT("items.json tiene las pilas de FBackpackRules::Default"), [this]()
	{
		const FBackpackRules Defaults = FBackpackRules::Default();
		TestEqual(TEXT("Objetos"), Loaded.Backpack.Items.Num(), Defaults.Items.Num());
		TestEqual(TEXT("Huecos"), Loaded.Backpack.Slots, Defaults.Slots);
		for (const TPair<FName, FBackpackItemRule>& Pair : Defaults.Items)
		{
			const FBackpackItemRule* Rule = Loaded.Backpack.Find(Pair.Key);
			if (TestNotNull(*Pair.Key.ToString(), Rule))
			{
				TestEqual(*FString::Printf(TEXT("Pila de %s"), *Pair.Key.ToString()), Rule->Stack, Pair.Value.Stack);
				TestEqual(*FString::Printf(TEXT("Pesado %s"), *Pair.Key.ToString()), Rule->bHeavy, Pair.Value.bHeavy);
			}
		}
	});

	It(TEXT("recipes.json y verbs.json son las 10 recetas y 6 verbos de FRecipeBook::Default"), [this]()
	{
		const FRecipeBook Defaults = FRecipeBook::Default();
		TestEqual(TEXT("Verbos"), Loaded.Recipes.Verbs, Defaults.Verbs);
		TestEqual(TEXT("Recetas"), Loaded.Recipes.Recipes.Num(), Defaults.Recipes.Num());
		for (const FRecipeDef& Expected : Defaults.Recipes)
		{
			const FRecipeDef* Recipe = Loaded.Recipes.Find(Expected.Id);
			if (!TestNotNull(*Expected.Id.ToString(), Recipe))
			{
				continue;
			}
			const FString Id = Expected.Id.ToString();
			TestEqual(*(Id + TEXT(" verbo")), Recipe->Verb, Expected.Verb);
			TestEqual(*(Id + TEXT(" lugar")), static_cast<int32>(Recipe->Place), static_cast<int32>(Expected.Place));
			TestEqual(*(Id + TEXT(" segundos")), Recipe->Seconds, Expected.Seconds);
			TestEqual(*(Id + TEXT(" pasos")), Recipe->Steps, Expected.Steps);
			TestEqual(*(Id + TEXT(" entradas")), Recipe->Inputs.Num(), Expected.Inputs.Num());
			TestEqual(*(Id + TEXT(" salida")), Recipe->OutputItem, Expected.OutputItem);
			TestEqual(*(Id + TEXT(" mundo")), Recipe->OutputWorld, Expected.OutputWorld);
		}
	});

	It(TEXT("fuels.json, survival_needs.json y fish.json coinciden con los modelos"), [this]()
	{
		const FCampfireConfig Fire = FCampfireConfig::Default();
		TestEqual(TEXT("Arde"), Loaded.Campfire.BurnSeconds, Fire.BurnSeconds);
		TestEqual(TEXT("Leña"), Loaded.Campfire.FuelSeconds.FindRef(TEXT("lena")), Fire.FuelSeconds.FindRef(TEXT("lena")));
		TestEqual(TEXT("Radio"), Loaded.Campfire.HeatRadiusMeters, Fire.HeatRadiusMeters);
		const FConsumableEffect* Coconut = Loaded.Consumables.Find(TEXT("coco"));
		if (TestNotNull(TEXT("Coco"), Coconut))
		{
			TestEqual(TEXT("Sed del coco"), Coconut->Thirst, FIslandConsumables::CoconutThirst);
			TestEqual(TEXT("Deja coco vacío"), Coconut->Leaves, FName(TEXT("coco_vacio")));
		}
		const FConsumableEffect* Canteen = Loaded.Consumables.Find(TEXT("cantimplora"));
		TestTrue(TEXT("La cantimplora va a sorbos"), Canteen && Canteen->bSips);
		TestEqual(TEXT("Espera media"), Loaded.Fishing.MeanBiteSeconds, FCastFishingConfig::DefaultMeanBiteSeconds);
		TestEqual(TEXT("Captura"), Loaded.Fishing.Catch, FName(TEXT("pez_crudo")));
	});

	It(TEXT("el registro de objetos de UE sigue leyendo items.json"), [this]()
	{
		TMap<FName, FItemDefinition> Items;
		FString Error;
		TestTrue(TEXT("Parsea"), UItemRegistrySubsystem::ParseItemsJson(ReadData(TEXT("items.json")), Items, Error));
		TestEqual(TEXT("17 objetos"), Items.Num(), 17);
		TestEqual(TEXT("El tronco va a dos manos"), Items.FindRef(TEXT("tronco")).Size, EItemSize::DosManos);
	});

	It(TEXT("un recipes.json roto deja las recetas por defecto y lo informa"), [this]()
	{
		FIslandData Data = FIslandData::Defaults();
		const TArray<FString> Broken = FIslandData::ParseInto(Data, FString(), TEXT("{ \"recipes\": [ { \"id\": \"x\" } ] }"),
			ReadData(TEXT("verbs.json")), FString(), FString(), FString());
		TestTrue(TEXT("Avisa"), Broken.Num() > 0);
		TestEqual(TEXT("Siguen las 10"), Data.Recipes.Recipes.Num(), 10);
	});
}

#endif
