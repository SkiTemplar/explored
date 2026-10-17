#pragma once

#include "CoreMinimal.h"

#include "Save/SaveValue.h"
#include "WorldGen/FellingModel.h"

class FSaveScatterDeltas;

/**
 * Reloj de la vegetación talada (GDD v2 §3.12, docs/tecnico/tala-integracion.md):
 * la sección de guardado «vegetationClock» con la hora de tala de cada tocón.
 * Los deltas del mundo (FSaveScatterDeltas) solo guardan índices; sin esta
 * sección un tocón no sabría cuándo rebrotar.
 *
 * Reglas:
 * - Una entrada por instancia talada que aún no ha vuelto a ser adulta:
 *   (celda, componente, índice) → FStumpState.
 * - Arrancar con pala saca la entrada: el índice pasa a la capa «uprooted» y
 *   nunca rebrota, así que ya no necesita reloj.
 * - Al llegar a Mature la entrada sale del reloj y el índice de la capa
 *   «felled» (PruneMature): el guardado no crece sin límite.
 * - Ante la duda, más tarde y nunca antes: un talado sin hora (sección perdida o
 *   ilegible) se toma como talado al cargar, y una hora futura (reloj
 *   manipulado) se acota a la hora actual (Reconcile).
 *
 * Forma en la partida: {"version": 1, "stumps": [[X, Y, "<componente>", Índice,
 * FelledAtMinute, UprootWork], ...]}, ordenada por (Y, X), componente e índice,
 * así que el texto no depende del orden de tala.
 */

struct EXPLORED_API FVegetationStumpKey
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	/** Nombre del componente HISM de la celda (el mismo que la capa «felled:<componente>»). */
	FName Component;
	int32 Index = 0;

	bool operator==(const FVegetationStumpKey& Other) const
	{
		return Cell == Other.Cell && Component == Other.Component && Index == Other.Index;
	}
	bool operator!=(const FVegetationStumpKey& Other) const { return !(*this == Other); }
};

struct EXPLORED_API FVegetationStumpEntry
{
	FVegetationStumpKey Key;
	FStumpState Stump;
};

class EXPLORED_API FVegetationClockModel
{
public:
	static constexpr int32 SaveVersion = 1;
	/** Tope de entradas al cargar: muy por encima de lo que se tala en una partida, acota la memoria ante un fichero manipulado. */
	static constexpr int32 MaxEntries = 1 << 20;
	/** Celdas de ±2^20 × 32 m: todo el archipiélago y mucho margen, sin llegar a desbordar int32 al pasar a centímetros. */
	static constexpr int32 MaxAbsCell = 1 << 20;
	/** ±2^40 minutos (unos dos millones de años de juego): acota los minutos sin tocar ningún reloj real. */
	static constexpr int64 MaxAbsMinute = (int64)1 << 40;
	/** Longitud máxima del nombre de un componente. */
	static constexpr int32 MaxComponentLength = 128;

	/**
	 * Registra la tala de esa instancia. Si ya tenía tocón (talar el brote), la hora vuelve a empezar y se pierde el trabajo de pala.
	 * Con MaxEntries entradas no añade ninguna nueva: el reloj nunca escribe un guardado que Load rechazaría.
	 */
	void RecordFelled(const FVegetationStumpKey& Key, int64 NowMinute);

	/** Tocón de esa instancia, o nullptr si no está en el reloj. */
	const FStumpState* Find(const FVegetationStumpKey& Key) const;
	FStumpState* FindMutable(const FVegetationStumpKey& Key);

	/** Saca una entrada (arrancada con pala o instancia que ya no existe). Devuelve false si no estaba. */
	bool Remove(const FVegetationStumpKey& Key);

	/**
	 * Saca las entradas cuya etapa es Mature a esa hora y las devuelve en
	 * OutMatured (en el orden del guardado), para que quien llama quite el índice
	 * de la capa «felled». Un componente sin perfil (ProfileOf devuelve nullptr)
	 * no se toca. Una entrada arrancada no madura nunca.
	 */
	int32 PruneMature(TFunctionRef<const FFellingProfile*(FName Component)> ProfileOf, int64 NowMinute, TArray<FVegetationStumpKey>* OutMatured = nullptr);

	/**
	 * Casa el reloj con la capa «felled:<Component>» al cargar:
	 * - un índice talado sin entrada entra con FelledAtMinute = NowMinute (rebrota más tarde, nunca antes);
	 * - una entrada de ese componente cuyo índice no está talado se descarta (huérfana);
	 * - una hora de tala posterior a NowMinute se acota a NowMinute.
	 * Devuelve cuántas entradas cambió (añadidas + descartadas + acotadas).
	 */
	int32 Reconcile(FName Component, const FSaveScatterDeltas& Felled, int64 NowMinute);

	int32 Num() const { return Entries.Num(); }
	bool IsEmpty() const { return Entries.Num() == 0; }
	void Reset() { Entries.Reset(); }
	/** Entradas en el orden del guardado. */
	const TArray<FVegetationStumpEntry>& GetEntries() const { return Entries; }

	FSaveValue Save() const;
	/**
	 * Sustituye el contenido por el de la partida. Devuelve false y deja el reloj
	 * vacío si la sección no es válida: tipo o versión, filas mal formadas, fuera
	 * de rango o repetidas. Después de un false hay que llamar a Reconcile con
	 * cada capa «felled» para que los tocones vuelvan con la hora de carga.
	 */
	bool Load(const FSaveValue& Value);

	/** Orden del guardado: (Y, X), componente por texto (no por el índice interno de FName) e índice. */
	static bool KeyLess(const FVegetationStumpKey& A, const FVegetationStumpKey& B);

private:
	/** Posición de Key en Entries; si no está, dónde se insertaría. */
	int32 LowerBound(const FVegetationStumpKey& Key) const;

	/** Ordenadas por KeyLess y sin repetir. */
	TArray<FVegetationStumpEntry> Entries;
};
