#include "Misc/AutomationTest.h"

#include "Items/BackpackModel.h"
#include "Weather/RainBarrelModel.h"

#include <limits>

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

	It(TEXT("un barril leído con NaN, infinito o fuera de rango queda saneado y sigue llenándose"), [this]()
	{
		const FRainBarrelState Nan = FRainBarrelModel::Sanitize(FRainBarrelState{ 1, std::numeric_limits<float>::quiet_NaN() });
		TestEqual(TEXT("NaN pasa a 0 s"), Nan.RainSeconds, 0.0f);
		TestEqual(TEXT("Conserva la carga"), Nan.Charges, 1);
		TestEqual(TEXT("Infinito pasa a 0 s"), FRainBarrelModel::Sanitize(FRainBarrelState{ 0, std::numeric_limits<float>::infinity() }).RainSeconds, 0.0f);
		TestEqual(TEXT("Negativo pasa a 0 s"), FRainBarrelModel::Sanitize(FRainBarrelState{ 0, -7.0f }).RainSeconds, 0.0f);
		TestTrue(TEXT("Por debajo de una carga"), FRainBarrelModel::Sanitize(FRainBarrelState{ 0, 1.0e6f }).RainSeconds < FRainBarrelModel::RainSecondsPerCharge);
		TestEqual(TEXT("Cargas acotadas"), FRainBarrelModel::Sanitize(FRainBarrelState{ 99, 0.0f }).Charges, FRainBarrelModel::MaxCharges);
		TestEqual(TEXT("Cargas negativas"), FRainBarrelModel::Sanitize(FRainBarrelState{ -3, 0.0f }).Charges, 0);
		// Aunque llegue sin sanear, el barril no se queda roto: la lluvia siguiente da cargas.
		const FRainBarrelState Broken{ 0, std::numeric_limits<float>::quiet_NaN() };
		TestEqual(TEXT("Llena tras un NaN"), FRainBarrelModel::Collect(Broken, FRainBarrelModel::ShowerSeconds).Charges, 2);
		TestTrue(TEXT("Lluvia NaN no cuenta"), FRainBarrelModel::Collect(FRainBarrelState(), std::numeric_limits<float>::quiet_NaN()) == FRainBarrelState());
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
