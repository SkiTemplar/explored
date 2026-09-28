#pragma once

#include "CoreMinimal.h"

/**
 * Cola de remallado de chunks de edición, pura (docs/tecnico/terreno-editable.md): qué
 * chunks sucios se mandan a una tarea de fondo en cada fotograma.
 *
 * - Un chunk está sucio, en vuelo (su tarea no ha vuelto) o en reposo. Un chunk en vuelo
 *   que se vuelve a ensuciar no lanza otra tarea: espera a que vuelva la primera y se
 *   relanza después, así los resultados nunca llegan desordenados.
 * - Agrupa golpes: un chunk no se relanza hasta MinSecondsBetweenStarts después de su
 *   último arranque (GDD v2 §7.4); mientras tanto el golpe ya se oye y se ve.
 * - Tope de tareas en vuelo y de arranques por fotograma (el arranque copia deltas en el
 *   hilo de juego). Prioridad: menor valor primero (distancia al jugador); empate, (Z, Y, X).
 */
class EXPLORED_API FTerrainRemeshQueueModel
{
public:
	static constexpr double MinSecondsBetweenStarts = 0.15;

	void MarkDirty(const FIntVector& Chunk);
	void MarkDirty(const TArray<FIntVector>& InChunks);

	/**
	 * Elige hasta MaxStarts chunks sucios, fuera de vuelo y con su último arranque hace al
	 * menos MinSecondsBetweenStarts, sin pasar de MaxInFlight en vuelo. Los pasa a en vuelo
	 * y los añade a Out en orden de prioridad. Un Now no finito no arranca nada.
	 */
	void SelectStarts(double Now, int32 MaxInFlight, int32 MaxStarts, TFunctionRef<double(const FIntVector&)> Priority,
		TArray<FIntVector>& Out);
	/** La tarea del chunk ha vuelto (se haya aplicado o no). */
	void MarkFinished(const FIntVector& Chunk);

	bool IsDirty(const FIntVector& Chunk) const { return Dirty.Contains(Chunk); }
	bool IsInFlight(const FIntVector& Chunk) const { return InFlight.Contains(Chunk); }
	int32 NumDirty() const { return Dirty.Num(); }
	int32 NumInFlight() const { return InFlight.Num(); }
	bool IsIdle() const { return Dirty.Num() == 0 && InFlight.Num() == 0; }
	/** Olvida los últimos arranques: lo sucio puede salir ya (vaciado forzoso en carga y tests). */
	void ClearThrottle() { LastStart.Reset(); }
	void Reset();

private:
	TSet<FIntVector> Dirty;
	TSet<FIntVector> InFlight;
	/** Último arranque de cada chunk; se poda lo que ya no frena nada. */
	TMap<FIntVector, double> LastStart;
};
