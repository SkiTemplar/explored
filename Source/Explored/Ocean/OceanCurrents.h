#pragma once

#include "CoreMinimal.h"

struct FArchipelagoLayout;

/**
 * Marea del archipiélago: dos pleamares al día (GDD §5.1), con las mareas
 * vivas (más fuertes) en luna nueva y llena, y las muertas en cuarto
 * creciente/menguante. Funciones puras y testeables sin mundo; quien las
 * usa inyecta los días totales y la fase lunar de UTimeOfDaySubsystem.
 */
struct EXPLORED_API FOceanTide
{
	/** Pleamares al día. */
	static constexpr float CyclesPerDay = 2.0f;

	/** Nivel de marea en [-1 bajamar, 1 pleamar] para el instante (días totales de juego). */
	static float Level(float TotalDays);

	/**
	 * Corriente de la marea: -1 vaciante .. 1 llenante. Máxima a media marea
	 * (subiendo o bajando más rápido) y nula en el cambio de marea, como en
	 * la realidad.
	 */
	static float Flow(float TotalDays);

	/**
	 * Fuerza de la marea por la fase lunar: 1 en marea viva (nueva o llena),
	 * 0.4 en marea muerta (cuartos). MoonPhase01 en [0, 1) (ver
	 * ExploredSky::MoonPhase).
	 */
	static float SpringNeapFactor(float MoonPhase01);
};

/** Estrecho navegable entre dos islas consecutivas de la cadena volcánica (centímetros, espacio de mundo). */
struct EXPLORED_API FOceanStrait
{
	FVector2D CoastA = FVector2D::ZeroVector;
	FVector2D CoastB = FVector2D::ZeroVector;
	float HalfWidthCm = 5000.0f;
};

/**
 * Corrientes de marea en los estrechos entre islas consecutivas de la cadena
 * (GDD §4.1, «corriente fuerte en los canales»). Empujan a lo largo del
 * canal, calmadas en las dos costas y máximas en el centro. La fuerza
 * depende de la marea y del viento, que inyecta quien llama
 * (UExploredWeatherSubsystem, UTimeOfDaySubsystem); esta estructura no
 * conoce ninguno de los dos.
 */
struct EXPLORED_API FOceanCurrents
{
	/** Corriente máxima (cm/s) en el centro de un canal, marea viva y viento fuerte. */
	static constexpr float MaxSpeedCmS = 180.0f;

	/** Un estrecho por cada par de islas consecutivas (Layout.Islands[i], Islands[i + 1]). */
	static TArray<FOceanStrait> BuildStraits(const FArchipelagoLayout& Layout);

	/**
	 * Corriente (cm/s, mundo) en PositionCm. FlowSign en [-1, 1] (ver
	 * FOceanTide::Flow); TideStrength01 en [0, 1] (ver
	 * FOceanTide::SpringNeapFactor); Wind01 en [0, 1] (FWeatherSample::Wind).
	 */
	static FVector2D CurrentAt(const TArray<FOceanStrait>& Straits, const FVector2D& PositionCm,
		float FlowSign, float TideStrength01, float Wind01);
};
