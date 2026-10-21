#include "Misc/AutomationTest.h"

#include "Debug/PlaytestReportModel.h"
#include "Debug/PlaytestRunModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PlaytestRunTest
{
	/** Partida que cumple el criterio: 10 etapas, epílogo, 50 min, sin errores. */
	FPlaytestRunSummary GoodRun()
	{
		FPlaytestRunSummary Run;
		Run.Route = TEXT("competente");
		Run.bRan = true;
		Run.bReachedEpilogue = true;
		Run.Clock.Est1xSeconds = 49.0 * 60.0;
		Run.CreditsSeconds = 60.0;
		for (int32 Stage = 0; Stage < FPlaytestRunModel::StageCount; ++Stage)
		{
			Run = FPlaytestRunModel::MarkStage(Run, Stage, FString::Printf(TEXT("E%d"), Stage));
		}
		return Run;
	}
}

BEGIN_DEFINE_SPEC(FPlaytestRunSpec, "Explored.Debug.PlaytestRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FPlaytestRunSpec)

void FPlaytestRunSpec::Define()
{
	using namespace PlaytestRunTest;

	Describe(TEXT("reloj a 1×"), [this]()
	{
		It(TEXT("un segundo real a ×6 cuenta 6 s de partida; a ×1, uno"), [this]()
		{
			FPlaytestRunClock Clock;
			Clock = FPlaytestRunModel::Accumulate(Clock, 1.0, 6.0);
			Clock = FPlaytestRunModel::Accumulate(Clock, 2.0, 1.0);
			TestEqual(TEXT("Real"), Clock.RealSeconds, 3.0);
			TestEqual(TEXT("A 1×"), Clock.Est1xSeconds, 8.0);
		});

		It(TEXT("dt o escala no válidos no suman (caso negativo)"), [this]()
		{
			FPlaytestRunClock Clock;
			Clock = FPlaytestRunModel::Accumulate(Clock, -1.0, 6.0);
			Clock = FPlaytestRunModel::Accumulate(Clock, 1.0, std::numeric_limits<double>::quiet_NaN());
			Clock = FPlaytestRunModel::Accumulate(Clock, std::numeric_limits<double>::infinity(), 1.0);
			TestEqual(TEXT("Nada"), Clock.Est1xSeconds, 0.0);
			TestEqual(TEXT("Nada real"), Clock.RealSeconds, 0.0);
		});

		It(TEXT("un trayecto de 90 m a pie son 25 s a 1× y 5 s reales a ×5"), [this]()
		{
			const double Seconds = FPlaytestRunModel::TravelSeconds1x(90.0);
			TestEqual(TEXT("90 × 1,25 / 4,5"), Seconds, 25.0);
			TestEqual(TEXT("A ×5"), FPlaytestRunModel::RealWaitSeconds(Seconds, 5.0), 5.0);
			TestEqual(TEXT("Escala < 1 cuenta como 1"), FPlaytestRunModel::RealWaitSeconds(Seconds, 0.5), 25.0);
			TestEqual(TEXT("Distancia negativa"), FPlaytestRunModel::TravelSeconds1x(-3.0), 0.0);
		});

		It(TEXT("formatea la hora absoluta como día y hora"), [this]()
		{
			TestEqual(TEXT("D2 15:40"), FPlaytestRunModel::FormatGameClock(24.0 + 15.0 + 40.0 / 60.0), FString(TEXT("D2 15:40")));
			TestEqual(TEXT("D1 07:00"), FPlaytestRunModel::FormatGameClock(7.0), FString(TEXT("D1 07:00")));
			TestEqual(TEXT("No válida"), FPlaytestRunModel::FormatGameClock(-1.0), FString(TEXT("?")));
		});
	});

	Describe(TEXT("criterio de salida"), [this]()
	{
		It(TEXT("una partida completa de 50 min sin errores pasa"), [this]()
		{
			const FPlaytestRunSummary Run = GoodRun();
			TestEqual(TEXT("50 min con créditos"), FPlaytestRunModel::EstimatedMinutes(Run), 50.0);
			TestTrue(TEXT("Etapas"), FPlaytestRunModel::AllStagesReached(Run));
			TestTrue(TEXT("Pasa"), FPlaytestRunModel::Passed(Run));
		});

		It(TEXT("fuera de 47-65 min, sin epílogo, con errores o con un fallo de guardado no pasa"), [this]()
		{
			FPlaytestRunSummary Short = GoodRun();
			Short.Clock.Est1xSeconds = 40.0 * 60.0;
			TestFalse(TEXT("Corta"), FPlaytestRunModel::Passed(Short));
			FPlaytestRunSummary Long = GoodRun();
			Long.Clock.Est1xSeconds = 70.0 * 60.0;
			TestFalse(TEXT("Larga"), FPlaytestRunModel::IsDurationInTarget(Long));
			FPlaytestRunSummary NoEnd = GoodRun();
			NoEnd.bReachedEpilogue = false;
			TestFalse(TEXT("Sin epílogo"), FPlaytestRunModel::Passed(NoEnd));
			const FPlaytestRunSummary Errors = FPlaytestRunModel::AddLogError(GoodRun(), TEXT("LogExplored: Error: x"));
			TestFalse(TEXT("Con errores"), FPlaytestRunModel::Passed(Errors));
			FPlaytestRunSummary Save = GoodRun();
			Save.SaveLoadFailures = 1;
			TestFalse(TEXT("Guardado roto"), FPlaytestRunModel::Passed(Save));
		});

		It(TEXT("las etapas se cuentan una vez y fuera de rango se ignoran"), [this]()
		{
			FPlaytestRunSummary Run;
			Run = FPlaytestRunModel::MarkStage(Run, 2, TEXT("Faro"));
			Run = FPlaytestRunModel::MarkStage(Run, 2, TEXT("Faro"));
			Run = FPlaytestRunModel::MarkStage(Run, 10, TEXT("?"));
			Run = FPlaytestRunModel::MarkStage(Run, -1, TEXT("?"));
			TestEqual(TEXT("Una"), Run.StagesReached.Num(), 1);
			TestFalse(TEXT("Incompleta"), FPlaytestRunModel::AllStagesReached(Run));
		});

		It(TEXT("las muestras de error tienen tope"), [this]()
		{
			FPlaytestRunSummary Run;
			for (int32 Index = 0; Index < 30; ++Index)
			{
				Run = FPlaytestRunModel::AddLogError(Run, TEXT("e"));
			}
			TestEqual(TEXT("30 errores"), Run.LogErrors, 30);
			TestEqual(TEXT("Tope de muestras"), Run.ErrorSamples.Num(), FPlaytestRunModel::MaxErrorSamples);
		});
	});

	Describe(TEXT("informe"), [this]()
	{
		It(TEXT("el JSON y el resumen llevan la duración, las etapas y los softlocks"), [this]()
		{
			FPlaytestReport Report;
			Report.Run = GoodRun();
			Report.Softlocks.Add({ TEXT("caja \"vacía\""), false, TEXT("faltan: lena") });
			const FString Json = FPlaytestReportModel::ToJson(Report);
			TestTrue(TEXT("Minutos"), Json.Contains(TEXT("\"estimated_1x_minutes\": 50.00")));
			TestTrue(TEXT("Escapa comillas"), Json.Contains(TEXT("caja \\\"vacía\\\"")));
			TestTrue(TEXT("Softlocks"), Json.Contains(TEXT("\"softlocks_found\": 1")));
			TestTrue(TEXT("No pasa con un softlock"), Json.Contains(TEXT("\"passed\": false")));
			const FString Text = FPlaytestReportModel::ToReadableSummary(Report);
			TestTrue(TEXT("Resumen"), Text.Contains(TEXT("SOFTLOCK")) && Text.Contains(TEXT("RESULTADO: ROJO")));
			Report.Softlocks[0].bCompletable = true;
			TestTrue(TEXT("Verde"), FPlaytestReportModel::ToReadableSummary(Report).Contains(TEXT("RESULTADO: VERDE")));
		});

		It(TEXT("sin partida ni softlocks el resumen no añade la sección"), [this]()
		{
			const FString Text = FPlaytestReportModel::ToReadableSummary(FPlaytestReport());
			TestFalse(TEXT("Sin sección"), Text.Contains(TEXT("Partida completa del bot")));
			TestTrue(TEXT("JSON con run"), FPlaytestReportModel::ToJson(FPlaytestReport()).Contains(TEXT("\"run\": {")));
		});
	});
}

#endif
