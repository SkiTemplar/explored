#pragma once

#include "CoreMinimal.h"

/**
 * Partida completa del bot de Baliza Siete (UExploredPlaytestBot con «-BalizaRun»): reloj de la
 * prueba, conversión a minutos de juego a 1×, etapas alcanzadas, desmayos, errores de log,
 * guardar/cargar por etapa y resultado frente al criterio de salida de F3 (docs/v3/plan.md).
 *
 * Conversión a 1×: el bot acelera el reloj (×Scale) solo mientras espera (al reloj o el tiempo de
 * un trayecto a pie que sustituye por un teletransporte) y lo deja a ×1 en las acciones (recoger,
 * craftear, pescar, remar). Así un segundo real a ×Scale equivale a Scale segundos de partida a
 * 1×, y la estimación es la suma de dt × Scale. Los saltos del reloj (dormir, desmayo) no suman.
 *
 * Modelo puro (solo CoreMinimal.h); lo prueba Tests/PlaytestRunSpec.cpp en el host.
 */

/** Umbrales del criterio de salida y velocidades del jugador de referencia. */
struct EXPLORED_API FPlaytestRunTuning
{
	/** Criterio de F3/F6: partida completa entre 47 y 65 min de juego a 1× (créditos incluidos, GDD §3). */
	double MinMinutes = 47.0;
	double MaxMinutes = 65.0;
	/** Créditos del GDD §9.2 (60 s); el bot se detiene en el epílogo y los suma aparte. */
	double CreditsSeconds = 60.0;
	/** Paso de Iria andando (CharacterMovement 450 cm/s) y rodeo medio de un camino real. */
	double WalkSpeedMps = 4.5;
	double PathFactor = 1.25;
};

/** Reloj de la prueba: segundos reales y su equivalente a 1×. */
struct EXPLORED_API FPlaytestRunClock
{
	double RealSeconds = 0.0;
	double Est1xSeconds = 0.0;
};

struct EXPLORED_API FPlaytestRunStep
{
	FString Name;
	bool bOk = true;
	/** Reloj a 1× al terminar el paso (s) y hora de juego («D2 15:40»). */
	double Est1xSeconds = 0.0;
	FString GameClock;
	FString Stage;
};

/** Un escenario del auditor de softlocks, ya decidido. */
struct EXPLORED_API FPlaytestSoftlockCheck
{
	FString Name;
	bool bCompletable = false;
	/** Qué falta (si no se puede terminar) o qué ha entregado la caja. */
	FString Detail;
};

struct EXPLORED_API FPlaytestRunSummary
{
	FString Route;
	bool bRan = false;
	/** El guion llegó al final (pantalla de epílogo). */
	bool bReachedEpilogue = false;
	/** Paso que falló (vacío si ninguno). */
	FString FailedStep;
	FPlaytestRunClock Clock;
	double CreditsSeconds = 0.0;
	int32 Faints = 0;
	int32 Sleeps = 0;
	int32 PagesRead = 0;
	/** Bit i = etapa i (EBeaconStage) alcanzada; los nombres, en orden de llegada. */
	uint32 StagesMask = 0;
	TArray<FString> StagesReached;
	int32 LogErrors = 0;
	int32 LogWarnings = 0;
	TArray<FString> ErrorSamples;
	int32 SaveLoadChecks = 0;
	int32 SaveLoadFailures = 0;
	TArray<FString> SaveLoadNotes;
	TArray<FPlaytestRunStep> Steps;
};

struct EXPLORED_API FPlaytestRunModel
{
	static constexpr int32 StageCount = 10;
	static constexpr int32 MaxErrorSamples = 12;

	/** Suma un fotograma: dt reales a la escala de reloj dada. dt o escala no finitos o negativos no suman. */
	static FPlaytestRunClock Accumulate(const FPlaytestRunClock& Clock, double RealDeltaSeconds, double ClockScale);

	/** Segundos a 1× de un trayecto a pie (distancia en línea recta, en metros). */
	static double TravelSeconds1x(double DistanceMeters, const FPlaytestRunTuning& Tuning = FPlaytestRunTuning());

	/** Segundos reales que dura esperar Seconds1x con el reloj a ×ClockScale (≥ 1). */
	static double RealWaitSeconds(double Seconds1x, double ClockScale);

	/** Marca una etapa alcanzada (0-9); la primera vez añade su nombre. Fuera de rango no cambia nada. */
	static FPlaytestRunSummary MarkStage(const FPlaytestRunSummary& Summary, int32 StageIndex, const FString& StageName);

	/** Añade un error de log (y una muestra mientras haya sitio). */
	static FPlaytestRunSummary AddLogError(const FPlaytestRunSummary& Summary, const FString& Line);

	/** Minutos de partida a 1×, créditos incluidos. */
	static double EstimatedMinutes(const FPlaytestRunSummary& Summary);

	static bool IsDurationInTarget(const FPlaytestRunSummary& Summary, const FPlaytestRunTuning& Tuning = FPlaytestRunTuning());

	static bool AllStagesReached(const FPlaytestRunSummary& Summary);

	/** Criterio del bot: epílogo, las 10 etapas, duración en rango, guardar/cargar sin fallos y 0 errores de log. */
	static bool Passed(const FPlaytestRunSummary& Summary, const FPlaytestRunTuning& Tuning = FPlaytestRunTuning());

	static bool AllCompletable(const TArray<FPlaytestSoftlockCheck>& Checks);

	/** Hora de juego legible a partir de horas absolutas («D2 15:40»). */
	static FString FormatGameClock(double AbsHours);

	/** Objeto JSON «run» (sin llave de cierre final de línea) para playtest_report.json. */
	static FString ToJson(const FPlaytestRunSummary& Summary, const TArray<FPlaytestSoftlockCheck>& Softlocks,
		const FPlaytestRunTuning& Tuning = FPlaytestRunTuning());

	/** Resumen legible en español para playtest_report.txt. */
	static FString ToReadable(const FPlaytestRunSummary& Summary, const TArray<FPlaytestSoftlockCheck>& Softlocks,
		const FPlaytestRunTuning& Tuning = FPlaytestRunTuning());
};
