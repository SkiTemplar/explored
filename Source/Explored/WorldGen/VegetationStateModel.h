#pragma once

#include "CoreMinimal.h"

#include "Save/SaveValue.h"

/**
 * Estado de cada instancia de vegetación talable (biblia 02 §1.2 y §1.6,
 * biblia 08 §2.3): etapa, golpes, cuándo se taló y cuándo rebrota; cómo viaja
 * por la red y cómo se guarda. Modelo puro: FVegetationRuntimeState
 * (VegetationHarvestState.h) lo lleva en el motor, el GameState lo replicará
 * con FFastArraySerializer y FSaveWorldDeltas guarda el reloj.
 *
 * El tiempo va en minutos de juego enteros (int64), como FFellingModel.
 */

/** Etapa de red (2 bits, biblia 08 §2.3). */
enum class EVegetationStage : uint8
{
	Intact = 0,		// «Intacta»: en pie, sin golpes
	Felling = 1,	// «Talandose»: en pie, con golpes
	Stump = 2,		// «Tocon»: talada, sin brote todavía (o arrancada, sin rebrote)
	Sapling = 3		// «Brote»: creciendo, todavía no se puede talar
};

/** Lo que el servidor sabe de una instancia. */
struct EXPLORED_API FVegetationInstanceState
{
	/** Golpes recibidos mientras está en pie. */
	int32 Hits = 0;
	/** Minuto de juego de la tala; < 0 = en pie. */
	int64 FelledAtMinute = -1;
	/** Minuto en que asoma el brote; < 0 = no asoma. */
	int64 SproutAtMinute = -1;
	/** Minuto en que vuelve a ser talable; < 0 = no rebrota (arrancada o especie permanente). */
	int64 RegrowAtMinute = -1;
};

/** Clave de red de una instancia (7 bytes en el cable). */
struct EXPLORED_API FVegetationNetKey
{
	int16 CellX = 0;
	int16 CellY = 0;
	/** Hueco de la especie (HISM) en la celda, 0–15 (SpeciesSlots). */
	uint8 Species = 0;
	uint16 Index = 0;

	bool operator==(const FVegetationNetKey& Other) const
	{
		return CellX == Other.CellX && CellY == Other.CellY && Species == Other.Species && Index == Other.Index;
	}
};

/** Estado de red de una instancia (3 bytes en el cable). */
struct EXPLORED_API FVegetationNetState
{
	EVegetationStage Stage = EVegetationStage::Intact;
	/** Golpes, con tope en MaxNetHits. */
	uint8 Hits = 0;
	/** Día de rebrote × 4, redondeado hacia arriba; NoRegrow = no rebrota. */
	uint16 RegrowAtQuarterDays = 0;

	bool operator==(const FVegetationNetState& Other) const
	{
		return Stage == Other.Stage && Hits == Other.Hits && RegrowAtQuarterDays == Other.RegrowAtQuarterDays;
	}
};

struct EXPLORED_API FVegetationNetEntry
{
	FVegetationNetKey Key;
	FVegetationNetState State;
};

/** Hora de tala de una instancia, tal como va al guardado. */
struct EXPLORED_API FVegetationClockEntry
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	/** Nombre del HISM (el mismo que la capa de FSaveWorldDeltas). */
	FName Component;
	int32 Index = 0;
	int64 FelledAtMinute = 0;
};

/**
 * Sección «vegetationClock» del guardado (biblia 02 §1.6): cuándo se taló cada
 * instancia que espera rebrote. Los índices talados siguen en la capa de su
 * componente de FSaveWorldDeltas; esto solo añade la hora. Ordenado por
 * (Y, X, componente, índice), así que el texto es determinista.
 *
 * Forma: [[X, Y, "<componente>", Índice, MinutoDeTala], …].
 */
class EXPLORED_API FVegetationClock
{
public:
	/** Añade o sustituye. Devuelve false si el índice es negativo. */
	bool Set(const FIntPoint& Cell, FName Component, int32 Index, int64 FelledAtMinute);
	bool Remove(const FIntPoint& Cell, FName Component, int32 Index);
	/** Minuto de tala guardado, o nullptr si no hay. */
	const int64* Find(const FIntPoint& Cell, FName Component, int32 Index) const;

	int32 Num() const { return Entries.Num(); }
	bool IsEmpty() const { return Entries.Num() == 0; }
	void Reset() { Entries.Reset(); }
	const TArray<FVegetationClockEntry>& GetEntries() const { return Entries; }

	FSaveValue ToValue() const;
	/**
	 * Sustituye el contenido. Una entrada ilegible se descarta sola, sin tumbar
	 * las demás (su árbol se toma como talado ahora: ver ResolveFelledAt).
	 * Devuelve cuántas se descartaron, o -1 si el valor no es una lista.
	 */
	int32 FromValue(const FSaveValue& Value);

	bool operator==(const FVegetationClock& Other) const;

private:
	int32 LowerBound(const FIntPoint& Cell, FName Component, int32 Index) const;

	TArray<FVegetationClockEntry> Entries;
};

struct EXPLORED_API FVegetationStateModel
{
	static constexpr int64 MinutesPerDay = 1440;
	static constexpr int64 MinutesPerQuarterDay = 360;
	static constexpr int32 KeyBytes = 7;
	static constexpr int32 StateBytes = 3;
	static constexpr int32 MaxSpeciesSlots = 16;
	static constexpr int32 MaxNetHits = 15;
	/** RegrowAtQuarterDays reservado para «no rebrota». */
	static constexpr uint16 NoRegrow = 0xFFFF;
	/** Tope del array replicado (biblia 08 §2.3). */
	static constexpr int32 MaxReplicated = 4096;

	// --- Estado ------------------------------------------------------------------

	/** Estado recién talado: brota a los SproutMinutes y es talable a los RegrowMinutes (0 = no rebrota). */
	static FVegetationInstanceState Fell(int64 NowMinute, int64 SproutMinutes, int64 RegrowMinutes);

	/** Etapa a esa hora. Rebrotada del todo cuenta como Intact (el motor la vuelve a mostrar y reinicia el estado). */
	static EVegetationStage StageAt(const FVegetationInstanceState& State, int64 NowMinute);

	/** true si estaba talada y a esa hora ya vuelve a ser talable. */
	static bool IsRegrown(const FVegetationInstanceState& State, int64 NowMinute);

	/**
	 * Hora de tala al cargar: la guardada, o NowMinute si falta, es negativa o
	 * es posterior a NowMinute (reloj manipulado). Así un dato perdido retrasa
	 * el rebrote, nunca lo adelanta.
	 */
	static int64 ResolveFelledAt(const int64* SavedMinute, int64 NowMinute);

	/** Día total de juego (UTimeOfDaySubsystem::GetTotalDays) a minuto entero, por suelo. NaN o negativo → 0. */
	static int64 MinuteFromDays(double TotalDays);

	// --- Red -----------------------------------------------------------------------

	/**
	 * Tabla de huecos de especie de una celda: los nombres de sus HISM,
	 * ordenados y sin repetir, hasta MaxSpeciesSlots. Las dos puntas la
	 * construyen igual desde los mismos datos.
	 */
	static TArray<FName> SpeciesSlots(const TArray<FName>& ComponentNames);
	/** Hueco de un componente en la tabla, o INDEX_NONE. */
	static int32 FindSpeciesSlot(const TArray<FName>& Slots, FName Component);

	/** Clave de red; false si la celda no cabe en int16, el hueco en 0–15 o el índice en uint16. */
	static bool MakeKey(const FIntPoint& Cell, int32 SpeciesSlot, int32 Index, FVegetationNetKey& Out);

	static FVegetationNetState ToNet(const FVegetationInstanceState& State, int64 NowMinute);
	/** Minuto de rebrote que lleva un estado de red; -1 si no rebrota. */
	static int64 RegrowMinuteFromNet(const FVegetationNetState& State);

	/** Añade los bytes al final de Out (little-endian). */
	static void EncodeKey(const FVegetationNetKey& Key, TArray<uint8>& Out);
	static void EncodeState(const FVegetationNetState& State, TArray<uint8>& Out);
	/** Lee a partir de Offset; false (sin tocar Out) si faltan bytes o el valor es imposible. */
	static bool DecodeKey(const TArray<uint8>& In, int32 Offset, FVegetationNetKey& Out);
	static bool DecodeState(const TArray<uint8>& In, int32 Offset, FVegetationNetState& Out);

	/** Orden total de las claves: (Y, X, hueco, índice). */
	static bool KeyLess(const FVegetationNetKey& A, const FVegetationNetKey& B);

	/**
	 * Tope del array replicado: si pasa de Cap, salen los tocones sin rebrote
	 * pendiente (Stump con NoRegrow), en orden de clave, hasta bajar al tope;
	 * pasan al snapshot por celda (OutMoved). Nada más se quita: si aun así
	 * queda por encima, el array se queda como está. Devuelve cuántas salieron.
	 */
	static int32 Compact(TArray<FVegetationNetEntry>& Entries, int32 Cap, TArray<FVegetationNetKey>* OutMoved = nullptr);
};
