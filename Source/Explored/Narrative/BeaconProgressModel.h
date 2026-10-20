#pragma once

#include "CoreMinimal.h"

#include "Narrative/NotebookModel.h"

/**
 * Progreso de la historia de Baliza Siete (GDD v3 §5.9): máquina de etapas e
 * hitos guardados. Modelo puro: ExploredVoyageSubsystem lo conecta con el
 * mundo, ExploredHUD pinta el objetivo y SaveSystemStates guarda el registro.
 *
 * Reglas de las etapas:
 * - Solo se avanza hacia delante; se pueden saltar etapas sin requisito para
 *   que ningún orden de tareas bloquee la partida (PlaytestAuditor, plan F3).
 * - Tres etapas tienen requisito: LenteEnMano (lente recogida), LinternaMontada
 *   (3 ranuras llenas) y Final (baliza encendida).
 * - Los hitos que definen una etapa la alcanzan solos: recoger la lente lleva a
 *   LenteEnMano, llenar la tercera ranura a LinternaMontada y encender a Final.
 *
 * Todas las funciones son puras: devuelven un estado nuevo y no tocan el original.
 */

/** Etapas de la historia, en orden (GDD §5.9). Los nombres siguen el GDD. */
enum class EBeaconStage : uint8
{
	Prologo,
	Orilla,
	Faro,
	Balsa,
	Carguero,
	LenteEnMano,
	Ciclon,
	Preparacion,
	LinternaMontada,
	Final,
	Count
};

/** Ranuras de la linterna del faro (GDD §5.6, receta 9). */
enum class ELanternSlot : uint8
{
	Lens,  // lente de Fresnel en su aro
	Oil,   // los dos tarros de aceite vertidos
	Wick,  // mecha apretada
	Count
};

/** Hitos que se registran una vez (el tarro de aceite cuenta hasta 2). */
enum class EBeaconMilestone : uint8
{
	FirstFireLit,     // objetivo «Haz fuego» del día 1
	FirstFishCooked,  // objetivo «Pesca y cocina un pez»
	RaftBuilt,        // balsa atada en Playa Norte
	LensCollected,
	OilJar,           // un tarro más (tope: 2)
	WickMade,
	Count
};

/** Variante del epílogo según las páginas leídas (GDD §2). */
enum class EBeaconEpilogue : uint8
{
	Complete,  // 8 de 8: Iria entrega el cuaderno y firma el informe
	Partial,   // menos de 8
};

struct EXPLORED_API FBeaconProgressState
{
	EBeaconStage Stage = EBeaconStage::Prologo;
	bool bFirstFireLit = false;
	bool bFirstFishCooked = false;
	bool bRaftBuilt = false;
	bool bLensCollected = false;
	bool bWickMade = false;
	bool bBeaconLit = false;
	/** Tarros de aceite hechos para la linterna (0-2); un tercer tarro no cuenta aquí. */
	int32 OilJars = 0;
	/** Ranuras llenas: bit i = ELanternSlot i. */
	uint8 LanternSlots = 0;

	bool operator==(const FBeaconProgressState& Other) const;
};

/** Registro plano para el guardado: solo enteros pequeños, sin punteros ni cadenas. */
struct EXPLORED_API FBeaconProgressRecord
{
	uint8 Stage = 0;
	/** Hitos booleanos: fuego, pez, balsa, lente, mecha y baliza encendida (bits 0-5, por ese orden). */
	uint8 Flags = 0;
	uint8 OilJars = 0;
	uint8 LanternSlots = 0;
};

/** Resultado de una operación: bOk falso deja State igual que la entrada. */
struct EXPLORED_API FBeaconProgressResult
{
	bool bOk = false;
	FBeaconProgressState State;
};

/** Lo que el objetivo necesita saber del resto del juego. */
struct EXPLORED_API FBeaconObjectiveContext
{
	/** Página 1 del cuaderno leída (FNotebookModel). */
	bool bPageOneRead = false;
	/** Iria está dentro de un ShelterVolume (refugio techado o base del faro). */
	bool bSheltered = false;
};

struct EXPLORED_API FBeaconProgressModel
{
	static constexpr int32 RequiredOilJars = 2;
	static constexpr uint8 AllSlotsMask = 0x7;

	// --- Etapas ----------------------------------------------------------------

	/** Hacia delante, dentro de rango y con el requisito de la etapa destino cumplido. */
	static bool CanAdvance(const FBeaconProgressState& State, EBeaconStage Target);
	static FBeaconProgressResult Advance(const FBeaconProgressState& State, EBeaconStage Target);

	/** Requisito para estar en una etapa (LenteEnMano, LinternaMontada y Final). */
	static bool MeetsStageRequirement(const FBeaconProgressState& State, EBeaconStage Stage);

	// --- Hitos -----------------------------------------------------------------

	/** Registra un hito. Falla si ya estaba (o si ya hay 2 tarros). */
	static FBeaconProgressResult RecordMilestone(const FBeaconProgressState& State, EBeaconMilestone Milestone);

	static bool IsSlotFilled(const FBeaconProgressState& State, ELanternSlot Slot);

	/**
	 * Llena una ranura de la linterna. Falla si ya está llena o si falta lo que
	 * va en ella: la lente recogida, 2 tarros o la mecha.
	 */
	static FBeaconProgressResult FillSlot(const FBeaconProgressState& State, ELanternSlot Slot);

	static bool IsLanternMounted(const FBeaconProgressState& State);

	/** Enciende la baliza: exige la linterna montada y que no esté ya encendida. */
	static FBeaconProgressResult Light(const FBeaconProgressState& State);

	// --- Objetivo y epílogo ------------------------------------------------------

	/** Id del objetivo actual en objectives.json (NAME_None en Final). */
	static FName CurrentObjective(const FBeaconProgressState& State, const FBeaconObjectiveContext& Context);

	static EBeaconEpilogue EpilogueFor(const FNotebookState& Notebook);

	/** Id de texto de la línea variable del epílogo; la común es CommonEpilogueTextId. */
	static FName EpilogueTextId(EBeaconEpilogue Epilogue);
	static FName CommonEpilogueTextId();

	// --- Guardado --------------------------------------------------------------

	static FBeaconProgressRecord ToRecord(const FBeaconProgressState& State);

	/**
	 * Reconstruye el estado. Rechaza (bOk falso) un registro fuera de rango o
	 * incoherente: etapa desconocida, bits sobrantes, más de 2 tarros, ranura
	 * llena sin su objeto, baliza encendida sin linterna o etapa sin su requisito.
	 */
	static FBeaconProgressResult FromRecord(const FBeaconProgressRecord& Record);
};
