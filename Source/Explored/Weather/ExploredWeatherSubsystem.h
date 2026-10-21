#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Weather/StormClockModel.h"
#include "Weather/WeatherModel.h"

#include "ExploredWeatherSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWeatherStateChanged, EWeatherState /*Old*/, EWeatherState /*New*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSeasonChanged, ESeason);

/**
 * Clima del mundo. Lee la hora del UTimeOfDaySubsystem y, en Baliza Siete, la curva fija de
 * FStormClockModel (storm_curve.json, GDD v3 §5.8): chubasco guiado, nubes, mar cerrado y ciclón
 * con muro 1, ojo y muro 2. Reparte el resultado al océano (olas), al ambiente sonoro (lluvia y
 * mar) y al cielo (nubes, niebla y luz). Otros sistemas (cuerpo, fuego, barril, HUD) leen
 * GetCurrent(), GetStormState() y el barómetro. FWeatherModel queda solo como planificador
 * heredado (semilla de eventos).
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

	/** Reloj de tormenta de la partida (storm_curve.json o el del GDD). */
	const FStormClockModel& GetStorm() const { return Storm; }
	/** Horas absolutas desde el día 1 a las 00:00 (las de FStormClockModel). */
	double GetAbsHours() const;
	EStormState GetStormState() const { return StormState; }
	/** Lectura del barómetro (hPa) y su tendencia ahora. */
	float GetPressureHpa() const;
	EBarometerTrend GetPressureTrend() const;
	/** Muro 1 o muro 2 del ciclón (o un ciclón forzado por depuración). */
	bool IsCycloneWall() const;
	EWeatherState GetState() const { return State; }
	ESeason GetSeason() const { return Season; }

	/** Semilla del clima del mundo (la usa FWorldEventsModel para ver el mismo cielo). */
	static uint32 GetWorldWeatherSeed();
	/** Planificador determinista (lo usa el huerto para la lluvia de días que no se han jugado). */
	const FWeatherModel* GetModel() const { return Model.Get(); }

	/** Fuerza un estado durante un tiempo (depuración y eventos de guion). Duración en horas de juego. */
	void ForceState(EWeatherState InState, float DurationHours);

	/** Estado forzado vigente (Count si no hay) y hasta cuándo (días totales): para el guardado. */
	EWeatherState GetForcedState(float& OutUntilDays) const { OutUntilDays = ForcedUntilDays; return ForcedState; }
	/** Restaura un estado forzado guardado (Count lo quita). */
	void RestoreForcedState(EWeatherState InState, float UntilDays);

	/**
	 * Categoría del ciclón activo (1–3, FWeatherModel::CycloneCategoryAt); 0 si no
	 * hay ciclón. Un ciclón forzado sin planificar da ForcedCycloneCategory.
	 */
	int32 GetCycloneCategory() const;

	/** Categoría de un ciclón forzado a mano (depuración). */
	int32 ForcedCycloneCategory = 2;

	FOnWeatherStateChanged OnWeatherStateChanged;
	FOnSeasonChanged OnSeasonChanged;

private:
	void Apply();

	TUniquePtr<FWeatherModel> Model;
	FStormClockModel Storm;
	EStormState StormState = EStormState::Clear;
	FWeatherSample Current;
	EWeatherState State = EWeatherState::Clear;
	ESeason Season = ESeason::Dry;
	EWeatherState ForcedState = EWeatherState::Count;
	float ForcedUntilDays = 0.0f;
	float ApplyTimer = 0.0f;
};
