#include "Misc/AutomationTest.h"

#include "Narrative/FinaleSequenceModel.h"
#include "Narrative/NotebookModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FFinaleSequenceSpec, "Explored.Narrative.FinaleSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FFinaleSequenceSpec)

void FFinaleSequenceSpec::Define()
{
	Describe(TEXT("Tiempos"), [this]()
	{
		It(TEXT("encadena bocina, fundido y epílogo con los valores del GDD"), [this]()
		{
			const FFinaleTiming Timing;
			TestEqual(TEXT("encender"), Timing.IgniteSeconds, 3.0f);
			TestEqual(TEXT("bocina"), FFinaleSequenceModel::HornAtSeconds(Timing), 9.6f, 1e-4f);
			TestEqual(TEXT("fundido"), FFinaleSequenceModel::FadeAtSeconds(Timing), 14.6f, 1e-4f);
			TestEqual(TEXT("epílogo"), FFinaleSequenceModel::EpilogueAtSeconds(Timing), 16.6f, 1e-4f);
			TestEqual(TEXT("retraso del fundido"), FFinaleSequenceModel::FadeDelaySeconds(Timing), 5.0f);
		});

		It(TEXT("sanea valores no finitos, negativos y fuera de rango"), [this]()
		{
			FFinaleTiming Bad;
			Bad.IgniteSeconds = NAN;
			Bad.MistralApproachSeconds = -4.0f;
			Bad.HornAtApproachFraction = 3.0f;
			Bad.HornToFadeSeconds = 0.0f;
			Bad.EndCreditsSeconds = 1e9f;
			Bad.CameraBlendSeconds = -1.0f;
			const FFinaleTiming Clean = FFinaleSequenceModel::Sanitize(Bad);
			TestEqual(TEXT("NaN usa el valor por defecto"), Clean.IgniteSeconds, 3.0f);
			TestEqual(TEXT("llegada mínima"), Clean.MistralApproachSeconds, FFinaleSequenceModel::MinStepSeconds);
			TestEqual(TEXT("fracción a 1"), Clean.HornAtApproachFraction, 1.0f);
			TestEqual(TEXT("fundido mínimo"), Clean.HornToFadeSeconds, FFinaleSequenceModel::MinStepSeconds);
			TestEqual(TEXT("créditos con tope"), Clean.EndCreditsSeconds, FFinaleSequenceModel::MaxSeconds);
			TestEqual(TEXT("cámara sin negativo"), Clean.CameraBlendSeconds, 0.0f);
			TestTrue(TEXT("la bocina nunca a 0"), FFinaleSequenceModel::HornDelaySeconds(Bad) >= FFinaleSequenceModel::MinStepSeconds);
		});

		It(TEXT("la bocina con fracción 0 suena en el mínimo, no en el instante 0"), [this]()
		{
			FFinaleTiming Timing;
			Timing.HornAtApproachFraction = 0.0f;
			TestEqual(TEXT("mínimo"), FFinaleSequenceModel::HornDelaySeconds(Timing), FFinaleSequenceModel::MinStepSeconds);
		});
	});

	Describe(TEXT("PhaseAt"), [this]()
	{
		It(TEXT("recorre las cuatro fases en orden"), [this]()
		{
			const FFinaleTiming Timing;
			TestEqual(TEXT("antes"), FFinaleSequenceModel::PhaseAt(Timing, -1.0f), EFinalePhase::Approach);
			TestEqual(TEXT("NaN"), FFinaleSequenceModel::PhaseAt(Timing, NAN), EFinalePhase::Approach);
			TestEqual(TEXT("llegando"), FFinaleSequenceModel::PhaseAt(Timing, 9.5f), EFinalePhase::Approach);
			TestEqual(TEXT("bocina"), FFinaleSequenceModel::PhaseAt(Timing, 9.6f), EFinalePhase::Horn);
			TestEqual(TEXT("fundido"), FFinaleSequenceModel::PhaseAt(Timing, 15.0f), EFinalePhase::FadeOut);
			TestEqual(TEXT("epílogo"), FFinaleSequenceModel::PhaseAt(Timing, 16.6f), EFinalePhase::Epilogue);
			TestEqual(TEXT("se queda en el epílogo"), FFinaleSequenceModel::PhaseAt(Timing, 500.0f), EFinalePhase::Epilogue);
		});
	});

	Describe(TEXT("Llama y haz"), [this]()
	{
		It(TEXT("la llama crece en los 3 s de encender y se queda en 1"), [this]()
		{
			const FFinaleTiming Timing;
			TestEqual(TEXT("al empezar"), FFinaleSequenceModel::FlameStrength(Timing, 0.0f), 0.0f);
			TestEqual(TEXT("NaN"), FFinaleSequenceModel::FlameStrength(Timing, NAN), 0.0f);
			TestEqual(TEXT("mitad"), FFinaleSequenceModel::FlameStrength(Timing, 1.5f), 0.5f, 1e-4f);
			TestEqual(TEXT("al terminar"), FFinaleSequenceModel::FlameStrength(Timing, 3.0f), 1.0f);
			TestEqual(TEXT("después"), FFinaleSequenceModel::FlameStrength(Timing, 30.0f), 1.0f);
			TestTrue(TEXT("crece"), FFinaleSequenceModel::FlameStrength(Timing, 1.0f) < FFinaleSequenceModel::FlameStrength(Timing, 2.0f));
		});

		It(TEXT("el cono se alarga en 1,5 s"), [this]()
		{
			const FFinaleTiming Timing;
			TestEqual(TEXT("encendida ahora"), FFinaleSequenceModel::BeamGrow(Timing, 0.0f), 0.0f);
			TestEqual(TEXT("mitad"), FFinaleSequenceModel::BeamGrow(Timing, 0.75f), 0.5f, 1e-4f);
			TestEqual(TEXT("largo completo"), FFinaleSequenceModel::BeamGrow(Timing, 1.5f), 1.0f);
		});

		It(TEXT("el parpadeo queda entre 0,8 y 1,2"), [this]()
		{
			for (int32 Step = 0; Step < 2000; ++Step)
			{
				const float Value = FFinaleSequenceModel::Flicker(static_cast<float>(Step) * 0.037f);
				if (Value < 0.8f - 1e-4f || Value > 1.2f + 1e-4f)
				{
					AddError(FString::Printf(TEXT("parpadeo fuera de rango: %.3f"), Value));
					return;
				}
			}
			TestEqual(TEXT("NaN"), FFinaleSequenceModel::Flicker(NAN), 1.0f);
		});
	});

	Describe(TEXT("Epílogo"), [this]()
	{
		It(TEXT("solo cambia con las 8 páginas"), [this]()
		{
			for (int32 Pages = -1; Pages < FNotebookModel::PageCount; ++Pages)
			{
				TestEqual(FString::Printf(TEXT("%d páginas"), Pages), FFinaleSequenceModel::EpilogueForPages(Pages), EBeaconEpilogue::Partial);
			}
			TestEqual(TEXT("8 de 8"), FFinaleSequenceModel::EpilogueForPages(8), EBeaconEpilogue::Complete);
		});

		It(TEXT("coincide con FBeaconProgressModel::EpilogueFor"), [this]()
		{
			FNotebookState Notebook;
			for (int32 Page = 1; Page <= FNotebookModel::PageCount; ++Page)
			{
				TestEqual(FString::Printf(TEXT("con %d leídas"), Page - 1), FFinaleSequenceModel::EpilogueForPages(FNotebookModel::ReadCount(Notebook)),
					FBeaconProgressModel::EpilogueFor(Notebook));
				Notebook = FNotebookModel::MarkRead(Notebook, Page).State;
			}
			TestEqual(TEXT("completo"), FFinaleSequenceModel::EpilogueForPages(FNotebookModel::ReadCount(Notebook)), FBeaconProgressModel::EpilogueFor(Notebook));
			TestEqual(TEXT("y es el completo"), FBeaconProgressModel::EpilogueFor(Notebook), EBeaconEpilogue::Complete);
		});
	});

	Describe(TEXT("CreditsStep"), [this]()
	{
		It(TEXT("vuelve al menú con «Volver» o a los 60 s"), [this]()
		{
			const FFinaleTiming Timing;
			TestEqual(TEXT("recién empezados"), FFinaleSequenceModel::CreditsStep(Timing, 0.0f, false), EFinaleCreditsStep::Stay);
			TestEqual(TEXT("a los 59 s"), FFinaleSequenceModel::CreditsStep(Timing, 59.0f, false), EFinaleCreditsStep::Stay);
			TestEqual(TEXT("a los 60 s"), FFinaleSequenceModel::CreditsStep(Timing, 60.0f, false), EFinaleCreditsStep::MainMenu);
			TestEqual(TEXT("con Volver"), FFinaleSequenceModel::CreditsStep(Timing, 1.0f, true), EFinaleCreditsStep::MainMenu);
			TestEqual(TEXT("NaN"), FFinaleSequenceModel::CreditsStep(Timing, NAN, false), EFinaleCreditsStep::Stay);
		});
	});
}

#endif
