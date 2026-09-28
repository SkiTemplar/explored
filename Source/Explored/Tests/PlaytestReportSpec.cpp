#include "Misc/AutomationTest.h"

#include "Debug/PlaytestReportModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FPlaytestReportSpec, "Explored.Debug.PlaytestReport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FPlaytestReportSpec)

void FPlaytestReportSpec::Define()
{
	It("clasifica una instancia por encima del umbral como flotando", [this]()
	{
		bool bFloating = false, bBuried = false;
		const bool bIssue = FPlaytestReportModel::IsFloatingOrBuried(FPlaytestReportModel::FloatingThresholdCm + 1.0f, bFloating, bBuried);
		TestTrue(TEXT("Es un defecto"), bIssue);
		TestTrue(TEXT("Flotando"), bFloating);
		TestFalse(TEXT("No enterrado"), bBuried);
	});

	It("clasifica una instancia por debajo del umbral como enterrada", [this]()
	{
		bool bFloating = false, bBuried = false;
		const bool bIssue = FPlaytestReportModel::IsFloatingOrBuried(FPlaytestReportModel::BuriedThresholdCm - 1.0f, bFloating, bBuried);
		TestTrue(TEXT("Es un defecto"), bIssue);
		TestFalse(TEXT("No flotando"), bFloating);
		TestTrue(TEXT("Enterrado"), bBuried);
	});

	It("no marca defecto dentro del margen de tolerancia", [this]()
	{
		bool bFloating = false, bBuried = false;
		const bool bIssue = FPlaytestReportModel::IsFloatingOrBuried(0.0f, bFloating, bBuried);
		TestFalse(TEXT("Sin defecto sobre el suelo"), bIssue);
	});

	It("detecta deriva de física por distancia recorrida", [this]()
	{
		const FVector Start(0.0, 0.0, 100.0);
		const FVector Quieto(0.5, 0.0, 100.0);
		const FVector Caido(0.0, 0.0, 50.0);
		TestFalse(TEXT("Sin deriva significativa"), FPlaytestReportModel::DidPhysicsDrift(Start, Quieto));
		TestTrue(TEXT("Deriva por caída"), FPlaytestReportModel::DidPhysicsDrift(Start, Caido));
	});

	It("acumula defectos y los vuelca a JSON válido en su forma (llaves y corchetes balanceados)", [this]()
	{
		FPlaytestReport Report;
		FPlaytestReportModel::AppendMaterialIssue(Report, TEXT("SM_AcantiladoBloque_01"), TEXT("0"), 12, FVector(10.0, 20.0, 5.0));
		FPlaytestReportModel::AppendPhysicsDriftIssue(Report, TEXT("Platano_3"), FVector(1, 1, 3), FVector(1, 1, -2), true, false);
		FPlaytestReportModel::AppendFloatingOrBuriedIssue(Report, TEXT("SM_Palmera_02"), FVector(4, 4, 2), 45.0f, true);
		FPlaytestReportModel::AppendSpawnBlockedIssue(Report, TEXT("PlayerStart_0"), FVector(0, 0, 0), TEXT("AExploredVegetationCell_3"));
		Report.FrameSamples.Add({ TEXT("Landing_orilla_amanecer"), 58.2f, 41.0f, 1024.5 });

		TestEqual(TEXT("Cuatro defectos"), Report.Issues.Num(), 4);

		const FString Json = FPlaytestReportModel::ToJson(Report);
		int32 OpenBraces = 0, CloseBraces = 0, OpenBrackets = 0, CloseBrackets = 0;
		for (int32 Index = 0; Index < Json.Len(); ++Index)
		{
			const TCHAR Character = Json[Index];
			if (Character == '{') ++OpenBraces;
			if (Character == '}') ++CloseBraces;
			if (Character == '[') ++OpenBrackets;
			if (Character == ']') ++CloseBrackets;
		}
		TestEqual(TEXT("Llaves balanceadas"), OpenBraces, CloseBraces);
		TestEqual(TEXT("Corchetes balanceados"), OpenBrackets, CloseBrackets);
		TestTrue(TEXT("Incluye el tipo de defecto"), Json.Contains(TEXT("MaterialMissing")));
		TestTrue(TEXT("Incluye la muestra de rendimiento"), Json.Contains(TEXT("Landing_orilla_amanecer")));

		const FString Summary = FPlaytestReportModel::ToReadableSummary(Report);
		TestTrue(TEXT("El resumen menciona los defectos de material"), Summary.Contains(TEXT("Materiales nulos o por defecto")));
		TestTrue(TEXT("El resumen menciona el rendimiento"), Summary.Contains(TEXT("58.2")));
	});

	It("escapa comillas y barras invertidas al serializar a JSON", [this]()
	{
		FPlaytestReport Report;
		FPlaytestReportModel::AppendSpawnBlockedIssue(Report, TEXT("Spawn \"principal\""), FVector::ZeroVector, TEXT("C:\\ruta\\rara"));
		const FString Json = FPlaytestReportModel::ToJson(Report);
		TestTrue(TEXT("Comillas escapadas"), Json.Contains(TEXT("\\\"principal\\\"")));
		TestTrue(TEXT("Barra invertida escapada"), Json.Contains(TEXT("C:\\\\ruta\\\\rara")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
