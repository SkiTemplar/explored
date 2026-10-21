#include "Misc/AutomationTest.h"

#include "Cooking/CampfireModel.h"
#include "Cooking/FireActionsModel.h"
#include "Crafting/RecipeBookModel.h"
#include "Interaction/VerbChoiceModel.h"
#include "Items/BackpackModel.h"
#include "Items/IslandConsumablesModel.h"
#include "UI/BackpackMenuModel.h"
#include "UI/IslandHudModel.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Lo que un jugador humano puede hacer con el teclado y el ratón en Baliza Siete: elegir el verbo de
 * la hoguera (hervir aceite con peces de sobra para asar), usar la ranura rápida y el inventario
 * (comer, beber, trenzar, atar). Modelos puros: lo que validan AExploredFire y AExploredCharacter.
 */
BEGIN_DEFINE_SPEC(FIslandInputSpec, "Explored.IslandInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FBackpackRules Rules;
	FRecipeBook Book;
	FIslandConsumables Consumables;
	FCampfireConfig Config;

	struct FItemCount
	{
		const TCHAR* Id;
		int32 Count;
	};

	FBackpackState Pack(std::initializer_list<FItemCount> Items) const
	{
		FBackpackState State = FBackpackModel::StarterKit(Rules);
		for (const FItemCount& Item : Items)
		{
			FBackpackState Next;
			if (FBackpackModel::Add(State, Rules, FName(Item.Id), Item.Count, Next) == EBackpackResult::Ok)
			{
				State = Next;
			}
		}
		return State;
	}
END_DEFINE_SPEC(FIslandInputSpec)

void FIslandInputSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = FBackpackRules::Default();
		Book = FRecipeBook::Default();
		Consumables = FIslandConsumables::Default();
		Config = FCampfireConfig::Default();
	});

	Describe(TEXT("elección de verbo"), [this]()
	{
		const TArray<FName> Three = { TEXT("cocinar"), TEXT("hervir"), TEXT("avivar") };

		It(TEXT("sin elección ejecuta el primero; la elegida se respeta mientras siga disponible"), [this, Three]()
		{
			TestEqual(TEXT("Primero"), FVerbChoiceModel::Resolve(Three, NAME_None), FName(TEXT("cocinar")));
			TestEqual(TEXT("Elegida"), FVerbChoiceModel::Resolve(Three, TEXT("hervir")), FName(TEXT("hervir")));
			TestEqual(TEXT("Ya no vale"), FVerbChoiceModel::Resolve({ TEXT("avivar") }, TEXT("hervir")), FName(TEXT("avivar")));
			TestTrue(TEXT("Nada"), FVerbChoiceModel::Resolve({}, TEXT("hervir")).IsNone());
		});

		It(TEXT("la tecla de combinar recorre los verbos y vuelve al principio"), [this, Three]()
		{
			TestEqual(TEXT("cocinar → hervir"), FVerbChoiceModel::Next(Three, TEXT("cocinar")), FName(TEXT("hervir")));
			TestEqual(TEXT("avivar → cocinar"), FVerbChoiceModel::Next(Three, TEXT("avivar")), FName(TEXT("cocinar")));
			TestEqual(TEXT("Desconocido → primero"), FVerbChoiceModel::Next(Three, TEXT("montar")), FName(TEXT("cocinar")));
			TestTrue(TEXT("Vacío"), FVerbChoiceModel::Next({}, NAME_None).IsNone());
		});

		It(TEXT("el aviso pone primero el que ejecuta E, como mucho 3"), [this, Three]()
		{
			const TArray<FName> Ordered = FVerbChoiceModel::Ordered(Three, TEXT("hervir"));
			TestEqual(TEXT("3"), Ordered.Num(), 3);
			TestEqual(TEXT("Primero"), Ordered[0], FName(TEXT("hervir")));
			TestEqual(TEXT("Siguiente"), Ordered[1], FName(TEXT("avivar")));
			const TArray<FName> Four = { TEXT("recoger"), TEXT("cocinar"), TEXT("hervir"), TEXT("avivar") };
			TestEqual(TEXT("Tope"), FVerbChoiceModel::Ordered(Four, NAME_None).Num(), FVerbChoiceModel::MaxVerbs);
		});
	});

	Describe(TEXT("hoguera"), [this]()
	{
		It(TEXT("con 2 peces y un coco vacío se puede hervir aceite eligiendo el verbo (no solo asar)"), [this]()
		{
			const FCampfireState Lit = FCampfireModel::Ignite(FCampfireState(), Config);
			const FBackpackState Oil = Pack({ { TEXT("pez_crudo"), 2 }, { TEXT("coco_vacio"), 1 }, { TEXT("lena"), 1 } });
			const TArray<FName> Actions = FFireActionsModel::Available(Lit, Oil, Book, Config);
			TestTrue(TEXT("Cocinar"), Actions.Contains(FFireActionsModel::Cook()));
			TestTrue(TEXT("Hervir"), Actions.Contains(FFireActionsModel::Boil()));
			TestTrue(TEXT("Avivar"), Actions.Contains(FFireActionsModel::Feed()));
			// El fallo de F3: la E solo hacía Actions[0] (cocinar). Con la tecla de combinar se llega a hervir.
			FName Chosen = FVerbChoiceModel::Resolve(Actions, NAME_None);
			TestEqual(TEXT("Sin elegir, asa"), Chosen, FFireActionsModel::Cook());
			Chosen = FVerbChoiceModel::Next(Actions, Chosen);
			TestEqual(TEXT("C una vez: hervir"), FVerbChoiceModel::Resolve(Actions, Chosen), FFireActionsModel::Boil());
		});

		It(TEXT("apagada solo ofrece encender, y solo con 3 leñas y el cuchillo"), [this]()
		{
			TestEqual(TEXT("Sin leña"), FFireActionsModel::Available(FCampfireState(), Pack({}), Book, Config).Num(), 0);
			const TArray<FName> Actions = FFireActionsModel::Available(FCampfireState(), Pack({ { TEXT("lena"), 3 } }), Book, Config);
			TestEqual(TEXT("Una"), Actions.Num(), 1);
			TestEqual(TEXT("Encender"), Actions.Num() > 0 ? Actions[0] : FName(), FFireActionsModel::Light());
		});

		It(TEXT("con un trabajo en marcha no se cocina otra cosa"), [this]()
		{
			FCampfireState Busy = FCampfireModel::Ignite(FCampfireState(), Config);
			Busy.Job.RecipeId = FFireActionsModel::GrillRecipe();
			Busy.Job.RemainingSeconds = 10.0f;
			const TArray<FName> Actions = FFireActionsModel::Available(Busy, Pack({ { TEXT("pez_crudo"), 2 }, { TEXT("coco_vacio"), 1 } }), Book, Config);
			TestFalse(TEXT("Sin cocinar"), Actions.Contains(FFireActionsModel::Cook()));
			TestFalse(TEXT("Sin hervir"), Actions.Contains(FFireActionsModel::Boil()));
		});
	});

	Describe(TEXT("aviso con elección"), [this]()
	{
		It(TEXT("«E · Hervir   C · Añadir leña» y, con un verbo, como el aviso normal"), [this]()
		{
			TestEqual(TEXT("Dos"), FIslandHudModel::ChoicePromptText(TEXT("E"), TEXT("C"), { TEXT("Hervir"), TEXT("Añadir leña") }),
				FString(TEXT("E · Hervir   C · Añadir leña")));
			TestEqual(TEXT("Uno"), FIslandHudModel::ChoicePromptText(TEXT("E"), TEXT("C"), { TEXT("Cocinar") }), FString(TEXT("E · Cocinar")));
			TestEqual(TEXT("Sin tecla de cambio"), FIslandHudModel::ChoicePromptText(TEXT("E"), FString(), { TEXT("Cocinar"), TEXT("Hervir") }),
				FString(TEXT("E · Cocinar")));
			TestTrue(TEXT("Nada"), FIslandHudModel::ChoicePromptText(TEXT("E"), TEXT("C"), {}).IsEmpty());
		});
	});

	Describe(TEXT("inventario y ranura rápida"), [this]()
	{
		It(TEXT("el coco y el pescado se usan; el kit no se suelta; la cantimplora vacía no se usa"), [this]()
		{
			FBackpackState State = Pack({ { TEXT("coco"), 1 }, { TEXT("pez_asado"), 1 }, { TEXT("fibra"), 2 } });
			const TArray<FBackpackMenuEntry> Entries = FBackpackMenuModel::Entries(State, Consumables);
			const FBackpackMenuEntry* Coco = Entries.FindByPredicate([](const FBackpackMenuEntry& E) { return E.ItemId == FName(TEXT("coco")); });
			const FBackpackMenuEntry* Fibra = Entries.FindByPredicate([](const FBackpackMenuEntry& E) { return E.ItemId == FName(TEXT("fibra")); });
			const FBackpackMenuEntry* Knife = Entries.FindByPredicate([](const FBackpackMenuEntry& E) { return E.ItemId == FName(TEXT("cuchillo")); });
			const FBackpackMenuEntry* Canteen = Entries.FindByPredicate([](const FBackpackMenuEntry& E) { return E.ItemId == FBackpackModel::CanteenId(); });
			if (!TestTrue(TEXT("Filas"), Coco && Fibra && Knife && Canteen))
			{
				return;
			}
			TestTrue(TEXT("Coco se bebe"), Coco->bCanUse && Coco->bCanDrop);
			TestFalse(TEXT("Fibra no se come"), Fibra->bCanUse);
			TestEqual(TEXT("Fibra ×2"), Fibra->Count, 2);
			TestFalse(TEXT("Cuchillo del kit"), Knife->bCanDrop);
			TestEqual(TEXT("Sorbos"), Canteen->Sips, FBackpackModel::StarterCanteenSips);
			TestTrue(TEXT("Con sorbos"), Canteen->bCanUse);
			TestTrue(TEXT("Pez asado"), FBackpackMenuModel::CanUse(State, Consumables, TEXT("pez_asado")));
			State.CanteenSips = 0;
			TestFalse(TEXT("Cantimplora vacía"), FBackpackMenuModel::CanUse(State, Consumables, FBackpackModel::CanteenId()));
			TestFalse(TEXT("Lo que no lleva"), FBackpackMenuModel::CanUse(State, Consumables, TEXT("pez_crudo")));
		});

		It(TEXT("recetas de mano: cuerda con 2 fibras; las de hoguera, balsa o faro no salen"), [this]()
		{
			const FBackpackState State = Pack({ { TEXT("fibra"), 2 } });
			const TArray<FBackpackMenuRecipe> Recipes = FBackpackMenuModel::HandRecipes(Book, State);
			TestTrue(TEXT("Hay recetas"), Recipes.Num() >= 4);
			for (const FBackpackMenuRecipe& Recipe : Recipes)
			{
				const FRecipeDef* Def = Book.Find(Recipe.RecipeId);
				TestTrue(TEXT("De mano"), Def && (Def->Place == ECraftPlace::Anywhere || Def->Place == ECraftPlace::Ground));
			}
			TestTrue(TEXT("Cuerda"), FBackpackMenuModel::CanCraft(Book, State, TEXT("cuerda")));
			TestFalse(TEXT("Mecha (3 fibras)"), FBackpackMenuModel::CanCraft(Book, State, TEXT("mecha")));
			TestFalse(TEXT("Aceite es de hoguera"),
				FBackpackMenuModel::CanCraft(Book, Pack({ { TEXT("pez_crudo"), 2 }, { TEXT("coco_vacio"), 1 } }), FFireActionsModel::OilRecipe()));
			TestFalse(TEXT("Desconocida"), FBackpackMenuModel::CanCraft(Book, State, TEXT("nada")));
		});

		It(TEXT("el clic bebe de la cantimplora en su ranura y no hace nada con el cuchillo o con carga pesada"), [this]()
		{
			FBackpackState State = Pack({});
			const int32 CanteenSlot = State.Stacks.IndexOfByPredicate([](const FBackpackStack& S) { return S.Id == FBackpackModel::CanteenId(); });
			const int32 KnifeSlot = State.Stacks.IndexOfByPredicate([](const FBackpackStack& S) { return S.Id == FName(TEXT("cuchillo")); });
			TestEqual(TEXT("Cantimplora"), FBackpackMenuModel::QuickUseItem(State, CanteenSlot, Consumables), FBackpackModel::CanteenId());
			TestTrue(TEXT("Cuchillo"), FBackpackMenuModel::QuickUseItem(State, KnifeSlot, Consumables).IsNone());
			TestTrue(TEXT("Fuera de rango"), FBackpackMenuModel::QuickUseItem(State, 7, Consumables).IsNone());
			State.HeldHeavy = TEXT("tronco");
			TestTrue(TEXT("Manos ocupadas"), FBackpackMenuModel::QuickUseItem(State, CanteenSlot, Consumables).IsNone());
		});
	});
}

#endif
