#include "Sky/MoonModel.h"

const TCHAR* LexToString(EMoonPhase Phase)
{
	switch (Phase)
	{
	case EMoonPhase::New: return TEXT("New");
	case EMoonPhase::WaxingCrescent: return TEXT("WaxingCrescent");
	case EMoonPhase::FirstQuarter: return TEXT("FirstQuarter");
	case EMoonPhase::WaxingGibbous: return TEXT("WaxingGibbous");
	case EMoonPhase::Full: return TEXT("Full");
	case EMoonPhase::WaningGibbous: return TEXT("WaningGibbous");
	case EMoonPhase::LastQuarter: return TEXT("LastQuarter");
	case EMoonPhase::WaningCrescent: return TEXT("WaningCrescent");
	default: return TEXT("Unknown");
	}
}

float FMoonModel::Phase(float TotalDays)
{
	// Misma aritmética que tenía ExploredSky::MoonPhase (para días >= 0 el resultado es
	// idéntico); además se envuelve a [0, 1) para instantes negativos.
	float Result = FMath::Fmod(TotalDays / static_cast<float>(DaysPerCycle), 1.0f);
	if (Result < 0.0f)
	{
		Result += 1.0f;
	}
	return Result >= 1.0f ? 0.0f : Result;
}

float FMoonModel::Illumination(float Phase01)
{
	return 0.5f * (1.0f - FMath::Cos(UE_TWO_PI * Phase01));
}

float FMoonModel::IlluminationAt(float TotalDays)
{
	return Illumination(Phase(TotalDays));
}

EMoonPhase FMoonModel::NamedPhase(float Phase01)
{
	// Octavos centrados: la luna nueva ocupa [-1/16, 1/16), la llena [7/16, 9/16)...
	const int32 Octant = FMath::FloorToInt32(FMath::Frac(Phase01 + 1.0f / 16.0f) * 8.0f);
	return static_cast<EMoonPhase>(FMath::Clamp(Octant, 0, static_cast<int32>(EMoonPhase::Count) - 1));
}

int32 FMoonModel::CycleIndex(float TotalDays)
{
	return FMath::FloorToInt32(TotalDays / static_cast<float>(DaysPerCycle));
}

float FMoonModel::FullMoonOfCycle(int32 Cycle)
{
	return static_cast<float>(Cycle) * DaysPerCycle + DaysPerCycle * 0.5f;
}

float FMoonModel::NewMoonOfCycle(int32 Cycle)
{
	return static_cast<float>(Cycle) * DaysPerCycle;
}

bool FMoonModel::IsFullMoonWindow(float TotalDays)
{
	return FMath::Abs(TotalDays - FullMoonOfCycle(CycleIndex(TotalDays))) <= PhaseWindowDays;
}

bool FMoonModel::IsNewMoonWindow(float TotalDays)
{
	// La luna nueva más cercana puede ser la que abre este ciclo o la que abre el siguiente.
	const int32 Cycle = CycleIndex(TotalDays);
	const float Distance = FMath::Min(FMath::Abs(TotalDays - NewMoonOfCycle(Cycle)), FMath::Abs(NewMoonOfCycle(Cycle + 1) - TotalDays));
	return Distance <= PhaseWindowDays;
}

float FMoonModel::NextFullMoon(float AfterDays)
{
	int32 Cycle = CycleIndex(AfterDays);
	while (FullMoonOfCycle(Cycle) <= AfterDays)
	{
		++Cycle;
	}
	return FullMoonOfCycle(Cycle);
}

float FMoonModel::NextNewMoon(float AfterDays)
{
	int32 Cycle = CycleIndex(AfterDays);
	while (NewMoonOfCycle(Cycle) <= AfterDays)
	{
		++Cycle;
	}
	return NewMoonOfCycle(Cycle);
}

float FMoonModel::Bioluminescence(float Phase01)
{
	// Cae deprisa en cuanto la Luna empieza a alumbrar: la oscuridad manda.
	return FMath::Square(1.0f - Illumination(Phase01));
}
