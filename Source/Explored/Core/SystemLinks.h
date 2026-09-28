#pragma once

#include "CoreMinimal.h"

#include "Boats/BoatTypes.h"
#include "Building/BuildingTypes.h"
#include "Events/WorldEventsModel.h"
#include "Fauna/FaunaTypes.h"
#include "Fauna/MarineCreatureBrain.h"
#include "Ruins/RuinsModel.h"
#include "Survival/BodyModel.h"
#include "Survival/SurvivalModel.h"
#include "Weather/WeatherModel.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/PointsOfInterest.h"

/**
 * Pegamento entre sistemas (paquete P-WIRE): las reglas que deciden cómo se
 * afectan unos sistemas a otros, en funciones puras para poder probarlas en el
 * host (Explored.Links). La capa de Unreal (UExploredWiringSubsystem, el modo de
 * juego y los componentes) solo reúne los datos del mundo y aplica el resultado.
 */
namespace ExploredLinks
{
	// --- Supervivencia ------------------------------------------------------------------

	/**
	 * Calor combinado de varios fuegos (cada uno 0–1 en el punto del jugador):
	 * 1 − Π(1 − calor). Dos hogueras calientan más que una, sin pasar nunca de 1.
	 */
	EXPLORED_API float CombineFireHeat(const TArray<float>& Heats);

	/** Lo que otros sistemas aportan al entorno del cuerpo (FSurvivalInputs). */
	struct EXPLORED_API FSurvivalLinkInputs
	{
		/** Calor de los fuegos cercanos ya combinado (CombineFireHeat). */
		float FireHeat = 0.0f;
		/** Bajo un techo construido (FBuildingModel::IsSheltered) o junto a un fuego a cubierto. */
		bool bBuildingShelter = false;
		/** Peso llevado / capacidad cómoda (UCarryComponent). */
		float CarriedWeightRatio = 0.0f;
		/** Tocando la flauta (FFluteModel::IsPerforming). */
		bool bPlayingMusic = false;
		/** Sombrero puesto (reservado: aún no hay objeto de sombrero). */
		bool bHasHat = false;
	};

	/** Mezcla los enlaces con el entorno que ya midió el cuerpo (clima, sol, agua, techo). */
	EXPLORED_API void ApplySurvivalLinks(FSurvivalInputs& InOut, const FSurvivalLinkInputs& Links);

	// --- Muerte y reaparición (GDD §7, §8.6, §11) -------------------------------------

	enum class ERespawnDecision : uint8
	{
		/** En la fogata o punto de reaparición encendido más cercano. */
		AtRespawnPoint,
		/** Sin fuegos encendidos: en el inicio de la partida (PlayerStart). */
		AtStart,
		/** Náufrago: sin reaparición. */
		GameOver,
	};

	EXPLORED_API ERespawnDecision DecideRespawn(const FSurvivalModeSettings& Mode, bool bHasRespawnPoint);

	/** Índice del punto más cercano o INDEX_NONE si no hay ninguno. */
	EXPLORED_API int32 NearestPoint(const TArray<FVector>& Points, const FVector& From);

	/**
	 * Cuerpo al reaparecer: vivo pero tocado (mitad de salud, sin heridas ni
	 * estados agudos), con hambre, sed y ánimo bajos. Conserva lo lento (escorbuto,
	 * nutrientes, sol acumulado) para que morir no sea una cura.
	 */
	EXPLORED_API FSurvivalState MakeRespawnState(const FSurvivalState& Dead);

	// --- Peligro para la música (FMusicDirectorModel vía SetDanger) ---------------------

	/** Hipotermia, golpe de calor, salud baja o sangrado (0–1). */
	EXPLORED_API float BodyDanger01(const FSurvivalState& State);

	/** Temporal a la intemperie: la galerna inquieta, el ciclón es peligro (0–1). */
	EXPLORED_API float StormDanger01(EWeatherState Weather, bool bSheltered);

	/** Amenaza de una criatura según su especie, su estado y la distancia al jugador (0–1). */
	EXPLORED_API float PredatorThreat01(EFaunaSpecies Species, EMarineState State, float DistanceCm);

	// --- Fauna → cuerpo ----------------------------------------------------------------

	/** Cómo se traduce el daño de la fauna en el cuerpo (FBodyModel). */
	struct EXPLORED_API FFaunaHarm
	{
		/** Profundidad del corte (0–1) de un mordisco; 0 = ninguno. */
		float CutDepth = 0.0f;
		bool bSting = false;
		EStingKind Sting = EStingKind::Jellyfish;
	};

	EXPLORED_API FFaunaHarm HarmFromFauna(EFaunaSpecies Species, float Damage);

	// --- Estadísticas de logros (docs/tecnico/estadisticas.md) --------------------------

	/** Id de boats.json de una embarcación (conjunto «boats_built»). */
	EXPLORED_API FName BoatStatId(EBoatType Type);

	/** Id de «events_witnessed»: el nombre del enumerador (LexToString(EWorldEventType)). */
	EXPLORED_API FName WorldEventStatId(EWorldEventType Type);

	/** Id de «wayfinding_techniques»: el de ruins.json (LexToString(EWayfindingTechnique)). */
	EXPLORED_API FName TechniqueStatId(EWayfindingTechnique Technique);

	/** Id de «places_visited» de un punto de interés; NAME_None si no es un lugar singular. */
	EXPLORED_API FName PlaceStatId(EPoiType Type);

	/** Lugar singular «tubo_lava»: la ruina del Humo (Marae del tubo de lava). */
	EXPLORED_API FName PlaceStatIdForRuinSite(FName SiteId);

	/** Distancia (m) a la que se considera que el jugador está en un lugar singular. */
	constexpr float PlaceVisitRadiusMeters = 30.0f;
	/** Distancia (m) a la costa de una isla desde la que se presencia un evento de esa isla. */
	constexpr float EventWitnessRangeMeters = 1500.0f;

	/**
	 * Isla cuya forma nominal contiene el punto (radio con un 10 % de margen por los
	 * lóbulos, y los islotes de Los Dientes); la más cercana si hay varias.
	 * INDEX_NONE en el mar.
	 */
	EXPLORED_API int32 FindIslandAt(const FArchipelagoLayout& Layout, const FVector2D& PositionMeters);

	/** Isla más cercana y distancia (m) a su costa nominal (0 dentro). INDEX_NONE sin islas. */
	EXPLORED_API int32 NearestIsland(const FArchipelagoLayout& Layout, const FVector2D& PositionMeters, float& OutDistanceToCoastMeters);

	/**
	 * ¿Presencia el jugador el evento? Los de todo el mar (Island == Count) sí; los
	 * de una isla, si está en ella o a menos de EventWitnessRangeMeters de su costa.
	 */
	EXPLORED_API bool IsEventWitnessed(const FWorldEvent& Event, const FArchipelagoLayout& Layout, const FVector2D& PlayerMeters);

	/** Días completos vividos en la partida desde su inicio (días totales de juego). */
	EXPLORED_API int32 DaysSurvived(float TotalDays, float RunStartDays);

	/**
	 * Cuentakilómetros de navegación a vela: acumula el recorrido horizontal mientras
	 * la vela va izada y devuelve los metros enteros nuevos para «distance_sailed_m».
	 * Un salto mayor que MaxStepCm entre muestras (teletransporte, carga) no cuenta.
	 */
	struct EXPLORED_API FSailingOdometer
	{
		static constexpr double MaxStepCm = 5000.0;

		/** Devuelve los metros enteros recorridos a vela desde la última llamada que devolvió algo. */
		int32 Step(const FVector2D& PositionCm, bool bUnderSail);
		void Reset() { bHasLast = false; PendingCm = 0.0; }

		FVector2D Last = FVector2D::ZeroVector;
		bool bHasLast = false;
		double PendingCm = 0.0;
	};

	/** Integridad de una pieza en un instante (para vigilar un ciclón). */
	struct EXPLORED_API FPieceIntegrity
	{
		int32 Id = 0;
		float Integrity = 0.0f;
		float MaxIntegrity = 1.0f;
	};

	/**
	 * «Ojo del ciclón»: un ciclón superado sin que la base pierda integridad por el
	 * temporal. Se toma una foto al empezar y se compara al terminar; el desgaste
	 * normal (hasta ToleranceFraction de la integridad máxima) no cuenta, una pieza
	 * que desaparece sí. Hace falta tener alguna pieza al empezar.
	 */
	struct EXPLORED_API FCycloneWatch
	{
		static constexpr float ToleranceFraction = 0.05f;

		/** Devuelve true una vez, al terminar un ciclón superado intacto. */
		bool Update(bool bCycloneActive, const TArray<FPieceIntegrity>& Pieces);

		bool bActive = false;
		TMap<int32, float> StartIntegrity;
	};

	/**
	 * Melodías junto al fuego («flute_played_by_fire»): cuenta una cada vez que el
	 * jugador empieza a tocar de verdad (FFluteModel::IsPerforming) con un fuego que
	 * calienta al menos MinFireHeat.
	 */
	struct EXPLORED_API FFluteMelodyWatch
	{
		static constexpr float MinFireHeat = 0.2f;

		bool Update(bool bPerforming, float FireHeat);

		bool bWasPerformingByFire = false;
	};

	/** Daño por hora de juego a una embarcación durante un ciclón (0–1 del casco). */
	EXPLORED_API float BoatCycloneDamagePerHour(int32 CycloneCategory, bool bGrounded);

	/** ¿Mira el jugador hacia donde mira la estatua? (yaw de Unreal en grados). */
	EXPLORED_API bool IsFacingAlong(float ViewerYawDeg, float TargetYawDeg, float ToleranceDeg = 20.0f);

	// --- Materiales de construcción desde el inventario ----------------------------------

	/** Lo que el modelo ha descontado de un inventario en recuento (Before − After, solo positivos). */
	EXPLORED_API TArray<FBuildingCost> SpentMaterials(const TMap<FName, int32>& Before, const TMap<FName, int32>& After);

	/** Una pila de material que lleva el jugador. */
	struct EXPLORED_API FMaterialStack
	{
		int64 InstanceId = 0;
		FName Item;
		int32 Count = 1;
		/** Menor = se gasta antes (angarillas y mochila antes que las manos). */
		int32 Priority = 0;
	};

	/** Unidades que hay que sacar de una pila. */
	struct EXPLORED_API FMaterialTake
	{
		int64 InstanceId = 0;
		int32 Count = 0;

		bool operator==(const FMaterialTake& Other) const { return InstanceId == Other.InstanceId && Count == Other.Count; }
	};

	/**
	 * Reparte un coste entre las pilas que lleva el jugador: por prioridad y, a
	 * igualdad, en el orden de la lista. False (y OutTakes vacío) si no alcanza.
	 */
	EXPLORED_API bool PlanMaterialTakes(const TArray<FMaterialStack>& Stacks, const TArray<FBuildingCost>& Costs, TArray<FMaterialTake>& OutTakes);

	// --- Cooperativo: dormir en grupo (biblia 08 §5.1) ----------------------------------

	/** Multiplicador del reloj con todo el grupo acostado: 8 h de juego en 6,7 s reales. */
	constexpr float GroupSleepTimeScale = 120.0f;
	/** Tope de horas de juego que se saltan de una vez (un sueño completo). */
	constexpr float GroupSleepMaxHours = 8.0f;
	/** El salto también para al amanecer (el mismo que usan los eventos del mundo). */
	constexpr float GroupSleepDawnHour = FWorldEventsModel::DawnStartHour;

	/** Un jugador conectado, visto por la regla de dormir. */
	struct EXPLORED_API FCoopSleeper
	{
		bool bInBed = false;
		/** `Derribado` (§5.2): no cuenta como acostado y bloquea el sueño de todos. */
		bool bDowned = false;
	};

	/** Qué pasa con la noche según quién está acostado (el aviso de biblia 08 §6.6). */
	enum class EGroupSleepStatus : uint8
	{
		/** Nadie acostado: el reloj sigue a ×1, sin aviso. */
		NobodyInBed,
		/** Alguno acostado, otros en pie: ×1; «Hay {Count} en pie todavía.» / «Queda uno en pie.». */
		WaitingForOthers,
		/** Alguien derribado: ×1; «No se duerme con alguien en el suelo.». */
		BlockedByDowned,
		/** Todos acostados: ×120 hasta el amanecer o las 8 h. */
		AllInBed,
	};

	struct EXPLORED_API FGroupSleepDecision
	{
		EGroupSleepStatus Status = EGroupSleepStatus::NobodyInBed;
		/** Jugadores que no están acostados (los derribados cuentan como en pie). */
		int32 StillUp = 0;
	};

	/** Regla pura de §5.1: la noche solo se salta si todos los conectados están acostados. */
	EXPLORED_API FGroupSleepDecision DecideGroupSleep(const TArray<FCoopSleeper>& Players);

	/** Horas de juego desde HoursOfDay hasta el próximo amanecer, en (0, 24]. */
	EXPLORED_API float HoursUntilDawn(float HoursOfDay);

	/** Qué ha pasado en esta actualización del sueño de grupo. */
	enum class EGroupSleepEvent : uint8
	{
		None,
		/** Se acaban de acostar todos: empieza el salto (fundido en cada cliente). */
		Started,
		/** Llegó el amanecer o las 8 h: vuelta a ×1 con el sueño completo. */
		Completed,
		/** Alguien se levantó (o cayó, o entró uno nuevo) a mitad: vuelta a ×1, horas conservadas. */
		Interrupted,
	};

	/**
	 * Sueño de grupo en el servidor (solo el servidor fija `TimeScale`, que viaja en el
	 * paquete de reloj de §2.8). Se actualiza cada tick con los jugadores conectados, la
	 * hora del día y las horas de juego que han pasado desde la actualización anterior.
	 * Si alguien se levanta a mitad, el reloj vuelve a ×1 de inmediato y se conservan las
	 * horas ya ganadas, con recuperación proporcional (06 §2.12). Tras completar, no vuelve
	 * a empezar hasta que alguien se levante y se acueste de nuevo.
	 */
	struct EXPLORED_API FGroupSleepSession
	{
		struct FResult
		{
			FGroupSleepDecision Decision;
			EGroupSleepEvent Event = EGroupSleepEvent::None;
			/** `TimeScale` que debe fijar el servidor ahora. */
			float TimeScale = 1.0f;
			/** Horas de juego dormidas en el tramo que acaba de terminar (Completed/Interrupted). */
			float HoursSlept = 0.0f;
			/** Fracción del sueño completo recuperada (HoursSlept / 8, 0–1). */
			float Recovery01 = 0.0f;
		};

		FResult Update(const TArray<FCoopSleeper>& Players, float HoursOfDay, float DeltaGameHours);

		bool bActive = false;
		/** Completado y todos siguen acostados: no se encadena otro salto. */
		bool bCompletedLatch = false;
		float HoursSlept = 0.0f;
		float TargetHours = 0.0f;
	};

	// --- Cooperativo: derribado y reanimación (biblia 08 §5.2) --------------------------

	constexpr float DownedSeconds = 90.0f;
	/** De la tercera reanimación del día en adelante, el derribado dura menos. */
	constexpr float DownedSecondsAfterCap = 30.0f;
	constexpr int32 MaxRevivesPerDay = 2;
	constexpr float DownedCrawlSpeedMps = 0.6f;
	constexpr float ReviveSeconds = 6.0f;
	constexpr float ReviveSecondsWithMedicine = 3.0f;
	/** Salud al levantarse (de 100) y golpe de ánimo (`moraleEvents.Injured`). */
	constexpr float RevivedHealth = 25.0f;
	constexpr float RevivedMoraleDelta = -6.0f;

	/** Qué pasa cuando un jugador llega a Health = 0. */
	enum class ECoopHealthZero : uint8
	{
		/** En cooperativo, fuera de Náufrago: `Derribado` durante DownedSeconds (o 30 s tras el tope). */
		Downed,
		/** En solitario: la muerte normal de 01 §7 (ver DecideRespawn). */
		Dead,
		/** Náufrago en cooperativo: morir es morir; espectador hasta que el anfitrión recargue. */
		Spectator,
	};

	/** Estado de derribado de un jugador (lo lleva el servidor, uno por jugador vivo). */
	struct EXPLORED_API FCoopDownState
	{
		bool bDowned = false;
		float SecondsLeft = 0.0f;
		/** Segundos de reanimación acumulados por el compañero que le atiende ahora. */
		float ReviveProgressSeconds = 0.0f;
		/** Día de juego al que se refiere RevivesToday (INDEX_NONE = nunca reanimado). */
		int32 RevivesDay = INDEX_NONE;
		int32 RevivesToday = 0;
	};

	/** Reanimaciones ya gastadas en ese día de juego (0 si el contador es de otro día). */
	EXPLORED_API int32 RevivesUsedOn(const FCoopDownState& State, int32 Day);

	/**
	 * Health = 0: decide y, si toca, abre el derribado. PlayersConnected cuenta a todos,
	 * incluido este; con 1 no hay cooperativo y se muere como en solitario.
	 */
	EXPLORED_API ECoopHealthZero OnHealthZero(FCoopDownState& State, const FSurvivalModeSettings& Mode, int32 PlayersConnected, int32 Day);

	/** Objetos que acortan la reanimación a 3 s si van en la mano (y se consumen). */
	EXPLORED_API bool IsReviveMedicine(FName ItemId);

	/**
	 * Avanza la reanimación de un derribado: true cuando se completa (6 s, o 3 s con
	 * medicina en la mano). Si el compañero suelta, CancelRevive vuelve a empezar de cero.
	 */
	EXPLORED_API bool AdvanceRevive(FCoopDownState& State, float DeltaSeconds, bool bMedicineInHand);
	EXPLORED_API void CancelRevive(FCoopDownState& State);

	/**
	 * Levanta al derribado: 25 de salud, ánimo −6, heridas abiertas sin curar; gasta una
	 * reanimación del día Day. No hace nada si no estaba derribado.
	 */
	EXPLORED_API void FinishRevive(FCoopDownState& State, FSurvivalState& Body, int32 Day);

	/**
	 * Avanza el derribado de todo el grupo (solo los jugadores vivos y conectados, en el
	 * mismo orden siempre) y devuelve en OutDied los índices que mueren ahora: los que
	 * agotan su tiempo y, si en ese momento no queda nadie en pie, todos los derribados
	 * con ellos (el empate de cuatro personas gateando no existe).
	 */
	EXPLORED_API void TickGroupDowned(TArray<FCoopDownState>& Players, float DeltaSeconds, TArray<int32>& OutDied);

	// --- Cooperativo: escalado y reparto (biblia 08 §5.6, §5.7) -------------------------

	/** Jugadores efectivos para el escalado: 1–4. */
	EXPLORED_API int32 CoopPlayersClamped(int32 Players);

	/** `1 + 0,25·(N−1)`: vetas finitas, fauna cazable, pescado y cangrejos. */
	EXPLORED_API float CoopAbundanceScale(int32 Players);

	/** Unidades de una veta finita con N jugadores: Base × escala, redondeo abajo (nunca menos que Base). */
	EXPLORED_API int32 ScaleFiniteVein(int32 BaseUnits, int32 Players);

	/** Asaltantes piratas [F3]: `BaseRaiders × (1 + 0,4·(N−1))` al entero más cercano (5 → 7 / 9 / 11). */
	EXPLORED_API int32 PirateRaidersForPlayers(int32 Players, int32 BaseRaiders = 5);

	/** Categoría extra de asalto [F3]: +1 por cada 2 jugadores por encima de 1 (N=2 → 0, 3 → 1, 4 → 1). */
	EXPLORED_API int32 PirateCategoryBonus(int32 Players);

	/** Quién desbloquea un logro en cooperativo (`coopScope` de achievements.json, §5.7). */
	enum class ECoopScope : uint8
	{
		/** Solo quien hace la acción. */
		Actor,
		/** Todos los conectados en ese momento. */
		World,
		/** Quien esté a menos de CoopWitnessRadiusCm del hecho (y quien lo hace). */
		Witness,
	};

	constexpr double CoopWitnessRadiusCm = 5000.0;

	/** "actor" / "world" / "witness" (sin distinguir mayúsculas); false si no es ninguno. */
	EXPLORED_API bool ParseCoopScope(const FString& Text, ECoopScope& OutScope);

	struct EXPLORED_API FCoopPlayerSpot
	{
		int32 PlayerId = INDEX_NONE;
		FVector PositionCm = FVector::ZeroVector;
		bool bConnected = true;
	};

	/**
	 * Jugadores (PlayerId, en el orden de Players) a los que el servidor manda el RPC de
	 * desbloqueo. El actor siempre lo recibe si está conectado; una posición no finita
	 * nunca cuenta como testigo.
	 */
	EXPLORED_API TArray<int32> AchievementRecipients(ECoopScope Scope, int32 ActorId, const FVector& EventCm, const TArray<FCoopPlayerSpot>& Players);
}
