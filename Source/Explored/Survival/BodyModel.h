#pragma once

#include "CoreMinimal.h"

#include "Survival/SurvivalModel.h"

/**
 * Cuerpo del náufrago más allá de las necesidades básicas (GDD §8.3, biblia §5.4):
 * escorbuto, cortes e infección, caídas y esguinces, picaduras, intoxicaciones,
 * quemaduras solares, dieta monótona, falta de sueño y fuentes de ánimo.
 *
 * Todo son funciones puras sobre FSurvivalState. FSurvivalModel::Tick llama a
 * FBodyModel::Tick en cada paso; el resto son acciones que el juego dispara
 * (caerse, cortarse, vendarse, descubrir algo). Las constantes están reflejadas
 * en Content/Data/survival_needs.json («body»), que Tools/DataCheck compara.
 */

/** Etapas del escorbuto (GDD §8.3: encías, visión, sangrado leve). */
enum class EScurvyStage : uint8
{
	None,
	Gums,      // encías doloridas: baja el ánimo
	Vision,    // la vista pierde color
	Bleeding,  // sangrado leve y heridas que tardan en cerrar
};

/** Cómo se trata un corte (biblia §3.7). */
enum class EWoundTreatment : uint8
{
	CleanWater,    // agua salada o hervida: limpia (reinicia el reloj de infección), no para la sangre
	ClothBandage,  // venda de tela: para la sangre de cortes no muy profundos
	LeafBandage,   // venda de hojas medicinales: igual que la tela y cicatriza más rápido
	Suture,        // aguja de hueso y tendón: cierra incluso los cortes profundos
};

/** Dónde se aterriza tras una caída (GDD §8.1). */
enum class ELandingSurface : uint8
{
	Rock,
	Ground,
	Sand,
	Water,
};

/** Picaduras del mar (GDD §8.3). */
enum class EStingKind : uint8
{
	Jellyfish,
	Ray,
};

/** Fuentes de intoxicación y riesgos de lo que se come o se bebe (GDD §8.3, biblia §5.2). */
enum class EFoodHazard : uint8
{
	None,
	UnboiledWater,         // río o arroyo sin hervir: parásitos, intoxicación leve
	SwampWater,            // charca o manglar: grave
	SeaWater,              // deshidrata
	ToxicMushroom,
	HallucinogenicMushroom,
	RawCassava,            // yuca cruda
	RawCashew,             // anacardo sin tostar
};

/** Momentos que suben o bajan el ánimo de golpe (GDD §8.3, biblia §5.4). */
enum class EMoraleEvent : uint8
{
	Discovery,     // ruina, tesoro o especie nueva
	MapProgress,   // un tramo de costa confirmado
	IslandMapped,  // el mapa de una isla completo
	MusicPlayed,   // tocar la flauta
	HotMeal,       // comer caliente
	SleptInBed,    // dormir en tu cama
	Injured,       // cortarse, caerse, picarse
	StormHit,      // un temporal te pilla fuera
	Count
};

/** Resultado de una caída. */
struct EXPLORED_API FFallResult
{
	float Damage = 0.0f;       // salud perdida
	float SprainHours = 0.0f;  // > 0 si se tuerce el tobillo
};

struct EXPLORED_API FBodyModel
{
	// --- Escorbuto -------------------------------------------------------------

	static EScurvyStage ScurvyStage(float Severity);

	// --- Cortes e infección ----------------------------------------------------

	/** Abre un corte (Depth 0–1): sangra en proporción a su profundidad. */
	static void AddCut(FSurvivalState& State, float Depth);

	/** Aplica un tratamiento a todos los cortes abiertos; devuelve a cuántos afectó. */
	static int32 TreatWounds(FSurvivalState& State, EWoundTreatment Treatment);

	/** Sangrado total (0–1): cortes abiertos más el estado Bleeding genérico. */
	static float TotalBleeding(const FSurvivalState& State);

	/** Salud perdida por hora por los cortes que sangran ahora mismo. */
	static float BleedingDamagePerHour(const FSurvivalState& State);

	// --- Caídas (GDD §8.1) -----------------------------------------------------

	/** Daño y esguince de una caída de HeightM metros. Monótono en la altura. */
	static FFallResult FallDamage(float HeightM, ELandingSurface Surface);

	/** Aplica una caída al estado (en Explorador no mata, como las necesidades). */
	static FFallResult ApplyFall(FSurvivalState& State, float HeightM, ELandingSurface Surface,
		const FSurvivalModeSettings& Mode, TArray<ESurvivalEvent>& OutEvents);

	// --- Picaduras -------------------------------------------------------------

	static void ApplySting(FSurvivalState& State, EStingKind Kind, TArray<ESurvivalEvent>& OutEvents);

	/** Remedio de planta para cada picadura: vinagre (medusa) o antídoto de corteza (raya). */
	static FConsumable Antidote(EStingKind Kind);

	// --- Intoxicaciones --------------------------------------------------------

	/** Añade a una comida o bebida el riesgo de su origen (toxicidad, deshidratación, alucinaciones). */
	static FConsumable WithHazard(const FConsumable& Base, EFoodHazard Hazard);

	// --- Ánimo -----------------------------------------------------------------

	static float MoraleEventDelta(EMoraleEvent Event);
	static void ApplyMoraleEvent(FSurvivalState& State, EMoraleEvent Event);

	// --- Sueño, dolor y dieta --------------------------------------------------

	/** Torpeza (0–1): falta de sueño, esguince, frío. El juego la usa para fallos y temblor de puntería. */
	static float Clumsiness(const FSurvivalState& State);

	/** Alucinación (0–1): plena con la seta; leve (≤ 0.35) solo por falta de sueño extrema. */
	static float HallucinationIntensity(const FSurvivalState& State);

	/** Dolor (0–1): cortes, esguince, picaduras, quemaduras e infección. */
	static float Pain(const FSurvivalState& State);

	/** Cuánto pesa la dieta monótona (0–1); consecuencias leves, nunca mata. */
	static float MonotonyFactor(const FSurvivalState& State);

	// --- Paso de simulación ----------------------------------------------------

	/**
	 * Avanza lo propio del cuerpo. Lo llama FSurvivalModel::Tick: suma el daño a
	 * InOutDamage (que luego recorta el modo) y el ánimo por hora a InOutMoralePerHour.
	 */
	static void Tick(FSurvivalState& State, const FSurvivalInputs& In, float DeltaHours, const FSurvivalModeSettings& Mode,
		float& InOutDamage, float& InOutMoralePerHour, TArray<ESurvivalEvent>& OutEvents);
};
