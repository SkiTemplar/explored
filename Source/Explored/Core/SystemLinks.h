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
}
