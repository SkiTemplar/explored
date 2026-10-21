#include "Misc/AutomationTest.h"

#include "Debug/BenchBudgetModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FBenchBudgetSpec, "Explored.Debug.BenchBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	static TArray<float> Frames(int32 Count, float Seconds)
	{
		TArray<float> Out;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Out.Add(Seconds);
		}
		return Out;
	}
END_DEFINE_SPEC(FBenchBudgetSpec)

void FBenchBudgetSpec::Define()
{
	Describe("ReduceView", [this]()
	{
		It("aprueba 100 fotogramas a 16 ms (62,5 fps)", [this]()
		{
			const FBenchViewStats Stats = FBenchBudgetModel::ReduceView(TEXT("cala"), Frames(100, 0.016f));
			TestEqual(TEXT("fotogramas"), Stats.Frames, 100);
			TestEqual(TEXT("media"), Stats.AverageFps, 62.5f, 0.01f);
			TestEqual(TEXT("mínimo"), Stats.MinimumFps, 62.5f, 0.01f);
			TestEqual(TEXT("segundos"), Stats.Seconds, 1.6f, 0.001f);
			TestTrue(TEXT("aprueba"), Stats.bPassed);
		});

		It("la media es fotogramas / tiempo, no la media de los fps", [this]()
		{
			// 1 fotograma de 100 ms y 9 de 10 ms: 10 / 0,19 s = 52,6 fps (la media aritmética de
			// los fps daría 91 y aprobaría en falso).
			TArray<float> Mixed = Frames(9, 0.010f);
			Mixed.Add(0.100f);
			const FBenchViewStats Stats = FBenchBudgetModel::ReduceView(TEXT("palmar"), Mixed);
			TestEqual(TEXT("media"), Stats.AverageFps, 10.0f / 0.19f, 0.01f);
			TestFalse(TEXT("no aprueba"), Stats.bPassed);
		});

		It("un solo tirón por debajo de 45 fps suspende la vista aunque la media sea alta (caso negativo)", [this]()
		{
			TArray<float> Hitch = Frames(299, 0.008f);
			Hitch.Add(1.0f / 40.0f);
			const FBenchViewStats Stats = FBenchBudgetModel::ReduceView(TEXT("faro"), Hitch);
			TestTrue(TEXT("media alta"), Stats.AverageFps > 100.0f);
			TestEqual(TEXT("mínimo"), Stats.MinimumFps, 40.0f, 0.01f);
			TestFalse(TEXT("suspende"), Stats.bPassed);
		});

		It("el 1 % bajo sale del percentil y no del mínimo", [this]()
		{
			// 200 fotogramas: 2 a 25 fps y 198 a 100 fps; el índice del percentil 1 (floor(0,01 × 199) = 1)
			// cae en el segundo más lento.
			TArray<float> Data = Frames(198, 0.010f);
			Data.Add(0.040f);
			Data.Add(0.040f);
			const FBenchViewStats Stats = FBenchBudgetModel::ReduceView(TEXT("ciclon"), Data);
			TestEqual(TEXT("1 %"), Stats.OnePercentLowFps, 25.0f, 0.01f);
		});

		It("descarta tiempos no válidos y no aprueba sin fotogramas suficientes", [this]()
		{
			TArray<float> Bad = { 0.0f, -0.016f, 0.016f };
			const FBenchViewStats Stats = FBenchBudgetModel::ReduceView(TEXT("vacia"), Bad);
			TestEqual(TEXT("un fotograma válido"), Stats.Frames, 1);
			TestFalse(TEXT("menos de 30 fotogramas"), Stats.bPassed);

			const FBenchViewStats Empty = FBenchBudgetModel::ReduceView(TEXT("nada"), TArray<float>());
			TestEqual(TEXT("sin fotogramas"), Empty.Frames, 0);
			TestEqual(TEXT("media 0"), Empty.AverageFps, 0.0f);
			TestFalse(TEXT("no aprueba"), Empty.bPassed);
		});
	});

	Describe("Summarize", [this]()
	{
		It("junta las vistas por tiempo total y aprueba si todas cumplen", [this]()
		{
			TArray<FBenchViewStats> Views;
			Views.Add(FBenchBudgetModel::ReduceView(TEXT("a"), Frames(100, 0.010f)));
			Views.Add(FBenchBudgetModel::ReduceView(TEXT("b"), Frames(100, 0.015f)));
			const FBenchSummary Summary = FBenchBudgetModel::Summarize(Views);
			TestEqual(TEXT("vistas"), Summary.Views, 2);
			TestEqual(TEXT("fotogramas"), Summary.Frames, 200);
			TestEqual(TEXT("media"), Summary.AverageFps, 200.0f / 2.5f, 0.01f);
			TestEqual(TEXT("mínimo"), Summary.MinimumFps, 1.0f / 0.015f, 0.01f);
			TestTrue(TEXT("aprueba"), Summary.bPassed);
			TestTrue(TEXT("sin vistas fuera"), Summary.FailedViews.IsEmpty());
		});

		It("una vista fuera de presupuesto suspende el recorrido y se nombra (caso negativo)", [this]()
		{
			TArray<FBenchViewStats> Views;
			Views.Add(FBenchBudgetModel::ReduceView(TEXT("bench-cala"), Frames(100, 0.010f)));
			Views.Add(FBenchBudgetModel::ReduceView(TEXT("bench-ciclon"), Frames(100, 0.020f)));
			const FBenchSummary Summary = FBenchBudgetModel::Summarize(Views);
			TestFalse(TEXT("suspende"), Summary.bPassed);
			TestEqual(TEXT("una vista fuera"), Summary.FailedViews.Num(), 1);
			TestEqual(TEXT("la del ciclón"), Summary.FailedViews[0], FString(TEXT("bench-ciclon")));
			TestTrue(TEXT("el resumen la nombra"), FBenchBudgetModel::FormatSummary(Summary).Contains(TEXT("bench-ciclon")));
		});

		It("un recorrido sin vistas no aprueba", [this]()
		{
			const FBenchSummary Summary = FBenchBudgetModel::Summarize(TArray<FBenchViewStats>());
			TestFalse(TEXT("vacío"), Summary.bPassed);
			TestEqual(TEXT("media 0"), Summary.AverageFps, 0.0f);
		});

		It("respeta un presupuesto distinto", [this]()
		{
			FBenchBudget Lax;
			Lax.MinAverageFps = 30.0f;
			Lax.MinFrameFps = 20.0f;
			TArray<FBenchViewStats> Views;
			Views.Add(FBenchBudgetModel::ReduceView(TEXT("a"), Frames(100, 0.025f), Lax));
			TestTrue(TEXT("a 40 fps aprueba con 30/20"), FBenchBudgetModel::Summarize(Views, Lax).bPassed);
			TestFalse(TEXT("y suspende con 60/45"), FBenchBudgetModel::Summarize(Views).bPassed);
		});
	});

	Describe("SweepYawOffset", [this]()
	{
		It("va de -HalfSweep a +HalfSweep y se satura fuera de 0..1", [this]()
		{
			TestEqual(TEXT("inicio"), FBenchBudgetModel::SweepYawOffset(0.0f, 30.0f), -30.0f);
			TestEqual(TEXT("mitad"), FBenchBudgetModel::SweepYawOffset(0.5f, 30.0f), 0.0f, 0.001f);
			TestEqual(TEXT("final"), FBenchBudgetModel::SweepYawOffset(1.0f, 30.0f), 30.0f);
			TestEqual(TEXT("más allá"), FBenchBudgetModel::SweepYawOffset(3.0f, 30.0f), 30.0f);
			TestEqual(TEXT("negativo"), FBenchBudgetModel::SweepYawOffset(-1.0f, 30.0f), -30.0f);
		});
	});

	Describe("FormatView", [this]()
	{
		It("escribe la línea [Bench] con el veredicto", [this]()
		{
			const FString Line = FBenchBudgetModel::FormatView(FBenchBudgetModel::ReduceView(TEXT("bench-palmar"), Frames(60, 0.030f)));
			TestTrue(TEXT("prefijo"), Line.StartsWith(TEXT("[Bench] fps bench-palmar:")));
			TestTrue(TEXT("falla"), Line.EndsWith(TEXT("FALLA")));
		});
	});
}

#endif
