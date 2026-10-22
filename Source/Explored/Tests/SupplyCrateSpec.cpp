#include "Misc/AutomationTest.h"

#include "Narrative/BeaconProgressModel.h"
#include "Narrative/SupplyCrateModel.h"
#include "Save/SaveIslandSections.h"
#include "WorldGen/IslandPropsModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CrateTest
{
	int32 Get(const TMap<FName, int32>& Items, const TCHAR* Id)
	{
		return Items.FindRef(FName(Id));
	}

	/** Lente recogida, 2 tarros y mecha: el camino crítico solo espera montar. */
	FBeaconProgressState ReadyToMount()
	{
		FBeaconProgressState S;
		S.Stage = EBeaconStage::Preparacion;
		S.bLensCollected = true;
		S.bWickMade = true;
		S.OilJars = 2;
		return S;
	}
}

BEGIN_DEFINE_SPEC(FSupplyCrateSpec, "Explored.SupplyCrate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSupplyCrateSpec)

void FSupplyCrateSpec::Define()
{
	using namespace CrateTest;

	Describe(TEXT("lo que falta"), [this]()
	{
		It(TEXT("al empezar sin nada da los topes: 1 cuerda, 3 fibras, 2 peces y 3 leñas"), [this]()
		{
			const TMap<FName, int32> Missing = FSupplyCrateModel::ComputeMissing(TMap<FName, int32>(), FBeaconProgressState());
			TestEqual(TEXT("Cuerda"), Get(Missing, TEXT("cuerda")), 1);
			TestEqual(TEXT("Fibra"), Get(Missing, TEXT("fibra")), 3);
			TestEqual(TEXT("Pez"), Get(Missing, TEXT("pez_crudo")), 2);
			TestEqual(TEXT("Leña"), Get(Missing, TEXT("lena")), 3);
			TestEqual(TEXT("Nada más"), Missing.Num(), 4);
		});

		It(TEXT("con la isla llena no entrega nada y queda para cuando haga falta"), [this]()
		{
			TMap<FName, int32> Island;
			Island.Add(TEXT("fibra"), 15);
			Island.Add(TEXT("lena"), 20);
			Island.Add(TEXT("cana_bambu"), 3);
			TestTrue(TEXT("Nada falta"), FSupplyCrateModel::ComputeMissing(TMap<FName, int32>(), FBeaconProgressState(), Island).IsEmpty());
			const FSupplyCrateOpenResult Open = FSupplyCrateModel::Open(FSupplyCrateState(), TMap<FName, int32>(), FBeaconProgressState(), Island);
			TestFalse(TEXT("Sin entrega"), Open.bDelivered);
		});

		It(TEXT("leña quemada de más: da 3 leñas si quedan tarros por hervir"), [this]()
		{
			TMap<FName, int32> Pack;
			Pack.Add(TEXT("cana_pescar"), 1);
			Pack.Add(TEXT("mecha"), 1);
			Pack.Add(TEXT("lena"), 1);
			FBeaconProgressState S;
			S.bLensCollected = true;
			S.bWickMade = true;
			const TMap<FName, int32> Missing = FSupplyCrateModel::ComputeMissing(Pack, S, TMap<FName, int32>());
			TestEqual(TEXT("Dos leñas"), Get(Missing, TEXT("lena")), 2);
			TestEqual(TEXT("Con caña, sin peces"), Get(Missing, TEXT("pez_crudo")), 0);
			S.OilJars = 2;
			TestEqual(TEXT("Con los tarros, nada"), Get(FSupplyCrateModel::ComputeMissing(Pack, S, TMap<FName, int32>()), TEXT("lena")), 0);
		});

		It(TEXT("leña quemada dos veces (caja ya abierta): el mar deja leña de deriva al amanecer"), [this]()
		{
			TMap<FName, int32> Pack;
			Pack.Add(TEXT("cana_pescar"), 1);
			FBeaconProgressState S;
			S.bLensCollected = true;
			S.OilJars = 1;
			const TMap<FName, int32> Bare;
			TestEqual(TEXT("Sin leña en ningún sitio: 3"), FSupplyCrateModel::DriftwoodAtDawn(Pack, S, Bare, false), 3);
			TestEqual(TEXT("Con un fuego encendido no hace falta"), FSupplyCrateModel::DriftwoodAtDawn(Pack, S, Bare, true), 0);
			TMap<FName, int32> Island;
			Island.Add(TEXT("lena"), 4);
			TestEqual(TEXT("Con leña en la isla, nada"), FSupplyCrateModel::DriftwoodAtDawn(Pack, S, Island, false), 0);
			S.OilJars = 2;
			TestEqual(TEXT("Con los tarros hechos, nada"), FSupplyCrateModel::DriftwoodAtDawn(Pack, S, Bare, false), 0);
		});

		It(TEXT("fibras agotadas en la isla: cuerda y fibras para lo que falte"), [this]()
		{
			TMap<FName, int32> Pack;
			Pack.Add(TEXT("cana_pescar"), 1);
			Pack.Add(TEXT("fibra"), 4);
			TMap<FName, int32> Island;
			Island.Add(TEXT("lena"), 10);
			FBeaconProgressState S;
			S.bRaftBuilt = true;
			S.bLensCollected = true;
			// Mecha: 3 fibras, que ya están en la mochila: no falta nada.
			TestTrue(TEXT("Con 4 fibras basta"), FSupplyCrateModel::ComputeMissing(Pack, S, Island).IsEmpty());
			Pack[TEXT("fibra")] = 1;
			const TMap<FName, int32> Missing = FSupplyCrateModel::ComputeMissing(Pack, S, Island);
			TestEqual(TEXT("Faltan 2 fibras"), Get(Missing, TEXT("fibra")), 2);
		});

		It(TEXT("sin caña posible en la isla, los peces sí faltan"), [this]()
		{
			FBeaconProgressState S;
			S.bLensCollected = true;
			S.bWickMade = true;
			TMap<FName, int32> Island;
			Island.Add(TEXT("lena"), 10);
			Island.Add(TEXT("fibra"), 4);
			const TMap<FName, int32> Missing = FSupplyCrateModel::ComputeMissing(TMap<FName, int32>(), S, Island);
			TestEqual(TEXT("Dos peces"), Get(Missing, TEXT("pez_crudo")), 2);
		});

		It(TEXT("no pide nada con el camino crítico cubierto"), [this]()
		{
			TestTrue(TEXT("Vacío"), FSupplyCrateModel::ComputeMissing(TMap<FName, int32>(), ReadyToMount()).IsEmpty());
		});

		It(TEXT("las ranuras llenas cuentan como hechas"), [this]()
		{
			FBeaconProgressState S;
			S.bLensCollected = true;
			S.LanternSlots = 0x7;
			TestTrue(TEXT("Linterna montada: nada"), FSupplyCrateModel::ComputeMissing(TMap<FName, int32>(), S).IsEmpty());
		});

		It(TEXT("descuenta lo que hay en la mochila"), [this]()
		{
			FBeaconProgressState S = ReadyToMount();
			S.bWickMade = false;
			S.OilJars = 1;
			TMap<FName, int32> Inventory;
			Inventory.Add(FName(TEXT("fibra")), 2);
			Inventory.Add(FName(TEXT("pez_crudo")), 1);
			const TMap<FName, int32> Missing = FSupplyCrateModel::ComputeMissing(Inventory, S);
			TestEqual(TEXT("Una fibra para la mecha"), Get(Missing, TEXT("fibra")), 1);
			TestEqual(TEXT("Un pez para el segundo tarro"), Get(Missing, TEXT("pez_crudo")), 1);
			TestFalse(TEXT("Sin cuerda"), Missing.Contains(FName(TEXT("cuerda"))));
		});

		It(TEXT("una mecha en la mochila no pide fibras"), [this]()
		{
			FBeaconProgressState S = ReadyToMount();
			S.bWickMade = false;
			TMap<FName, int32> Inventory;
			Inventory.Add(FName(TEXT("mecha")), 1);
			TestTrue(TEXT("Vacío"), FSupplyCrateModel::ComputeMissing(Inventory, S).IsEmpty());
		});

		It(TEXT("sin lente pide cuerdas para la balsa y el remo; lo que la caja no cubre va en fibras"), [this]()
		{
			FBeaconProgressState S = ReadyToMount();
			S.bLensCollected = false;
			TMap<FName, int32> Inventory;
			Inventory.Add(FName(TEXT("cuerda")), 1);
			const TMap<FName, int32> NoOar = FSupplyCrateModel::ComputeMissing(Inventory, S);
			TestEqual(TEXT("Cuerda"), Get(NoOar, TEXT("cuerda")), 1);
			TestEqual(TEXT("Fibras de la otra cuerda"), Get(NoOar, TEXT("fibra")), 2);

			Inventory.Add(FName(TEXT("remo")), 1);
			const TMap<FName, int32> WithOar = FSupplyCrateModel::ComputeMissing(Inventory, S);
			TestEqual(TEXT("Con remo, una cuerda"), Get(WithOar, TEXT("cuerda")), 1);
			TestFalse(TEXT("Sin fibras"), WithOar.Contains(FName(TEXT("fibra"))));

			S.bRaftBuilt = true;
			TestTrue(TEXT("Balsa hecha: nada"), FSupplyCrateModel::ComputeMissing(TMap<FName, int32>(), S).IsEmpty());
		});

		It(TEXT("pide cuerda para la caña si faltan más peces de los que da la caja"), [this]()
		{
			FBeaconProgressState S = ReadyToMount();
			S.OilJars = 0;
			const TMap<FName, int32> NoRod = FSupplyCrateModel::ComputeMissing(TMap<FName, int32>(), S);
			TestEqual(TEXT("Dos peces (tope)"), Get(NoRod, TEXT("pez_crudo")), 2);
			TestEqual(TEXT("Cuerda para la caña"), Get(NoRod, TEXT("cuerda")), 1);

			TMap<FName, int32> Inventory;
			Inventory.Add(FName(TEXT("cana_pescar")), 1);
			TestFalse(TEXT("Con caña no hay cuerda"), FSupplyCrateModel::ComputeMissing(Inventory, S).Contains(FName(TEXT("cuerda"))));
		});

		It(TEXT("cantidades negativas en el inventario cuentan como 0"), [this]()
		{
			TMap<FName, int32> Inventory;
			Inventory.Add(FName(TEXT("pez_crudo")), -50);
			Inventory.Add(FName(TEXT("fibra")), MIN_int32);
			const TMap<FName, int32> Missing = FSupplyCrateModel::ComputeMissing(Inventory, FBeaconProgressState());
			TestEqual(TEXT("Pez"), Get(Missing, TEXT("pez_crudo")), 2);
			TestEqual(TEXT("Fibra"), Get(Missing, TEXT("fibra")), 3);
		});
	});

	Describe(TEXT("entrega"), [this]()
	{
		It(TEXT("se entrega una sola vez"), [this]()
		{
			const FSupplyCrateOpenResult First = FSupplyCrateModel::Open(FSupplyCrateState(), TMap<FName, int32>(), FBeaconProgressState());
			TestTrue(TEXT("Primera"), First.bDelivered);
			TestTrue(TEXT("Marcada"), First.State.bDelivered);
			TestEqual(TEXT("Cuatro objetos"), First.Items.Num(), 4);

			const FSupplyCrateOpenResult Second = FSupplyCrateModel::Open(First.State, TMap<FName, int32>(), FBeaconProgressState());
			TestFalse(TEXT("Segunda rechazada"), Second.bDelivered);
			TestTrue(TEXT("Nada"), Second.Items.IsEmpty());
			TestTrue(TEXT("Sigue gastada"), Second.State.bDelivered);
		});

		It(TEXT("si no falta nada no se gasta"), [this]()
		{
			const FSupplyCrateOpenResult Open = FSupplyCrateModel::Open(FSupplyCrateState(), TMap<FName, int32>(), ReadyToMount());
			TestFalse(TEXT("Sin entrega"), Open.bDelivered);
			TestFalse(TEXT("Queda para luego"), Open.State.bDelivered);
			TestTrue(TEXT("Nada"), Open.Items.IsEmpty());
		});

		It(TEXT("lo entregado nunca pasa de los topes"), [this]()
		{
			for (int32 Oil = 0; Oil <= 2; ++Oil)
			{
				for (int32 Flags = 0; Flags < 8; ++Flags)
				{
					FBeaconProgressState S;
					S.OilJars = Oil;
					S.bLensCollected = (Flags & 1) != 0;
					S.bWickMade = (Flags & 2) != 0;
					S.bRaftBuilt = (Flags & 4) != 0;
					const TMap<FName, int32> Missing = FSupplyCrateModel::ComputeMissing(TMap<FName, int32>(), S);
					const bool bWithinCaps = Get(Missing, TEXT("cuerda")) <= FSupplyCrateModel::MaxRope
						&& Get(Missing, TEXT("fibra")) <= FSupplyCrateModel::MaxFiber
						&& Get(Missing, TEXT("pez_crudo")) <= FSupplyCrateModel::MaxRawFish
						&& Get(Missing, TEXT("lena")) <= FSupplyCrateModel::MaxWood && Missing.Num() <= 4;
					TestTrue(FString::Printf(TEXT("Topes con %d tarros y hitos %d"), Oil, Flags), bWithinCaps);
				}
			}
		});
	});

	Describe(TEXT("leña de deriva"), [this]()
	{
		It(TEXT("se deja sobre la arena de la cala, no bajo ella"), [this]()
		{
			FIslandProp Beach;
			Beach.Id = FName(TEXT("lena_cala_1"));
			Beach.Location = FVector2D(-98.0, 18.0);
			// Con el suelo de la cala a 2,33 m, el código anterior (suelo 0) la dejaba a 30 cm de cota.
			const FVector Pile = FSupplyCrateModel::DriftwoodPileLocation(Beach, 2.33f);
			TestEqual(TEXT("X"), Pile.X, -9800.0);
			TestEqual(TEXT("Y"), Pile.Y, 1800.0);
			TestEqual(TEXT("Z = suelo + RestLiftCm"), Pile.Z, 233.0 + ExploredSaveStates::RestLiftCm, 0.01);
			Beach.Elevation = 0.5f;
			TestEqual(TEXT("respeta la elevación del montón"), FSupplyCrateModel::DriftwoodPileLocation(Beach, 2.33f).Z,
				283.0 + ExploredSaveStates::RestLiftCm, 0.01);
		});
	});
}

#endif
