#pragma once

#include "CoreMinimal.h"

/**
 * Presupuesto de rendimiento de F7 (docs/v3/plan.md): ≥ 60 fps de media y ≥ 45 fps de mínimo a
 * 1080p. Lo aplica el recorrido «-ExploredBench» (UExploredShotSubsystem, Tools/bench.ps1) a cada
 * vista de Map_IslaSal y al conjunto.
 *
 * Modelo puro (solo CoreMinimal.h); lo prueba Tests/BenchBudgetSpec.cpp en el host.
 */
struct EXPLORED_API FBenchBudget
{
	/** Media mínima aceptable (fotogramas / tiempo total). */
	float MinAverageFps = 60.0f;
	/** Ningún fotograma de la ventana de medida puede bajar de aquí. */
	float MinFrameFps = 45.0f;
	/** Una vista con menos fotogramas medidos no da cifra válida (ventana demasiado corta o cuelgue). */
	int32 MinFrames = 30;
};

/** Cifras de una vista del recorrido. */
struct EXPLORED_API FBenchViewStats
{
	FString Name;
	int32 Frames = 0;
	float Seconds = 0.0f;
	/** Fotogramas / segundos (media armónica de los fps, la que da un contador de fps). */
	float AverageFps = 0.0f;
	/** Fotograma más lento de la ventana. */
	float MinimumFps = 0.0f;
	/** Percentil 1 de los fps (el «1 % low»): informativo, el veredicto usa el mínimo. */
	float OnePercentLowFps = 0.0f;
	bool bPassed = false;
};

/** Veredicto de todo el recorrido. */
struct EXPLORED_API FBenchSummary
{
	int32 Views = 0;
	int32 Frames = 0;
	float AverageFps = 0.0f;
	float MinimumFps = 0.0f;
	/** Vistas que no cumplen el presupuesto (o sin fotogramas suficientes). */
	TArray<FString> FailedViews;
	bool bPassed = false;
};

class EXPLORED_API FBenchBudgetModel
{
public:
	/**
	 * Reduce los DeltaTime reales (s) de la ventana de medida de una vista. Se descartan los no
	 * finitos y los ≤ 0 (fotogramas pausados o relojes corruptos), que no son rendimiento.
	 */
	static FBenchViewStats ReduceView(const FString& Name, const TArray<float>& FrameSeconds, const FBenchBudget& Budget = FBenchBudget());

	/** Junta las vistas: media sobre todos los fotogramas, mínimo global y vistas fuera de presupuesto. */
	static FBenchSummary Summarize(const TArray<FBenchViewStats>& Views, const FBenchBudget& Budget = FBenchBudget());

	/**
	 * Desfase de guiñada (grados) del barrido de cámara durante la medida: de -HalfSweep a +HalfSweep
	 * a lo largo de la ventana (Alpha 0..1, se satura fuera). Así la vista mide un abanico de
	 * encuadres y no una sola imagen fija.
	 */
	static float SweepYawOffset(float Alpha, float HalfSweepDegrees);

	/** Línea de log de una vista: «[Bench] fps <vista>: media …, mínimo …, 1 % …, N fotogramas, OK|FALLA». */
	static FString FormatView(const FBenchViewStats& View);

	/** Línea de log del total: «[Bench] Resumen: … OK|FALLA (vistas: …)». */
	static FString FormatSummary(const FBenchSummary& Summary, const FBenchBudget& Budget = FBenchBudget());
};
