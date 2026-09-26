#pragma once

#include "CoreMinimal.h"

/** Modo de juego (GDD §7 y §11). */
enum class ESurvivalMode : uint8
{
	Explorer,   // Las necesidades no matan.
	Survivor,   // Experiencia prevista.
	Castaway,   // Necesidades más duras, sin reaparición.
	Custom,     // Personalizado: velocidad de necesidades elegida por el jugador (FSurvivalModeSettings).
};

/**
 * Ajustes del modo (GDD §11). Los tres modos fijos se construyen con FromMode;
 * el Personalizado añade un multiplicador de velocidad de las necesidades y
 * decide si pueden matar.
 */
struct EXPLORED_API FSurvivalModeSettings
{
	ESurvivalMode Mode = ESurvivalMode::Survivor;
	/** Solo en Personalizado: multiplicador de la velocidad de las necesidades (0.25–3). */
	float NeedSpeed = 1.0f;
	/** Solo en Personalizado: si las necesidades, heridas y caídas pueden matar. */
	bool bNeedsCanKill = true;

	static FSurvivalModeSettings FromMode(ESurvivalMode InMode);
	static FSurvivalModeSettings MakeCustom(float InNeedSpeed, bool bInNeedsCanKill);

	/** Escala total de las necesidades: la del modo por el multiplicador del Personalizado. */
	float NeedScale() const;
	/** Explorador nunca; Personalizado según bNeedsCanKill; el resto, sí. */
	bool NeedsCanKill() const;
	/** Náufrago: sin reaparición en fogatas. */
	bool HasPermadeath() const { return Mode == ESurvivalMode::Castaway; }
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
	JellyfishSting,  // picadura de medusa: escozor; se cura con vinagre
	RaySting,        // picadura de raya: dolor fuerte; antídoto de corteza
	Hallucinating,   // seta alucinógena (o falta de sueño extrema, ver FBodyModel)
	Count
};

/** Bit de un estado para FConsumable::Cures. */
constexpr uint32 SurvivalCureBit(ECondition C) { return 1u << static_cast<uint32>(C); }

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
	float StormIntensity = 0.0f;     // 0–1 (temporal, ciclón): baja el ánimo, sobre todo a la intemperie
	bool bPlayingMusic = false;      // tocando la flauta u oyendo música diegética (GDD §8.12)
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
	/** Estados que cura (bitmask de ECondition, ver SurvivalCureBit). */
	uint32 Cures = 0;
	/** Horas de alucinación (seta alucinógena). */
	float HallucinogenHours = 0.0f;
};

/**
 * Un corte abierto (GDD §8.3). Depth mide lo profundo que es (0–1, decide
 * cuánto tarda en cerrarse y si una venda basta); Bleeding es lo que sangra
 * ahora (0–1). Ver FBodyModel.
 */
struct EXPLORED_API FWound
{
	float Depth = 0.0f;
	float Bleeding = 0.0f;
	/** Horas abierto sin vendar ni limpiar: al llegar al umbral se infecta. */
	float HoursUntreated = 0.0f;
	/** Progreso de cicatrización (0–1); al llegar a 1 desaparece. */
	float Healed = 0.0f;
	bool bBandaged = false;
	/** Vendada con hojas medicinales: cicatriza más rápido. */
	bool bMedicinal = false;
	bool bInfected = false;
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
	SunBurned,
	WoundInfected,
	ScurvyWorse,     // el escorbuto pasa a una etapa peor (encías → visión → sangrado)
	Sprained,
	Stung,
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

	// Cuerpo (P-BODY, FBodyModel).
	/** Escorbuto (0–1): crece con la vitamina C agotada; ver FBodyModel::ScurvyStage. */
	float ScurvySeverity = 0.0f;
	/** Dosis de sol acumulada (horas equivalentes a pleno sol sin sombrero). */
	float SunDose = 0.0f;
	/** Horas seguidas con la dieta desequilibrada (p. ej. solo cocos). */
	float MonotonyHours = 0.0f;
	/** Cortes abiertos. */
	TArray<FWound> Wounds;

	bool HasCondition(ECondition C) const { return ConditionTime[static_cast<int32>(C)] > 0.0f; }
	void AddCondition(ECondition C, float Hours);
	void ClearCondition(ECondition C) { ConditionTime[static_cast<int32>(C)] = 0.0f; }

	bool IsDead() const { return Health <= 0.0f; }

	/** Energía máxima disponible: baja con hambre, sueño, heridas y dieta pobre. */
	float MaxEnergy() const;

	/** Multiplicador de eficiencia en el trabajo (0.5–1.15) según ánimo, sueño y temperatura. */
	float WorkEfficiency() const;

	/** Equilibrio de la dieta (0–1): 1 si proteína, hidratos y vitaminas van parejos. */
	float DietBalance() const;

	/** Falta de sueño (0–1): 0 con el sueño por encima de 30; 1 con el sueño agotado. */
	float SleepDeprivation() const;
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

	/** Igual que el anterior con los ajustes completos del modo (Personalizado incluido). */
	static void Tick(FSurvivalState& State, const FSurvivalInputs& In, float DeltaHours, const FSurvivalModeSettings& Mode,
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
