#include "Misc/AutomationTest.h"

#include "Debug/BeaconSoftlockModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SoftlockTest
{
	/** Recursos de island_props.json (GDD §5.3): 16 cocos, 20 leñas, 15 fibras, 3 cañas, 7 troncos. */
	TMap<FName, int32> GddIsland()
	{
		TMap<FName, int32> Island;
		Island.Add(TEXT("coco"), 16);
		Island.Add(TEXT("lena"), 20);
		Island.Add(TEXT("fibra"), 15);
		Island.Add(TEXT("cana_bambu"), 3);
		Island.Add(TEXT("tronco"), 7);
		return Island;
	}

	const FSoftlockScenario* Find(const TArray<FSoftlockScenario>& Scenarios, const TCHAR* Prefix)
	{
		return Scenarios.FindByPredicate([Prefix](const FSoftlockScenario& S) { return S.Name.StartsWith(Prefix); });
	}
}

BEGIN_DEFINE_SPEC(FBeaconSoftlockSpec, "Explored.Debug.BeaconSoftlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBeaconSoftlockSpec)

void FBeaconSoftlockSpec::Define()
{
	using namespace SoftlockTest;

	Describe(TEXT("escenarios del plan F3"), [this]()
	{
		It(TEXT("con la isla del GDD ningún escenario se bloquea"), [this]()
		{
			const TArray<FSoftlockScenario> Scenarios = FBeaconSoftlockModel::StandardScenarios(GddIsland());
			TestTrue(TEXT("Hay escenarios"), Scenarios.Num() >= 8);
			for (const FSoftlockScenario& Scenario : Scenarios)
			{
				const FSoftlockVerdict Verdict = FBeaconSoftlockModel::Evaluate(Scenario.Snapshot);
				TestTrue(Scenario.Name + TEXT(": ") + FBeaconSoftlockModel::Describe(Verdict), Verdict.bCompletable);
			}
		});

		It(TEXT("la leña quemada solo se salva con la caja (caso negativo: caja ya entregada)"), [this]()
		{
			const TArray<FSoftlockScenario> Scenarios = FBeaconSoftlockModel::StandardScenarios(GddIsland());
			const FSoftlockScenario* NoWood = Find(Scenarios, TEXT("agotados: toda la leña"));
			if (!TestNotNull(TEXT("Escenario"), NoWood))
			{
				return;
			}
			const FSoftlockVerdict WithCrate = FBeaconSoftlockModel::Evaluate(NoWood->Snapshot);
			TestEqual(TEXT("La caja da 3 leñas"), WithCrate.CrateItems.FindRef(TEXT("lena")), 3);
			FSoftlockSnapshot Spent = NoWood->Snapshot;
			Spent.Crate.bDelivered = true;
			const FSoftlockVerdict Without = FBeaconSoftlockModel::Evaluate(Spent);
			TestFalse(TEXT("Sin caja, softlock"), Without.bCompletable);
			TestTrue(TEXT("Dice qué falta"), FBeaconSoftlockModel::Describe(Without).Contains(TEXT("lena")));
		});

		It(TEXT("sin la reapertura del mar, la lente no recogida el día 2 es un softlock (caso negativo)"), [this]()
		{
			const TArray<FSoftlockScenario> Scenarios = FBeaconSoftlockModel::StandardScenarios(GddIsland());
			const FSoftlockScenario* NoPage = Find(Scenarios, TEXT("página 1 sin leer"));
			if (!TestNotNull(TEXT("Escenario"), NoPage))
			{
				return;
			}
			FSoftlockRules OldRules;
			OldRules.RaftReopenHour = TNumericLimits<double>::Max();
			TestFalse(TEXT("Antes de F3e"), FBeaconSoftlockModel::Evaluate(NoPage->Snapshot, OldRules).bCompletable);
			TestTrue(TEXT("Con reapertura"), FBeaconSoftlockModel::Evaluate(NoPage->Snapshot).bCompletable);
		});

		It(TEXT("una isla sin troncos ni caña impide la balsa y la pesca"), [this]()
		{
			FSoftlockSnapshot S;
			S.Pack.Add(TEXT("cuchillo"), 1);
			S.World.Add(TEXT("fibra"), 15);
			S.World.Add(TEXT("lena"), 20);
			S.World.Add(TEXT("coco"), 16);
			S.Crate.bDelivered = true;
			const FSoftlockVerdict Verdict = FBeaconSoftlockModel::Evaluate(S);
			TestFalse(TEXT("Softlock"), Verdict.bCompletable);
			const FString Text = FBeaconSoftlockModel::Describe(Verdict);
			TestTrue(TEXT("Troncos"), Text.Contains(TEXT("tronco: faltan 5")));
			TestTrue(TEXT("Caña"), Text.Contains(TEXT("cana_bambu")));
		});

		It(TEXT("con la linterna montada no falta nada aunque la isla esté vacía"), [this]()
		{
			FSoftlockSnapshot S;
			S.Progress.bLensCollected = true;
			S.Progress.bWickMade = true;
			S.Progress.OilJars = 2;
			S.Progress.LanternSlots = FBeaconProgressModel::AllSlotsMask;
			TestTrue(TEXT("Completable"), FBeaconSoftlockModel::Evaluate(S).bCompletable);
		});
	});

	Describe(TEXT("máquina de etapas"), [this]()
	{
		It(TEXT("los 24 órdenes (lente, aceite, mecha × página 1 × ciclón) llegan al final"), [this]()
		{
			const TArray<FString> Failures = FBeaconSoftlockModel::StoryOrderFailures();
			TestEqual(TEXT("Sin bloqueos: ") + FString::Join(Failures, TEXT(" | ")), Failures.Num(), 0);
		});
	});
}

#endif
