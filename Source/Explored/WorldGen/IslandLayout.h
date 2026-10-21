#pragma once

#include "CoreMinimal.h"

/**
 * Bioma de una tierra o de una zona. Conserva los nombres del catálogo de música
 * (`music_layers.json/day_variants`) y de los eventos; en la Isla de Sal cada zona
 * del GDD §6 toma el bioma que mejor describe su ambiente.
 */
enum class EIslandArchetype : uint8
{
	Landing,     // Playa y palmeral.
	Emerald,     // Selva o bosque denso (bambú).
	Smoke,       // Volcánico (sin uso en la Isla de Sal).
	Teeth,       // Rocoso: acantilados, islote del carguero.
	Mangrove,    // Llano húmedo (sin uso en la Isla de Sal).
	WhiteSands,  // Arenal abierto.
	Mesa,        // Colina o cresta alta.
	Count
};

EXPLORED_API const TCHAR* LexToString(EIslandArchetype Archetype);

/** Busca un bioma por su nombre de LexToString. False si no existe. */
EXPLORED_API bool IslandArchetypeFromName(const FString& Name, EIslandArchetype& OutArchetype);

/** Una tierra emergida con forma de elipse. Distancias en metros, ángulos en radianes. */
struct EXPLORED_API FIslandDesc
{
	FName Id;
	EIslandArchetype Archetype = EIslandArchetype::Landing;
	FVector2D Center = FVector2D::ZeroVector;
	/** Semieje mayor (eje X local), el radio nominal de la tierra. */
	float Radius = 100.0f;
	/** Semieje menor / semieje mayor, en (0, 1]. */
	float Aspect = 1.0f;
	/** Giro del eje mayor respecto a +X del mundo. */
	float Rotation = 0.0f;
	/** Altura de la meseta interior (m) sin contar las colinas de las zonas. */
	float MaxHeight = 4.0f;

	/** Distancia elíptica normalizada: < 1 dentro, 1 en la costa nominal. */
	float NormalizedDistance(const FVector2D& Point) const;
	/** Distancia aproximada (m) a la costa nominal; 0 dentro de la tierra. */
	float DistanceToCoast(const FVector2D& Point) const;
	/** Distancia del centro a la costa nominal en la dirección Direction (no hace falta normalizarla). */
	float RadiusAlong(const FVector2D& Direction) const;
};

/** Zona de juego del GDD §6: círculo con nombre y, si es una colina, su cota extra. */
struct EXPLORED_API FIslandZone
{
	FName Id;
	/** Nombre legible (español) para registros y depuración; la UI usa la localización. */
	FString Name;
	EIslandArchetype Biome = EIslandArchetype::Landing;
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 40.0f;
	/** Altura (m) que la zona suma en su centro sobre la meseta (gaussiana); 0 = llano. */
	float Hill = 0.0f;
};

/** Punto de autor (aparición, sitio de la balsa, faro…). Metros y grados. */
struct EXPLORED_API FIslandPoint
{
	FName Id;
	FVector2D Location = FVector2D::ZeroVector;
	float Yaw = 0.0f;
};

/** Altura y contexto de una columna (X, Y) del terreno aproximado. */
struct EXPLORED_API FIslandColumn
{
	/** Altura de la superficie en metros (nivel del mar = 0; negativa en el mar). */
	float Height = 0.0f;
	/** Tierra dominante (índice en Islands) o INDEX_NONE en mar abierto. */
	int32 IslandIndex = INDEX_NONE;
	/** Zona que contiene el punto (índice en Zones) o INDEX_NONE. */
	int32 ZoneIndex = INDEX_NONE;
	/** Distancia normalizada a la tierra dominante (1 = costa nominal). */
	float NormalizedDistance = 10.0f;
};

/**
 * Disposición de autor de la Isla de Sal (GDD §6): tierras, zonas y puntos. Los datos
 * vienen de `Content/Data/island.json` (lo lee ExploredIsland::Get); Provisional() es la
 * copia de respaldo en código. SampleColumn da un relieve analítico (elipses y gaussianas)
 * que sirve de aproximación hasta que exista el Landscape de F3.
 */
struct EXPLORED_API FIslandLayout
{
	/** Semilla base de las tiradas deterministas (eventos, clima, pesca, música). */
	static constexpr uint32 OfficialSeed = 20260926;
	static constexpr float SeaLevel = 0.0f;
	static constexpr float OceanFloor = -30.0f;
	/** Medio lado del área jugable (m): el mapa cabe en 400 × 400 m. */
	static constexpr float WorldHalfExtent = 200.0f;
	/** Pendiente del fondo fuera de la costa (m de bajada por m de distancia). */
	static constexpr float ShelfSlope = 0.12f;
	/** Más allá de esta distancia normalizada el punto es mar abierto. */
	static constexpr float OpenSeaNormalizedDistance = 1.6f;

	uint32 Seed = OfficialSeed;
	/** Isla principal primero; después los islotes. */
	TArray<FIslandDesc> Islands;
	TArray<FIslandZone> Zones;
	TArray<FIslandPoint> Points;

	/** Copia en código de island.json, para tests y como respaldo si el fichero falta. */
	static FIslandLayout Provisional();

	const FIslandDesc* FindIsland(EIslandArchetype Archetype) const;
	const FIslandDesc* FindIslandById(FName Id) const;
	const FIslandZone* FindZone(FName Id) const;
	const FIslandPoint* FindPoint(FName Id) const;

	/** Zona cuyo círculo contiene el punto (la de centro más cercano si hay varias) o INDEX_NONE. */
	int32 FindZoneAt(const FVector2D& Point) const;

	/** Bioma de la columna: el de su zona, si no el de su tierra; Count en mar abierto. */
	EIslandArchetype BiomeAt(const FIslandColumn& Column) const;

	/** Relieve aproximado en (X, Y) metros. */
	FIslandColumn SampleColumn(float X, float Y) const;

	/** Comprueba coherencia (ids únicos y no vacíos, radios positivos, zonas sobre tierra). */
	bool Validate(FString& OutError) const;
};
