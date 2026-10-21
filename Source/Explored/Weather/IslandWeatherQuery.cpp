#include "Weather/IslandWeatherQuery.h"

#include "Engine/World.h"
#include "EngineUtils.h"

#include "Sky/TimeOfDaySubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/ExploredShelterVolume.h"

float ExploredIslandWeather::GetHour(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	return Time ? Time->GetHours() : 12.0f;
}

float ExploredIslandWeather::GetRain(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	return Weather ? Weather->GetCurrent().Rain : 0.0f;
}

bool ExploredIslandWeather::IsCycloneWall(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	if (!Weather)
	{
		return false;
	}
	const FWeatherSample& Sample = Weather->GetCurrent();
	return Weather->GetCycloneCategory() > 0 || (Sample.Rain >= CycloneThreshold && Sample.Wind >= CycloneThreshold);
}

bool ExploredIslandWeather::IsSheltered(const UObject* WorldContext, const FVector& Location)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AExploredShelterVolume> It(World); It; ++It)
	{
		if (It->IsLocationSheltered(Location))
		{
			return true;
		}
	}
	return false;
}
