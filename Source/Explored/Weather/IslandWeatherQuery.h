#pragma once

#include "CoreMinimal.h"

class UObject;

/**
 * Consultas de hora y clima que necesitan los actores de la isla (pesca, fuego, barril, cuerpo). El
 * «muro del ciclón» es el de FStormClockModel en UExploredWeatherSubsystem (GDD v3 §5.8).
 */
namespace ExploredIslandWeather
{

	/** Hora del día (0-24) de UTimeOfDaySubsystem; 12 si no hay reloj. */
	EXPLORED_API float GetHour(const UObject* WorldContext);

	/** Lluvia actual 0-1 de UExploredWeatherSubsystem; 0 si no hay clima. */
	EXPLORED_API float GetRain(const UObject* WorldContext);

	/** Muro del ciclón: viento y lluvia a tope. */
	EXPLORED_API bool IsCycloneWall(const UObject* WorldContext);

	/** El punto está bajo techo (dentro de algún AExploredShelterVolume). */
	EXPLORED_API bool IsSheltered(const UObject* WorldContext, const FVector& Location);
}
