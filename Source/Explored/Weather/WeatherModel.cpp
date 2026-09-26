#include "Weather/WeatherModel.h"

#include "Core/ExploredRandom.h"
#include "Misc/ScopeLock.h"

namespace
{
	/** Duración de la transición entre bloques (días). */
	constexpr float TransitionDays = 0.06f;
	/** Antelación con que se notan los temporales (días). */
	constexpr float WarningDays = 1.5f;

	struct FStateWeight
	{
		EWeatherState State;
		float Weight;
	};

	/** Probabilidades de cada estado por estación (tabla de la biblia §6.1–6.2). */
	TArray<FStateWeight> WeightsFor(ESeason Season)
	{
		using W = EWeatherState;
		switch (Season)
		{
		case ESeason::Dry:
			return {{W::Clear, 5.0f}, {W::Cloudy, 1.5f}, {W::MorningFog, 0.6f}, {W::HeatWave, 1.0f}, {W::LightRain, 0.3f}};
		case ESeason::FirstRains:
			return {{W::Clear, 2.0f}, {W::Cloudy, 2.0f}, {W::Shower, 2.0f}, {W::Thunderstorm, 1.3f}, {W::LightRain, 1.0f}, {W::MorningFog, 0.8f}};
		case ESeason::Monsoon:
			return {{W::Cloudy, 1.5f}, {W::LightRain, 2.5f}, {W::Shower, 2.5f}, {W::Thunderstorm, 1.0f}, {W::MorningFog, 1.5f}, {W::Clear, 0.6f}};
		case ESeason::Cyclones:
		default:
			return {{W::Clear, 2.0f}, {W::Cloudy, 2.0f}, {W::HeatWave, 1.0f}, {W::Shower, 1.0f}, {W::Gale, 0.8f}};
		}
	}

	EWeatherState Pick(const TArray<FStateWeight>& Weights, FExploredRandom& Rng)
	{
		float Total = 0.0f;
		for (const FStateWeight& W : Weights)
		{
			Total += W.Weight;
		}
		float R = Rng.NextFloat() * Total;
		for (const FStateWeight& W : Weights)
		{
			R -= W.Weight;
			if (R <= 0.0f)
			{
				return W.State;
			}
		}
		return Weights.Last().State;
	}
}

const TCHAR* LexToString(ESeason Season)
{
	switch (Season)
	{
	case ESeason::Dry: return TEXT("Dry");
	case ESeason::FirstRains: return TEXT("FirstRains");
	case ESeason::Monsoon: return TEXT("Monsoon");
	case ESeason::Cyclones: return TEXT("Cyclones");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(EWeatherState State)
{
	switch (State)
	{
	case EWeatherState::Clear: return TEXT("Clear");
	case EWeatherState::Cloudy: return TEXT("Cloudy");
	case EWeatherState::MorningFog: return TEXT("MorningFog");
	case EWeatherState::LightRain: return TEXT("LightRain");
	case EWeatherState::Shower: return TEXT("Shower");
	case EWeatherState::Thunderstorm: return TEXT("Thunderstorm");
	case EWeatherState::HeatWave: return TEXT("HeatWave");
	case EWeatherState::Gale: return TEXT("Gale");
	case EWeatherState::Cyclone: return TEXT("Cyclone");
	default: return TEXT("Unknown");
	}
}

FWeatherSample FWeatherSample::Lerp(const FWeatherSample& A, const FWeatherSample& B, float Alpha)
{
	FWeatherSample R;
	R.CloudCover = FMath::Lerp(A.CloudCover, B.CloudCover, Alpha);
	R.Rain = FMath::Lerp(A.Rain, B.Rain, Alpha);
	R.Wind = FMath::Lerp(A.Wind, B.Wind, Alpha);
	R.SeaState = FMath::Lerp(A.SeaState, B.SeaState, Alpha);
	R.Fog = FMath::Lerp(A.Fog, B.Fog, Alpha);
	R.Lightning = FMath::Lerp(A.Lightning, B.Lightning, Alpha);
	R.Temperature = FMath::Lerp(A.Temperature, B.Temperature, Alpha);
	R.Pressure = FMath::Lerp(A.Pressure, B.Pressure, Alpha);
	return R;
}

FWeatherModel::FWeatherModel(uint32 InSeed)
	: Seed(InSeed)
{
}

ESeason FWeatherModel::SeasonForDay(float TotalDays)
{
	const int32 DayOfYear = FMath::FloorToInt32(FMath::Fmod(FMath::Max(0.0f, TotalDays), static_cast<float>(DaysPerYear)));
	return static_cast<ESeason>(FMath::Clamp(DayOfYear / DaysPerSeason, 0, static_cast<int32>(ESeason::Count) - 1));
}

FWeatherSample FWeatherModel::BaseSample(EWeatherState State, ESeason Season)
{
	FWeatherSample S;
	const float SeasonTemp[] = {30.0f, 28.0f, 25.0f, 29.0f};
	S.Temperature = SeasonTemp[static_cast<int32>(Season)];
	switch (State)
	{
	case EWeatherState::Clear:
		S = {0.15f, 0.0f, 0.2f, 0.12f, 0.0f, 0.0f, S.Temperature, 1013.0f};
		break;
	case EWeatherState::Cloudy:
		S = {0.6f, 0.0f, 0.3f, 0.2f, 0.05f, 0.0f, S.Temperature - 1.5f, 1009.0f};
		break;
	case EWeatherState::MorningFog:
		S = {0.45f, 0.0f, 0.05f, 0.08f, 0.85f, 0.0f, S.Temperature - 4.0f, 1011.0f};
		break;
	case EWeatherState::LightRain:
		S = {0.8f, 0.3f, 0.3f, 0.25f, 0.2f, 0.0f, S.Temperature - 3.0f, 1006.0f};
		break;
	case EWeatherState::Shower:
		S = {0.9f, 0.75f, 0.45f, 0.3f, 0.25f, 0.0f, S.Temperature - 4.0f, 1004.0f};
		break;
	case EWeatherState::Thunderstorm:
		S = {1.0f, 0.95f, 0.65f, 0.45f, 0.2f, 0.08f, S.Temperature - 5.0f, 998.0f};
		break;
	case EWeatherState::HeatWave:
		S = {0.05f, 0.0f, 0.05f, 0.08f, 0.1f, 0.0f, S.Temperature + 5.0f, 1015.0f};
		break;
	case EWeatherState::Gale:
		S = {0.75f, 0.35f, 0.85f, 0.8f, 0.1f, 0.0f, S.Temperature - 3.0f, 994.0f};
		break;
	case EWeatherState::Cyclone:
		S = {1.0f, 1.0f, 1.0f, 1.0f, 0.3f, 0.12f, S.Temperature - 6.0f, 965.0f};
		break;
	default:
		break;
	}
	return S;
}

void FWeatherModel::GenerateDay(int32 Day, TArray<FWeatherSpan>& Out) const
{
	FExploredRandom Rng(ExploredHash::Hash2D(Seed, Day, 0x57EA7));
	const ESeason Season = SeasonForDay(static_cast<float>(Day));
	const TArray<FStateWeight> Weights = WeightsFor(Season);

	// Ciclón: en la temporada de ciclones, uno cada ~4 días (noche completa), nunca los dos primeros días de la partida.
	const bool bCycloneNight = Season == ESeason::Cyclones && Day > 6 &&
		ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Seed, Day, 0xC1C1)) < 0.25f;

	// Niebla matinal: solo al amanecer; el resto del día se divide en 2–4 bloques.
	float T = static_cast<float>(Day);
	if (Rng.Chance(Season == ESeason::Monsoon ? 0.5f : 0.2f))
	{
		Out.Add({EWeatherState::MorningFog, T + 0.2f, T + 0.36f});
	}

	const int32 Blocks = Rng.RangeInt(2, 4);
	const float DayEnd = bCycloneNight ? T + 0.8f : T + 1.0f;
	float Cursor = T;
	for (int32 B = 0; B < Blocks; ++B)
	{
		const float End = B == Blocks - 1 ? DayEnd : FMath::Min(DayEnd, Cursor + Rng.RangeFloat(0.2f, 0.45f));
		EWeatherState State = Pick(Weights, Rng);
		// Las tormentas eléctricas y los chubascos son de tarde (biblia §6.1).
		if ((State == EWeatherState::Thunderstorm || State == EWeatherState::Shower) && Cursor < T + 0.5f && Season == ESeason::FirstRains)
		{
			State = EWeatherState::Cloudy;
		}
		Out.Add({State, Cursor, End});
		Cursor = End;
		if (Cursor >= DayEnd)
		{
			break;
		}
	}
	if (bCycloneNight)
	{
		Out.Add({EWeatherState::Cyclone, DayEnd, T + 1.25f});
	}
}

const TArray<FWeatherSpan>& FWeatherModel::SpansForDay(int32 Day) const
{
	FScopeLock Lock(&CacheLock);
	if (const TArray<FWeatherSpan>* Found = Cache.Find(Day))
	{
		return *Found;
	}
	TArray<FWeatherSpan>& Spans = Cache.Add(Day);
	GenerateDay(Day, Spans);
	return Spans;
}

int32 FWeatherModel::CycloneCategoryForDay(int32 Day) const
{
	const float Roll = ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Seed, Day, 0xCA7E6));
	return Roll < 0.5f ? 1 : (Roll < 0.85f ? 2 : 3);
}

int32 FWeatherModel::CycloneCategoryAt(float TotalDays) const
{
	if (StateAt(TotalDays) != EWeatherState::Cyclone)
	{
		return 0;
	}
	// El ciclón ocupa la noche de su día de anclaje (de D + 0,8 a D + 1,25).
	const int32 Day = FMath::FloorToInt32(TotalDays);
	for (const int32 D : {Day, Day - 1})
	{
		if (D < 0)
		{
			continue;
		}
		for (const FWeatherSpan& Span : SpansForDay(D))
		{
			if (Span.State == EWeatherState::Cyclone && TotalDays >= Span.Start && TotalDays < Span.End)
			{
				return CycloneCategoryForDay(D);
			}
		}
	}
	return 0;
}

EWeatherState FWeatherModel::StateAt(float TotalDays) const
{
	const int32 Day = FMath::FloorToInt32(TotalDays);
	// El ciclón del día anterior se extiende pasada la medianoche.
	for (const int32 D : {Day - 1, Day})
	{
		if (D < 0)
		{
			continue;
		}
		EWeatherState Result = EWeatherState::Count;
		for (const FWeatherSpan& Span : SpansForDay(D))
		{
			if (TotalDays >= Span.Start && TotalDays < Span.End)
			{
				// La niebla y los ciclones tienen prioridad sobre el bloque general.
				if (Result == EWeatherState::Count || Span.State == EWeatherState::MorningFog || Span.State == EWeatherState::Cyclone)
				{
					Result = Span.State;
				}
			}
		}
		if (Result != EWeatherState::Count && (D == Day || Result == EWeatherState::Cyclone))
		{
			return Result;
		}
	}
	return EWeatherState::Clear;
}

bool FWeatherModel::NextSevereEvent(float FromDays, FWeatherSpan& OutSpan) const
{
	const int32 First = FMath::Max(0, FMath::FloorToInt32(FromDays) - 1);
	for (int32 Day = First; Day < First + 40; ++Day)
	{
		for (const FWeatherSpan& Span : SpansForDay(Day))
		{
			if ((Span.State == EWeatherState::Cyclone || Span.State == EWeatherState::Gale) && Span.End > FromDays)
			{
				OutSpan = Span;
				return true;
			}
		}
	}
	return false;
}

FWeatherSample FWeatherModel::SampleAt(float TotalDays) const
{
	// Promedio temporal (filtro de caja) sobre la ventana de transición: continuo por construcción.
	constexpr int32 Taps = 48;
	FWeatherSample Sample;
	FMemory::Memzero(Sample);
	for (int32 I = 0; I < Taps; ++I)
	{
		const float T = TotalDays - TransitionDays * I / (Taps - 1);
		const FWeatherSample Tap = BaseSample(StateAt(T), SeasonForDay(T));
		Sample.CloudCover += Tap.CloudCover / Taps;
		Sample.Rain += Tap.Rain / Taps;
		Sample.Wind += Tap.Wind / Taps;
		Sample.SeaState += Tap.SeaState / Taps;
		Sample.Fog += Tap.Fog / Taps;
		Sample.Lightning += Tap.Lightning / Taps;
		Sample.Temperature += Tap.Temperature / Taps;
		Sample.Pressure += Tap.Pressure / Taps;
	}

	// Señales previas a un temporal: la presión cae, sube el mar de fondo y el viento.
	FWeatherSpan Severe;
	if (NextSevereEvent(TotalDays, Severe) && TotalDays < Severe.Start)
	{
		const float Lead = Severe.Start - TotalDays;
		if (Lead < WarningDays)
		{
			const float Warn = 1.0f - Lead / WarningDays;
			const float Depth = Severe.State == EWeatherState::Cyclone ? 40.0f : 15.0f;
			Sample.Pressure -= Depth * Warn;
			Sample.SeaState = FMath::Max(Sample.SeaState, 0.25f + 0.35f * Warn);
			Sample.Wind = FMath::Max(Sample.Wind, 0.3f * Warn);
		}
	}
	return Sample;
}
