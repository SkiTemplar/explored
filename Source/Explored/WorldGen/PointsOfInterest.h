#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"

class FTerrainDensity;

/** Tipos de punto de interés (GDD §3.3). */
enum class EPoiType : uint8
{
	WreckFuselage,    // Fuselaje del Albatros hundido en la laguna (batería).
	WreckWing,        // Ala en la playa (mochila, pistola de bengalas).
	WreckTail,        // Cola en el fondo del canal (antena).
	WreckEngine,      // Motor en Esmeralda (piezas y cable).
	HaldenCamp,       // Campamento de la expedición.
	RadioStation,     // Estación de radio (Manglar): pieza «radio».
	TideObservatory,  // Observatorio de mareas (Meseta).
	Lighthouse,       // Faro en ruinas (Los Dientes).
	StarCompass,      // Brújula estelar (cumbre del Humo).
	Waterfall,        // Cascada con cueva (Esmeralda).
	Viewpoint,        // Mirador (revela el mapa).
	Shipwreck,        // Pecio del velero (Arenas Blancas).
	TurtleBeach,      // Playa de desove.
	HotSpring,        // Aguas termales (Humo).
	TidePool,         // Pozas de marea.
	Note,             // Nota de Inés.
	HaldenPage,       // Página del diario Halden.
	Bottle,           // Mensaje en botella.
	Petroglyph,       // Petroglifo.
	BeaconSite,       // Lugar para montar la baliza (pico más alto).
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
