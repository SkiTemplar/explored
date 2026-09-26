#include "Fauna/FaunaAnimation.h"

#include "Core/ExploredRandom.h"

namespace FaunaAnimationDetail
{
	/** Frecuencia en reposo y a velocidad máxima (Hz) y amplitud en reposo y máxima por estilo. */
	struct FStyleCurve
	{
		float IdleHz;
		float MaxHz;
		float IdleAmplitude;
		float MaxAmplitude;
	};

	FStyleCurve CurveFor(EFaunaAnimStyle Style)
	{
		switch (Style)
		{
		case EFaunaAnimStyle::SpineWave: return {0.8f, 4.0f, 0.04f, 0.12f};
		case EFaunaAnimStyle::DiscWave: return {0.3f, 1.6f, 0.03f, 0.10f};
		case EFaunaAnimStyle::FlukeWave: return {0.3f, 1.4f, 0.03f, 0.08f};
		case EFaunaAnimStyle::Flipper: return {0.25f, 0.8f, 0.15f, 0.35f};
		case EFaunaAnimStyle::Pulse: return {0.7f, 0.7f, 0.18f, 0.18f};
		case EFaunaAnimStyle::Flap: return {2.0f, 3.5f, 20.0f, 45.0f};
		default: return {1.0f, 1.0f, 0.0f, 0.0f};
		}
	}
}

float FFaunaAnimation::InitialPhase(uint32 Seed)
{
	return ExploredHash::ToUnitFloat(ExploredHash::Hash32(Seed ^ 0xA11Au));
}

FFaunaAnimParams FFaunaAnimation::Advance(EFaunaSpecies Species, FFaunaAnimState& State, float SpeedCmS,
	float VerticalSpeedCmS, float DeltaSeconds, bool bResting)
{
	using namespace FaunaAnimationDetail;
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Species);
	const FStyleCurve Curve = CurveFor(Info.AnimStyle);
	const float Ratio = FMath::Clamp(SpeedCmS / FMath::Max(1.0f, Info.MaxSpeedCmS), 0.0f, 1.0f);

	FFaunaAnimParams Out;
	Out.WingFold = 0.0f;
	if (bResting)
	{
		Out.Phase01 = State.Phase01;
		return Out;
	}

	switch (Info.AnimStyle)
	{
	case EFaunaAnimStyle::Pulse:
	{
		// La medusa late a su ritmo, se mueva o no; la contracción es rápida y la expansión lenta.
		Out.FrequencyHz = Curve.IdleHz;
		Out.Amplitude = Curve.IdleAmplitude;
		break;
	}
	case EFaunaAnimStyle::Flap:
	{
		// Aleteo más rápido al acelerar; al bajar planean (alas abiertas y quietas). Nunca plegadas.
		const float Glide = FMath::Clamp(-VerticalSpeedCmS / 200.0f, 0.0f, 1.0f);
		Out.FrequencyHz = FMath::Lerp(Curve.IdleHz, Curve.MaxHz, Ratio) * (1.0f - 0.7f * Glide);
		Out.Amplitude = FMath::Lerp(Curve.IdleAmplitude, Curve.MaxAmplitude, Ratio) * (1.0f - Glide);
		Out.Secondary = Glide;
		break;
	}
	default:
		Out.FrequencyHz = FMath::Lerp(Curve.IdleHz, Curve.MaxHz, Ratio);
		Out.Amplitude = FMath::Lerp(Curve.IdleAmplitude, Curve.MaxAmplitude, Ratio);
		Out.Secondary = Info.AnimStyle == EFaunaAnimStyle::FlukeWave ? 1.0f : 0.0f;
		break;
	}

	State.Phase01 = FMath::Frac(State.Phase01 + Out.FrequencyHz * FMath::Max(0.0f, DeltaSeconds));
	if (State.Phase01 >= 1.0f)
	{
		State.Phase01 = 0.0f;
	}
	Out.Phase01 = State.Phase01;
	if (Info.AnimStyle == EFaunaAnimStyle::Pulse)
	{
		// Contracción: sube deprisa en el primer 30 % del ciclo y se relaja en el resto.
		const float P = Out.Phase01;
		Out.Secondary = P < 0.3f ? FMath::SmoothStep(0.0f, 0.3f, P) : 1.0f - FMath::SmoothStep(0.3f, 1.0f, P);
	}
	return Out;
}
