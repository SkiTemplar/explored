#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Weather/WeatherModel.h"

#include "ExploredWeatherSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWeatherStateChanged, EWeatherState /*Old*/, EWeatherState /*New*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSeasonChanged, ESeason);

/**
 * Clima del mundo. Lee la hora del UTimeOfDaySubsystem, evalúa FWeatherModel
 * y reparte el resultado: océano (mar), ambiente sonoro (lluvia y mar) y cielo
 * (nubes, niebla y luz). Otros sistemas (supervivencia, fuego, fauna) leen
 * GetCurrent().
 */
UCLASS()
class EXPLORED_API UExploredWeatherSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	const FWeatherSample& GetCurrent() const { return Current; }
	EWeatherState GetState() const { return State; }
	ESeason GetSeason() const { return Season; }

	/** Fuerza un estado durante un tiempo (depuración y eventos de guion). Duración en horas de juego. */
	void ForceState(EWeatherState InState, float DurationHours);

	FOnWeatherStateChanged OnWeatherStateChanged;
	FOnSeasonChanged OnSeasonChanged;

private:
	void Apply();

	TUniquePtr<FWeatherModel> Model;
	FWeatherSample Current;
	EWeatherState State = EWeatherState::Clear;
	ESeason Season = ESeason::Dry;
	EWeatherState ForcedState = EWeatherState::Count;
	float ForcedUntilDays = 0.0f;
	float ApplyTimer = 0.0f;
};
