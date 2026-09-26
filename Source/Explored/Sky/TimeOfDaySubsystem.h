#pragma once

#include "CoreMinimal.h"
#include "Sky/MoonModel.h"
#include "Subsystems/WorldSubsystem.h"

#include "TimeOfDaySubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnNewDay, int32 /*Day*/);

/** Astronomía simplificada del archipiélago (funciones puras y testeables). */
namespace ExploredSky
{
	/** Latitud del archipiélago en grados (trópico sur). */
	constexpr float LatitudeDeg = -12.0f;

	/** Días del año de juego (GDD: 4 estaciones de 8 días). */
	constexpr int32 DaysPerYear = 32;

	/** Días de un ciclo lunar completo (fuente única: FMoonModel). */
	constexpr int32 DaysPerLunarCycle = FMoonModel::DaysPerCycle;

	/** Declinación solar en grados para un día del año (0..DaysPerYear). */
	EXPLORED_API float SolarDeclinationDeg(float DayOfYear);

	/**
	 * Dirección hacia el Sol en el sistema de Unreal (X norte, Y este, Z arriba)
	 * para una hora local (0..24).
	 */
	EXPLORED_API FVector SunDirection(float Hours, float DayOfYear);

	/** Dirección hacia la Luna: opuesta al Sol, desplazada por la fase. */
	EXPLORED_API FVector MoonDirection(float Hours, float DayOfYear, float MoonPhase);

	/** Fase lunar en [0, 1): 0 luna nueva, 0.5 luna llena (delega en FMoonModel::Phase). */
	EXPLORED_API float MoonPhase(float TotalDays);

	/** Fracción iluminada del disco lunar (0 nueva, 1 llena; delega en FMoonModel). */
	EXPLORED_API float MoonIllumination(float Phase);
}

/**
 * Reloj del mundo. Avanza el tiempo de juego y expone la hora, el día y la
 * fase lunar. El resto de sistemas (cielo, clima, fauna) leen de aquí.
 */
UCLASS()
class EXPLORED_API UTimeOfDaySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Hora local en [0, 24). */
	float GetHours() const { return Hours; }
	int32 GetDay() const { return Day; }
	float GetTotalDays() const { return Day + Hours / 24.0f; }
	float GetDayOfYear() const { return FMath::Fmod(GetTotalDays(), static_cast<float>(ExploredSky::DaysPerYear)); }
	float GetMoonPhase() const { return ExploredSky::MoonPhase(GetTotalDays()); }
	bool IsNight() const { return ExploredSky::SunDirection(Hours, GetDayOfYear()).Z < -0.05f; }

	void SetTime(int32 InDay, float InHours);

	/** Minutos reales que dura un día de juego completo. */
	void SetDayLengthMinutes(float Minutes) { DayLengthMinutes = FMath::Max(1.0f, Minutes); }
	float GetDayLengthMinutes() const { return DayLengthMinutes; }

	/** Multiplicador temporal (dormir, depuración). */
	void SetTimeScale(float Scale) { TimeScale = FMath::Max(0.0f, Scale); }

	FOnNewDay OnNewDay;

private:
	float Hours = 7.5f;
	int32 Day = 4;
	float DayLengthMinutes = 40.0f;
	float TimeScale = 1.0f;
};
