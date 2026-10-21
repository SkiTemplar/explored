#include "Misc/AutomationTest.h"

#include "Narrative/BeaconProgressModel.h"
#include "Narrative/SupplyCrateModel.h"

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
		It(TEXT("al empezar sin nada da los topes: 1 cuerda, 3 fibras y 2 peces"), [this]()
		{
			const TMap<FName, int32> Missing = FSupplyCrateModel::ComputeMissing(TMap<FName, int32>(), FBeaconProgressState());
			TestEqual(TEXT("Cuerda"), Get(Missing, TEXT("cuerda")), 1);
			TestEqual(TEXT("Fibra"), Get(Missing, TEXT("fibra")), 3);
			TestEqual(TEXT("Pez"), Get(Missing, TEXT("pez_crudo")), 2);
			TestEqual(TEXT("Nada más"), Missing.Num(), 3);
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
			TestEqual(TEXT("Tres objetos"), First.Items.Num(), 3);

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
						&& Get(Missing, TEXT("pez_crudo")) <= FSupplyCrateModel::MaxRawFish && Missing.Num() <= 3;
					TestTrue(FString::Printf(TEXT("Topes con %d tarros y hitos %d"), Oil, Flags), bWithinCaps);
				}
			}
		});
	});
}

#endif
