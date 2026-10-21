#pragma once

#include "CoreMinimal.h"

#include "WorldGen/GroundField.h"
#include "WorldGen/IslandLayout.h"

struct FIslandProps;

/** Mallas del césped (kit K6, Content/Data/grass.json «meshes»). */
enum class EGrassKind : uint8
{
	TuftA,
	TuftB,
	TuftC,
	Tall,
	FlowerA,
	FlowerB,
	Fern,
	Clover,
	Count
};

/** Sitio de una mata: decide la mezcla de mallas (grass.json «mixes»). */
enum class EGrassContext : uint8
{
	Common,    // hierba corriente
	Edge,      // borde de camino o pie de roca: hierba alta
	Clearing,  // claro: flores y tréboles, menos denso
	Palm,      // corona al pie de una palmera: helechos
	Dirt,      // tierra suelta: solo tréboles, muy ralo
	Count
};

constexpr int32 GrassKindCount = static_cast<int32>(EGrassKind::Count);
constexpr int32 GrassContextCount = static_cast<int32>(EGrassContext::Count);

/** Nombre de grass.json: tuft_a, tuft_b, tuft_c, tall, flower_a, flower_b, fern, clover. */
EXPLORED_API const TCHAR* LexToString(EGrassKind Kind);
EXPLORED_API bool GrassKindFromName(const FString& Name, EGrassKind& Out);
/** Nombre de grass.json: common, edge, clearing, palm, dirt. */
EXPLORED_API const TCHAR* LexToString(EGrassContext Context);
EXPLORED_API bool GrassContextFromName(const FString& Name, EGrassContext& Out);

/** Pesos relativos de cada malla en un sitio. */
struct EXPLORED_API FGrassMix
{
	float Weights[GrassKindCount] = {};

	float Total() const;
	/** Malla para una tirada U en [0, 1) (proporcional a los pesos). */
	EGrassKind Pick(float U) const;
};

/**
 * Parámetros del césped. Los valores por defecto son los de Content/Data/grass.json (DataCheck compara
 * las constantes Default* y Zone* con el JSON): Defaults() sirve de respaldo si el fichero falta.
 */
struct EXPLORED_API FGrassConfig
{
	static constexpr float DefaultCellSizeM = 8.0f;
	static constexpr float DefaultRadiusM = 70.0f;
	static constexpr int32 DefaultCellsPerFrame = 4;
	static constexpr float DefaultMaxSlopeDeg = 34.0f;
	static constexpr float DefaultClearanceM = 1.5f;
	static constexpr float DefaultPathMarginM = 0.4f;
	static constexpr float DefaultDensity = 1.8f;
	static constexpr float DefaultDirtDensity = 0.3f;
	static constexpr float ZonePalmarDensity = 2.6f;
	static constexpr float ZoneColinaDensity = 2.4f;
	static constexpr float ZoneBambuDensity = 1.8f;
	static constexpr float ZoneCalaDensity = 1.4f;
	static constexpr float ZonePlayaDensity = 1.0f;
	static constexpr float ZoneRocasDensity = 0.6f;
	static constexpr float ZoneCargueroDensity = 1.2f;

	float CellSizeM = DefaultCellSizeM;
	/** Radio (m) alrededor de la cámara con celdas de césped. */
	float RadiusM = DefaultRadiusM;
	/** Celdas nuevas que se generan como mucho en cada Update. */
	int32 CellsPerFrame = DefaultCellsPerFrame;
	uint32 Seed = 7177;
	float MaxSlopeDeg = DefaultMaxSlopeDeg;
	/** Holgura (m) alrededor de interactuables, recogibles, balsa, faro y casa. */
	float ClearanceM = DefaultClearanceM;
	/** Distancia extra (m) al borde de los caminos de island.json. */
	float PathMarginM = DefaultPathMarginM;
	/** Holgura (m) alrededor de los puntos de autor (aparición, sitio de la balsa, faro). */
	float PointClearanceM = 3.0f;
	/** Hundimiento (m) de la base bajo el suelo, más el desnivel de la huella en pendiente. */
	float SinkM = 0.03f;
	/** Radio (m) de la base de una mata a escala 1 (para el hundimiento en pendiente). */
	float FootprintM = 0.2f;
	/** 0 = matas verticales, 1 = perpendiculares al suelo. */
	float AlignToNormal = 0.5f;
	float ScaleMin = 0.8f;
	float ScaleMax = 1.3f;
	/** Matas por m² fuera de las zonas con densidad propia. */
	float DensityPerM2 = DefaultDensity;
	/** Matas por m² sobre tierra suelta (tréboles). */
	float DirtDensityPerM2 = DefaultDirtDensity;
	/** Densidad (matas por m²) por id de zona de island.json. */
	TMap<FName, float> ZoneDensity;
	/** Ancho (m) del fundido de la densidad en el borde de cada zona. */
	float ZoneFeatherM = 8.0f;
	/** Claros: ruido de valor de esta celda (m) por encima del umbral. */
	float ClearingCellM = 22.0f;
	float ClearingThreshold = 0.64f;
	/** Factor de densidad dentro de un claro. */
	float ClearingDensity = 0.6f;
	/** Banda (m) de hierba alta junto al borde del camino (tras PathMarginM) y alrededor de las rocas. */
	float EdgeBandM = 2.0f;
	/** Corona de helechos alrededor de cada palmera: de PalmTrunkM a PalmRadiusM. */
	float PalmTrunkM = 0.7f;
	float PalmRadiusM = 3.5f;
	/** Escala propia de cada malla (se multiplica por la tirada ScaleMin-ScaleMax). */
	float KindScale[GrassKindCount] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 1.2f};
	FGrassMix Mixes[GrassContextCount];

	/** Valores de grass.json (mezclas y zonas incluidas). */
	static FGrassConfig Defaults();

	/** Mayor densidad posible (m⁻²): fija la rejilla de candidatos de cada celda. */
	float MaxDensity() const;
	bool Validate(FString& OutError) const;
};

/** Círculo sin césped (metros; el radio ya incluye la holgura). */
struct EXPLORED_API FGrassExclusion
{
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 1.0f;
};

/** Lo que el césped necesita del mundo. Se comparte entre el modelo y sus copias de prueba. */
struct EXPLORED_API FGrassWorld
{
	TSharedPtr<const FGroundField> Ground;
	FIslandLayout Layout;
	TArray<FGrassExclusion> Exclusions;
	/** Palmeras (de juego y decorativas): corona de helechos. */
	TArray<FVector2D> Palms;

	/**
	 * Círculos de island_props.json (cada colocación sin ancla, con el radio de su caja o volumen más
	 * ClearanceM) y de los puntos de autor de island.json (PointClearanceM).
	 */
	static TArray<FGrassExclusion> ExclusionsFrom(const FIslandProps& Props, const FIslandLayout& Layout, const FGrassConfig& Config);
	/** Posiciones de las colocaciones de tipo «palma». */
	static TArray<FVector2D> PalmsFrom(const FIslandProps& Props);
};

/** Una mata. Metros y grados; Up es el eje Z de la malla (vertical mezclada con la normal). */
struct EXPLORED_API FGrassInstance
{
	EGrassKind Kind = EGrassKind::TuftA;
	FVector Location = FVector::ZeroVector;
	float YawDeg = 0.0f;
	float Scale = 1.0f;
	FVector3f Up = FVector3f(0.0f, 0.0f, 1.0f);
	EGrassContext Context = EGrassContext::Common;
};

/** Escritura en la ranura Slot de la malla Kind (Slot == número de instancias: se añade al final). */
struct EXPLORED_API FGrassWrite
{
	int32 Slot = 0;
	FGrassInstance Instance;
};

/** Cambios de un Update para el componente de instancias de cada malla. */
struct EXPLORED_API FGrassUpdate
{
	TArray<FGrassWrite> Writes[GrassKindCount];
	/** Ranuras que quedan libres: se ocultan (escala 0 lejos de la cámara) hasta que se reutilicen. */
	TArray<int32> Hides[GrassKindCount];
	int32 CellsBuilt = 0;
	int32 CellsRemoved = 0;

	bool IsEmpty() const { return CellsBuilt == 0 && CellsRemoved == 0; }
	int32 WriteCount() const;
	int32 HideCount() const;
};

/**
 * Césped alrededor de la cámara (GDD v3 §11, F7): rejilla de celdas de CellSizeM en un radio RadiusM.
 * Cada celda se rellena de forma determinista por el hash de su coordenada (subrejilla con desplazamiento
 * y aceptación por densidad), sin depender del orden de llegada ni de la cámara. La altura sale de
 * FGroundField (la malla exacta), no de SampleColumn.
 *
 * Update(cámara) solo toca el borde: quita las celdas que salen del radio (con histéresis de media
 * celda) y genera como mucho CellsPerFrame celdas nuevas, las más cercanas primero. Las ranuras de
 * instancia se reciclan (las de una celda que sale pasan a la siguiente que entra), así que el
 * componente de cada malla nunca reordena índices.
 */
class EXPLORED_API FGrassFieldModel
{
public:
	FGrassFieldModel(const FGrassConfig& InConfig, TSharedPtr<const FGrassWorld> InWorld);

	/** Matas de una celda (determinista: misma celda, misma salida). */
	static void GenerateCell(const FGrassConfig& Config, const FGrassWorld& World, const FIntPoint& Cell, TArray<FGrassInstance>& Out);

	FGrassUpdate Update(const FVector2D& CameraMeters);

	/** Celda que contiene un punto (metros). */
	FIntPoint CellOf(const FVector2D& Meters) const;
	/** Distancia (m) del punto al rectángulo de la celda. */
	double DistanceToCell(const FVector2D& Meters, const FIntPoint& Cell) const;

	bool IsCellActive(const FIntPoint& Cell) const { return Cells.Contains(Cell); }
	int32 GetActiveCellCount() const { return Cells.Num(); }
	/** Celdas dentro del radio de la última cámara que aún no se han generado. */
	int32 GetPendingCellCount() const { return Pending; }
	int32 GetLiveInstanceCount() const { return LiveInstances; }
	int32 GetLiveCount(EGrassKind Kind) const;
	/** Ranuras usadas alguna vez (tamaño del componente) de una malla. */
	int32 GetSlotCount(EGrassKind Kind) const { return SlotHighWater[static_cast<int32>(Kind)]; }
	const TArray<FGrassInstance>* FindCell(const FIntPoint& Cell) const;
	const FGrassConfig& GetConfig() const { return Config; }

private:
	struct FCellState
	{
		TArray<FGrassInstance> Instances;
		TArray<int32> Slots;
	};

	int32 AcquireSlot(int32 Kind, FGrassUpdate& Update);
	void RemoveFarCells(const FVector2D& Camera, FGrassUpdate& Update);
	void AddNearCells(const FVector2D& Camera, FGrassUpdate& Update);

	FGrassConfig Config;
	TSharedPtr<const FGrassWorld> World;
	TMap<FIntPoint, FCellState> Cells;
	TArray<int32> FreeSlots[GrassKindCount];
	/** Ranuras liberadas en este Update: si nadie las reutiliza, se ocultan al final. */
	TArray<int32> ReleasedNow[GrassKindCount];
	/** 1 si la ranura tiene una mata visible. */
	TArray<uint8> SlotVisible[GrassKindCount];
	int32 SlotHighWater[GrassKindCount] = {};
	int32 LiveInstances = 0;
	int32 Pending = 0;
	FVector2D LastCamera = FVector2D::ZeroVector;
	bool bHasCamera = false;
};
