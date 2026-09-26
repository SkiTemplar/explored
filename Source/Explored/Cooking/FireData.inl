// Generado por Tools/DataCheck (uv run datacheck --write-cooking) desde Content/Data/fuels.json.
// No editar a mano: se incluye dentro de una función de rellenado con el parámetro D.
{
	FFireLevelDef& L = D.Levels.AddDefaulted_GetRef();
	L.Level = EFireLevel::Fogata;
	L.Id = FName(TEXT("fogata"));
	L.NameEs = TEXT("Fogata");
	L.PieceId = FName(TEXT("fogata"));
	L.MaxFuelHours = 4.0f;
	L.BurnRate = 1.0f;
	L.HeatScale = 0.7f;
	L.EmberHours = 2.0f;
	L.RainQuench = 2.5f;
	L.WindTolerance = 0.6f;
	L.bEnclosed = false;
	L.BaseSmoke = 0.3f;
}
{
	FFireLevelDef& L = D.Levels.AddDefaulted_GetRef();
	L.Level = EFireLevel::Hoguera;
	L.Id = FName(TEXT("hoguera"));
	L.NameEs = TEXT("Hoguera");
	L.PieceId = FName(TEXT("hoguera_senal"));
	L.MaxFuelHours = 10.0f;
	L.BurnRate = 1.3f;
	L.HeatScale = 1.0f;
	L.EmberHours = 4.0f;
	L.RainQuench = 1.5f;
	L.WindTolerance = 0.8f;
	L.bEnclosed = false;
	L.BaseSmoke = 0.35f;
}
{
	FFireLevelDef& L = D.Levels.AddDefaulted_GetRef();
	L.Level = EFireLevel::HornoArcilla;
	L.Id = FName(TEXT("horno_arcilla"));
	L.NameEs = TEXT("Horno de arcilla");
	L.PieceId = FName(TEXT("horno_barro"));
	L.MaxFuelHours = 12.0f;
	L.BurnRate = 0.7f;
	L.HeatScale = 1.2f;
	L.EmberHours = 8.0f;
	L.RainQuench = 0.0f;
	L.WindTolerance = 1.0f;
	L.bEnclosed = true;
	L.BaseSmoke = 0.5f;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("fibra_coco"));
	F.BurnHours = 0.05f;
	F.Heat = 0.3f;
	F.bTinder = true;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("yesca_hongo"));
	F.BurnHours = 0.05f;
	F.Heat = 0.3f;
	F.bTinder = true;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("algodon_silvestre"));
	F.BurnHours = 0.05f;
	F.Heat = 0.3f;
	F.bTinder = true;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("rama_seca"));
	F.BurnHours = 0.5f;
	F.Heat = 0.6f;
	F.bTinder = false;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("bambu_fino"));
	F.BurnHours = 0.3f;
	F.Heat = 0.5f;
	F.bTinder = false;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("madera_blanda"));
	F.BurnHours = 0.8f;
	F.Heat = 0.55f;
	F.bTinder = false;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("madera_flotante"));
	F.BurnHours = 0.8f;
	F.Heat = 0.5f;
	F.bTinder = false;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("madera_naufragio"));
	F.BurnHours = 1.2f;
	F.Heat = 0.7f;
	F.bTinder = false;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("madera_dura"));
	F.BurnHours = 2.0f;
	F.Heat = 1.0f;
	F.bTinder = false;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("tronco_pequeno"));
	F.BurnHours = 3.0f;
	F.Heat = 0.9f;
	F.bTinder = false;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("carbon_vegetal"));
	F.BurnHours = 2.5f;
	F.Heat = 1.0f;
	F.bTinder = false;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("resina"));
	F.BurnHours = 0.4f;
	F.Heat = 0.8f;
	F.bTinder = false;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("grasa"));
	F.BurnHours = 0.6f;
	F.Heat = 0.9f;
	F.bTinder = false;
	F.bGreen = false;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("musgo"));
	F.BurnHours = 0.1f;
	F.Heat = 0.2f;
	F.bTinder = false;
	F.bGreen = true;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("hoja_palma"));
	F.BurnHours = 0.1f;
	F.Heat = 0.2f;
	F.bTinder = false;
	F.bGreen = true;
}
{
	FFuelDef& F = D.Fuels.AddDefaulted_GetRef();
	F.ItemId = FName(TEXT("rama_verde"));
	F.BurnHours = 0.4f;
	F.Heat = 0.3f;
	F.bTinder = false;
	F.bGreen = true;
}
{
	FIgnitionDef& I = D.Ignitions.AddDefaulted_GetRef();
	I.Method = EIgnitionMethod::Matches;
	I.Id = FName(TEXT("cerillas"));
	I.NameEs = TEXT("Cerillas");
	I.ToolItemId = FName(TEXT("cerillas"));
	I.bConsumesTool = true;
	I.BaseChance = 0.9f;
	I.Minutes = 0.5f;
}
{
	FIgnitionDef& I = D.Ignitions.AddDefaulted_GetRef();
	I.Method = EIgnitionMethod::Flint;
	I.Id = FName(TEXT("pedernal"));
	I.NameEs = TEXT("Pedernal y metal");
	I.ToolItemId = FName(TEXT("pedernal"));
	I.bConsumesTool = false;
	I.BaseChance = 0.55f;
	I.Minutes = 2.0f;
}
{
	FIgnitionDef& I = D.Ignitions.AddDefaulted_GetRef();
	I.Method = EIgnitionMethod::Friction;
	I.Id = FName(TEXT("friccion"));
	I.NameEs = TEXT("Arco de fuego");
	I.ToolItemId = FName(TEXT("rama_seca"));
	I.bConsumesTool = false;
	I.BaseChance = 0.3f;
	I.Minutes = 5.0f;
}
