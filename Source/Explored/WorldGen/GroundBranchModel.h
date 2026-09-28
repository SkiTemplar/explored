#pragma once

#include "CoreMinimal.h"

#include "Save/SaveValue.h"

/**
 * Ramas sueltas del suelo (biblia 02 §1.3): bajo cada árbol, talado o intacto,
 * aparecen de 2 a 4 rama_seca por ciclo de 6 horas de juego, con un tope de 6
 * por árbol a la vez. Se recogen a mano y sin herramienta. Una celda de
 * vegetación lleva su propio estado; las fuentes son sus árboles
 * (FFellingModel::MakeBranchSource), en un orden estable (el índice de la
 * instancia), porque cada rama recuerda bajo qué fuente apareció.
 *
 * Los ciclos van alineados al reloj absoluto (minuto 0, 360, 720…) y la tirada
 * de cada ciclo y cada árbol sale de un hash, no de un generador con estado:
 * avanzar 2 días de golpe o 2880 veces un minuto da exactamente las mismas
 * ramas en las mismas posiciones.
 */

/** Un árbol de la celda que puede soltar ramas. */
struct EXPLORED_API FGroundBranchSource
{
	/** Posición del tronco en el plano, en centímetros. */
	FVector2D Position = FVector2D::ZeroVector;
	float CrownRadiusMeters = 1.0f;
	/** Tope de ramas a la vez bajo este árbol. 0 = no suelta ramas (arrancado, arbusto). */
	int32 Capacity = 0;
	/** Ramas nuevas por ciclo: entre CycleMin y CycleMax, ambos incluidos. */
	int32 CycleMin = 0;
	int32 CycleMax = 0;
	/** Objeto que suelta (rama_seca). */
	FName ItemId;
};

struct EXPLORED_API FGroundBranch
{
	/** Número de aparición en la celda: identifica la rama para recogerla y guardarla. */
	uint32 Serial = 0;
	FName ItemId;
	FVector2D Position = FVector2D::ZeroVector;
	/** Índice del árbol bajo el que apareció (la rama pertenece a la celda de ese árbol aunque caiga fuera). */
	int32 SourceIndex = INDEX_NONE;
};

struct EXPLORED_API FGroundBranchCell
{
	uint32 CellSeed = 0;
	int64 LastUpdateMinute = 0;
	uint32 NextSerial = 0;
	TArray<FGroundBranch> Present;
};

struct EXPLORED_API FGroundBranchModel
{
	static constexpr int64 MinutesPerDay = 1440;
	/** Un ciclo de ramas: 6 horas de juego (biblia 02 §1.3). */
	static constexpr int64 CycleMinutes = 360;
	/**
	 * Ciclos que se procesan como mucho en un Advance (≈ 2 años de juego). Con
	 * CycleMin ≥ 1 cualquier árbol se llena mucho antes, así que el recorte no
	 * cambia el resultado; solo acota el coste ante un reloj manipulado.
	 */
	static constexpr int64 MaxCyclesPerAdvance = 4096;

	static int32 TotalCapacity(const TArray<FGroundBranchSource>& Sources);

	/** Ramas presentes bajo ese árbol. */
	static int32 CountUnder(const FGroundBranchCell& Cell, int32 SourceIndex);

	/** Ciclo al que pertenece un minuto (división por suelo: bien con minutos negativos). */
	static int64 CycleOf(int64 Minute);

	/** Ramas que tira un árbol en un ciclo, en [CycleMin, CycleMax]. Determinista por (semilla, ciclo, árbol). */
	static int32 RollCycle(uint32 CellSeed, int64 Cycle, int32 SourceIndex, const FGroundBranchSource& Source);

	/** Estado inicial al generar el mundo: cada árbol empieza con la tirada de un ciclo (sin pasar del tope). */
	static FGroundBranchCell Initialize(uint32 CellSeed, const TArray<FGroundBranchSource>& Sources, int64 NowMinute);

	/**
	 * Avanza hasta NowMinute y devuelve cuántas ramas nuevas han aparecido: una
	 * tirada por cada comienzo de ciclo en (LastUpdateMinute, NowMinute] y por
	 * árbol, sin pasar de su tope. Un árbol lleno no acumula nada (no hay ráfaga
	 * al recoger). Una hora anterior a LastUpdateMinute no hace nada.
	 */
	static int32 Advance(FGroundBranchCell& Cell, const TArray<FGroundBranchSource>& Sources, int64 NowMinute);

	/** Recoge la rama con ese número. Devuelve false si no está (ya recogida o inexistente). */
	static bool Pick(FGroundBranchCell& Cell, uint32 Serial, FGroundBranch* OutBranch = nullptr);

	/** Dónde aparece la rama número Serial bajo el árbol SourceIndex: un punto de su copa. Determinista. */
	static FGroundBranch PlaceBranch(uint32 CellSeed, uint32 Serial, int32 SourceIndex, const TArray<FGroundBranchSource>& Sources);

	/**
	 * Guardado: {"last", "next", "present": [[serie, árbol], …]}. Las posiciones
	 * no se guardan: se recalculan con PlaceBranch desde las fuentes.
	 */
	static FSaveValue ToValue(const FGroundBranchCell& Cell);
	/**
	 * Carga sobre una celda con su semilla y sus fuentes. Devuelve false (y deja
	 * Out sin tocar) si el valor está roto: tipos que no encajan, series
	 * repetidas o que no son menores que «next», o un árbol que no existe.
	 */
	static bool FromValue(const FSaveValue& Value, uint32 CellSeed, const TArray<FGroundBranchSource>& Sources, FGroundBranchCell& Out);
};
