// Generado por Tools/DataCheck (uv run datacheck --write-survival) desde Content/Data/survival_needs.json (medicines).
// No editar a mano: se incluye dentro de una función de rellenado con el parámetro D.
{
	FMedicineDef& M = D.AddDefaulted_GetRef();
	M.ItemId = FName(TEXT("vendaje_tela"));
	M.bTreatsWounds = true;
	M.Treatment = EWoundTreatment::ClothBandage;
}
{
	FMedicineDef& M = D.AddDefaulted_GetRef();
	M.ItemId = FName(TEXT("antidoto_corteza"));
	M.Effects.Cures |= SurvivalCureBit(ECondition::RaySting);
	M.Effects.Healing = 5.0f;
}
{
	FMedicineDef& M = D.AddDefaulted_GetRef();
	M.ItemId = FName(TEXT("carbon_activado"));
	M.Effects.Cures |= SurvivalCureBit(ECondition::Poisoned);
}
{
	FMedicineDef& M = D.AddDefaulted_GetRef();
	M.ItemId = FName(TEXT("te_corteza_sauce"));
	M.Effects.Cures |= SurvivalCureBit(ECondition::Fever);
}
{
	FMedicineDef& M = D.AddDefaulted_GetRef();
	M.ItemId = FName(TEXT("ferula_bambu"));
	M.Effects.Cures |= SurvivalCureBit(ECondition::Sprain);
}
{
	FMedicineDef& M = D.AddDefaulted_GetRef();
	M.ItemId = FName(TEXT("gel_aloe"));
	M.Effects.Cures |= SurvivalCureBit(ECondition::SunBurn);
	M.Effects.Cures |= SurvivalCureBit(ECondition::ContactBurn);
}
