#pragma once

#include "CoreMinimal.h"

#include "Boats/BoatTypes.h"
#include "Events/WorldEventsModel.h"
#include "Fauna/FaunaTypes.h"
#include "Fauna/MarineCreatureBrain.h"
#include "Survival/BodyModel.h"
#include "Survival/SurvivalModel.h"
#include "Weather/WeatherModel.h"
#include "WorldGen/ArchipelagoLayout.h"

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

	// --- Islas y eventos ----------------------------------------------------------------

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

	/** Daño por hora de juego a una embarcación durante un ciclón (0–1 del casco). */
	EXPLORED_API float BoatCycloneDamagePerHour(int32 CycloneCategory, bool bGrounded);
}
