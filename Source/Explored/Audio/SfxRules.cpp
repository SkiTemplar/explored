#include "Audio/SfxRules.h"

namespace SfxRulesDetail
{
	const TCHAR* const SurfaceKeys[static_cast<int32>(EFootSurface::Count)] = {
		TEXT("sand"), TEXT("grass"), TEXT("rock"), TEXT("wood"), TEXT("water"),
	};
}

const TCHAR* LexToString(EFootSurface Surface)
{
	const int32 Index = static_cast<int32>(Surface);
	return Index >= 0 && Index < static_cast<int32>(EFootSurface::Count) ? SfxRulesDetail::SurfaceKeys[Index] : TEXT("sand");
}

EFootSurface ExploredSfxRules::SurfaceFor(const FFootGround& Ground)
{
	if (Ground.WaterDepthM > WadeDepthM)
	{
		return EFootSurface::Water;
	}
	if (!Ground.bTerrain)
	{
		return EFootSurface::Wood;
	}
	if (Ground.SlopeDeg >= RockSlopeDeg)
	{
		return EFootSurface::Rock;
	}
	return Ground.HeightM < SandTopM ? EFootSurface::Sand : EFootSurface::Grass;
}

FString ExploredSfxRules::FootstepSound(EFootSurface Surface, int32 Variant)
{
	const int32 N = (FMath::Abs(Variant) % 4) + 1;
	return FString::Printf(TEXT("sfx_footstep_%s_%02d"), LexToString(Surface), N);
}

float ExploredSfxRules::StrideFor(float SpeedCmS)
{
	return SpeedCmS >= RunSpeedCmS ? RunStrideCm : WalkStrideCm;
}

bool ExploredSfxRules::GullsAudible(float Coast, float Night, float Rain)
{
	return Coast > 0.35f && Night < 0.5f && Rain < 0.5f;
}

float ExploredSfxRules::GullGap(float Random01)
{
	return FMath::Lerp(GullMinGapS, GullMaxGapS, FMath::Clamp(Random01, 0.0f, 1.0f));
}

bool FSfxCadence::Advance(float Amount, float Period)
{
	if (!FMath::IsFinite(Amount) || !FMath::IsFinite(Period) || Period <= 0.0f || Amount <= 0.0f)
	{
		return false;
	}
	Accumulated += Amount;
	if (Accumulated < Period)
	{
		return false;
	}
	// Un salto grande (teletransporte, carga) no encadena pasos: se descarta lo que sobra.
	Accumulated = FMath::Fmod(Accumulated, Period);
	return true;
}
