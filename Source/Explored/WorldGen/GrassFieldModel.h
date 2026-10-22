#pragma once

#include "CoreMinimal.h"

#include "WorldGen/GrassThinning.h"
#include "WorldGen/GroundField.h"
#include "WorldGen/IslandLayout.h"

struct FIslandProps;

/**
 * Mallas del césped (kit K6, Content/Data/grass.json «meshes»). Los parches (PatchA-C, 17-18 hojas en
 * ~0,4 m de radio) forman la alfombra; el resto son acentos (flores, helechos, tréboles, hierba alta).
 */
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
	PatchA,
	PatchB,
	PatchC,
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

/** Nombre de grass.json: tuft_a, tuft_b, tuft_c, tall, flower_a, flower_b, fern, clover, patch_a, patch_b, patch_c. */
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
	static constexpr int32 DefaultWorkPerFrame = 700;
	static constexpr float DefaultMaxSlopeDeg = 34.0f;
	static constexpr float DefaultClearanceM = 1.5f;
	static constexpr float DefaultPathMarginM = 0.4f;
	static constexpr float DefaultDensity = 5.5f;
	static constexpr float DefaultDirtDensity = 0.5f;
	static constexpr float ZonePalmarDensity = 6.0f;
	static constexpr float ZoneColinaDensity = 5.5f;
	static constexpr float ZoneBambuDensity = 5.0f;
	static constexpr float ZoneCalaDensity = 5.0f;
	static constexpr float ZonePlayaDensity = 4.5f;
	static constexpr float ZoneRocasDensity = 3.0f;
	static constexpr float ZoneCargueroDensity = 4.0f;

	float CellSizeM = DefaultCellSizeM;
	/**
	 * Candidatos de la subrejilla que se evalúan en cada Update (≈ 1 µs cada uno): reparte entre frames
	 * el relleno tras un salto de cámara. Un trabajo empieza si su coste esperado cabe; el real puede
	 * pasarse un poco (como mucho una celda entera), y siempre se hace al menos uno por Update.
	 */
	int32 WorkPerFrame = DefaultWorkPerFrame;
	/** Adelgazamiento con la distancia (grass.json «thinning»): fija también el alcance de las celdas. */
	FGrassThinning Thinning;
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
	/** 0 = matas verticales, 1 = perpendiculares al suelo. */
	float AlignToNormal = 0.5f;
	float ScaleMin = 0.8f;
	float ScaleMax = 1.3f;
	/** Parches (o matas) por m² fuera de las zonas con densidad propia. */
	float DensityPerM2 = DefaultDensity;
	/** Por m² sobre tierra suelta (tréboles y matas sueltas). */
	float DirtDensityPerM2 = DefaultDirtDensity;
	/** Densidad (parches por m²) por id de zona de island.json. */
	TMap<FName, float> ZoneDensity;
	/** Ancho (m) del fundido de la densidad en el borde de cada zona. */
	float ZoneFeatherM = 8.0f;
	/** Claros: ruido de valor de esta celda (m) por encima del umbral. */
	float ClearingCellM = 22.0f;
	float ClearingThreshold = 0.64f;
	/** Factor de densidad dentro de un claro. */
	float ClearingDensity = 0.85f;
	/** Banda (m) de hierba alta junto al borde del camino (tras PathMarginM) y alrededor de las rocas. */
	float EdgeBandM = 2.0f;
	/**
	 * Fundido (m) del borde de la alfombra: junto a caminos, exclusiones y suelo que no es hierba (arena,
	 * tierra, roca) la densidad baja de 1 a 0 y la altura de 1 a EdgeMinHeight en esta distancia, en vez
	 * de cortar en seco.
	 */
	float EdgeFadeM = 1.5f;
	/** Altura relativa de los parches en el mismo borde (EdgeFadeM la sube hasta 1). */
	float EdgeMinHeight = 0.3f;
	/** Corona de helechos alrededor de cada palmera: de PalmTrunkM a PalmRadiusM. */
	float PalmTrunkM = 0.7f;
	float PalmRadiusM = 3.5f;
	/** Escala propia de cada malla (se multiplica por la tirada ScaleMin-ScaleMax). */
	float KindScale[GrassKindCount] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 1.2f, 1.0f, 1.0f, 1.0f};
	/** Radio (m) de la huella de cada malla a escala 1: hundimiento en pendiente y cobertura. */
	float KindFootprintM[GrassKindCount] = {0.2f, 0.2f, 0.2f, 0.3f, 0.1f, 0.1f, 0.4f, 0.15f, 0.433f, 0.428f, 0.413f};
	FGrassMix Mixes[GrassContextCount];

	/** Valores de grass.json (mezclas y zonas incluidas). */
	static FGrassConfig Defaults();

	/** Mayor densidad posible (m⁻²): fija la rejilla de candidatos de cada celda. */
	float MaxDensity() const;
	/** Candidatos de la subrejilla por eje de una celda. */
	int32 Subdivisions() const;
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

/** Una mata o parche. Metros y grados; Up es el eje Z de la malla (vertical mezclada con la normal). */
struct EXPLORED_API FGrassInstance
{
	EGrassKind Kind = EGrassKind::TuftA;
	FVector Location = FVector::ZeroVector;
	float YawDeg = 0.0f;
	float Scale = 1.0f;
	FVector3f Up = FVector3f(0.0f, 0.0f, 1.0f);
	EGrassContext Context = EGrassContext::Common;
	/** Rango de adelgazamiento en [0, 1) (dato por instancia 0 del material; ver FGrassThinning). */
	float Rank = 0.0f;
	/** Variación del parche en [0, 1) (dato por instancia 1: tinte y altura). */
	float Variation = 0.0f;
	/** Escala de la altura (eje Z de la malla) en el fundido del borde: EdgeMinHeight-1. */
	float HeightScale = 1.0f;
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
	/** Celdas creadas o subidas de nivel en este Update. */
	int32 CellsBuilt = 0;
	/** Celdas quitadas o bajadas de nivel en este Update. */
	int32 CellsRemoved = 0;
	/** Candidatos evaluados (el trabajo que limita WorkPerFrame). */
	int32 Work = 0;

	bool IsEmpty() const { return CellsBuilt == 0 && CellsRemoved == 0; }
	int32 WriteCount() const;
	int32 HideCount() const;
};

/**
 * Césped alrededor de la cámara (GDD v3 §11, F7-F8): rejilla de celdas de CellSizeM hasta
 * Thinning.ReachM(). Cada candidato de la subrejilla de una celda tiene un hash fijo (posición,
 * aceptación por densidad, malla, escala, giro, rango y variación), así que la salida no depende del
 * orden de llegada ni de la cámara. La altura sale de FGroundField (la malla exacta), no de SampleColumn.
 *
 * Cada celda guarda un nivel de densidad (FGrassThinning): solo los parches con rango < f_k, ordenados
 * por rango. Update(cámara) sube de nivel las celdas que se acercan (con MarginM de adelanto, generando
 * solo la franja de rangos que falta), baja las que se alejan (con HysteresisM más; quitar del final no
 * cuesta nada) y quita las que salen del alcance. Las subidas y las celdas nuevas se hacen por orden de
 * cercanía hasta gastar WorkPerFrame candidatos. Las ranuras de instancia se reciclan, así que el
 * componente de cada malla nunca reordena índices.
 */
class EXPLORED_API FGrassFieldModel
{
public:
	FGrassFieldModel(const FGrassConfig& InConfig, TSharedPtr<const FGrassWorld> InWorld);

	/**
	 * Matas de una celda con rango en [RankLo, RankHi), en orden de candidato (determinista: misma celda y
	 * franja, misma salida). Devuelve los candidatos evaluados (el trabajo).
	 */
	static int32 GenerateCell(const FGrassConfig& Config, const FGrassWorld& World, const FIntPoint& Cell,
		float RankLo, float RankHi, TArray<FGrassInstance>& Out);
	/** Todas las matas de la celda (rango en [0, 1)). */
	static void GenerateCell(const FGrassConfig& Config, const FGrassWorld& World, const FIntPoint& Cell, TArray<FGrassInstance>& Out);

	FGrassUpdate Update(const FVector2D& CameraMeters);

	/** Celda que contiene un punto (metros). */
	FIntPoint CellOf(const FVector2D& Meters) const;
	/** Distancia (m) del punto al rectángulo de la celda. */
	double DistanceToCell(const FVector2D& Meters, const FIntPoint& Cell) const;

	bool IsCellActive(const FIntPoint& Cell) const { return Cells.Contains(Cell); }
	int32 GetActiveCellCount() const { return Cells.Num(); }
	/** Nivel de densidad de una celda activa (INDEX_NONE si no está). */
	int32 GetCellLevel(const FIntPoint& Cell) const;
	/** Celdas que faltan por crear o subir de nivel para la última cámara. */
	int32 GetPendingCellCount() const { return Pending; }
	int32 GetLiveInstanceCount() const { return LiveInstances; }
	int32 GetLiveCount(EGrassKind Kind) const;
	/** Ranuras usadas alguna vez (tamaño del componente) de una malla. */
	int32 GetSlotCount(EGrassKind Kind) const { return SlotHighWater[static_cast<int32>(Kind)]; }
	const TArray<FGrassInstance>* FindCell(const FIntPoint& Cell) const;
	const FGrassConfig& GetConfig() const { return Config; }
	/**
	 * Triángulos de las matas vivas dentro de un sector horizontal (cámara, dirección y medio ángulo en
	 * radianes), con TrisPerKind triángulos por malla: el coste de geometría «a la vista».
	 */
	int64 CountTrianglesInView(const FVector2D& Camera, const FVector2D& Forward, float HalfAngleRad, const int32 (&TrisPerKind)[GrassKindCount]) const;

private:
	struct FCellState
	{
		/** Ordenadas por rango: bajar de nivel quita del final. */
		TArray<FGrassInstance> Instances;
		TArray<int32> Slots;
		int32 Level = 0;
	};

	struct FCellJob
	{
		FIntPoint Cell;
		double Dist = 0.0;
		int32 Level = 0;
	};

	int32 AcquireSlot(int32 Kind);
	void ReleaseTail(FCellState& State, int32 Keep);
	void ShrinkOrRemoveCells(const FVector2D& Camera, FGrassUpdate& Update);
	void GrowCells(const FVector2D& Camera, FGrassUpdate& Update);
	void RunJob(const FCellJob& Job, FGrassUpdate& Update);

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
