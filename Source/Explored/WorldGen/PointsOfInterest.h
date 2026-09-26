#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"

class FTerrainDensity;

/** Tipos de punto de interés (GDD §4.3). */
enum class EPoiType : uint8
{
	WreckFuselage,    // Fuselaje del Albatros hundido en la laguna (pieza para el barco).
	WreckWing,        // Ala en la playa (mochila, pieza para el barco).
	WreckTail,        // Cola en el fondo del canal (pieza para el barco).
	WreckEngine,      // Motor en Esmeralda (piezas y cable, pieza para el barco).
	HaldenCamp,       // Campamento abandonado de la expedición de 1974 (ruina, recursos).
	RadioStation,     // Estación de radio abandonada (Manglar): ruina, recursos.
	TideObservatory,  // Observatorio de mareas abandonado (Meseta): ruina, tabla de mareas.
	Lighthouse,       // Faro en ruinas (Los Dientes): objetivo emergente de reparación.
	StarCompass,      // Marae con brújula estelar (cumbre del Humo): wayfinding.
	Waterfall,        // Cascada con cueva (Esmeralda).
	Viewpoint,        // Mirador (boceto de mapa sin confirmar, GDD §5.3).
	Shipwreck,        // Pecio del velero (Arenas Blancas).
	TurtleBeach,      // Playa de desove.
	HotSpring,        // Aguas termales (Humo).
	TidePool,         // Pozas de marea.
	Bottle,           // Objeto de colección hallado en la playa.
	Petroglyph,       // Petroglifo (motivo del pueblo navegante, wayfinding).
	Count
};

EXPLORED_API const TCHAR* LexToString(EPoiType Type);

/** Un punto de interés colocado en el mundo (metros). */
struct EXPLORED_API FPointOfInterest
{
	EPoiType Type = EPoiType::Viewpoint;
	/** Identificador de contenido (id de nota, página, petroglifo…) o vacío. */
	FName ContentId;
	int32 IslandIndex = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.0f;
	/** Bajo el agua (se alcanza buceando). */
	bool bUnderwater = false;
};

/**
 * Coloca de forma determinista todos los puntos de interés garantizados del
 * archipiélago sobre la superficie real del terreno.
 */
struct EXPLORED_API FPoiLayout
{
	static TArray<FPointOfInterest> Generate(const FTerrainDensity& Density);

	/** Punto más alto de una isla (búsqueda en rejilla gruesa y refinada). */
	static FVector FindSummit(const FTerrainDensity& Density, const FIslandDesc& Island);

	/** Primer punto de playa (altura 1–3 m) en una dirección desde el centro. */
	static bool FindBeach(const FTerrainDensity& Density, const FIslandDesc& Island, float Angle, FVector& Out);

	/** Punto de tierra a una fracción del radio en una dirección, sobre la superficie. */
	static bool FindInland(const FTerrainDensity& Density, const FIslandDesc& Island, float Angle, float Fraction, FVector& Out);
};
