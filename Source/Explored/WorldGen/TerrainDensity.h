#pragma once

#include "CoreMinimal.h"
#include "Core/ExploredNoise.h"
#include "WorldGen/ArchipelagoLayout.h"

/** Cueva o arco: cápsula deformada que se excava en el terreno. Metros. */
struct EXPLORED_API FCaveDesc
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float Radius = 4.0f;
	uint32 Seed = 0;
};

/** Información de superficie en un punto del plano. */
struct EXPLORED_API FTerrainColumn
{
	/** Altura de la superficie en metros (nivel del mar = 0). */
	float Height = 0.0f;
	/** Isla dominante (índice en el layout) o INDEX_NONE en mar abierto. */
	int32 IslandIndex = INDEX_NONE;
	/** Distancia normalizada al centro de la isla dominante (1 = costa nominal). */
	float NormalizedDistance = 10.0f;
};

/**
 * Campo de densidad volumétrico del archipiélago. Convenio: densidad < 0 es
 * sólido, > 0 es aire; el valor aproxima la distancia a la superficie en
 * metros. Inmutable tras la construcción y seguro entre hilos.
 */
class EXPLORED_API FTerrainDensity
{
public:
	explicit FTerrainDensity(const FArchipelagoLayout& InLayout);

	/** Altura y contexto de la columna (X, Y) en metros. */
	FTerrainColumn SampleColumn(float X, float Y) const;

	/** Densidad en un punto 3D (metros). */
	float Density(const FVector& P) const;

	/** Densidad a partir de una columna ya evaluada (evita recalcularla). */
	float DensityWithColumn(const FVector& P, const FTerrainColumn& Column) const;

	/** Gradiente normalizado de la densidad (normal de la superficie). */
	FVector Normal(const FVector& P, float Step = 0.5f) const;

	/** Color de superficie (RGB lineal) y máscara de roca en alfa. */
	FLinearColor SurfaceColor(const FVector& P, const FVector& Normal) const;

	/**
	 * Pesos de las capas de textura del material del terreno, cada uno en 0..1:
	 * X = arena, Y = suelo de selva (hojarasca), Z = roca, W = carácter volcánico
	 * (1 = basalto y ceniza, 0 = caliza y arena blanca). La hierba es el resto
	 * (1 - X - Y - Z). Coherente con SurfaceColor, que da el tinte por isla.
	 */
	FVector4f SurfaceLayers(const FVector& P, const FVector& Normal) const;

	/** Rango de altura posible dentro de un rectángulo, con margen para cuevas y ruido 3D. */
	void HeightBounds(const FBox2D& Rect, float SampleSpacing, float& OutMin, float& OutMax) const;

	const FArchipelagoLayout& GetLayout() const { return Layout; }
	const TArray<FCaveDesc>& GetCaves() const { return Caves; }

	/** Amplitud máxima del ruido 3D que se suma a la altura (m). */
	static constexpr float OverhangAmplitude = 2.5f;

private:
	float IslandHeight(const FIslandDesc& Island, float X, float Y, float& OutT) const;
	float CaveCarve(const FVector& P) const;
	/** Relieve de cortado de la isla de mesetas: estratos que sobresalen o se retiran y canales verticales. */
	float MesaStrata(const FVector& P, float ColumnHeight, float D) const;
	void BuildCaves();

	FArchipelagoLayout Layout;
	TArray<FCaveDesc> Caves;
	FExploredNoise FloorNoise;
	FExploredNoise DetailNoise;
	FExploredNoise OverhangNoise;
};
