#pragma once

#include "CoreMinimal.h"

/**
 * Tipos de defecto que detecta el auditor automático de playtest (ver
 * UExploredPlaytestAuditor). GDD-adyacente: no forma parte del contenido del
 * juego, es tooling de verificación.
 */
enum class EPlaytestIssueType : uint8
{
	/** Malla o instancia con material nulo, WorldGridMaterial o el material por defecto del motor. */
	MaterialMissing,
	/** Ítem u objeto con física que se ha movido o caído más de lo esperado en los primeros segundos. */
	PhysicsDrift,
	/** Instancia o actor flotando sobre el terreno, o enterrado bajo él. */
	FloatingOrBuried,
	/** Punto de aparición (PlayerStart) con la cápsula del jugador solapada con colisión. */
	SpawnBlocked,
};

EXPLORED_API const TCHAR* LexToString(EPlaytestIssueType Type);

/**
 * Un defecto ya clasificado, listo para volcar a JSON o al resumen legible.
 * Todas las posiciones en metros (convención del resto de WorldGen).
 */
struct EXPLORED_API FPlaytestIssue
{
	EPlaytestIssueType Type = EPlaytestIssueType::MaterialMissing;
	/** Frase corta en español para el resumen legible. */
	FString Summary;
	/** Nombre de la malla, del actor o del punto de aparición implicado. */
	FString SubjectName;
	/** Detalle adicional (slot de material, actores que bloquean el spawn...). Vacío si no aplica. */
	FString Detail;
	FVector LocationMeters = FVector::ZeroVector;
	/** Solo tiene sentido en MaterialMissing: nº de instancias afectadas en ese componente. */
	int32 InstanceCount = 1;
};

/** Muestra de rendimiento tomada en un punto de captura del set «playtest» (ver ExploredShotSubsystem). */
struct EXPLORED_API FPlaytestFrameSample
{
	FString ShotName;
	float AvgFPS = 0.0f;
	float MinFPS = 0.0f;
	double VRAMUsedMB = 0.0;
};

/**
 * Un paso del bot de juego (ver UExploredPlaytestBot): se mueve a un punto, mira a lo más
 * cercano y, si UInteractionComponent le da foco, interactúa (recolectar, talar...) con las
 * mismas funciones que usaría el input del jugador.
 */
struct EXPLORED_API FPlaytestBotStep
{
	/** Nombre descriptivo del punto de la ruta (p. ej. "Landing_jungla"). */
	FString WaypointName;
	FVector LocationMeters = FVector::ZeroVector;
	/** Vacío si no encontró nada a tiro (ver UInteractionComponent::TraceDistanceCm). */
	FString FocusedActorName;
	bool bInteracted = false;
	/** Diferencia del inventario antes/después, en texto (p. ej. "+2 Platano, +1 FibraPalma"). */
	FString InventoryDelta;
};

struct EXPLORED_API FPlaytestReport
{
	TArray<FPlaytestIssue> Issues;
	TArray<FPlaytestFrameSample> FrameSamples;
	TArray<FPlaytestBotStep> BotSteps;
};

/**
 * Umbrales, clasificación de defectos y formato del informe de playtest.
 * Modelo puro (solo CoreMinimal.h, sin JSON del motor): la recolecta de
 * evidencia (recorrer actores, componentes, lanzar trazas) vive en
 * UExploredPlaytestAuditor; aquí solo se decide qué es un defecto y cómo se
 * escribe. Separación exigida por CLAUDE.md del proyecto para poder probarlo
 * en Tools/HostTests sin el editor.
 */
struct EXPLORED_API FPlaytestReportModel
{
	/** Por encima de esto (cm) una instancia o actor se considera flotando sobre el terreno. */
	static constexpr float FloatingThresholdCm = 30.0f;
	/** Por debajo de esto (cm, con signo: negativo = dentro del terreno) se considera enterrado. */
	static constexpr float BuriedThresholdCm = -15.0f;
	/** Desplazamiento (cm) en los primeros PhysicsSampleSeconds que ya cuenta como física a la deriva. */
	static constexpr float PhysicsDriftThresholdCm = 5.0f;
	/** Segundos que el auditor espera tras el primer fotograma jugable antes de comparar posiciones. */
	static constexpr float PhysicsSampleSeconds = 5.0f;

	/**
	 * Clasifica una distancia vertical con signo a la superficie (cm; positivo = por encima,
	 * negativo = por debajo). Devuelve true si es un defecto y pone a true el motivo en
	 * bOutFloating o bOutBuried (nunca los dos).
	 */
	static bool IsFloatingOrBuried(float VerticalClearanceCm, bool& bOutFloating, bool& bOutBuried);

	/** true si la distancia entre las dos posiciones (cm) supera ThresholdCm. */
	static bool DidPhysicsDrift(const FVector& StartLocationCm, const FVector& LocationAtSampleCm, float ThresholdCm = PhysicsDriftThresholdCm);

	static void AppendMaterialIssue(FPlaytestReport& Report, const FString& MeshName, const FString& SlotName,
		int32 InstanceCount, const FVector& LocationMeters);
	static void AppendPhysicsDriftIssue(FPlaytestReport& Report, const FString& ActorName,
		const FVector& StartLocationMeters, const FVector& LocationAtSampleMeters, bool bEndedBelowTerrain, bool bEndedBelowWater);
	static void AppendFloatingOrBuriedIssue(FPlaytestReport& Report, const FString& SubjectName,
		const FVector& LocationMeters, float ClearanceCm, bool bFloating);
	static void AppendSpawnBlockedIssue(FPlaytestReport& Report, const FString& SpawnName,
		const FVector& LocationMeters, const FString& BlockingActorsCsv);

	/** Añade un paso del bot de juego (recolecta de evidencia: UExploredPlaytestBot). */
	static void AppendBotStep(FPlaytestReport& Report, const FString& WaypointName, const FVector& LocationMeters,
		const FString& FocusedActorName, bool bInteracted, const FString& InventoryDelta);

	/** Serializa el informe a JSON. A mano (sin el módulo Json del motor) para seguir siendo un modelo puro. */
	static FString ToJson(const FPlaytestReport& Report);

	/** Resumen legible en español, para Saved/Shots/playtest_report.txt. */
	static FString ToReadableSummary(const FPlaytestReport& Report);

private:
	static FString EscapeJsonString(const FString& In);
};
