#pragma once

#include "CoreMinimal.h"

class UObject;

/**
 * Consultas de hora y clima que necesitan los actores de la isla (pesca, fuego, barril). Hasta que
 * F3c cablee FStormClockModel, el «muro del ciclón» se lee del tiempo actual: ciclón activo en
 * UExploredWeatherSubsystem, o lluvia y viento por encima de CycloneThreshold.
 */
namespace ExploredIslandWeather
{
	inline constexpr float CycloneThreshold = 0.8f;

	/** Hora del día (0-24) de UTimeOfDaySubsystem; 12 si no hay reloj. */
	EXPLORED_API float GetHour(const UObject* WorldContext);

	/** Lluvia actual 0-1 de UExploredWeatherSubsystem; 0 si no hay clima. */
	EXPLORED_API float GetRain(const UObject* WorldContext);

	/** Muro del ciclón: viento y lluvia a tope. */
	EXPLORED_API bool IsCycloneWall(const UObject* WorldContext);

	/** El punto está bajo techo (dentro de algún AExploredShelterVolume). */
	EXPLORED_API bool IsSheltered(const UObject* WorldContext, const FVector& Location);
}
