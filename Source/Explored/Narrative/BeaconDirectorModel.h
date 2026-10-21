#pragma once

#include "CoreMinimal.h"

#include "Narrative/BeaconProgressModel.h"
#include "Narrative/NotebookModel.h"

/**
 * Director de la historia de Baliza Siete (GDD v3 §2, §3 y §5.9): decide, a partir de lo que
 * pasa en el mundo, cuándo avanza la etapa, qué pensamiento de Iria toca, cuándo se autoguarda
 * y cuándo llega el Mistral. Modelo puro sobre FBeaconProgressModel y FNotebookModel;
 * UBeaconSubsystem le pasa los hechos del mundo cada medio segundo y aplica el resultado.
 *
 * El tiempo va en horas absolutas desde el día 1 a las 00:00 (día 2 a las 19:00 = 43), como
 * FStormClockModel. Todas las funciones devuelven estados nuevos y no tocan la entrada.
 */

/** Pensamientos de Iria (GDD §2.2): 12 disparadores, cada uno se muestra una sola vez. */
enum class EIriaThought : uint8
{
	FirstMove,      // primer movimiento
	SeeLighthouse,  // ver el faro
	FirstFire,      // primera hoguera
	EnterHouse,     // entrar en la casa del farero
	FirstNight,     // primera noche, al dormir
	BoardRaft,      // subir a la balsa
	SeeFreighter,   // ver el carguero
	TakeLens,       // coger la lente
	BarometerLow,   // barómetro bajo 1000
	CycloneEye,     // ojo del ciclón
	MountLantern,   // montar la linterna
	LightBeacon,    // encender
	Count
};

/** Umbrales y horas de la historia (valores del GDD; el subsistema puede sustituirlos). */
struct EXPLORED_API FBeaconDirectorTuning
{
	/** Fin del prólogo aunque no se haya bebido un coco (D1 08:30). */
	double PrologueEndHour = 8.5;
	/** El día 2 empieza la etapa de la balsa (si ya se leyó la página 1). */
	double RaftDayHour = 24.0 + 6.0;
	/** Inicio del ciclón (D2 19:00) y fin (D3 02:00). */
	double CycloneStartHour = 24.0 + 19.0;
	double CycloneEndHour = 48.0 + 2.0;
	/** Autoguardados: amanecer del día 2 y del día 3, y día 3 a las 20:00 (reintento del clímax). */
	TArray<double> AutosaveHours = { 24.0 + 6.0, 48.0 + 6.0, 48.0 + 20.0 };
	/** Noche para el Mistral: desde las 20:00 hasta las 06:00; de día llega a las 21:00. */
	double NightStartHour = 20.0;
	double NightEndHour = 6.0;
	double MistralDayArrivalHour = 21.0;

	/** Metros recorridos para el primer pensamiento. */
	float FirstMoveMeters = 1.5f;
	/** Distancia a la torre a la que se ve el faro. */
	float SeeLighthouseMeters = 110.0f;
	/** Distancia a la torre o a la casa que cuenta como «subir al faro». */
	float ReachLighthouseMeters = 25.0f;
	/** Distancia al centro de la casa que cuenta como entrar. */
	float EnterHouseMeters = 4.5f;
	/** Distancia al casco a la que se ve el carguero y a la que se está en el islote. */
	float SeeFreighterMeters = 130.0f;
	float OnIsletMeters = 45.0f;
	float BarometerLowHpa = 1000.0f;
};

/** Lo que el subsistema mide en el mundo en cada muestra. Distancia < 0 = desconocida. */
struct EXPLORED_API FBeaconFacts
{
	double AbsHours = 0.0;
	/** Metros recorridos desde el inicio de la partida (o desde la carga). */
	float MovedMeters = 0.0f;
	float MetersToTower = -1.0f;
	float MetersToHouse = -1.0f;
	float MetersToFreighter = -1.0f;
	/** Lectura del barómetro (0 = sin barómetro). */
	float BarometerHpa = 0.0f;
	bool bCycloneEye = false;
	/** Hay un coco vacío en la mochila (se ha bebido un coco). */
	bool bDrankCoconut = false;
	/** Alguna hoguera está encendida. */
	bool bFireLit = false;
	bool bHasCookedFish = false;
	int32 OilJarsInPack = 0;
	bool bHasWick = false;
	bool bHasLens = false;
};

/** Estado de la partida que guarda el director (secciones «progress» y «pages»). */
struct EXPLORED_API FBeaconRunState
{
	FBeaconProgressState Progress;
	FNotebookState Notebook;
	/** Pensamientos ya mostrados: bit i = EIriaThought i. */
	uint16 ThoughtsShown = 0;
	/** Última hora vista (para detectar el paso por una hora de autoguardado); < 0 = ninguna. */
	double LastAbsHours = -1.0;
	/** Hora a la que se encendió la baliza; < 0 = apagada. */
	double LitAtHours = -1.0;
	bool bMistralArrived = false;

	bool operator==(const FBeaconRunState& Other) const;
};

struct EXPLORED_API FBeaconStepResult
{
	FBeaconRunState State;
	/** Pensamientos nuevos, en orden de aparición. */
	TArray<EIriaThought> Thoughts;
	bool bStageChanged = false;
	/** Se ha cruzado una hora de autoguardado. */
	bool bAutosave = false;
	/** Toca la llegada del Mistral (una sola vez). */
	bool bMistralDue = false;
	/** La operación pedida se ha aplicado (ReadPage, FillSlot, Light). */
	bool bOk = false;
};

struct EXPLORED_API FBeaconDirectorModel
{
	static constexpr uint16 AllThoughtsMask = (1u << static_cast<uint32>(EIriaThought::Count)) - 1u;

	/** Aplica una muestra del mundo: hitos, etapas, pensamientos, autoguardado y Mistral. */
	static FBeaconStepResult Step(const FBeaconRunState& State, const FBeaconFacts& Facts,
		const FBeaconDirectorTuning& Tuning = FBeaconDirectorTuning());

	/** Pensamiento con disparador externo (dormir, subir a la balsa). Vacío si ya se mostró. */
	static FBeaconStepResult TriggerThought(const FBeaconRunState& State, EIriaThought Thought);

	/** Lee una página: la marca y, si es la 1, lleva al menos a la etapa Faro. bOk falso si no es 1-8. */
	static FBeaconStepResult ReadPage(const FBeaconRunState& State, int32 Page);

	/** Llena una ranura de la linterna; al llenar la tercera, pensamiento «montar la linterna». */
	static FBeaconStepResult FillSlot(const FBeaconRunState& State, ELanternSlot Slot);

	/** Enciende la baliza a la hora dada; exige la linterna montada. */
	static FBeaconStepResult Light(const FBeaconRunState& State, double AbsHours);

	static bool IsNight(double AbsHours, const FBeaconDirectorTuning& Tuning = FBeaconDirectorTuning());

	/** Hora absoluta a la que aparece el Mistral si la baliza se enciende a LitAtHours. */
	static double MistralArrivalHours(double LitAtHours, const FBeaconDirectorTuning& Tuning = FBeaconDirectorTuning());

	/** Hay una hora de autoguardado en (FromHours, ToHours]. */
	static bool CrossesAutosave(double FromHours, double ToHours, const FBeaconDirectorTuning& Tuning = FBeaconDirectorTuning());

	static bool WasThoughtShown(const FBeaconRunState& State, EIriaThought Thought);

	/** Id de texto del pensamiento en thoughts.json («pensamiento_faro»); NAME_None si no es válido. */
	static FName ThoughtTextId(EIriaThought Thought);

	/**
	 * Estado coherente al empezar una etapa (comando BalizaStage y pruebas de guardado):
	 * Faro con la hoguera y el pez hechos, Balsa con la página 1 leída, Carguero con la balsa,
	 * LenteEnMano, Ciclon y Preparacion con la lente, LinternaMontada con las 3 ranuras llenas y
	 * Final con la baliza encendida.
	 */
	static FBeaconRunState StateForStage(EBeaconStage Stage);

	/** Etapa por nombre del GDD («Faro», sin distinguir mayúsculas) o por número 0-9. */
	static bool ParseStage(const FString& Text, EBeaconStage& OutStage);
	static const TCHAR* StageName(EBeaconStage Stage);
};
