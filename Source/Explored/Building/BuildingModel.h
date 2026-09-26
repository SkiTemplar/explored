#pragma once

#include "CoreMinimal.h"

#include "Building/BuildingTypes.h"

/** Ajustes de estabilidad y desgaste de un nivel de material (no están en el JSON). */
struct EXPLORED_API FBuildingTierTuning
{
	/** Estabilidad que pierde una pieza por cada apoyo vertical (encima de otra). */
	float VerticalLoss = 0.2f;
	/** Estabilidad que pierde por cada apoyo horizontal (en voladizo junto a otra). */
	float HorizontalLoss = 0.3f;
	/** Fracción de la integridad máxima que pierde por día a cubierto y sin lluvia. */
	float DecayPerDay = 0.01f;
	/** Multiplicador del desgaste bajo lluvia intensa (Rain = 1) si no está a cubierto. */
	float RainDecayMultiplier = 3.0f;
};

/**
 * Construcción por piezas y encaje (GDD §8.6): rejilla, apoyos, integridad,
 * daño por temporales, reparación, coste y puntos de reaparición. Modelo puro
 * (solo CoreMinimal): UBuildingSubsystem lo conecta con el mundo.
 *
 * Rejilla (docs/art/kit-construccion.md): celdas de 2 m, pisos de 2,65 m. Cada
 * base tiene su origen (centro de la celda 0,0 al pie de los pilotes) y
 * FBuildingPlacement::Cell se interpreta según el encaje:
 *  - Pilar: esquina (X, Y) = esquina mínima de la celda (X, Y).
 *  - Suelo, techo, mueble, terreno: celda (X, Y).
 *  - Pared y puerta: lado. Rotación par → lado sur de la celda (X, Y) (corre a lo
 *    largo de X); impar → lado oeste de la celda (X, Y) (corre a lo largo de Y).
 *  - Escalera: celda de arranque; sube hacia el +Y local girado (dos celdas).
 * Z es el piso. Un techo del piso L cubre las paredes del piso L y ocupa la
 * altura del suelo del piso L + 1.
 *
 * Apoyos (como Valheim): una pieza que toca el terreno tiene estabilidad 1; si
 * no, la de su mejor apoyo menos una pérdida por nivel de material (vertical u
 * horizontal). Con estabilidad ≤ MinStability no se puede colocar, y al quitar o
 * romper piezas se caen todas las que se quedan por debajo.
 */
class EXPLORED_API FBuildingModel
{
public:
	/** Cotas de la rejilla en cm (constantes de Tools/Blender/props/kit_construccion.py). */
	static constexpr double CellSizeCm = 200.0;
	static constexpr double StoreyHeightCm = 265.0;
	static constexpr double FoundationHeightCm = 60.0;
	static constexpr double FloorThicknessCm = 15.0;
	static constexpr double WallHeightCm = 250.0;

	/** Radio horizontal en el que un punto pertenece a una base existente. */
	static constexpr double BaseRadiusCm = 4000.0;
	/** Pisos por encima o por debajo del origen que admite una base. */
	static constexpr int32 MaxLevels = 8;

	/** Por debajo de esta estabilidad una pieza no se sostiene. */
	static constexpr float MinStability = 0.05f;
	/** Por debajo de esta estabilidad una pieza está «mal apoyada» ante los temporales. */
	static constexpr float WellSupportedStability = 0.5f;
	/** Daño por hora de temporal, en fracción de la integridad máxima, por cada categoría de exceso. */
	static constexpr float StormDamagePerExcessHour = 0.2f;
	/** Fracción del temporal que sufre una pieza a cubierto (bajo un techo). */
	static constexpr float ShelteredStormFactor = 0.5f;
	/** Fracción del coste de construcción que cuesta una reparación completa desde 0. */
	static constexpr float RepairCostFraction = 0.5f;

	explicit FBuildingModel(FBuildingCatalog InCatalog);

	const FBuildingCatalog& GetCatalog() const { return Catalog; }

	/** Ajustes del nivel de material por su orden (0 = palma … 3 = piedra). */
	static FBuildingTierTuning TuningForTierOrder(int32 Order);

	// --- Bases y rejilla -------------------------------------------------

	/** Base cuyo origen está a menos de BaseRadiusCm en horizontal; INDEX_NONE si no hay. */
	int32 FindBaseAt(const FVector& WorldPoint) const;
	/** Igual que FindBaseAt, pero crea una base con origen en ese punto (XY en la rejilla global) si no hay. */
	int32 FindOrCreateBase(const FVector& GroundPoint);
	const FBuildingBaseState* FindBase(int32 BaseId) const;

	/** Encaja un punto del mundo en la rejilla de la base para un tipo de encaje y un giro pedido. */
	FBuildingPlacement SnapToGrid(int32 BaseId, const FVector& WorldPoint, EBuildSocket Socket, int32 Rotation) const;
	/** Punto de pivote en el mundo (base de la malla, como en el kit) y guiñada en grados. */
	bool PlacementToWorld(const FBuildingPlacement& Placement, EBuildSocket Socket, FVector& OutLocation, float& OutYawDegrees) const;

	// --- Colocación ------------------------------------------------------

	/**
	 * ¿Se puede colocar aquí? Comprueba pieza, giro, requisitos de la base, huecos
	 * y apoyos; no mira el inventario. OutStability recibe la estabilidad que tendría.
	 */
	EBuildFailReason CanPlace(const FBuildingPlaceRequest& Request, float* OutStability = nullptr) const;

	/** Herramientas (del nivel y de la pieza) y materiales. OutMissing recibe lo que falta. */
	EBuildFailReason CanAfford(FName DefId, const TMap<FName, int32>& Inventory, const TSet<FName>& HeldTools,
		TArray<FBuildingCost>* OutMissing = nullptr, TArray<FName>* OutMissingTools = nullptr) const;

	/** Todas las comprobaciones; si pasan, descuenta el coste del inventario y coloca la pieza. */
	EBuildFailReason TryPlace(const FBuildingPlaceRequest& Request, TMap<FName, int32>& Inventory,
		const TSet<FName>& HeldTools, int32& OutPieceId);

	/** Minutos de juego de trabajo a una eficiencia dada (1 = normal; se limita a ≥ 0,1). */
	float GetBuildMinutes(FName DefId, float WorkEfficiency = 1.0f) const;

	/** Quita una pieza (desmontar o destruir) y devuelve las que se caen por falta de apoyo. */
	TArray<int32> RemovePiece(int32 PieceId);

	// --- Integridad y temporales -----------------------------------------

	/** Desgaste con el tiempo, lluvia y temporales durante DeltaHours de juego. Rompe y derrumba lo que toque. */
	FBuildingChangeResult Tick(float DeltaHours, const FBuildingWeather& Weather);

	/** Daño directo (rayo, fuego, golpe). Si la rompe, la quita y derrumba lo que dependía de ella. */
	FBuildingChangeResult ApplyDamage(int32 PieceId, float Points);

	/** Materiales que cuesta devolver la pieza a su integridad máxima (fracción del coste). */
	TArray<FBuildingCost> GetRepairCost(int32 PieceId) const;
	/** Repara del todo si hay herramientas y materiales; los descuenta del inventario. */
	EBuildFailReason TryRepair(int32 PieceId, TMap<FName, int32>& Inventory, const TSet<FName>& HeldTools);

	// --- Reaparición -----------------------------------------------------

	/** Enciende o apaga el fuego de una pieza. False si no existe. */
	bool SetLit(int32 PieceId, bool bLit);
	/** Piezas que ahora mismo son puntos de reaparición (encendidas y con bRespawnPoint), en orden de id. */
	TArray<int32> GetRespawnPoints() const;
	/** Punto de reaparición más cercano a un lugar del mundo. False si no hay ninguno. */
	bool FindNearestRespawnPoint(const FVector& From, int32& OutPieceId, FVector& OutLocation) const;

	// --- Consultas -------------------------------------------------------

	const TArray<FBuildingPieceState>& GetPieces() const { return Pieces; }
	const FBuildingPieceState* FindPiece(int32 PieceId) const;
	const FBuildingPieceDef* FindPieceDef(int32 PieceId) const;
	/** Estabilidad actual (0–1); 0 si la pieza no existe. */
	float GetStability(int32 PieceId) const;
	/** Hay un techo sobre la pieza (a cubierto de lluvia y viento directo). */
	bool IsSheltered(int32 PieceId) const;

	/**
	 * Ruta del StaticMesh: la malla propia de la pieza (/Game/Generated/Meshes/Construccion/SM_*)
	 * o la del kit modular de su material y encaje (/Game/Generated/Meshes/Kit<Material>/SM_Kit_*).
	 * Vacía si no hay ninguna (la capa de UE usa una forma básica).
	 */
	FString ResolveMeshPath(const FBuildingPieceDef& Def) const;

	// --- Guardado --------------------------------------------------------

	FBuildingSaveState SaveState() const;
	/**
	 * Restaura un estado guardado. Descarta (y cuenta en OutDropped) las piezas con
	 * definición desconocida o hueco repetido; no derrumba nada al cargar.
	 */
	bool LoadState(const FBuildingSaveState& State, int32* OutDropped = nullptr);

private:
	struct FSupportLink
	{
		int32 PieceId = INDEX_NONE;
		bool bVertical = true;
	};

	void CollectOccupiedSlots(EBuildSocket Socket, const FBuildingPlacement& Placement, TArray<uint64>& OutSlots) const;
	bool IsBlocked(EBuildSocket Socket, const FBuildingPlacement& Placement) const;
	void CollectSupports(EBuildSocket Socket, const FBuildingPlacement& Placement, TArray<FSupportLink>& OutLinks) const;
	/** Estabilidad que tendría la pieza con los apoyos actuales; bOutHasSupport = hay algún apoyo en pie. */
	float EvaluateStability(const FBuildingPieceDef& Def, const FBuildingPlacement& Placement, bool bGroundContact,
		bool& bOutHasSupport) const;
	bool HasPieceOfDefInBase(int32 BaseId, FName DefId) const;
	bool HasRoofOver(EBuildSocket Socket, const FBuildingPlacement& Placement) const;
	TArray<FName> RequiredTools(const FBuildingPieceDef& Def) const;

	void AddPieceInternal(const FBuildingPieceState& Piece, const FBuildingPieceDef& Def);
	void RemovePieceInternal(int32 PieceId);
	void RebuildIndex();
	void RecomputeStability();
	/** Quita las piezas que no se sostienen tras un cambio; devuelve sus ids en orden. */
	TArray<int32> CollapseUnsupported();
	int32 PieceAtSlot(uint64 Slot) const;

	FBuildingCatalog Catalog;
	/** Id de definición → índice en Catalog.Pieces. */
	TMap<FName, int32> DefLookup;
	TArray<FBuildingBaseState> Bases;
	TArray<FBuildingPieceState> Pieces;
	/** Id de pieza → índice en Pieces. */
	TMap<int32, int32> PieceIndex;
	/** Hueco empaquetado → id de pieza. */
	TMap<uint64, int32> SlotOwners;
	/** Id de pieza → estabilidad. */
	TMap<int32, float> Stability;
	int32 NextPieceId = 1;
	int32 NextBaseId = 1;
};
