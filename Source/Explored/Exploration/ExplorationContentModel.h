#pragma once

#include "CoreMinimal.h"

/**
 * Catálogo narrativo de lugares de interés con nombre (docs/diseno/exploracion.md):
 * una capa de datos ligera sobre los puntos de interés ya colocados por
 * FPoiLayout (WorldGen/PointsOfInterest.h) y las ruinas de FRuinsModel, con el
 * nombre, el tono y la posición aproximada de cada uno para que diseño y
 * localización trabajen sobre JSON sin tocar la generación procedural.
 *
 * No sustituye a FPoiLayout ni a FRuinsLayout: un FExplorationLandmark puede
 * enlazar con un punto ya colocado (LinkedPoi) o describir uno pendiente de
 * colocar en el mundo (LinkedPoi vacío), documentado como tal.
 */

/** Un lugar de interés con nombre (Content/Data/exploration.json). */
struct EXPLORED_API FExplorationLandmark
{
	/** Id estable, único en el catálogo. */
	FName Id;
	/** Isla a la que pertenece: «landing», «emerald», «smoke», «teeth», «mangrove», «whitesands», «mesa». */
	FName IslandId;
	/** Tipo de lugar: «cala», «cueva», «mirador», «naufragio», «ruina», «cascada», «arco_marino», «poza», «jardin_coral», «cumbre», «campamento», «recurso» o «vista». */
	FName Kind;
	FString NameEs;
	FString NameEn;
	/**
	 * Posición aproximada en coordenadas locales de la isla: ángulo en grados
	 * (sentido horario desde +X, como Island.Rotation) y distancia como fracción
	 * del radio de la isla. Determinista y suficiente para que FPoiLayout la
	 * resuelva sobre el terreno real con FindBeach/FindInland cuando se coloque.
	 */
	float AngleDeg = 0.0f;
	float DistanceFrac = 0.5f;
	/** ContentId o LexToString(EPoiType) del punto ya colocado por FPoiLayout, o NAME_None si aún no existe en el mundo. */
	FName LinkedPoi;
	/** Sello del mapa a mano que sugiere (Cartography/CartographyModel.h): «water», «cave», «danger», «resource», «ruin», «wreck», o NAME_None. */
	FName MapMark;
	/** Cómo se llega: ruta, obstáculo, herramienta o condición de marea/hora, en un texto corto (sin lore largo). */
	FString AccessEs;
	/** Qué da: recurso, tesoro, pista de navegación o vista, en un texto corto. */
	FString RewardEs;
};

/** Catálogo completo, parseado de Content/Data/exploration.json. */
struct EXPLORED_API FExplorationCatalog
{
	TArray<FExplorationLandmark> Landmarks;

	static bool IsValidIslandId(FName IslandId);
	static bool IsValidKind(FName Kind);
	/** NAME_None cuenta como válido (sin sello). */
	static bool IsValidMapMark(FName MapMark);

	const FExplorationLandmark* FindLandmark(FName Id) const;
	int32 CountForIsland(FName IslandId) const;

	/** Reglas de docs/diseno/exploracion.md: ids únicos, campos no vacíos, rangos e ids conocidos. */
	bool Validate(TArray<FString>& OutErrors) const;
};
