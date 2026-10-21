#include "Misc/AutomationTest.h"

#include "Crafting/RecipeBookModel.h"
#include "Items/BackpackModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RecipeBookTest
{
	FBackpackState With(const FBackpackRules& Rules, std::initializer_list<TPair<const TCHAR*, int32>> Items)
	{
		FBackpackState S;
		for (const TPair<const TCHAR*, int32>& Pair : Items)
		{
			FBackpackModel::Add(S, Rules, FName(Pair.Key), Pair.Value, S);
		}
		return S;
	}
}

BEGIN_DEFINE_SPEC(FRecipeBookSpec, "Explored.RecipeBook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FBackpackRules Rules;
	FRecipeBook Book;
	TMap<FName, int32> NoStation;
END_DEFINE_SPEC(FRecipeBookSpec)

void FRecipeBookSpec::Define()
{
	using namespace RecipeBookTest;

	BeforeEach([this]()
	{
		Rules = FBackpackRules::Default();
		Book = FRecipeBook::Default();
	});

	Describe(TEXT("la lista cerrada del GDD §5.6"), [this]()
	{
		It(TEXT("tiene 10 recetas, 6 verbos y es coherente"), [this]()
		{
			FString Error;
			TestTrue(TEXT("Valida"), Book.Validate(Error));
			TestEqual(TEXT("Recetas"), Book.Recipes.Num(), 10);
			TestEqual(TEXT("Verbos"), Book.Verbs.Num(), 6);
		});

		It(TEXT("las salidas a la mochila son objetos de las reglas"), [this]()
		{
			for (const FRecipeDef& Recipe : Book.Recipes)
			{
				if (!Recipe.OutputItem.IsNone())
				{
					TestNotNull(*Recipe.Id.ToString(), Rules.Find(Recipe.OutputItem));
				}
				for (const FRecipeAmount& Input : Recipe.Inputs)
				{
					TestNotNull(*Input.Item.ToString(), Rules.Find(Input.Item));
				}
			}
		});

		It(TEXT("una receta con un verbo inventado no valida"), [this]()
		{
			FRecipeBook Broken = Book;
			Broken.Recipes[0].Verb = TEXT("golpear");
			FString Error;
			TestFalse(TEXT("Inválida"), Broken.Validate(Error));
		});

		It(TEXT("una receta con dos salidas no valida"), [this]()
		{
			FRecipeBook Broken = Book;
			Broken.Recipes[1].OutputWorld = TEXT("balsa");
			FString Error;
			TestFalse(TEXT("Inválida"), Broken.Validate(Error));
		});
	});

	Describe(TEXT("las opciones al combinar"), [this]()
	{
		It(TEXT("fibra sobre fibra ofrece cuerda y mecha"), [this]()
		{
			const TArray<FName> Options = Book.FindOptions(TEXT("fibra"), TEXT("fibra"));
			TestEqual(TEXT("Dos"), Options.Num(), 2);
			TestTrue(TEXT("Cuerda"), Options.Contains(FName(TEXT("cuerda"))));
			TestTrue(TEXT("Mecha"), Options.Contains(FName(TEXT("mecha"))));
		});

		It(TEXT("leña sobre el cuchillo ofrece la hoguera"), [this]()
		{
			const TArray<FName> Options = Book.FindOptions(TEXT("lena"), TEXT("cuchillo"));
			TestEqual(TEXT("Una"), Options.Num(), 1);
			TestEqual(TEXT("Hoguera"), Options[0], FName(TEXT("hoguera")));
		});

		It(TEXT("un pez sobre la hoguera ofrece asar y hervir; pez sobre pez solo hervir"), [this]()
		{
			const TArray<FName> OnFire = Book.FindOptions(TEXT("pez_crudo"), TEXT("hoguera"));
			TestEqual(TEXT("Dos"), OnFire.Num(), 2);
			const TArray<FName> TwoFish = Book.FindOptions(TEXT("pez_crudo"), TEXT("pez_crudo"));
			TestEqual(TEXT("Una"), TwoFish.Num(), 1);
			TestEqual(TEXT("Aceite"), TwoFish[0], FName(TEXT("tarro_aceite")));
		});

		It(TEXT("lo que no es receta no ofrece nada (No sirve así)"), [this]()
		{
			TestEqual(TEXT("Coco y cuerda"), Book.FindOptions(TEXT("coco"), TEXT("cuerda")).Num(), 0);
			TestEqual(TEXT("Cuchillo consigo mismo"), Book.FindOptions(TEXT("cuchillo"), TEXT("cuchillo")).Num(), 0);
			TestEqual(TEXT("Nada"), Book.FindOptions(NAME_None, TEXT("fibra")).Num(), 0);
		});

		It(TEXT("nunca ofrece más de 3"), [this]()
		{
			FRecipeBook Many = Book;
			for (int32 I = 0; I < 4; ++I)
			{
				FRecipeDef Copy = *Book.Find(TEXT("cuerda"));
				Copy.Id = FName(*FString::Printf(TEXT("cuerda_%d"), I));
				Many.Recipes.Add(Copy);
			}
			TestEqual(TEXT("Tope"), Many.FindOptions(TEXT("fibra"), TEXT("fibra")).Num(), FRecipeBook::MaxOptions);
		});
	});

	Describe(TEXT("aplicar recetas"), [this]()
	{
		It(TEXT("trenzar 2 fibras da 1 cuerda"), [this]()
		{
			const FBackpackState In = With(Rules, { { TEXT("fibra"), 3 } });
			FBackpackState Out;
			TestEqual(TEXT("Ok"), FRecipeBook::Apply(*Book.Find(TEXT("cuerda")), In, Rules, ECraftPlace::Anywhere, NoStation, Out), ECraftResult::Ok);
			TestEqual(TEXT("Cuerda"), FBackpackModel::Count(Out, TEXT("cuerda")), 1);
			TestEqual(TEXT("Queda 1 fibra"), FBackpackModel::Count(Out, TEXT("fibra")), 1);
			TestEqual(TEXT("La entrada no cambia"), FBackpackModel::Count(In, TEXT("fibra")), 3);
		});

		It(TEXT("la hoguera gasta 3 leñas y no el cuchillo"), [this]()
		{
			const FBackpackState In = With(Rules, { { TEXT("cuchillo"), 1 }, { TEXT("lena"), 4 } });
			FBackpackState Out;
			TestEqual(TEXT("Ok"), FRecipeBook::Apply(*Book.Find(TEXT("hoguera")), In, Rules, ECraftPlace::Ground, NoStation, Out), ECraftResult::Ok);
			TestEqual(TEXT("Leña"), FBackpackModel::Count(Out, TEXT("lena")), 1);
			TestEqual(TEXT("Cuchillo"), FBackpackModel::Count(Out, TEXT("cuchillo")), 1);
		});

		It(TEXT("sin cuchillo no hay hoguera"), [this]()
		{
			const FBackpackState In = With(Rules, { { TEXT("lena"), 3 } });
			FBackpackState Out;
			TestEqual(TEXT("Falta"), FRecipeBook::Apply(*Book.Find(TEXT("hoguera")), In, Rules, ECraftPlace::Ground, NoStation, Out), ECraftResult::MissingInputs);
			TestEqual(TEXT("Leña intacta"), FBackpackModel::Count(Out, TEXT("lena")), 3);
		});

		It(TEXT("asar y hervir piden estar junto a la hoguera"), [this]()
		{
			const FBackpackState In = With(Rules, { { TEXT("pez_crudo"), 2 }, { TEXT("coco_vacio"), 1 } });
			FBackpackState Out;
			TestEqual(TEXT("Asar lejos"), FRecipeBook::Apply(*Book.Find(TEXT("pez_asado")), In, Rules, ECraftPlace::Anywhere, NoStation, Out), ECraftResult::WrongPlace);
			TestEqual(TEXT("Hervir en la hoguera"), FRecipeBook::Apply(*Book.Find(TEXT("tarro_aceite")), In, Rules, ECraftPlace::Fire, NoStation, Out), ECraftResult::Ok);
			TestEqual(TEXT("Tarro"), FBackpackModel::Count(Out, TEXT("tarro_aceite")), 1);
			TestEqual(TEXT("Sin peces"), FBackpackModel::Count(Out, TEXT("pez_crudo")), 0);
		});

		It(TEXT("el remo usa el tronco que se lleva en las manos"), [this]()
		{
			const FBackpackState In = With(Rules, { { TEXT("tronco"), 1 }, { TEXT("cuerda"), 1 } });
			FBackpackState Out;
			TestEqual(TEXT("Ok"), FRecipeBook::Apply(*Book.Find(TEXT("remo")), In, Rules, ECraftPlace::Anywhere, NoStation, Out), ECraftResult::Ok);
			TestTrue(TEXT("Manos libres"), Out.HeldHeavy.IsNone());
			TestEqual(TEXT("Remo"), FBackpackModel::Count(Out, TEXT("remo")), 1);
		});

		It(TEXT("la balsa pide 4 troncos puestos en el sitio"), [this]()
		{
			const FBackpackState In = With(Rules, { { TEXT("cuerda"), 2 } });
			FBackpackState Out;
			TMap<FName, int32> Site;
			Site.Add(TEXT("tronco"), 3);
			TestEqual(TEXT("Con 3"), FRecipeBook::Apply(*Book.Find(TEXT("balsa")), In, Rules, ECraftPlace::RaftSite, Site, Out), ECraftResult::StationNotReady);
			Site.Add(TEXT("tronco"), 4);
			TestEqual(TEXT("Con 4"), FRecipeBook::Apply(*Book.Find(TEXT("balsa")), In, Rules, ECraftPlace::RaftSite, Site, Out), ECraftResult::Ok);
			TestEqual(TEXT("Cuerdas gastadas"), FBackpackModel::Count(Out, TEXT("cuerda")), 0);
		});

		It(TEXT("si la salida no cabe no gasta nada (No te cabe más)"), [this]()
		{
			FBackpackState In;
			for (int32 I = 0; I < 7; ++I)
			{
				FBackpackModel::Add(In, Rules, TEXT("cuerda"), 4, In);
			}
			FBackpackModel::Add(In, Rules, TEXT("fibra"), 4, In);
			FBackpackState Out;
			TestEqual(TEXT("Llena"), FRecipeBook::Apply(*Book.Find(TEXT("mecha")), In, Rules, ECraftPlace::Anywhere, NoStation, Out), ECraftResult::Full);
			TestEqual(TEXT("Fibras intactas"), FBackpackModel::Count(Out, TEXT("fibra")), 4);
		});
	});
}

#endif
