#include "Crafting/RecipeBookModel.h"

bool CraftPlaceFromName(const FString& Name, ECraftPlace& Out)
{
	if (Name == TEXT("cualquier")) { Out = ECraftPlace::Anywhere; return true; }
	if (Name == TEXT("suelo")) { Out = ECraftPlace::Ground; return true; }
	if (Name == TEXT("hoguera")) { Out = ECraftPlace::Fire; return true; }
	if (Name == TEXT("sitio_balsa")) { Out = ECraftPlace::RaftSite; return true; }
	if (Name == TEXT("linterna")) { Out = ECraftPlace::Lantern; return true; }
	return false;
}

FName CraftPlaceToken(ECraftPlace Place)
{
	switch (Place)
	{
	case ECraftPlace::Fire: return FName(TEXT("hoguera"));
	case ECraftPlace::RaftSite: return FName(TEXT("sitio_balsa"));
	case ECraftPlace::Lantern: return FName(TEXT("linterna"));
	default: return NAME_None;
	}
}

const TCHAR* LexToString(ECraftResult Result)
{
	switch (Result)
	{
	case ECraftResult::Ok: return TEXT("Ok");
	case ECraftResult::NotValid: return TEXT("NotValid");
	case ECraftResult::MissingInputs: return TEXT("MissingInputs");
	case ECraftResult::WrongPlace: return TEXT("WrongPlace");
	case ECraftResult::StationNotReady: return TEXT("StationNotReady");
	case ECraftResult::Full: return TEXT("Full");
	default: return TEXT("?");
	}
}

const FRecipeDef* FRecipeBook::Find(FName Id) const
{
	return Recipes.FindByPredicate([Id](const FRecipeDef& Recipe) { return Recipe.Id == Id; });
}

namespace RecipeBookDetail
{
	FRecipeDef Make(const TCHAR* Id, const TCHAR* Es, const TCHAR* En, const TCHAR* Verb, std::initializer_list<FRecipeAmount> Inputs,
		ECraftPlace Place, float Seconds, const TCHAR* OutputItem, const TCHAR* OutputWorld)
	{
		FRecipeDef Recipe;
		Recipe.Id = Id;
		Recipe.NameEs = Es;
		Recipe.NameEn = En;
		Recipe.Verb = Verb;
		Recipe.Inputs = Inputs;
		Recipe.Place = Place;
		Recipe.Seconds = Seconds;
		Recipe.OutputItem = OutputItem ? FName(OutputItem) : NAME_None;
		Recipe.OutputCount = OutputItem ? 1 : 0;
		Recipe.OutputWorld = OutputWorld ? FName(OutputWorld) : NAME_None;
		return Recipe;
	}

	/** Cantidad con la que entra Item en la receta (entradas, herramientas, sitio o lo puesto en él). */
	int32 Involvement(const FRecipeDef& Recipe, FName Item)
	{
		int32 Total = 0;
		for (const FRecipeAmount& Input : Recipe.Inputs)
		{
			Total += Input.Item == Item ? Input.Count : 0;
		}
		for (const FRecipeAmount& Need : Recipe.StationNeeds)
		{
			Total += Need.Item == Item ? Need.Count : 0;
		}
		Total += Recipe.Tools.Contains(Item) ? 1 : 0;
		Total += (!Item.IsNone() && CraftPlaceToken(Recipe.Place) == Item) ? 1 : 0;
		return Total;
	}
}

FRecipeBook FRecipeBook::Default()
{
	using RecipeBookDetail::Make;
	FRecipeBook Book;
	for (const TCHAR* Verb : { TEXT("encender"), TEXT("trenzar"), TEXT("atar"), TEXT("cocinar"), TEXT("hervir"), TEXT("montar") })
	{
		Book.Verbs.Add(Verb);
	}
	FRecipeDef Campfire = Make(TEXT("hoguera"), TEXT("Hoguera"), TEXT("Campfire"), TEXT("encender"), { { TEXT("lena"), 3 } },
		ECraftPlace::Ground, 3.0f, nullptr, TEXT("hoguera"));
	Campfire.Tools.Add(TEXT("cuchillo"));
	Book.Recipes.Add(Campfire);
	Book.Recipes.Add(Make(TEXT("cuerda"), TEXT("Cuerda"), TEXT("Rope"), TEXT("trenzar"), { { TEXT("fibra"), 2 } },
		ECraftPlace::Anywhere, 4.0f, TEXT("cuerda"), nullptr));
	Book.Recipes.Add(Make(TEXT("cana_pescar"), TEXT("Caña de pescar"), TEXT("Fishing rod"), TEXT("atar"),
		{ { TEXT("cana_bambu"), 1 }, { TEXT("cuerda"), 1 } }, ECraftPlace::Anywhere, 3.0f, TEXT("cana_pescar"), nullptr));
	Book.Recipes.Add(Make(TEXT("pez_asado"), TEXT("Pez asado"), TEXT("Cooked fish"), TEXT("cocinar"), { { TEXT("pez_crudo"), 1 } },
		ECraftPlace::Fire, 20.0f, TEXT("pez_asado"), nullptr));
	Book.Recipes.Add(Make(TEXT("mecha"), TEXT("Mecha"), TEXT("Wick"), TEXT("trenzar"), { { TEXT("fibra"), 3 } },
		ECraftPlace::Anywhere, 6.0f, TEXT("mecha"), nullptr));
	Book.Recipes.Add(Make(TEXT("tarro_aceite"), TEXT("Tarro de aceite"), TEXT("Oil jar"), TEXT("hervir"),
		{ { TEXT("pez_crudo"), 2 }, { TEXT("coco_vacio"), 1 } }, ECraftPlace::Fire, 40.0f, TEXT("tarro_aceite"), nullptr));
	Book.Recipes.Add(Make(TEXT("remo"), TEXT("Remo"), TEXT("Paddle"), TEXT("atar"), { { TEXT("tronco"), 1 }, { TEXT("cuerda"), 1 } },
		ECraftPlace::Anywhere, 3.0f, TEXT("remo"), nullptr));
	FRecipeDef Raft = Make(TEXT("balsa"), TEXT("Balsa"), TEXT("Raft"), TEXT("atar"), { { TEXT("cuerda"), 2 } },
		ECraftPlace::RaftSite, 20.0f, nullptr, TEXT("balsa"));
	Raft.StationNeeds.Add({ TEXT("tronco"), 4 });
	Raft.Steps = 3;
	Book.Recipes.Add(Raft);
	FRecipeDef Lantern = Make(TEXT("linterna"), TEXT("Linterna montada"), TEXT("Mounted lantern"), TEXT("montar"),
		{ { TEXT("lente"), 1 }, { TEXT("tarro_aceite"), 2 }, { TEXT("mecha"), 1 } }, ECraftPlace::Lantern, 3.0f, nullptr,
		TEXT("linterna_montada"));
	Lantern.Steps = 3;
	Book.Recipes.Add(Lantern);
	FRecipeDef Beacon = Make(TEXT("baliza"), TEXT("Baliza encendida"), TEXT("Beacon lit"), TEXT("encender"), {},
		ECraftPlace::Lantern, 3.0f, nullptr, TEXT("final"));
	Beacon.StationNeeds.Add({ TEXT("linterna_montada"), 1 });
	Book.Recipes.Add(Beacon);
	return Book;
}

bool FRecipeBook::PlaceAllows(ECraftPlace Required, ECraftPlace Here)
{
	switch (Required)
	{
	case ECraftPlace::Anywhere: return true;
	case ECraftPlace::Ground: return Here == ECraftPlace::Ground || Here == ECraftPlace::Anywhere;
	default: return Required == Here;
	}
}

TArray<FName> FRecipeBook::FindOptions(FName A, FName B) const
{
	TArray<FName> Options;
	if (A.IsNone() || B.IsNone())
	{
		return Options;
	}
	for (const FRecipeDef& Recipe : Recipes)
	{
		const int32 WithA = RecipeBookDetail::Involvement(Recipe, A);
		const int32 WithB = RecipeBookDetail::Involvement(Recipe, B);
		const bool bMatches = A == B ? WithA >= 2 : (WithA > 0 && WithB > 0);
		if (bMatches)
		{
			Options.Add(Recipe.Id);
			if (Options.Num() == MaxOptions)
			{
				break;
			}
		}
	}
	return Options;
}

ECraftResult FRecipeBook::Check(const FRecipeDef& Recipe, const FBackpackState& Pack, ECraftPlace Here, const TMap<FName, int32>& Station)
{
	if (!PlaceAllows(Recipe.Place, Here))
	{
		return ECraftResult::WrongPlace;
	}
	for (const FRecipeAmount& Need : Recipe.StationNeeds)
	{
		if (Station.FindRef(Need.Item) < Need.Count)
		{
			return ECraftResult::StationNotReady;
		}
	}
	for (const FName& Tool : Recipe.Tools)
	{
		if (FBackpackModel::Count(Pack, Tool) <= 0)
		{
			return ECraftResult::MissingInputs;
		}
	}
	TMap<FName, int32> Needed;
	for (const FRecipeAmount& Input : Recipe.Inputs)
	{
		Needed.FindOrAdd(Input.Item) += Input.Count;
	}
	for (const TPair<FName, int32>& Pair : Needed)
	{
		if (FBackpackModel::Count(Pack, Pair.Key) < Pair.Value)
		{
			return ECraftResult::MissingInputs;
		}
	}
	return ECraftResult::Ok;
}

ECraftResult FRecipeBook::Apply(const FRecipeDef& Recipe, const FBackpackState& In, const FBackpackRules& Rules, ECraftPlace Here,
	const TMap<FName, int32>& Station, FBackpackState& Out)
{
	Out = In;
	const ECraftResult Checked = Check(Recipe, In, Here, Station);
	if (Checked != ECraftResult::Ok)
	{
		return Checked;
	}
	TMap<FName, int32> ToRemove;
	for (const FRecipeAmount& Input : Recipe.Inputs)
	{
		ToRemove.FindOrAdd(Input.Item) += Input.Count;
	}
	TMap<FName, int32> ToAdd;
	if (!Recipe.OutputItem.IsNone() && Recipe.OutputCount > 0)
	{
		ToAdd.Add(Recipe.OutputItem, Recipe.OutputCount);
	}
	const EBackpackResult Result = FBackpackModel::Exchange(In, Rules, ToRemove, ToAdd, Out);
	if (Result == EBackpackResult::Ok)
	{
		return ECraftResult::Ok;
	}
	Out = In;
	return Result == EBackpackResult::Full || Result == EBackpackResult::HandsBusy ? ECraftResult::Full : ECraftResult::NotValid;
}

bool FRecipeBook::Validate(FString& OutError) const
{
	TSet<FName> Ids;
	for (const FRecipeDef& Recipe : Recipes)
	{
		const bool bAlreadyThere = Ids.Contains(Recipe.Id);
		Ids.Add(Recipe.Id);
		if (Recipe.Id.IsNone() || bAlreadyThere)
		{
			OutError = FString::Printf(TEXT("Receta sin id o repetida: %s"), *Recipe.Id.ToString());
			return false;
		}
		if (!Verbs.Contains(Recipe.Verb))
		{
			OutError = FString::Printf(TEXT("Receta %s: verbo desconocido %s"), *Recipe.Id.ToString(), *Recipe.Verb.ToString());
			return false;
		}
		if (Recipe.Seconds <= 0.0f || Recipe.Steps < 1)
		{
			OutError = FString::Printf(TEXT("Receta %s: tiempo o pasos no positivos"), *Recipe.Id.ToString());
			return false;
		}
		for (const FRecipeAmount& Amount : Recipe.Inputs)
		{
			if (Amount.Item.IsNone() || Amount.Count <= 0)
			{
				OutError = FString::Printf(TEXT("Receta %s: entrada sin objeto o con cantidad no positiva"), *Recipe.Id.ToString());
				return false;
			}
		}
		const bool bItemOutput = !Recipe.OutputItem.IsNone() && Recipe.OutputCount > 0;
		if (bItemOutput == !Recipe.OutputWorld.IsNone())
		{
			OutError = FString::Printf(TEXT("Receta %s: necesita exactamente una salida (objeto o mundo)"), *Recipe.Id.ToString());
			return false;
		}
	}
	return true;
}
