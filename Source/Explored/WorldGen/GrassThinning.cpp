#include "WorldGen/GrassThinning.h"

namespace GrassThinningDetail
{
	/** Tope de niveles (LevelRatio 0,25-0,9 y MinLevel ≥ 0,001). */
	constexpr int32 MaxLevels = 16;
	constexpr float MinFraction = 1e-4f;

	float SmoothStep01(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / FMath::Max(B - A, 1e-4f), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}
}

float FGrassThinning::Fraction(double DistanceM) const
{
	if (!FMath::IsFinite(DistanceM))
	{
		return 0.0f;
	}
	const float D = static_cast<float>(FMath::Max(DistanceM, 0.0));
	if (D >= EndM)
	{
		return 0.0f;
	}
	const float Base = D <= DenseM ? 1.0f : FMath::Pow(DenseM / D, Exponent);
	const float Tail = 1.0f - GrassThinningDetail::SmoothStep01(EndM - EndFadeM, EndM, D);
	return FMath::Clamp(Base * Tail, 0.0f, 1.0f);
}

float FGrassThinning::Visibility(float Rank, double DistanceM) const
{
	const float F = Fraction(DistanceM);
	if (F <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp((F * (1.0f + Band) - Rank) / FMath::Max(Band * F, GrassThinningDetail::MinFraction), 0.0f, 1.0f);
}

float FGrassThinning::Reach(double DistanceM) const
{
	return FMath::Min(Fraction(DistanceM) * (1.0f + Band), 1.0f);
}

float FGrassThinning::Grow(double DistanceM) const
{
	const float F = FMath::Max(Fraction(DistanceM), GrassThinningDetail::MinFraction);
	return FMath::Clamp(1.0f / FMath::Sqrt(F), 1.0f, MaxGrow);
}

int32 FGrassThinning::LevelCount() const
{
	int32 Count = 1;
	float F = 1.0f;
	while (F > MinLevel && Count < GrassThinningDetail::MaxLevels)
	{
		F *= LevelRatio;
		++Count;
	}
	return Count;
}

float FGrassThinning::LevelFraction(int32 Level) const
{
	return FMath::Pow(LevelRatio, static_cast<float>(FMath::Clamp(Level, 0, LevelCount() - 1)));
}

int32 FGrassThinning::LevelFor(double CellDistanceM, double ExtraM) const
{
	const float Needed = Reach(FMath::Max(CellDistanceM - FMath::Max(ExtraM, 0.0), 0.0));
	if (Needed <= 0.0f)
	{
		return INDEX_NONE;
	}
	for (int32 Level = LevelCount() - 1; Level > 0; --Level)
	{
		if (LevelFraction(Level) >= Needed)
		{
			return Level;
		}
	}
	return 0;
}

bool FGrassThinning::Validate(FString& OutError) const
{
	auto Bad = [&OutError](const TCHAR* What) { OutError = What; return false; };
	if (!(DenseM > 0.0f && EndM > DenseM && EndM <= 200.0f)) { return Bad(TEXT("thinning: debe ser 0 < denseM < endM ≤ 200")); }
	if (!(EndFadeM >= 0.0f && EndFadeM <= EndM - DenseM)) { return Bad(TEXT("thinning.endFadeM fuera de 0-(endM - denseM)")); }
	if (!(Exponent >= 0.5f && Exponent <= 4.0f)) { return Bad(TEXT("thinning.exponent fuera de 0,5-4")); }
	if (!(MaxGrow >= 1.0f && MaxGrow <= 4.0f)) { return Bad(TEXT("thinning.maxGrow fuera de 1-4")); }
	if (!(GrowZ >= 0.0f && GrowZ <= 1.0f)) { return Bad(TEXT("thinning.growZ fuera de 0-1")); }
	if (!(Band > 0.0f && Band <= 1.0f)) { return Bad(TEXT("thinning.band fuera de (0, 1]")); }
	if (!(LevelRatio >= 0.25f && LevelRatio <= 0.9f)) { return Bad(TEXT("thinning.levelRatio fuera de 0,25-0,9")); }
	if (!(MinLevel >= 0.001f && MinLevel <= 0.5f)) { return Bad(TEXT("thinning.minLevel fuera de 0,001-0,5")); }
	if (!(MarginM >= 0.0f && MarginM <= 20.0f && HysteresisM >= 0.0f && HysteresisM <= 20.0f))
	{
		return Bad(TEXT("thinning: marginM e hysteresisM entre 0 y 20"));
	}
	return true;
}
