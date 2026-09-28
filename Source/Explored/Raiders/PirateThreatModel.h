#pragma once

#include "CoreMinimal.h"
#include "Save/SaveArchive.h"

/**
 * Amenaza pirata y programador de asaltos [F3] (biblia 05 §2.1-2.3, biblia 08 §5.6). Modelo
 * puro: el contador 0-100, sus reglas de subida y bajada, la categoría de asalto que sale de
 * él y el calendario de asaltos, determinista por semilla.
 *
 * La Amenaza no es la reputación del pueblo (Villages/ReputationModel.h): son dos contadores
 * aparte. Lo único que cruza es la bandera «la aldea más cercana está Hostil», que el llamador
 * pasa en FRaidConditions.
 *
 * Red (biblia 08 §1.3, §5.5-5.6): la Amenaza es del grupo y el programador corre solo en el
 * servidor. Los clientes reciben el aviso (humo, tambor) como evento cosmético multicast y
 * los piratas como fauna terrestre replicada (§2.7); nunca el calendario.
 */

/** Tipos de pirata (biblia 05 §2.1). */
enum class EPirateType : uint8
{
	Raider,       // Saqueador: 40 de vida, machete (Filo 3, daño 9).
	Archer,       // Arquero: 30, arco improvisado (daño 9 a < 15 m).
	Firestarter,  // Incendiario: 35, bolas de brea (alcance 12 m).
	Captain,      // Capitán: 90, machete de aluminio (Filo 5, daño 15) y aturde.
	Count
};

EXPLORED_API const TCHAR* LexToString(EPirateType Type);

/** Números de combate de un tipo de pirata (biblia 05 §2.1). */
struct EXPLORED_API FPirateStats
{
	float Health = 0.0f;
	/** Daño por golpe o por flecha; 0 si el daño es fuego (Incendiario, ver biblia 05 §4.3). */
	float Damage = 0.0f;
	/** Alcance útil del ataque en metros. */
	float RangeM = 0.0f;
	bool bStuns = false;
};

/** Acciones que mueven la Amenaza (biblia 05 §2.3). */
enum class EPirateThreatAction : uint8
{
	LootOrBurnCamp,         // +8: saquear o incendiar un campamento.
	DefeatShipCrew,         // +15: derrotar la tripulación de un barco pirata.
	KillInProvokedRaid,     // +4: matar un pirata en un asalto que provocó el jugador.
	RepelUnprovokedRaid,    // +0: defenderse no sube la Amenaza (ni cuenta como agresión).
	Count
};

EXPLORED_API const TCHAR* LexToString(EPirateThreatAction Action);

/** Categoría de asalto (biblia 05 §2.3). */
enum class ERaidCategory : uint8
{
	None,    // 0-24: solo patrullas visibles que no atacan.
	Low,     // 1: 25-49, cada 6-9 días.
	Medium,  // 2: 50-74, cada 4-6 días.
	High,    // 3: 75-100, cada 2-4 días.
	Count
};

EXPLORED_API const TCHAR* LexToString(ERaidCategory Category);

/** Grupo de un asalto. */
struct EXPLORED_API FRaidParty
{
	int32 Raiders = 0;
	int32 Archers = 0;
	int32 Firestarters = 0;
	int32 Captains = 0;

	int32 Total() const { return Raiders + Archers + Firestarters + Captains; }
	int32 Num(EPirateType Type) const;

	bool operator==(const FRaidParty& Other) const
	{
		return Raiders == Other.Raiders && Archers == Other.Archers && Firestarters == Other.Firestarters &&
			Captains == Other.Captains;
	}
};

/** Lo que el mundo sabe el día en que se evalúa el programador. */
struct EXPLORED_API FRaidConditions
{
	/** Hay al menos una torre de vigía o una bandera construida (base marcada). */
	bool bBaseMarked = false;
	/** Hay al menos un recurso apilado fuera de un contenedor cerrado a menos de 40 m de la base. */
	bool bResourcesExposed = false;
	/** La aldea más cercana a la base está en el tramo Hostil (< 20). */
	bool bNearestVillageHostile = false;
	/** Jugadores conectados (1-4; se recorta). */
	int32 PlayerCount = 1;
};

/** Estado persistente (sección de guardado «raiders»). */
struct EXPLORED_API FPirateThreatState
{
	/** 0-100; empieza en 0. */
	int32 Threat = 0;
	/** Día desde el que se cuentan los 10 días sin agresión para el −5. */
	int32 CalmSinceDay = 0;
	/** Día previsto del próximo asalto; −1 = ninguno programado. */
	int32 NextRaidDay = -1;
	/** Asaltos programados hasta ahora: índice de la tirada en la semilla. */
	int32 RaidSerial = 0;
	/** Último día que ya se evaluó (no se repite un día ni se vuelve atrás); −1 = ninguno. */
	int32 LastEvaluatedDay = -1;

	bool operator==(const FPirateThreatState& Other) const
	{
		return Threat == Other.Threat && CalmSinceDay == Other.CalmSinceDay && NextRaidDay == Other.NextRaidDay &&
			RaidSerial == Other.RaidSerial && LastEvaluatedDay == Other.LastEvaluatedDay;
	}
};

/** Qué pasa en un día. */
enum class ERaidDayEvent : uint8
{
	/** Nada que ver: sin base marcada, Amenaza < 25 o el asalto aún queda lejos. */
	Nothing,
	/** Se acaba de programar un asalto (NextRaidDay). */
	Scheduled,
	/** Víspera: humo negro en el horizonte. */
	Warning,
	/** Toca asalto pero no hay nada a la vista ni la aldea es Hostil: se aplaza un día. */
	Postponed,
	/** Hoy hay asalto (con tambor de madrugada si es de categoría 3). */
	Raid,
};

EXPLORED_API const TCHAR* LexToString(ERaidDayEvent Event);

struct EXPLORED_API FRaidDayResult
{
	ERaidDayEvent Event = ERaidDayEvent::Nothing;
	/** Categoría por Amenaza (sin modificadores). */
	ERaidCategory BaseCategory = ERaidCategory::None;
	/** Categoría del asalto con aldea Hostil y jugadores (solo con Event == Raid). */
	ERaidCategory Category = ERaidCategory::None;
	FRaidParty Party;
	/** Humo negro en el horizonte (víspera). */
	bool bSmokeWarning = false;
	/** Tambor más fuerte la madrugada del asalto (solo categoría 3). */
	bool bDawnDrum = false;
	/** Día del asalto programado tras este día; −1 si no hay. */
	int32 NextRaidDay = -1;
};

struct EXPLORED_API FPirateThreatModel
{
	static constexpr int32 MinThreat = 0;
	static constexpr int32 MaxThreat = 100;
	/** Biblia 05 §2.3: −5 por cada 10 días de juego sin agresión del jugador. */
	static constexpr int32 CalmDays = 10;
	static constexpr int32 CalmDecay = 5;
	/** Biblia 05 §2.3: radio de «recursos visibles» alrededor de la base (m). */
	static constexpr float ExposedResourceRadiusM = 40.0f;
	/** Biblia 08 §5.6: grupo base sobre el que se escala por jugadores. */
	static constexpr int32 CoopBasePartySize = 5;
	static constexpr int32 MaxPlayers = 4;
	/** Biblia 05 §2.2: si el Capitán muere, el grupo se retira en 3 s. */
	static constexpr float CaptainDownRetreatSeconds = 3.0f;
	/** Propuesta: probabilidad del segundo Saqueador en la categoría 2 («a veces»). */
	static constexpr float MediumExtraRaiderChance = 0.5f;

	static FPirateStats Stats(EPirateType Type);
	/** Cambio fijo de cada acción: +8, +15, +4, +0. */
	static int32 ActionDelta(EPirateThreatAction Action);
	/** Las acciones que cuentan como agresión y reinician los 10 días de calma. */
	static bool IsAggression(EPirateThreatAction Action);

	static ERaidCategory CategoryOf(int32 Threat);
	/** Intervalo de días entre asaltos de una categoría: [6, 9], [4, 6], [2, 4]; [0, 0] sin asalto. */
	static void IntervalDays(ERaidCategory Category, int32& OutMin, int32& OutMax);

	/**
	 * Categoría efectiva de un asalto: +1 si la aldea más cercana es Hostil y +1 por cada 2
	 * jugadores por encima de 1 (biblia 08 §5.6: +0/+1/+1 con 2/3/4), con tope 3. Nunca crea
	 * un asalto donde la Amenaza no lo pide: con BaseCategory None devuelve None.
	 */
	static ERaidCategory EffectiveCategory(ERaidCategory Base, bool bNearestVillageHostile, int32 PlayerCount);

	/**
	 * Grupo de un asalto de esa categoría (biblia 05 §2.3), escalado por jugadores con
	 * ×(1 + 0,4·(N−1)) redondeado (5 → 7/9/11). Los que se añaden son Saqueadores, Arqueros e
	 * Incendiarios por turnos; nunca un segundo Capitán. Determinista por Seed y Serial.
	 */
	static FRaidParty MakeParty(ERaidCategory Category, int32 PlayerCount, uint64 Seed, int32 Serial);

	/** Número de piratas escalado: round(Base × (1 + 0,4·(N−1))). */
	static int32 ScaledPartySize(int32 Base, int32 PlayerCount);

	/** Aplica una acción el día Day. Devuelve el cambio real tras el recorte a 0-100. */
	static int32 Apply(FPirateThreatState& State, EPirateThreatAction Action, int32 Day);

	/**
	 * Bajada pasiva: −5 por cada 10 días completos desde CalmSinceDay. Se puede llamar con
	 * saltos de muchos días (dormir, cargar); un día anterior no hace nada.
	 */
	static void AdvanceCalm(FPirateThreatState& State, int32 Day);

	/**
	 * El programador, un paso por día de juego (en el servidor, al empezar el día). Llama
	 * antes a AdvanceCalm. Reglas de biblia 05 §2.3:
	 * - sin base marcada o con Amenaza < 25 no hay asaltos y se borra el programado;
	 * - si no hay asalto programado, se programa con el intervalo de la categoría actual;
	 * - la víspera hay humo; el día del asalto, si no hay recursos a la vista ni aldea Hostil,
	 *   se aplaza al día siguiente sin volver a tirar el intervalo;
	 * - al ocurrir, se programa el siguiente.
	 * Un día igual o anterior al último evaluado devuelve Nothing sin tocar el estado.
	 */
	static FRaidDayResult EvaluateDay(FPirateThreatState& State, int32 Day, const FRaidConditions& Conditions, uint64 Seed);

	// --- Guardado (sección «raiders») ---------------------------------------------------
	static void Save(FSaveArchive& Ar, const FPirateThreatState& State);
	/** Parte del estado por defecto; recorta la Amenaza a 0-100 y descarta un NextRaidDay imposible. */
	static void Load(const FSaveArchive& Ar, FPirateThreatState& OutState);
};
