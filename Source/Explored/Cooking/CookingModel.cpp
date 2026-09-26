#include "Cooking/CookingModel.h"

namespace CookingModelDetail
{
	/** Humo mínimo para que el ahumadero haga algo y humo al que ahúma a ritmo 1. */
	constexpr float MinSmoke = 0.1f;
	constexpr float ReferenceSmoke = 0.4f;
	constexpr float MaxRate = 1.5f;

	/** Etiquetas del ingrediente: las que trae o, si no trae, las de su comida. */
	const TArray<FName>& TagsOf(const FCookingData& Data, const FCookIngredient& In)
	{
		static const TArray<FName> NoTags;
		if (In.Tags.Num() > 0)
		{
			return In.Tags;
		}
		const FFoodDef* Food = Data.FindFood(In.ItemId);
		return Food ? Food->Tags : NoTags;
	}

	/**
	 * Por id casa siempre; por etiqueta solo si el ingrediente está crudo: lo
	 * ya preparado (pescado asado, fruta seca) no vuelve a entrar en una receta
	 * genérica de «pescado» o «fruta».
	 */
	bool ReqAccepts(const FCookingData& Data, const FCookIngredientReq& Req, const FCookIngredient& In)
	{
		if (!Req.ItemId.IsNone())
		{
			return Req.ItemId == In.ItemId;
		}
		if (Req.Tag.IsNone() || !TagsOf(Data, In).Contains(Req.Tag))
		{
			return false;
		}
		const FFoodDef* Food = Data.FindFood(In.ItemId);
		return !Food || Food->State == EFoodState::Raw;
	}

	/** Asignación uno a uno de ingredientes a huecos (vuelta atrás; como mucho MaxIngredients). */
	bool AssignFrom(const FCookingData& Data, const TArray<const FCookIngredientReq*>& Slots, int32 SlotIndex,
		const TArray<FCookIngredient>& Ingredients, TArray<uint8>& Used)
	{
		if (SlotIndex >= Slots.Num())
		{
			return true;
		}
		for (int32 I = 0; I < Ingredients.Num(); ++I)
		{
			if (Used[I] || !ReqAccepts(Data, *Slots[SlotIndex], Ingredients[I]))
			{
				continue;
			}
			Used[I] = 1;
			if (AssignFrom(Data, Slots, SlotIndex + 1, Ingredients, Used))
			{
				return true;
			}
			Used[I] = 0;
		}
		return false;
	}

	bool RecipeMatches(const FCookingData& Data, const FCookRecipeDef& Recipe, const TArray<FCookIngredient>& Ingredients)
	{
		TArray<const FCookIngredientReq*> Slots;
		for (const FCookIngredientReq& Req : Recipe.Ingredients)
		{
			for (int32 C = 0; C < Req.Count; ++C)
			{
				Slots.Add(&Req);
			}
		}
		if (Slots.Num() != Ingredients.Num())
		{
			return false;
		}
		TArray<uint8> Used;
		Used.Init(0, Ingredients.Num());
		return AssignFrom(Data, Slots, 0, Ingredients, Used);
	}

	void AddScaled(FConsumable& Sum, const FConsumable& Add, float Scale)
	{
		Sum.Food += Add.Food * Scale;
		Sum.Water += Add.Water * Scale;
		Sum.Protein += Add.Protein * Scale;
		Sum.Carbs += Add.Carbs * Scale;
		Sum.Vitamins += Add.Vitamins * Scale;
		Sum.Healing += Add.Healing * Scale;
		Sum.Cures |= Add.Cures;
	}

	void ScaleNutrition(FConsumable& C, float Scale)
	{
		C.Food *= Scale;
		C.Protein *= Scale;
		C.Carbs *= Scale;
		C.Vitamins *= Scale;
	}
}

const TCHAR* LexToString(ECookTechnique Technique)
{
	switch (Technique)
	{
	case ECookTechnique::Roast: return TEXT("asar");
	case ECookTechnique::Boil: return TEXT("hervir");
	case ECookTechnique::Stew: return TEXT("guisar");
	case ECookTechnique::Smoke: return TEXT("ahumar");
	case ECookTechnique::Salt: return TEXT("salar");
	case ECookTechnique::Dry: return TEXT("secar");
	case ECookTechnique::Bake: return TEXT("hornear");
	default: return TEXT("desconocida");
	}
}

const TCHAR* LexToString(EFoodState State)
{
	switch (State)
	{
	case EFoodState::Raw: return TEXT("crudo");
	case EFoodState::Cooked: return TEXT("cocinado");
	case EFoodState::Smoked: return TEXT("ahumado");
	case EFoodState::Salted: return TEXT("salado");
	case EFoodState::Dried: return TEXT("seco");
	default: return TEXT("desconocido");
	}
}

bool TechniqueNeedsFire(ECookTechnique Technique)
{
	return Technique == ECookTechnique::Roast || Technique == ECookTechnique::Boil
		|| Technique == ECookTechnique::Stew || Technique == ECookTechnique::Bake;
}

int32 FCookRecipeDef::NumIngredientUnits() const
{
	int32 N = 0;
	for (const FCookIngredientReq& Req : Ingredients)
	{
		N += Req.Count;
	}
	return N;
}

bool FCookingPot::operator==(const FCookingPot& O) const
{
	return Status == O.Status && Technique == O.Technique && VesselId == O.VesselId && RecipeId == O.RecipeId
		&& IngredientIds == O.IngredientIds && ProgressMinutes == O.ProgressMinutes;
}

bool FFoodFreshness::operator==(const FFoodFreshness& O) const
{
	return ItemId == O.ItemId && AgeHours == O.AgeHours && HoursSinceCooked == O.HoursSinceCooked;
}

// --- Datos -------------------------------------------------------------------

namespace CookingModelDetail
{
	void FillDefaultCookingData(FCookingData& D)
	{
#include "Cooking/CookingData.inl"
	}
}

const FCookingData& FCookingData::Default()
{
	static const FCookingData Data = []()
	{
		FCookingData D;
		CookingModelDetail::FillDefaultCookingData(D);
		return D;
	}();
	return Data;
}

const FFoodDef* FCookingData::FindFood(FName ItemId) const
{
	return Foods.FindByPredicate([ItemId](const FFoodDef& F) { return F.ItemId == ItemId; });
}

const FCookRecipeDef* FCookingData::FindRecipe(FName RecipeId) const
{
	return Recipes.FindByPredicate([RecipeId](const FCookRecipeDef& R) { return R.Id == RecipeId; });
}

const FCookVesselDef* FCookingData::FindVessel(FName VesselId) const
{
	return Vessels.FindByPredicate([VesselId](const FCookVesselDef& V) { return V.Id == VesselId; });
}

FName FCookingData::VesselForItem(FName ItemId) const
{
	for (const FCookVesselDef& Vessel : Vessels)
	{
		if (Vessel.ItemIds.Contains(ItemId))
		{
			return Vessel.Id;
		}
	}
	return NAME_None;
}

// --- Cocción -----------------------------------------------------------------

const FCookRecipeDef* FCookingModel::FindRecipe(const FCookingData& Data, ECookTechnique Technique, FName VesselId,
	const TArray<FCookIngredient>& Ingredients, EFireLevel FireLevel)
{
	if (Ingredients.Num() == 0 || Ingredients.Num() > MaxIngredients)
	{
		return nullptr;
	}
	const FCookRecipeDef* Best = nullptr;
	for (const FCookRecipeDef& Recipe : Data.Recipes)
	{
		if (Recipe.Technique != Technique)
		{
			continue;
		}
		if (Recipe.Vessels.Num() > 0 && !Recipe.Vessels.Contains(VesselId))
		{
			continue;
		}
		if (TechniqueNeedsFire(Technique) && static_cast<uint8>(FireLevel) < static_cast<uint8>(Recipe.MinFireLevel))
		{
			continue;
		}
		if (!CookingModelDetail::RecipeMatches(Data, Recipe, Ingredients))
		{
			continue;
		}
		if (!Best || Recipe.NumIngredientUnits() > Best->NumIngredientUnits())
		{
			Best = &Recipe;
		}
	}
	return Best;
}

bool FCookingModel::StartPot(FCookingPot& OutPot, const FCookingData& Data, ECookTechnique Technique, FName VesselId,
	const TArray<FCookIngredient>& Ingredients, EFireLevel FireLevel, FString& OutFailReason)
{
	if (Ingredients.Num() == 0)
	{
		OutFailReason = TEXT("No hay nada que cocinar.");
		return false;
	}
	if (Ingredients.Num() > MaxIngredients)
	{
		OutFailReason = TEXT("Son demasiadas cosas a la vez.");
		return false;
	}
	const FCookVesselDef* Vessel = VesselId.IsNone() ? nullptr : Data.FindVessel(VesselId);
	if (!VesselId.IsNone() && !Vessel)
	{
		OutFailReason = TEXT("Eso no sirve para cocinar.");
		return false;
	}
	if (Vessel && Ingredients.Num() > Vessel->Capacity)
	{
		OutFailReason = FString::Printf(TEXT("No cabe tanto en %s."), *Vessel->NameEs);
		return false;
	}
	const bool bNeedsWatertight = Technique == ECookTechnique::Boil || Technique == ECookTechnique::Stew;
	if (bNeedsWatertight && (!Vessel || !Vessel->bWatertight))
	{
		OutFailReason = TEXT("Hace falta un recipiente que no pierda agua.");
		return false;
	}

	const FCookRecipeDef* Recipe = FindRecipe(Data, Technique, VesselId, Ingredients, FireLevel);
	if (!Recipe)
	{
		const bool bAllFood = !Ingredients.ContainsByPredicate([&Data](const FCookIngredient& In) { return Data.FindFood(In.ItemId) == nullptr; });
		if (Technique != Data.Improvised.Technique || !bAllFood || Data.Improvised.ResultItemId.IsNone())
		{
			OutFailReason = FString::Printf(TEXT("Así no se puede %s."), LexToString(Technique));
			return false;
		}
	}

	FCookingPot Pot;
	Pot.Status = EPotStatus::Cooking;
	Pot.Technique = Technique;
	Pot.VesselId = VesselId;
	Pot.RecipeId = Recipe ? Recipe->Id : FName(NAME_None);
	for (const FCookIngredient& In : Ingredients)
	{
		Pot.IngredientIds.Add(In.ItemId);
	}
	OutPot = Pot;
	return true;
}

float FCookingModel::ProgressRate(ECookTechnique Technique, const FCookEnvironment& Env)
{
	using namespace CookingModelDetail;
	switch (Technique)
	{
	case ECookTechnique::Roast:
	case ECookTechnique::Boil:
	case ECookTechnique::Stew:
	case ECookTechnique::Bake:
		return Env.FireHeat < MinCookHeat ? 0.0f : FMath::Clamp(Env.FireHeat / ReferenceHeat, 0.0f, MaxRate);
	case ECookTechnique::Smoke:
		return Env.Smoke < MinSmoke ? 0.0f : FMath::Clamp(Env.Smoke / ReferenceSmoke, 0.0f, MaxRate);
	case ECookTechnique::Salt:
		return 1.0f;
	case ECookTechnique::Dry:
	{
		// La lluvia sin techo lo para; al sol y con viento seca antes; el fuego cerca ayuda.
		if (!Env.bSheltered && Env.Rain > 0.05f)
		{
			return 0.0f;
		}
		const float Sun = Env.bSheltered ? 0.0f : FMath::Clamp(Env.Sun, 0.0f, 1.0f);
		const float Rate = 0.3f + 0.7f * Sun + 0.3f * FMath::Clamp(Env.Wind, 0.0f, 1.0f) + 0.5f * FMath::Max(0.0f, Env.FireHeat);
		return FMath::Clamp(Rate, 0.0f, MaxRate);
	}
	default:
		return 0.0f;
	}
}

float FCookingModel::CookMinutesFor(const FCookingPot& Pot, const FCookingData& Data)
{
	const FCookRecipeDef* Recipe = Pot.RecipeId.IsNone() ? nullptr : Data.FindRecipe(Pot.RecipeId);
	return Recipe ? Recipe->CookMinutes : Data.Improvised.CookMinutes;
}

float FCookingModel::BurnAfterMinutesFor(const FCookingPot& Pot, const FCookingData& Data)
{
	const FCookRecipeDef* Recipe = Pot.RecipeId.IsNone() ? nullptr : Data.FindRecipe(Pot.RecipeId);
	return Recipe ? Recipe->BurnAfterMinutes : Data.Improvised.BurnAfterMinutes;
}

void FCookingModel::TickPot(FCookingPot& Pot, const FCookingData& Data, const FCookEnvironment& Env, float DeltaMinutes)
{
	if (DeltaMinutes <= 0.0f || Pot.Status == EPotStatus::Empty || Pot.Status == EPotStatus::Burnt)
	{
		return;
	}
	Pot.ProgressMinutes += ProgressRate(Pot.Technique, Env) * DeltaMinutes;
	const float Cook = CookMinutesFor(Pot, Data);
	const float BurnAfter = BurnAfterMinutesFor(Pot, Data);
	if (BurnAfter >= 0.0f && Pot.ProgressMinutes >= Cook + BurnAfter)
	{
		Pot.Status = EPotStatus::Burnt;
	}
	else if (Pot.ProgressMinutes >= Cook)
	{
		Pot.Status = EPotStatus::Done;
	}
}

bool FCookingModel::Collect(const FCookingPot& Pot, const FCookingData& Data, FCookedFood& OutFood)
{
	FCookedFood Food;
	if (Pot.Status == EPotStatus::Burnt)
	{
		Food.ItemId = Data.BurntItemId;
		Food.Count = 1;
	}
	else if (Pot.Status == EPotStatus::Done)
	{
		const FCookRecipeDef* Recipe = Pot.RecipeId.IsNone() ? nullptr : Data.FindRecipe(Pot.RecipeId);
		if (!Recipe)
		{
			Food.ItemId = Data.Improvised.ResultItemId;
			Food.Count = 1;
			Food.Effects = ImprovisedEffects(Data, Pot.IngredientIds);
			Food.State = EFoodState::Cooked;
			OutFood = Food;
			return !Food.ItemId.IsNone();
		}
		Food.ItemId = Recipe->ResultItemId;
		Food.Count = FMath::Max(1, Recipe->ResultCount);
	}
	else
	{
		return false;
	}
	if (const FFoodDef* Def = Data.FindFood(Food.ItemId))
	{
		Food.Effects = Def->Effects;
		Food.State = Def->State;
	}
	OutFood = Food;
	return !Food.ItemId.IsNone();
}

FConsumable FCookingModel::ImprovisedEffects(const FCookingData& Data, const TArray<FName>& IngredientIds)
{
	FConsumable Sum;
	for (const FName& Id : IngredientIds)
	{
		const FFoodDef* Food = Data.FindFood(Id);
		if (!Food)
		{
			continue;
		}
		CookingModelDetail::AddScaled(Sum, Food->Effects, Data.Improvised.Efficiency);
		const float Toxicity = Food->bCookingRemovesToxicity ? 0.0f : Food->Effects.Toxicity;
		Sum.Toxicity = FMath::Max(Sum.Toxicity, Toxicity);
	}
	Sum.Warmth = Data.Improvised.Warmth;
	Sum.Morale = Data.Improvised.Morale;
	return Sum;
}

float FCookingModel::VesselWearPerUse(const FCookVesselDef& Vessel)
{
	return Vessel.MaxUses > 0 ? 1.0f / static_cast<float>(Vessel.MaxUses) : 0.0f;
}

// --- Conservación ------------------------------------------------------------

float FCookingModel::ShelfLifeHours(const FCookingData& Data, FName ItemId)
{
	const FFoodDef* Food = Data.FindFood(ItemId);
	if (!Food)
	{
		return -1.0f;
	}
	const float* Factor = Data.Preservation.FamilyFactor.Find(Food->Family);
	const float F = Factor ? *Factor : 1.0f;
	if (F <= 0.0f)
	{
		return -1.0f;
	}
	return Data.Preservation.StateHours[static_cast<int32>(Food->State)] * F;
}

EFreshness FCookingModel::GetFreshness(const FCookingData& Data, const FFoodFreshness& Food)
{
	const float Life = ShelfLifeHours(Data, Food.ItemId);
	if (Life <= 0.0f)
	{
		return EFreshness::Fresh;
	}
	if (Food.AgeHours >= Life)
	{
		return EFreshness::Spoiled;
	}
	return Food.AgeHours >= Life * Data.Preservation.StaleFraction ? EFreshness::Stale : EFreshness::Fresh;
}

void FCookingModel::Age(FFoodFreshness& Food, float DeltaHours, float StorageFactor)
{
	if (DeltaHours <= 0.0f)
	{
		return;
	}
	Food.AgeHours += DeltaHours * FMath::Max(0.0f, StorageFactor);
	if (Food.HoursSinceCooked >= 0.0f)
	{
		Food.HoursSinceCooked += DeltaHours;
	}
}

FConsumable FCookingModel::CurrentEffects(const FCookingData& Data, const FFoodFreshness& Food)
{
	const FFoodDef* Def = Data.FindFood(Food.ItemId);
	if (!Def)
	{
		return FConsumable();
	}
	const FPreservationDef& P = Data.Preservation;
	FConsumable C = Def->Effects;

	// Solo da calor recién hecha; se enfría en WarmHours.
	if (Food.HoursSinceCooked < 0.0f || P.WarmHours <= 0.0f)
	{
		C.Warmth = 0.0f;
	}
	else
	{
		C.Warmth *= FMath::Clamp(1.0f - Food.HoursSinceCooked / P.WarmHours, 0.0f, 1.0f);
	}

	const float Life = ShelfLifeHours(Data, Food.ItemId);
	switch (GetFreshness(Data, Food))
	{
	case EFreshness::Stale:
		CookingModelDetail::ScaleNutrition(C, P.StaleNutrition);
		break;
	case EFreshness::Spoiled:
	{
		CookingModelDetail::ScaleNutrition(C, P.StaleNutrition * 0.5f);
		const float Rot = FMath::Clamp((Food.AgeHours - Life) / Life, 0.0f, 1.0f);
		C.Toxicity = FMath::Max(C.Toxicity, FMath::Lerp(P.SpoiledToxicity, P.RottenToxicity, Rot));
		C.Morale += P.SpoiledMorale;
		break;
	}
	default:
		break;
	}
	return C;
}

FFoodFreshness FCookingModel::FreshFromPot(const FCookedFood& Food)
{
	FFoodFreshness F;
	F.ItemId = Food.ItemId;
	F.AgeHours = 0.0f;
	F.HoursSinceCooked = 0.0f;
	return F;
}
