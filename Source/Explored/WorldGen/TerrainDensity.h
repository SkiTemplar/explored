#pragma once

#include "CoreMinimal.h"
#include "Core/ExploredNoise.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/CoastalCliffModel.h"
#include "WorldGen/IslandShapeModel.h"
#include "WorldGen/IslandShelfModel.h"
#include "WorldGen/IslandReliefModel.h"
#include "WorldGen/KarstTowerModel.h"
#include "WorldGen/SeafloorModel.h"
#include "WorldGen/TerrainEdits.h"

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
 *
 * Terreno editable (GDD v2 §7.3 punto 1): con SetEdits, Density y DensityWithColumn
 * consultan antes que nada la capa de ediciones del jugador (FTerrainEdits) y le suman
 * su delta al campo procedural; sin capa, o lejos de cualquier edición, cuestan lo mismo
 * que antes. La capa sí cambia: quien la edita no puede hacerlo mientras otro hilo lee
 * esta densidad (el remallado va en el hilo de juego o sobre una copia de la capa).
 * Las ediciones la usan como base con ProceduralDensity, nunca con Density.
 */
class EXPLORED_API FTerrainDensity
{
public:
	explicit FTerrainDensity(const FArchipelagoLayout& InLayout);

	/** Altura y contexto de la columna (X, Y) en metros. */
	FTerrainColumn SampleColumn(float X, float Y) const;

	/** Densidad en un punto 3D (metros): ediciones del jugador + campo procedural. */
	float Density(const FVector& P) const;

	/** Densidad a partir de una columna ya evaluada (evita recalcularla), con ediciones. */
	float DensityWithColumn(const FVector& P, const FTerrainColumn& Column) const;

	/** Solo el campo procedural, sin ediciones: la base sobre la que se guardan los deltas. */
	float ProceduralDensity(const FVector& P) const;
	float ProceduralDensityWithColumn(const FVector& P, const FTerrainColumn& Column) const;

	/** Engancha (o suelta, con nullptr) la capa de ediciones. Las copias de esta densidad la comparten. */
	void SetEdits(TSharedPtr<const FTerrainEdits> InEdits) { Edits = MoveTemp(InEdits); }
	const FTerrainEdits* GetEdits() const { return Edits ? &*Edits : nullptr; }

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
	const FSeafloorModel& GetSeafloor() const { return Seafloor; }
	/** Relieve erosionado de la isla (índice del layout); null si no se erosiona. */
	const FIslandReliefGrid* GetRelief(int32 IslandIndex) const
	{
		return Reliefs.IsValidIndex(IslandIndex) && Reliefs[IslandIndex].Grid ? &*Reliefs[IslandIndex].Grid : nullptr;
	}

	/** Amplitud máxima del ruido 3D que se suma a la altura (m). */
	static constexpr float OverhangAmplitude = 2.5f;

private:
	float IslandHeight(const FIslandDesc& Island, float X, float Y, float& OutT) const;
	float CaveCarve(const FVector& P) const;
	void BuildCaves();

	FArchipelagoLayout Layout;
	TArray<FCaveDesc> Caves;
	/** Fondo marino continuo (dorsal, llanura abisal, montículos e islotes sueltos). */
	FSeafloorModel Seafloor;
	FExploredNoise DetailNoise;
	FExploredNoise OverhangNoise;

	/**
	 * Relieve erosionado (erosión hidráulica y térmica más ríos, FIslandReliefModel) y torres
	 * kársticas (FKarstTowerModel, solo el macizo de La Meseta) de cada isla, alineados con
	 * Layout.Islands; nulos donde no aplican. Las rejillas se calculan una vez por isla y se
	 * comparten entre instancias (la erosión no es gratis); los punteros se fijan en el
	 * constructor, así que no rompen la inmutabilidad de la clase.
	 */
	struct FIslandRelief
	{
		TSharedPtr<const FIslandReliefGrid> Grid;
		TSharedPtr<const FKarstLayout> Karst;
		/** Plataforma submarina tabulada por rumbo (FIslandShelfModel); todas las islas. */
		TSharedPtr<const FIslandShelf> Shelf;
		/** Tramos de acantilado marino (FCoastalCliffModel); solo las islas que los tienen. */
		TSharedPtr<const FCoastalCliffs> Cliffs;
	};
	TArray<FIslandRelief> Reliefs;

	void BuildReliefs();
	void BuildCoast(int32 IslandIndex);
	const FIslandRelief* FindRelief(const FIslandDesc& Island) const;
	/** Perfil submarino (plataforma, talud y fondo real) de la isla en T >= 1; pie de costa en T < 1. */
	float UnderwaterHeight(const FIslandDesc& Island, const FIslandRelief* Relief, const FExploredNoise& N, const FVector2D& Q,
		float T, float Angle, float X, float Y) const;
	/** Tierra ya erosionada: diferencia de la erosión y torres kársticas sobre la base. */
	float FinishLand(const FIslandDesc& Island, const FIslandRelief* Relief, const FVector2D& Q, float T, float Land) const;

	/** Capa de ediciones del jugador; nula en el mundo recién generado. */
	TSharedPtr<const FTerrainEdits> Edits;
};
