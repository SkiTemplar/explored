#include "Misc/AutomationTest.h"

#include "Narrative/RunPacingModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FRunPacingSpec, "Explored.Narrative.RunPacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FRunPacingSpec)

void FRunPacingSpec::Define()
{
	Describe(TEXT("Competent"), [this]()
	{
		It(TEXT("cuenta 47 h despierto con los valores del GDD"), [this]()
		{
			const FRunPacing Pacing = FRunPacingModel::Competent(FRunPacingInput());
			TestEqual(TEXT("día 1: 07:00-20:00"), Pacing.Day1Hours, 13.0, 1e-9);
			TestEqual(TEXT("día 2: 06:00-02:00 con el ciclón"), Pacing.Day2Hours, 20.0, 1e-9);
			TestEqual(TEXT("día 3: 06:00-20:00"), Pacing.Day3Hours, 14.0, 1e-9);
			TestEqual(TEXT("total"), Pacing.AwakeHours, 47.0, 1e-9);
		});

		It(TEXT("el día de 26 min deja la partida competente en 50-60 min"), [this]()
		{
			const FRunPacing Pacing = FRunPacingModel::Competent(FRunPacingInput());
			TestEqual(TEXT("juego hasta encender"), Pacing.PlayMinutes, 47.0 * 26.0 / 24.0, 1e-9);
			TestEqual(TEXT("final hasta el epílogo"), Pacing.FinaleMinutes, (3.0 + 16.6) / 60.0, 1e-4);
			TestEqual(TEXT("total con créditos"), Pacing.TotalMinutes, 52.24, 0.01);
			TestTrue(TEXT("en el objetivo"), FRunPacingModel::IsInTarget(Pacing));
		});

		It(TEXT("con el día de 24 min de F3e se queda corta (caso negativo)"), [this]()
		{
			FRunPacingInput Input;
			Input.DayLengthMinutes = 24.0;
			const FRunPacing Pacing = FRunPacingModel::Competent(Input);
			TestTrue(TEXT("por debajo de 50"), Pacing.TotalMinutes < FRunPacingModel::TargetMinMinutes);
			TestFalse(TEXT("fuera del objetivo"), FRunPacingModel::IsInTarget(Pacing));
		});

		It(TEXT("con un día de 32 min se pasa de 60"), [this]()
		{
			FRunPacingInput Input;
			Input.DayLengthMinutes = 32.0;
			TestFalse(TEXT("fuera del objetivo"), FRunPacingModel::IsInTarget(FRunPacingModel::Competent(Input)));
		});

		It(TEXT("no da horas negativas ni NaN con valores rotos"), [this]()
		{
			FRunPacingInput Input;
			Input.StartHour = 30.0;
			Input.DayLengthMinutes = NAN;
			Input.Director.CycloneEndHour = 10.0;
			const FRunPacing Pacing = FRunPacingModel::Competent(Input);
			TestEqual(TEXT("día 1"), Pacing.Day1Hours, 0.0);
			TestEqual(TEXT("día 2"), Pacing.Day2Hours, 0.0);
			TestEqual(TEXT("minutos de juego"), Pacing.PlayMinutes, 0.0);
			TestTrue(TEXT("total finito"), FMath::IsFinite(Pacing.TotalMinutes));
		});
	});
}

#endif
