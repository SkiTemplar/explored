// Generado por Tools/DataCheck (uv run datacheck --write-cooking) desde Content/Data/recipes.json e items.json.
// No editar a mano: se incluye dentro de una función de rellenado con el parámetro D.
{
	FCookVesselDef& V = D.Vessels.AddDefaulted_GetRef();
	V.Id = FName(TEXT("espeto"));
	V.NameEs = TEXT("Espeto");
	V.ItemIds.Add(FName(TEXT("palo_recto")));
	V.ItemIds.Add(FName(TEXT("estaca")));
	V.ItemIds.Add(FName(TEXT("rama_verde")));
	V.ItemIds.Add(FName(TEXT("bambu_fino")));
	V.PieceId = FName(NAME_None);
	V.Capacity = 2;
	V.bWatertight = false;
	V.MaxUses = 0;
}
{
	FCookVesselDef& V = D.Vessels.AddDefaulted_GetRef();
	V.Id = FName(TEXT("olla_coco"));
	V.NameEs = TEXT("Olla de coco");
	V.ItemIds.Add(FName(TEXT("recipiente_coco")));
	V.PieceId = FName(NAME_None);
	V.Capacity = 2;
	V.bWatertight = true;
	V.MaxUses = 6;
}
{
	FCookVesselDef& V = D.Vessels.AddDefaulted_GetRef();
	V.Id = FName(TEXT("vasija_barro"));
	V.NameEs = TEXT("Vasija de barro");
	V.ItemIds.Add(FName(TEXT("vasija_barro")));
	V.PieceId = FName(NAME_None);
	V.Capacity = 4;
	V.bWatertight = true;
	V.MaxUses = 0;
}
{
	FCookVesselDef& V = D.Vessels.AddDefaulted_GetRef();
	V.Id = FName(TEXT("bandeja"));
	V.NameEs = TEXT("Piedra plana o concha");
	V.ItemIds.Add(FName(TEXT("piedra_plana")));
	V.ItemIds.Add(FName(TEXT("concha_grande")));
	V.PieceId = FName(NAME_None);
	V.Capacity = 1;
	V.bWatertight = false;
	V.MaxUses = 0;
}
{
	FCookVesselDef& V = D.Vessels.AddDefaulted_GetRef();
	V.Id = FName(TEXT("secadero"));
	V.NameEs = TEXT("Secadero y ahumadero");
	V.PieceId = FName(TEXT("secadero"));
	V.Capacity = 6;
	V.bWatertight = false;
	V.MaxUses = 0;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("agua_hervida"));
	R.NameEs = TEXT("Agua hervida");
	R.Technique = ECookTechnique::Boil;
	R.Vessels.Add(FName(TEXT("olla_coco")));
	R.Vessels.Add(FName(TEXT("vasija_barro")));
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("agua_sin_tratar"));
		Q.Tag = FName(NAME_None);
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("agua_hervida"));
	R.ResultCount = 1;
	R.CookMinutes = 10.0f;
	R.BurnAfterMinutes = -1.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("sal_por_hervido"));
	R.NameEs = TEXT("Sal por evaporación al fuego");
	R.Technique = ECookTechnique::Boil;
	R.Vessels.Add(FName(TEXT("olla_coco")));
	R.Vessels.Add(FName(TEXT("vasija_barro")));
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("agua_mar"));
		Q.Tag = FName(NAME_None);
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("sal_marina"));
	R.ResultCount = 1;
	R.CookMinutes = 60.0f;
	R.BurnAfterMinutes = -1.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("sal_al_sol"));
	R.NameEs = TEXT("Sal por evaporación al sol");
	R.Technique = ECookTechnique::Dry;
	R.Vessels.Add(FName(TEXT("bandeja")));
	R.Vessels.Add(FName(TEXT("secadero")));
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("agua_mar"));
		Q.Tag = FName(NAME_None);
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("sal_marina"));
	R.ResultCount = 1;
	R.CookMinutes = 480.0f;
	R.BurnAfterMinutes = -1.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("pescado_asado"));
	R.NameEs = TEXT("Pescado asado en espeto");
	R.Technique = ECookTechnique::Roast;
	R.Vessels.Add(FName(TEXT("espeto")));
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("pescado"));
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("pescado_asado"));
	R.ResultCount = 1;
	R.CookMinutes = 20.0f;
	R.BurnAfterMinutes = 15.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("cangrejo_caparazon"));
	R.NameEs = TEXT("Cangrejo en su caparazón");
	R.Technique = ECookTechnique::Roast;
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("cangrejo"));
		Q.Tag = FName(NAME_None);
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("cangrejo_caparazon"));
	R.ResultCount = 1;
	R.CookMinutes = 15.0f;
	R.BurnAfterMinutes = 10.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("batata_asada"));
	R.NameEs = TEXT("Batata asada");
	R.Technique = ECookTechnique::Roast;
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("batata"));
		Q.Tag = FName(NAME_None);
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("batata_asada"));
	R.ResultCount = 1;
	R.CookMinutes = 30.0f;
	R.BurnAfterMinutes = 20.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("huevo_brasa"));
	R.NameEs = TEXT("Huevo a la brasa");
	R.Technique = ECookTechnique::Roast;
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("huevo"));
		Q.Tag = FName(NAME_None);
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("huevo_brasa"));
	R.ResultCount = 1;
	R.CookMinutes = 8.0f;
	R.BurnAfterMinutes = 6.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("taro_hervido"));
	R.NameEs = TEXT("Taro hervido");
	R.Technique = ECookTechnique::Boil;
	R.Vessels.Add(FName(TEXT("olla_coco")));
	R.Vessels.Add(FName(TEXT("vasija_barro")));
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("taro"));
		Q.Tag = FName(NAME_None);
		Q.Count = 1;
	}
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("agua_dulce"));
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("taro_hervido"));
	R.ResultCount = 1;
	R.CookMinutes = 30.0f;
	R.BurnAfterMinutes = 60.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("yuca_cocida"));
	R.NameEs = TEXT("Yuca cocida");
	R.Technique = ECookTechnique::Boil;
	R.Vessels.Add(FName(TEXT("olla_coco")));
	R.Vessels.Add(FName(TEXT("vasija_barro")));
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("yuca"));
		Q.Tag = FName(NAME_None);
		Q.Count = 1;
	}
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("agua_dulce"));
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("yuca_cocida"));
	R.ResultCount = 1;
	R.CookMinutes = 40.0f;
	R.BurnAfterMinutes = 60.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("sopa_pescado"));
	R.NameEs = TEXT("Sopa de pescado");
	R.Technique = ECookTechnique::Stew;
	R.Vessels.Add(FName(TEXT("olla_coco")));
	R.Vessels.Add(FName(TEXT("vasija_barro")));
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("pescado"));
		Q.Count = 1;
	}
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("agua_dulce"));
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("sopa_pescado"));
	R.ResultCount = 1;
	R.CookMinutes = 45.0f;
	R.BurnAfterMinutes = 40.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("estofado_pescado"));
	R.NameEs = TEXT("Estofado de pescado");
	R.Technique = ECookTechnique::Stew;
	R.Vessels.Add(FName(TEXT("vasija_barro")));
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("pescado"));
		Q.Count = 1;
	}
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("tuberculo"));
		Q.Count = 1;
	}
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("agua_dulce"));
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("estofado_pescado"));
	R.ResultCount = 2;
	R.CookMinutes = 60.0f;
	R.BurnAfterMinutes = 40.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("pescado_ahumado"));
	R.NameEs = TEXT("Pescado ahumado");
	R.Technique = ECookTechnique::Smoke;
	R.Vessels.Add(FName(TEXT("secadero")));
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("pescado"));
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("pescado_ahumado"));
	R.ResultCount = 1;
	R.CookMinutes = 360.0f;
	R.BurnAfterMinutes = -1.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("pescado_salado"));
	R.NameEs = TEXT("Pescado en salazón");
	R.Technique = ECookTechnique::Salt;
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("pescado"));
		Q.Count = 1;
	}
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("sal_marina"));
		Q.Tag = FName(NAME_None);
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("pescado_salado"));
	R.ResultCount = 1;
	R.CookMinutes = 120.0f;
	R.BurnAfterMinutes = -1.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("fruta_seca"));
	R.NameEs = TEXT("Fruta seca");
	R.Technique = ECookTechnique::Dry;
	R.Vessels.Add(FName(TEXT("secadero")));
	R.MinFireLevel = EFireLevel::Fogata;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(NAME_None);
		Q.Tag = FName(TEXT("fruta"));
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("fruta_seca"));
	R.ResultCount = 1;
	R.CookMinutes = 480.0f;
	R.BurnAfterMinutes = -1.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("vasija_barro"));
	R.NameEs = TEXT("Vasija de barro cocido");
	R.Technique = ECookTechnique::Bake;
	R.MinFireLevel = EFireLevel::HornoArcilla;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("arcilla_roja"));
		Q.Tag = FName(NAME_None);
		Q.Count = 2;
	}
	R.ResultItemId = FName(TEXT("vasija_barro"));
	R.ResultCount = 1;
	R.CookMinutes = 180.0f;
	R.BurnAfterMinutes = -1.0f;
}
{
	FCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();
	R.Id = FName(TEXT("carbon_vegetal"));
	R.NameEs = TEXT("Carbón vegetal");
	R.Technique = ECookTechnique::Bake;
	R.MinFireLevel = EFireLevel::HornoArcilla;
	{
		FCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();
		Q.ItemId = FName(TEXT("tronco_pequeno"));
		Q.Tag = FName(NAME_None);
		Q.Count = 1;
	}
	R.ResultItemId = FName(TEXT("carbon_vegetal"));
	R.ResultCount = 2;
	R.CookMinutes = 240.0f;
	R.BurnAfterMinutes = -1.0f;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("coco_verde"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("coco"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("coco")));
	F.Effects.Food = 5.0f;
	F.Effects.Water = 25.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 8.0f;
	F.Effects.Vitamins = 8.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("coco_maduro"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("coco"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("coco")));
	F.Effects.Food = 15.0f;
	F.Effects.Water = 5.0f;
	F.Effects.Protein = 8.0f;
	F.Effects.Carbs = 16.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("platano"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("fruta"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("fruta")));
	F.Effects.Food = 12.0f;
	F.Effects.Water = 3.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 16.0f;
	F.Effects.Vitamins = 16.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("mango_fruta"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("fruta"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("fruta")));
	F.Effects.Food = 10.0f;
	F.Effects.Water = 5.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 8.0f;
	F.Effects.Vitamins = 24.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 2.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("pina"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("fruta"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("fruta")));
	F.Tags.Add(FName(TEXT("semilla")));
	F.Effects.Food = 14.0f;
	F.Effects.Water = 6.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 16.0f;
	F.Effects.Vitamins = 24.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 2.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("maracuya"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("fruta"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("fruta")));
	F.Tags.Add(FName(TEXT("semilla")));
	F.Effects.Food = 5.0f;
	F.Effects.Water = 2.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 8.0f;
	F.Effects.Vitamins = 24.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("limon"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("fruta"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("fruta")));
	F.Tags.Add(FName(TEXT("citrico")));
	F.Tags.Add(FName(TEXT("semilla")));
	F.Effects.Food = 2.0f;
	F.Effects.Water = 1.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 40.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("lima_silvestre"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("fruta"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("fruta")));
	F.Tags.Add(FName(TEXT("citrico")));
	F.Effects.Food = 2.0f;
	F.Effects.Water = 1.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 32.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("taro"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("tuberculo"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("tuberculo")));
	F.Effects.Food = 8.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 24.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -2.0f;
	F.Effects.Toxicity = 0.2f;
	F.bCookingRemovesToxicity = true;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("batata"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("tuberculo"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("tuberculo")));
	F.Tags.Add(FName(TEXT("semilla")));
	F.Effects.Food = 10.0f;
	F.Effects.Water = 1.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 24.0f;
	F.Effects.Vitamins = 8.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("yuca"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("tuberculo"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("tuberculo")));
	F.Effects.Food = 12.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 24.0f;
	F.Effects.Vitamins = 8.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -2.0f;
	F.Effects.Toxicity = 0.8f;
	F.bCookingRemovesToxicity = true;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("cangrejo"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("marisco"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("marisco")));
	F.Effects.Food = 6.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 16.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -2.0f;
	F.Effects.Toxicity = 0.3f;
	F.bCookingRemovesToxicity = true;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("pescado_arrecife"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("pescado"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("pescado")));
	F.Effects.Food = 12.0f;
	F.Effects.Water = 1.0f;
	F.Effects.Protein = 24.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -2.0f;
	F.Effects.Toxicity = 0.15f;
	F.bCookingRemovesToxicity = true;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("huevo"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("huevo"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Effects.Food = 6.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 8.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -1.0f;
	F.Effects.Toxicity = 0.1f;
	F.bCookingRemovesToxicity = true;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("seta_comestible"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("seta"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("seta")));
	F.Effects.Food = 3.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 8.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 0.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("grasa"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("grasa"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("combustible")));
	F.Effects.Food = 6.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 16.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -3.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("sal_marina"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("mineral"));
	F.Tags.Add(FName(TEXT("sal")));
	F.Tags.Add(FName(TEXT("comida")));
	F.Effects.Food = 0.0f;
	F.Effects.Water = -2.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 0.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("agua_sin_tratar"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("agua"));
	F.Tags.Add(FName(TEXT("agua")));
	F.Tags.Add(FName(TEXT("agua_dulce")));
	F.Effects.Food = 0.0f;
	F.Effects.Water = 30.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 0.0f;
	F.Effects.Toxicity = 0.35f;
	F.bCookingRemovesToxicity = true;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("agua_hervida"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("agua"));
	F.Tags.Add(FName(TEXT("agua")));
	F.Tags.Add(FName(TEXT("agua_dulce")));
	F.Tags.Add(FName(TEXT("bebida")));
	F.Effects.Food = 0.0f;
	F.Effects.Water = 30.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 1.0f;
	F.Effects.Morale = 1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("agua_mar"));
	F.State = EFoodState::Raw;
	F.Family = FName(TEXT("agua"));
	F.Tags.Add(FName(TEXT("agua")));
	F.Tags.Add(FName(TEXT("agua_salada")));
	F.Effects.Food = 0.0f;
	F.Effects.Water = -10.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -2.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("pescado_asado"));
	F.State = EFoodState::Cooked;
	F.Family = FName(TEXT("pescado"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("pescado")));
	F.Tags.Add(FName(TEXT("cocinado")));
	F.Effects.Food = 22.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 24.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 3.0f;
	F.Effects.Morale = 3.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("cangrejo_caparazon"));
	F.State = EFoodState::Cooked;
	F.Family = FName(TEXT("marisco"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("marisco")));
	F.Tags.Add(FName(TEXT("cocinado")));
	F.Effects.Food = 12.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 16.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 3.0f;
	F.Effects.Morale = 3.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("taro_hervido"));
	F.State = EFoodState::Cooked;
	F.Family = FName(TEXT("tuberculo"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("tuberculo")));
	F.Tags.Add(FName(TEXT("cocinado")));
	F.Effects.Food = 16.0f;
	F.Effects.Water = 3.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 24.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 3.0f;
	F.Effects.Morale = 1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("yuca_cocida"));
	F.State = EFoodState::Cooked;
	F.Family = FName(TEXT("tuberculo"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("tuberculo")));
	F.Tags.Add(FName(TEXT("cocinado")));
	F.Effects.Food = 20.0f;
	F.Effects.Water = 2.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 24.0f;
	F.Effects.Vitamins = 8.0f;
	F.Effects.Warmth = 3.0f;
	F.Effects.Morale = 1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("batata_asada"));
	F.State = EFoodState::Cooked;
	F.Family = FName(TEXT("tuberculo"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("tuberculo")));
	F.Tags.Add(FName(TEXT("cocinado")));
	F.Effects.Food = 18.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 24.0f;
	F.Effects.Vitamins = 8.0f;
	F.Effects.Warmth = 3.0f;
	F.Effects.Morale = 3.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("huevo_brasa"));
	F.State = EFoodState::Cooked;
	F.Family = FName(TEXT("huevo"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("cocinado")));
	F.Effects.Food = 9.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 8.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 2.0f;
	F.Effects.Morale = 2.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("sopa_pescado"));
	F.State = EFoodState::Cooked;
	F.Family = FName(TEXT("guiso"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("pescado")));
	F.Tags.Add(FName(TEXT("cocinado")));
	F.Effects.Food = 22.0f;
	F.Effects.Water = 15.0f;
	F.Effects.Protein = 24.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 8.0f;
	F.Effects.Warmth = 6.0f;
	F.Effects.Morale = 4.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("estofado_pescado"));
	F.State = EFoodState::Cooked;
	F.Family = FName(TEXT("guiso"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("pescado")));
	F.Tags.Add(FName(TEXT("cocinado")));
	F.Effects.Food = 34.0f;
	F.Effects.Water = 10.0f;
	F.Effects.Protein = 24.0f;
	F.Effects.Carbs = 24.0f;
	F.Effects.Vitamins = 8.0f;
	F.Effects.Warmth = 8.0f;
	F.Effects.Morale = 6.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("guiso_improvisado"));
	F.State = EFoodState::Cooked;
	F.Family = FName(TEXT("guiso"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("cocinado")));
	F.Effects.Food = 18.0f;
	F.Effects.Water = 8.0f;
	F.Effects.Protein = 8.0f;
	F.Effects.Carbs = 8.0f;
	F.Effects.Vitamins = 8.0f;
	F.Effects.Warmth = 4.0f;
	F.Effects.Morale = 2.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("comida_quemada"));
	F.State = EFoodState::Cooked;
	F.Family = FName(TEXT("guiso"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Effects.Food = 4.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = -4.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("pescado_ahumado"));
	F.State = EFoodState::Smoked;
	F.Family = FName(TEXT("pescado"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("pescado")));
	F.Tags.Add(FName(TEXT("conserva")));
	F.Effects.Food = 18.0f;
	F.Effects.Water = -2.0f;
	F.Effects.Protein = 24.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 2.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("pescado_salado"));
	F.State = EFoodState::Salted;
	F.Family = FName(TEXT("pescado"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("pescado")));
	F.Tags.Add(FName(TEXT("conserva")));
	F.Effects.Food = 16.0f;
	F.Effects.Water = -5.0f;
	F.Effects.Protein = 24.0f;
	F.Effects.Carbs = 0.0f;
	F.Effects.Vitamins = 0.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 1.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
{
	FFoodDef& F = D.Foods.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("fruta_seca"));
	F.State = EFoodState::Dried;
	F.Family = FName(TEXT("fruta"));
	F.Tags.Add(FName(TEXT("comida")));
	F.Tags.Add(FName(TEXT("fruta")));
	F.Tags.Add(FName(TEXT("conserva")));
	F.Effects.Food = 10.0f;
	F.Effects.Water = 0.0f;
	F.Effects.Protein = 0.0f;
	F.Effects.Carbs = 16.0f;
	F.Effects.Vitamins = 16.0f;
	F.Effects.Warmth = 0.0f;
	F.Effects.Morale = 2.0f;
	F.Effects.Toxicity = 0.0f;
	F.bCookingRemovesToxicity = false;
}
D.Preservation.StateHours[static_cast<int32>(EFoodState::Raw)] = 24.0f;
D.Preservation.StateHours[static_cast<int32>(EFoodState::Cooked)] = 72.0f;
D.Preservation.StateHours[static_cast<int32>(EFoodState::Smoked)] = 336.0f;
D.Preservation.StateHours[static_cast<int32>(EFoodState::Salted)] = 480.0f;
D.Preservation.StateHours[static_cast<int32>(EFoodState::Dried)] = 400.0f;
D.Preservation.FamilyFactor.Add(FName(TEXT("pescado")), 1.0f);
D.Preservation.FamilyFactor.Add(FName(TEXT("marisco")), 0.75f);
D.Preservation.FamilyFactor.Add(FName(TEXT("huevo")), 4.0f);
D.Preservation.FamilyFactor.Add(FName(TEXT("fruta")), 3.0f);
D.Preservation.FamilyFactor.Add(FName(TEXT("tuberculo")), 6.0f);
D.Preservation.FamilyFactor.Add(FName(TEXT("coco")), 5.0f);
D.Preservation.FamilyFactor.Add(FName(TEXT("seta")), 1.5f);
D.Preservation.FamilyFactor.Add(FName(TEXT("grasa")), 2.0f);
D.Preservation.FamilyFactor.Add(FName(TEXT("guiso")), 1.0f);
D.Preservation.FamilyFactor.Add(FName(TEXT("agua")), 0.0f);
D.Preservation.FamilyFactor.Add(FName(TEXT("mineral")), 0.0f);
D.Preservation.StaleFraction = 0.75f;
D.Preservation.StaleNutrition = 0.8f;
D.Preservation.SpoiledToxicity = 0.35f;
D.Preservation.RottenToxicity = 0.9f;
D.Preservation.SpoiledMorale = -5.0f;
D.Preservation.WarmHours = 1.0f;
D.Improvised.Technique = ECookTechnique::Stew;
D.Improvised.ResultItemId = FName(TEXT("guiso_improvisado"));
D.Improvised.CookMinutes = 45.0f;
D.Improvised.BurnAfterMinutes = 30.0f;
D.Improvised.Efficiency = 0.9f;
D.Improvised.Warmth = 4.0f;
D.Improvised.Morale = 2.0f;
D.BurntItemId = FName(TEXT("comida_quemada"));
