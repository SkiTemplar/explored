#pragma once

#include "CoreMinimal.h"

#include "Save/SaveWorldDeltas.h"

/**
 * Estado replicado de cada instancia de vegetación talada o golpeada (biblia 08 §2.3).
 *
 * En el servidor el estado vive en `FVegetationRuntimeState` (con `FName` de componente
 * y `float` de rebrote); por el cable va una entrada de **10 bytes**:
 *
 * - **Clave (7 B):** `int16 CellX`, `int16 CellY`, `uint8 Species`, `uint16 Index`.
 *   `Species` es el índice del HISM dentro de la celda (0–15) en una tabla ordenada y
 *   determinista que sale de los mismos datos en las dos puntas (BuildSpeciesTable).
 * - **Estado (3 B):** `uint8` con la etapa (2 bits) y los golpes (4 bits, tope 15), y
 *   `uint16` día de rebrote × 4 (0,25 días de resolución; 0 = sin rebrote).
 *
 * Con los 4 B de `FFastArraySerializer`, 14 B por cambio. El array replicado no pasa de
 * **4096 instancias**: al superarlo, los tocones sin rebrote pendiente salen del array y
 * pasan al snapshot por celda (`FSaveIndexSet::Encode`, el mismo texto del guardado).
 *
 * El mismo modelo sirve en las dos puntas: el servidor llama a Set/Compact/ConsumeDelta y
 * el cliente a ApplyDelta; View da en los dos lo que se ve de cada instancia.
 */
enum class EVegetationNetStage : uint8
{
	/** Sin tocar (o rebrotada del todo): es lo que hay si no se dice nada. */
	Intact,
	/** Golpeada, con los golpes acumulados. */
	Felling,
	/** Talada: tocón (con o sin rebrote pendiente). */
	Stump,
	/** Brote creciendo hacia adulto. */
	Sprout,
	Count
};

struct EXPLORED_API FVegetationNetKey
{
	int16 CellX = 0;
	int16 CellY = 0;
	uint8 Species = 0;
	uint16 Index = 0;

	FIntPoint Cell() const { return FIntPoint(CellX, CellY); }

	bool operator==(const FVegetationNetKey& Other) const
	{
		return CellX == Other.CellX && CellY == Other.CellY && Species == Other.Species && Index == Other.Index;
	}
	bool operator!=(const FVegetationNetKey& Other) const { return !(*this == Other); }
	/** Orden total (celda X, celda Y, especie, índice): el de los deltas y la compactación. */
	bool operator<(const FVegetationNetKey& Other) const;
};

EXPLORED_API uint32 GetTypeHash(const FVegetationNetKey& Key);

struct EXPLORED_API FVegetationNetState
{
	EVegetationNetStage Stage = EVegetationNetStage::Intact;
	/** Golpes acumulados (0–15); solo cuentan en Felling. */
	uint8 Hits = 0;
	/** Día total de juego × 4 en que rebrota; 0 = sin rebrote. Solo en Stump y Sprout. */
	uint16 RegrowAtQuarterDays = 0;

	bool operator==(const FVegetationNetState& Other) const
	{
		return Stage == Other.Stage && Hits == Other.Hits && RegrowAtQuarterDays == Other.RegrowAtQuarterDays;
	}
	bool operator!=(const FVegetationNetState& Other) const { return !(*this == Other); }
};

class EXPLORED_API FVegetationNetStateModel
{
public:
	static constexpr int32 KeyBytes = 7;
	static constexpr int32 StateBytes = 3;
	static constexpr int32 EntryBytes = KeyBytes + StateBytes;
	static constexpr int32 FastArrayOverheadBytes = 4;
	/** Lo que cuesta un cambio en el cable (08 §2.3): 10 B + 4 B de sobrecarga. */
	static constexpr int32 BytesPerChange = EntryBytes + FastArrayOverheadBytes;
	static constexpr int32 MaxSpeciesPerCell = 16;
	static constexpr int32 MaxHits = 15;
	/** Tope duro del array replicado. */
	static constexpr int32 MaxEntries = 4096;
	/** Al compactar se baja hasta aquí, para no compactar una a una en cada tala. */
	static constexpr int32 CompactTarget = 3072;
	/** El progreso de tala (golpes) solo va a los clientes a menos de 60 m. */
	static constexpr double FellingProgressRadiusCm = 6000.0;

	// --- Clave y estado --------------------------------------------------------------

	/** Clave de red; false si la celda no cabe en int16, la especie pasa de 15 o el índice de 65 535. */
	static bool MakeKey(const FIntPoint& Cell, int32 Species, int32 Index, FVegetationNetKey& OutKey);

	/**
	 * Tabla de especies de una celda: los nombres de sus HISM, sin repetir y ordenados
	 * por texto sin distinguir mayúsculas (como compara `FName`), igual en las dos puntas.
	 * Los nombres vacíos no cuentan.
	 */
	static void BuildSpeciesTable(const TArray<FName>& ComponentNames, TArray<FName>& OutTable);

	/** Índice en la tabla, o INDEX_NONE si no está o pasa del tope de 16. */
	static int32 SpeciesIndex(const TArray<FName>& Table, FName Component);

	/**
	 * Día de rebrote × 4 redondeado hacia arriba: el cliente nunca ve rebrotar antes que el
	 * servidor. Negativo, NaN o infinito → 0 (sin rebrote); acotado a [1, 65 535].
	 */
	static uint16 QuantizeRegrowDays(float RegrowAtDays);
	static float RegrowDays(uint16 QuarterDays);

	/** Estado canónico: golpes solo en Felling (acotados a 15), rebrote solo en Stump y Sprout. */
	static FVegetationNetState Canonical(const FVegetationNetState& State);
	static FVegetationNetState MakeState(EVegetationNetStage Stage, int32 Hits, float RegrowAtDays);

	/** Añade los 10 bytes de una entrada al final de Out. */
	static void AppendEntry(const FVegetationNetKey& Key, const FVegetationNetState& State, TArray<uint8>& Out);

	/**
	 * Lee una entrada de exactamente 10 bytes. False si el tamaño no cuadra, la especie
	 * pasa de 15, la etapa no existe, los bits reservados no van a cero o el estado no es
	 * canónico (golpes fuera de Felling, rebrote fuera de Stump/Sprout).
	 */
	static bool DecodeEntry(const TArray<uint8>& In, FVegetationNetKey& OutKey, FVegetationNetState& OutState);

	/** ¿Va este cambio a un cliente a esta distancia? El progreso de tala, solo cerca. */
	static bool ShouldSendToClient(const FVegetationNetState& State, double DistanceCm);

	/** Índice dentro del conjunto de la celda en el snapshot: especie × 65 536 + índice. */
	static int32 SnapshotIndex(const FVegetationNetKey& Key) { return (static_cast<int32>(Key.Species) << 16) | Key.Index; }

	// --- Array replicado -------------------------------------------------------------

	/** Lo que el servidor manda en una ronda de replicación. */
	struct FDelta
	{
		/** Entradas nuevas o cambiadas, en el orden de la clave. */
		TArray<TPair<FVegetationNetKey, FVegetationNetState>> Changed;
		/** Entradas que salen del array (vuelven a lo que diga el snapshot, o a intactas). */
		TArray<FVegetationNetKey> Removed;
		/** Snapshot completo de las celdas que han cambiado, con el texto del guardado. */
		TArray<TPair<FIntPoint, FString>> CellSnapshots;

		bool IsEmpty() const { return Changed.Num() == 0 && Removed.Num() == 0 && CellSnapshots.Num() == 0; }
		/** Coste aproximado en el cable: 14 B por cambio, 4 B por baja, 6 B + texto por celda. */
		int32 EstimateBytes() const;
	};

	/** Servidor: fija el estado de una instancia (se canoniza antes de guardar). */
	void Set(const FVegetationNetKey& Key, const FVegetationNetState& State);

	/** Lo que se ve de una instancia: la entrada si la hay, si no el snapshot (tocón) o intacta. */
	FVegetationNetState View(const FVegetationNetKey& Key) const;

	int32 Num() const { return Entries.Num(); }
	bool IsInSnapshot(const FVegetationNetKey& Key) const;

	/**
	 * Servidor: si el array pasa de 4096, lo baja a 3072 moviendo al snapshot primero los
	 * tocones sin rebrote pendiente (de la entrada más antigua a la más nueva) y, si no
	 * bastan, los tocones con rebrote pendiente (el servidor los vuelve a meter con Set al
	 * brotar). Talas en curso y brotes nunca salen. Devuelve cuántas entradas movió.
	 */
	int32 Compact();

	/** Servidor: saca lo pendiente de replicar desde la última llamada. */
	void ConsumeDelta(FDelta& OutDelta);

	/**
	 * Cliente: aplica una ronda en el orden snapshots → bajas → cambios. Un snapshot que no
	 * se puede leer o una entrada no canónica se ignoran (y devuelve false); el resto se
	 * aplica igual.
	 */
	bool ApplyDelta(const FDelta& Delta);

	/** Snapshot de una celda con el texto del guardado (vacío si no hay tocones compactados). */
	FString CellSnapshot(const FIntPoint& Cell) const;

private:
	struct FEntry
	{
		FVegetationNetState State;
		/** Orden de la última modificación: la compactación empieza por la más antigua. */
		uint64 Seq = 0;
	};

	void RemoveEntry(const FVegetationNetKey& Key);
	void MoveToSnapshot(const FVegetationNetKey& Key);
	static bool IsPlainStump(const FVegetationNetState& State);

	TMap<FVegetationNetKey, FEntry> Entries;
	TMap<FIntPoint, FSaveIndexSet> Snapshots;
	TSet<FVegetationNetKey> DirtyKeys;
	TSet<FIntPoint> DirtyCells;
	uint64 NextSeq = 1;
};
