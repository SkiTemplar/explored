#pragma once

#include "CoreMinimal.h"

/**
 * Ramas sueltas del suelo (GDD v2 §3.12): se recogen a mano y reaparecen bajo
 * los árboles en pie con el tiempo. Una celda de vegetación lleva su propio
 * estado; la capacidad y el ritmo salen de los ejemplares en pie de esa celda
 * (FFellingProfile::GroundBranchCapacity / GroundBranchPerDayMilli), así que
 * talar el bosque deja de dar ramas y un tocón no cuenta.
 *
 * Todo es entero (minutos de juego y milésimas de rama): avanzar 2 días de
 * golpe o 2880 veces un minuto da exactamente las mismas ramas en las mismas
 * posiciones.
 */

/** Un ejemplar en pie de la celda que puede soltar ramas. */
struct EXPLORED_API FGroundBranchSource
{
	/** Posición del tronco en el plano, en centímetros. */
	FVector2D Position = FVector2D::ZeroVector;
	float CrownRadiusMeters = 1.0f;
	int32 Capacity = 0;
	int32 PerDayMilli = 0;
	/** Objeto que suelta (rama_seca; hoja_palma bajo las palmeras). */
	FName ItemId;
};

struct EXPLORED_API FGroundBranch
{
	/** Número de aparición en la celda: identifica la rama para recogerla y guardarla. */
	uint32 Serial = 0;
	FName ItemId;
	FVector2D Position = FVector2D::ZeroVector;
	/** Índice del ejemplar bajo el que apareció (la rama pertenece a la celda de ese ejemplar aunque caiga fuera). */
	int32 SourceIndex = INDEX_NONE;
};

struct EXPLORED_API FGroundBranchCell
{
	uint32 CellSeed = 0;
	int64 LastUpdateMinute = 0;
	/** Milésimas de rama por minuto acumuladas (Σ PerDayMilli · minutos); se emite una rama cada MilliPerBranch. */
	int64 Accumulator = 0;
	uint32 NextSerial = 0;
	TArray<FGroundBranch> Present;
};

struct EXPLORED_API FGroundBranchModel
{
	static constexpr int64 MinutesPerDay = 1440;
	/** Unidades del acumulador por rama: 1000 milésimas · 1440 minutos. */
	static constexpr int64 MilliPerBranch = 1000 * MinutesPerDay;

	static int32 TotalCapacity(const TArray<FGroundBranchSource>& Sources);

	/** Estado inicial al generar el mundo: la celda empieza llena. */
	static FGroundBranchCell Initialize(uint32 CellSeed, const TArray<FGroundBranchSource>& Sources, int64 NowMinute);

	/**
	 * Avanza hasta NowMinute y devuelve cuántas ramas nuevas han aparecido.
	 * Con la celda llena el acumulador no crece (no hay ráfaga al recoger). Una
	 * hora anterior a LastUpdateMinute no hace nada.
	 */
	static int32 Advance(FGroundBranchCell& Cell, const TArray<FGroundBranchSource>& Sources, int64 NowMinute);

	/** Recoge la rama con ese número. Devuelve false si no está (ya recogida o inexistente). */
	static bool Pick(FGroundBranchCell& Cell, uint32 Serial, FGroundBranch* OutBranch = nullptr);

	/** Dónde aparece la rama número Serial: bajo un ejemplar elegido por capacidad y dentro de su copa. Determinista. */
	static FGroundBranch PlaceBranch(uint32 CellSeed, uint32 Serial, const TArray<FGroundBranchSource>& Sources);
};
