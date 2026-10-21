#include "Weather/ExploredWeatherSubsystem.h"

#include "EngineUtils.h"
#include "Engine/World.h"

#include "Audio/ExploredAmbienceSubsystem.h"
#include "Explored.h"
#include "Items/IslandDataLoader.h"
#include "Ocean/ExploredOcean.h"
#include "Sky/ExploredSkyController.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "WorldGen/IslandLayout.h"

namespace
{
	constexpr uint32 WeatherSeed = FIslandLayout::OfficialSeed ^ 0x77EA7u;
	/** Frecuencia con que se reparten los valores (s). */
	constexpr float ApplyInterval = 0.25f;
}

bool UExploredWeatherSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExploredWeatherSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTimeOfDaySubsystem>();
	Model = MakeUnique<FWeatherModel>(WeatherSeed);
	Storm = FStormClockModel(ExploredIslandData::Get().Storm);
	if (Storm.UsesFallback())
	{
		UE_LOG(LogExplored, Warning, TEXT("[Clima] storm_curve.json no es válido: se usa la curva del GDD"));
	}
}

double UExploredWeatherSubsystem::GetAbsHours() const
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	return Time ? FStormClockModel::ToAbsoluteHours(Time->GetDay(), Time->GetHours()) : 0.0;
}

float UExploredWeatherSubsystem::GetPressureHpa() const
{
	return Storm.PressureAt(GetAbsHours());
}

EBarometerTrend UExploredWeatherSubsystem::GetPressureTrend() const
{
	return Storm.TrendAt(GetAbsHours());
}

bool UExploredWeatherSubsystem::IsCycloneWall() const
{
	return FStormClockModel::IsWall(StormState) || (ForcedState == EWeatherState::Cyclone);
}

namespace ExploredWeatherDetail
{
	FWeatherSample ToSample(const FWeatherLook& Look)
	{
		FWeatherSample Sample;
		Sample.CloudCover = Look.CloudCover;
		Sample.Rain = Look.Rain;
		Sample.Wind = Look.Wind;
		Sample.SeaState = Look.SeaState;
		Sample.Fog = Look.Fog;
		Sample.Lightning = Look.Lightning;
		Sample.Temperature = Look.Temperature;
		Sample.Pressure = Look.Pressure;
		return Sample;
	}

	EWeatherState ToWeatherState(EStormState State)
	{
		switch (State)
		{
		case EStormState::Squall: return EWeatherState::Shower;
		case EStormState::HighClouds: return EWeatherState::Cloudy;
		case EStormState::SeaClosed: return EWeatherState::Gale;
		case EStormState::CycloneWall1:
		case EStormState::CycloneWall2: return EWeatherState::Cyclone;
		default: return EWeatherState::Clear;
		}
	}
}

uint32 UExploredWeatherSubsystem::GetWorldWeatherSeed()
{
	return WeatherSeed;
}

void UExploredWeatherSubsystem::ForceState(EWeatherState InState, float DurationHours)
{
	const UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>();
	ForcedState = InState;
	ForcedUntilDays = (Time ? Time->GetTotalDays() : 0.0f) + DurationHours / 24.0f;
}

void UExploredWeatherSubsystem::RestoreForcedState(EWeatherState InState, float UntilDays)
{
	ForcedState = InState;
	ForcedUntilDays = UntilDays;
}

int32 UExploredWeatherSubsystem::GetCycloneCategory() const
{
	if (State != EWeatherState::Cyclone)
	{
		return 0;
	}
	if (FStormClockModel::IsWall(StormState))
	{
		return 3;
	}
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	const int32 Planned = (Model && Time) ? Model->CycloneCategoryAt(Time->GetTotalDays()) : 0;
	return Planned > 0 ? Planned : FMath::Clamp(ForcedCycloneCategory, 1, 3);
}

void UExploredWeatherSubsystem::Tick(float DeltaTime)
{
	const UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>();
	if (!Time || !Model)
	{
		return;
	}

	const float Days = Time->GetTotalDays();
	FWeatherSample Target;
	EWeatherState NewState;
	if (ForcedState != EWeatherState::Count && Days < ForcedUntilDays)
	{
		NewState = ForcedState;
		Target = FWeatherModel::BaseSample(ForcedState, FWeatherModel::SeasonForDay(Days));
	}
	else
	{
		// Baliza Siete: el cielo sigue la curva fija del reloj de tormenta (GDD v3 §5.8).
		ForcedState = EWeatherState::Count;
		const double AbsHours = FStormClockModel::ToAbsoluteHours(Time->GetDay(), Time->GetHours());
		StormState = Storm.StateAt(AbsHours);
		NewState = ExploredWeatherDetail::ToWeatherState(StormState);
		Target = ExploredWeatherDetail::ToSample(Storm.LookAt(AbsHours));
	}

	// Suavizado en tiempo real: la curva cambia de estado de golpe y el cielo no debe saltar.
	const float Alpha = 1.0f - FMath::Exp(-DeltaTime * 0.5f);
	Current = FWeatherSample::Lerp(Current, Target, Alpha);

	if (NewState != State)
	{
		const EWeatherState Old = State;
		State = NewState;
		OnWeatherStateChanged.Broadcast(Old, State);
	}
	const ESeason NewSeason = FWeatherModel::SeasonForDay(Days);
	if (NewSeason != Season)
	{
		Season = NewSeason;
		OnSeasonChanged.Broadcast(Season);
	}

	ApplyTimer -= DeltaTime;
	if (ApplyTimer <= 0.0f)
	{
		ApplyTimer = ApplyInterval;
		Apply();
	}
}

void UExploredWeatherSubsystem::Apply()
{
	UWorld* World = GetWorld();
	if (UExploredAmbienceSubsystem* Ambience = World->GetSubsystem<UExploredAmbienceSubsystem>())
	{
		Ambience->SetRainIntensity(Current.Rain);
		Ambience->SetSeaState(Current.SeaState);
	}
	for (TActorIterator<AExploredOcean> It(World); It; ++It)
	{
		It->SetSeaState(Current.SeaState);
	}
	for (TActorIterator<AExploredSkyController> It(World); It; ++It)
	{
		It->SetWeather(Current);
	}
}

TStatId UExploredWeatherSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredWeatherSubsystem, STATGROUP_Tickables);
}
