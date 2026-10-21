#include "Narrative/FinaleSequenceModel.h"

#include "Narrative/NotebookModel.h"

namespace FinaleSequenceDetail
{
	float Seconds(float Value, float Fallback, float Min)
	{
		const float Finite = FMath::IsFinite(Value) ? Value : Fallback;
		return FMath::Clamp(Finite, Min, FFinaleSequenceModel::MaxSeconds);
	}

	/** Avance 0-1 suave de Elapsed sobre Duration (Elapsed no finito = 0). */
	float Ramp(float Elapsed, float Duration)
	{
		if (!FMath::IsFinite(Elapsed) || Elapsed <= 0.0f)
		{
			return 0.0f;
		}
		return FMath::SmoothStep(0.0f, 1.0f, FMath::Min(Elapsed / FMath::Max(Duration, FFinaleSequenceModel::MinStepSeconds), 1.0f));
	}
}

FFinaleTiming FFinaleSequenceModel::Sanitize(const FFinaleTiming& Timing)
{
	using namespace FinaleSequenceDetail;
	const FFinaleTiming Defaults;
	FFinaleTiming Out;
	Out.IgniteSeconds = Seconds(Timing.IgniteSeconds, Defaults.IgniteSeconds, MinStepSeconds);
	Out.BeamGrowSeconds = Seconds(Timing.BeamGrowSeconds, Defaults.BeamGrowSeconds, MinStepSeconds);
	Out.CameraBlendSeconds = Seconds(Timing.CameraBlendSeconds, Defaults.CameraBlendSeconds, 0.0f);
	Out.MistralApproachSeconds = Seconds(Timing.MistralApproachSeconds, Defaults.MistralApproachSeconds, MinStepSeconds);
	const float Fraction = FMath::IsFinite(Timing.HornAtApproachFraction) ? Timing.HornAtApproachFraction : Defaults.HornAtApproachFraction;
	Out.HornAtApproachFraction = FMath::Clamp(Fraction, 0.0f, 1.0f);
	Out.HornToFadeSeconds = Seconds(Timing.HornToFadeSeconds, Defaults.HornToFadeSeconds, MinStepSeconds);
	Out.FadeToBlackSeconds = Seconds(Timing.FadeToBlackSeconds, Defaults.FadeToBlackSeconds, MinStepSeconds);
	Out.FadeFromBlackSeconds = Seconds(Timing.FadeFromBlackSeconds, Defaults.FadeFromBlackSeconds, MinStepSeconds);
	Out.EndCreditsSeconds = Seconds(Timing.EndCreditsSeconds, Defaults.EndCreditsSeconds, MinCreditsSeconds);
	return Out;
}

float FFinaleSequenceModel::HornDelaySeconds(const FFinaleTiming& Timing)
{
	const FFinaleTiming T = Sanitize(Timing);
	return FMath::Max(MinStepSeconds, T.MistralApproachSeconds * T.HornAtApproachFraction);
}

float FFinaleSequenceModel::FadeDelaySeconds(const FFinaleTiming& Timing)
{
	return Sanitize(Timing).HornToFadeSeconds;
}

float FFinaleSequenceModel::HornAtSeconds(const FFinaleTiming& Timing)
{
	return HornDelaySeconds(Timing);
}

float FFinaleSequenceModel::FadeAtSeconds(const FFinaleTiming& Timing)
{
	return HornDelaySeconds(Timing) + FadeDelaySeconds(Timing);
}

float FFinaleSequenceModel::EpilogueAtSeconds(const FFinaleTiming& Timing)
{
	return FadeAtSeconds(Timing) + Sanitize(Timing).FadeToBlackSeconds;
}

EFinalePhase FFinaleSequenceModel::PhaseAt(const FFinaleTiming& Timing, float SinceMistral)
{
	if (!FMath::IsFinite(SinceMistral) || SinceMistral < HornAtSeconds(Timing))
	{
		return EFinalePhase::Approach;
	}
	if (SinceMistral < FadeAtSeconds(Timing))
	{
		return EFinalePhase::Horn;
	}
	return SinceMistral < EpilogueAtSeconds(Timing) ? EFinalePhase::FadeOut : EFinalePhase::Epilogue;
}

float FFinaleSequenceModel::FlameStrength(const FFinaleTiming& Timing, float SinceIgnite)
{
	return FinaleSequenceDetail::Ramp(SinceIgnite, Sanitize(Timing).IgniteSeconds);
}

float FFinaleSequenceModel::BeamGrow(const FFinaleTiming& Timing, float SinceLit)
{
	return FinaleSequenceDetail::Ramp(SinceLit, Sanitize(Timing).BeamGrowSeconds);
}

float FFinaleSequenceModel::Flicker(float Time)
{
	if (!FMath::IsFinite(Time))
	{
		return 1.0f;
	}
	// Dos senos de periodos no múltiplos: 0,12 + 0,08 = 0,2 de amplitud máxima.
	return 1.0f + 0.12f * FMath::Sin(Time * 23.0f) + 0.08f * FMath::Sin(Time * 7.3f + 1.1f);
}

EBeaconEpilogue FFinaleSequenceModel::EpilogueForPages(int32 PagesRead)
{
	return PagesRead >= FNotebookModel::PageCount ? EBeaconEpilogue::Complete : EBeaconEpilogue::Partial;
}

EFinaleCreditsStep FFinaleSequenceModel::CreditsStep(const FFinaleTiming& Timing, float CreditsElapsed, bool bBackPressed)
{
	if (bBackPressed)
	{
		return EFinaleCreditsStep::MainMenu;
	}
	if (!FMath::IsFinite(CreditsElapsed))
	{
		return EFinaleCreditsStep::Stay;
	}
	return CreditsElapsed >= Sanitize(Timing).EndCreditsSeconds ? EFinaleCreditsStep::MainMenu : EFinaleCreditsStep::Stay;
}

bool FFinaleSequenceModel::IsEpilogueContinueKey(FName KeyName)
{
	return KeyName == FName(TEXT("Escape"))
		|| KeyName == FName(TEXT("Gamepad_FaceButton_Right"))
		|| KeyName == FName(TEXT("Gamepad_Special_Right"));
}

bool FFinaleSequenceModel::CanOpenPause(bool bCutsceneActive, bool bShowingEnding)
{
	return !bCutsceneActive && !bShowingEnding;
}
