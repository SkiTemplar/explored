#include "Misc/AutomationTest.h"

#include "Items/BackpackModel.h"
#include "Weather/RainBarrelModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FRainBarrelSpec, "Explored.RainBarrel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FRainBarrelSpec)

void FRainBarrelSpec::Define()
{
	It(TEXT("el chubasco de 90 s llena 2 cargas y el ciclón no pasa de 4 (GDD §5.5)"), [this]()
	{
		const FRainBarrelState Shower = FRainBarrelModel::Collect(FRainBarrelState(), FRainBarrelModel::ShowerSeconds);
		TestEqual(TEXT("2 cargas"), Shower.Charges, 2);
		const FRainBarrelState Storm = FRainBarrelModel::Collect(Shower, 3600.0f);
		TestEqual(TEXT("Tope"), Storm.Charges, FRainBarrelModel::MaxCharges);
	});

	It(TEXT("la lluvia se acumula entre muestras"), [this]()
	{
		FRainBarrelState S;
		for (int32 I = 0; I < 9; ++I)
		{
			S = FRainBarrelModel::Collect(S, 5.0f);
		}
		TestEqual(TEXT("45 s = 1 carga"), S.Charges, 1);
		TestEqual(TEXT("Resto"), S.RainSeconds, 0.0f);
		TestEqual(TEXT("Sin lluvia no cambia"), FRainBarrelModel::Collect(S, -3.0f).Charges, 1);
	});

	It(TEXT("una carga deja la cantimplora a 4 sorbos"), [this]()
	{
		const FBackpackRules Rules = FBackpackRules::Default();
		const FBackpackState Pack = FBackpackModel::StarterKit(Rules);
		const FRainBarrelState Barrel = FRainBarrelModel::AddCharges(FRainBarrelState(), 2);
		FRainBarrelState OutBarrel;
		FBackpackState OutPack;
		TestEqual(TEXT("Ok"), FRainBarrelModel::RefillCanteen(Barrel, Pack, OutBarrel, OutPack), ERainBarrelResult::Ok);
		TestEqual(TEXT("Sorbos"), OutPack.CanteenSips, 4);
		TestEqual(TEXT("Queda 1 carga"), OutBarrel.Charges, 1);
		TestEqual(TEXT("La mochila de entrada no cambia"), Pack.CanteenSips, 2);
	});

	It(TEXT("sin agua, sin cantimplora o con ella llena no gasta nada"), [this]()
	{
		const FBackpackRules Rules = FBackpackRules::Default();
		FBackpackState Pack = FBackpackModel::StarterKit(Rules);
		FRainBarrelState OutBarrel;
		FBackpackState OutPack;
		TestEqual(TEXT("Vacío"), FRainBarrelModel::RefillCanteen(FRainBarrelState(), Pack, OutBarrel, OutPack), ERainBarrelResult::Empty);
		const FRainBarrelState Barrel = FRainBarrelModel::AddCharges(FRainBarrelState(), 1);
		TestEqual(TEXT("Sin cantimplora"), FRainBarrelModel::RefillCanteen(Barrel, FBackpackState(), OutBarrel, OutPack), ERainBarrelResult::NoCanteen);
		Pack.CanteenSips = FBackpackModel::CanteenMaxSips;
		TestEqual(TEXT("Llena"), FRainBarrelModel::RefillCanteen(Barrel, Pack, OutBarrel, OutPack), ERainBarrelResult::CanteenFull);
		TestEqual(TEXT("Carga intacta"), OutBarrel.Charges, 1);
		TestEqual(TEXT("Cargas negativas no restan"), FRainBarrelModel::AddCharges(Barrel, -5).Charges, 1);
	});
}

#endif
