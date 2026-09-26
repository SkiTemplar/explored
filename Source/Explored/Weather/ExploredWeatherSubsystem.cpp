#include "Weather/ExploredWeatherSubsystem.h"

#include "EngineUtils.h"
#include "Engine/World.h"

#include "Audio/ExploredAmbienceSubsystem.h"
#include "Ocean/ExploredOcean.h"
#include "Sky/ExploredSkyController.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"

namespace
{
	constexpr uint32 WeatherSeed = FArchipelagoLayout::OfficialSeed ^ 0x77EA7u;
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
		ForcedState = EWeatherState::Count;
		NewState = Model->StateAt(Days);
		Target = Model->SampleAt(Days);
	}

	// Suavizado adicional en tiempo real (el modelo interpola en días de juego).
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
