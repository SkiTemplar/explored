#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Debug/BenchBudgetModel.h"

#include "ExploredShotSubsystem.generated.h"

class ACameraActor;

/** Un punto de vista para la verificación visual. */
struct FExploredShot
{
	FString Name;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float Hours = 12.0f;
	/** Día de juego de la vista (0 = el actual); el clima del reloj de tormenta depende de él. */
	int32 Day = 0;
	/** Incluye la interfaz (HUD y Slate) en la captura. */
	bool bShowUI = false;
	/** Acción de juego antes de la vista (conjunto «beacon»): «interact», «lantern» (linterna montada), «light» (encender) o «wait». */
	FName Action;
	/** Punto del mundo (cm) al que mira el personaje en «interact». */
	FVector Target = FVector::ZeroVector;
};

/**
 * Herramienta de verificación visual (no se incluye en Shipping). Con
 * «-ExploredShots=<conjunto>» en la línea de comandos del juego espera a que
 * terminen los shaders, recorre una lista de cámaras y horas, guarda una
 * captura de cada una en Saved/Shots/ y cierra el juego.
 *
 * Conjuntos: «islands» (una vista por isla a media tarde), «day» (ciclo del
 * día en la isla de inicio), «spawn» (vista del jugador al aparecer), «playtest»
 * (recorrido a pie por isla y punto de interés, en orilla/vegetación/cima/cueva,
 * a varias horas; activa también a UExploredPlaytestAuditor y a UExploredPlaytestBot,
 * ver Tools/playtest.ps1) y «all».
 *
 * Con «-ExploredBench» en vez de (o junto a) «-ExploredShots» recorre Map_IslaSal (conjunto
 * «bench»): Cala Varada desde el mar, el Palmar a la altura de los ojos, la base del faro, la
 * galería del faro mirando a toda la isla y el ciclón (D2 20:30) en la orilla de la Cala. En cada
 * vista, tras el mismo asentamiento de streaming que usan las capturas, mide BenchMeasureSeconds
 * con un barrido lento de la cámara y escribe media, mínimo y 1 % de fps frente al presupuesto de
 * F7 (FBenchBudgetModel: ≥ 60 de media y ≥ 45 de mínimo), vuelca `stat unit`/`stat gpu` (vía
 * los contadores internos de RenderCore, en ms), `r.Nanite.ShowStats` y `memreport -full` a
 * Saved/Logs, y al final una línea «[Bench] Resumen» con el veredicto (Tools/bench.ps1).
 */
UCLASS()
class EXPLORED_API UExploredShotSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	/** Pone al personaje mirando a Target (cm) para que la interacción lo enfoque. */
	void PlacePawnFacing(const FVector& Target);
	/** Interactúa con lo que enfoque el personaje y lo deja en el log. */
	void InteractWithFocus(const FString& ShotName);
	void ShowFirstPerson(const FExploredShot& Shot);
	/** El final del juego pausa el mundo; las capturas tienen que seguir. */
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

private:
	void BuildShotList(const FString& Set);
	void BeginShot(int32 Index);
	/** Conjunto «f5»: cada pantalla y elemento de la interfaz a 1080p (ExploredShotSubsystemUi.cpp). */
	void BuildUiShots();
	/** Prepara una vista «ui_*»; devuelve false si la vista no es de interfaz. */
	bool BeginUiShot(const FExploredShot& Shot);
	/** Escribe la muestra de rendimiento de la posición actual (stat unit/gpu, Nanite, memreport). */
	void LogBenchSample(int32 Index);
	/** Media y mínimo de fps de la vista actual en el log (decisión de Lumen y presupuesto de F7). */
	void LogShotFrameRate(const FString& Name) const;
	/** Activa o quita el sombreado por bandas (M_PP_Toon) de los PostProcessVolume del mapa. */
	void SetToonBands(bool bEnabled) const;

	TArray<FExploredShot> Shots;
	int32 Current = INDEX_NONE;
	float Timer = 0.0f;
	bool bActive = false;
	bool bRequested = false;
	FString OutputDir;

	/** «-ExploredBench»: vuelca cifras de rendimiento a Saved/Logs en vez de (o además de) capturas. */
	bool bBenchMode = false;
	/** Los «stat» del modo bench ya están encendidos (son interruptores: no repetir). */
	bool bBenchStatsEnabled = false;
	/** La vista actual está en su ventana de medida (después del asentamiento). */
	bool bBenchMeasuring = false;
	float BenchMeasureTimer = 0.0f;
	/** Rotación de la vista antes del barrido de medida (se restaura para la captura). */
	FRotator BenchBaseRotation = FRotator::ZeroRotator;
	/** DeltaTime real (FApp) de cada fotograma de la ventana de medida. */
	TArray<float> BenchFrameSeconds;
	/** Cifras de las vistas ya medidas, para el resumen final. */
	TArray<FBenchViewStats> BenchViews;

	/** Avanza la ventana de medida del bench; devuelve true cuando la vista ya está medida. */
	bool TickBenchMeasure();
	/** Escribe la línea «[Bench] Resumen» con el veredicto de todo el recorrido. */
	void LogBenchSummary() const;

	/**
	 * Fotogramas por segundo (1/DeltaTime) acumulados mientras se espera el asentamiento de la
	 * vista actual; al pedir la captura se reduce a media/mínimo y, si hay un
	 * UExploredPlaytestAuditor en el mundo (conjunto «playtest»), se le pasa junto con la VRAM.
	 */
	TArray<float> CurrentShotFrameRates;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
};
