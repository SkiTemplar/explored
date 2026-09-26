#pragma once

#include "CoreMinimal.h"

/** Modo de juego (GDD §7). */
enum class ESurvivalMode : uint8
{
	Explorer,   // Las necesidades no matan.
	Survivor,   // Experiencia prevista.
	Castaway,   // Necesidades más duras, sin reaparición.
};

/** Actividad física del jugador en el intervalo. */
enum class EActivity : uint8
{
	Resting,
	Walking,
	Sprinting,
	Swimming,
	Working,    // talar, picar, construir
	Sleeping,
};

/** Estados que afectan al cuerpo (biblia §5.4 y GDD §4.3). */
enum class ECondition : uint8
{
	Bleeding,
	Poisoned,
	Fever,
	SunBurn,
	Sprain,
	Infection,
	Count
};

/** Entorno que percibe el cuerpo. */
struct EXPLORED_API FSurvivalInputs
{
	EActivity Activity = EActivity::Walking;
	float AirTemperature = 28.0f;   // °C
	float Wind = 0.2f;              // 0–1
	float Rain = 0.0f;              // 0–1
	float SunExposure = 0.0f;       // 0–1 (al sol a mediodía sin sombra = 1)
	float FireHeat = 0.0f;          // 0–1 (junto a una hoguera = 1)
	bool bInWater = false;
	bool bSheltered = false;        // bajo techo
	bool bHasHat = false;
	float ClothingInsulation = 0.0f; // 0–1
	float CarriedWeightRatio = 0.0f; // peso / capacidad cómoda
	bool bCompanionNearby = false;   // reservado (sin animal de compañía en el diseño actual)
};

/** Lo que aporta una comida o bebida. */
struct EXPLORED_API FConsumable
{
	float Food = 0.0f;       // puntos de hambre que recupera
	float Water = 0.0f;      // puntos de sed que recupera
	float Protein = 0.0f;
	float Carbs = 0.0f;
	float Vitamins = 0.0f;
	float Warmth = 0.0f;     // °C de calor corporal (comida caliente)
	float Morale = 0.0f;
	float Toxicity = 0.0f;   // 0–1, probabilidad de intoxicarse
	float Healing = 0.0f;    // salud inmediata (medicinas)
	/** Estados que cura (bitmask de ECondition). */
	uint32 Cures = 0;
};

/** Eventos que produce el modelo para que el juego reaccione (sonidos, efectos, textos). */
enum class ESurvivalEvent : uint8
{
	Starving,
	Dehydrated,
	Exhausted,
	Hypothermia,
	Heatstroke,
	GotPoisoned,
	Died,
	Count
};

/**
 * Estado del cuerpo del náufrago. Todas las necesidades van de 0 a 100
 * (100 = satisfecho). El tiempo se mide en horas de juego.
 */
struct EXPLORED_API FSurvivalState
{
	float Health = 100.0f;
	float Hunger = 85.0f;
	float Thirst = 70.0f;
	float Energy = 100.0f;       // aguante a corto plazo (correr, nadar, trepar)
	float Rest = 90.0f;          // sueño
	float Morale = 60.0f;
	float BodyTemperature = 37.0f;
	float Wetness = 0.0f;        // 0–1
	float Protein = 50.0f;
	float Carbs = 50.0f;
	float Vitamins = 50.0f;
	float ConditionTime[static_cast<int32>(ECondition::Count)] = {};

	bool HasCondition(ECondition C) const { return ConditionTime[static_cast<int32>(C)] > 0.0f; }
	void AddCondition(ECondition C, float Hours);
	void ClearCondition(ECondition C) { ConditionTime[static_cast<int32>(C)] = 0.0f; }

	bool IsDead() const { return Health <= 0.0f; }

	/** Energía máxima disponible: baja con hambre, sueño, heridas y dieta pobre. */
	float MaxEnergy() const;

	/** Multiplicador de eficiencia en el trabajo (0.5–1.15) según ánimo, sueño y temperatura. */
	float WorkEfficiency() const;
};

/** Reglas del cuerpo. Funciones puras: fáciles de probar y de equilibrar. */
struct EXPLORED_API FSurvivalModel
{
	/**
	 * Avanza el estado. DeltaHours en horas de juego; RandomRoll en [0, 1)
	 * para los sucesos probabilísticos (infecciones), inyectado para los tests.
	 */
	static void Tick(FSurvivalState& State, const FSurvivalInputs& In, float DeltaHours, ESurvivalMode Mode,
		float RandomRoll, TArray<ESurvivalEvent>& OutEvents);

	/** Consume algo. RandomRoll decide la intoxicación. */
	static void Consume(FSurvivalState& State, const FConsumable& Item, float RandomRoll, TArray<ESurvivalEvent>& OutEvents);

	/** Temperatura efectiva que siente el cuerpo (°C). */
	static float EffectiveTemperature(const FSurvivalState& State, const FSurvivalInputs& In);

	/** Consumo de energía a corto plazo por segundo real de la actividad (valores de juego). */
	static float EnergyDrainPerSecond(EActivity Activity, float CarriedWeightRatio);

	/**
	 * Oxígeno (0–100) consumido por segundo real en apnea (buceo, GDD §4.1).
	 * LungCapacityRatio es la mejora de pulmones (1 = base; más alto aguanta
	 * más); CarriedWeightRatio penaliza nadar cargado; bExerting es nadar con
	 * esfuerzo (perseguir algo, luchar contra una corriente) bajo el agua.
	 */
	static float OxygenDrainPerSecond(float CarriedWeightRatio, float LungCapacityRatio, bool bExerting);

	/** Oxígeno recuperado por segundo respirando en superficie; mejor pulmón, jadeo más corto. */
	static float OxygenRecoveryPerSecond(float LungCapacityRatio);
};
