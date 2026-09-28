#pragma once

#include "CoreMinimal.h"
#include "Ruins/MuseumModel.h"
#include "Save/SaveArchive.h"

/**
 * Reputación con el pueblo del arrecife [F3] (biblia 05 §1.2, §1.5, §1.6). Modelo puro:
 * un contador 0–100 por asentamiento, las acciones que lo mueven y el enfriamiento del
 * trueque tras una ofensa grave.
 *
 * Reglas que manda la biblia y que este modelo hace cumplir:
 * - Arenas Blancas y La Meseta llevan su propia reputación; respetar a uno no compra al otro.
 * - La reputación nace en el primer contacto con valor 50; antes no existe.
 * - **Sin decaimiento pasivo**: no hay ningún paso de tiempo que la mueva; solo cambian las
 *   acciones del jugador (por eso el modelo no tiene Tick).
 * - Si cae por debajo de 20, el trueque queda cerrado 15 días de juego aunque el número se
 *   recupere antes. Una ofensa nueva estando por debajo de 20 alarga el plazo, nunca lo acorta.
 * - Los navegantes **nunca** son enemigos (decisión cerrada del director): no hay barra de
 *   vida, no se les puede herir y ningún valor de reputación los vuelve combatientes.
 *
 * Red (biblia 08 §5.5): la reputación es del grupo y vive en el servidor. Los clientes piden
 * cada acción por RPC; el servidor la aplica aquí y replica solo el tramo de cada
 * asentamiento (el número exacto no se enseña nunca en pantalla, biblia 06 §2.11).
 */

/** Asentamientos del pueblo del arrecife (biblia 05 §1.1). */
enum class ESettlement : uint8
{
	WhiteSands,  // «whitesands»: aldea principal de Arenas Blancas (10 NPC).
	Mesa,        // «mesa»: puesto de trueque de La Meseta (4 NPC).
	Count
};

/** Id del asentamiento, el mismo que usa Content/Data/fases_futuras.json. */
EXPLORED_API const TCHAR* LexToString(ESettlement Settlement);

/** Tramos de reputación (biblia 05 §1.5). */
enum class EReputationTier : uint8
{
	Hostile,  // 0–19: trueque cerrado.
	Wary,     // 20–39: tasa ×0,75, solo objetos básicos.
	Neutral,  // 40–69: tasa ×1.
	Good,     // 70–89: tasa ×1,25, enseña wayfinding.
	High,     // 90–100: tasa ×1,5, animales, aviso de asaltos, ayuda en defensa.
	Count
};

/** Id del tramo, el mismo de fases_futuras.json («hostil», «cauta»…). */
EXPLORED_API const TCHAR* LexToString(EReputationTier Tier);

/** Acciones del jugador que mueven la reputación (biblia 05 §1.5). */
enum class EReputationAction : uint8
{
	Trade,             // +3, solo el primer trueque de cada día.
	ReturnRitual,      // +5, una vez por objeto en toda la partida.
	BoardRequest,      // +4 y un trueque gratis ese día.
	RespectfulDay,     // +2, una vez al día.
	DefendRaid,        // +10, solo con reputación ≥ 40.
	HuntNearby,        // −15: cazar a menos de 150 m de la aldea.
	MineOrFellNearMarae, // −10: minar o talar a menos de 50 m de un marae activo.
	Loot,              // −20: saquear un contenedor o las ofrendas.
	StrikeVillager,    // −25: golpear a un aldeano (huye; nunca se le hiere).
	Count
};

EXPLORED_API const TCHAR* LexToString(EReputationAction Action);

/** Estado de un asentamiento. Los días son días de juego absolutos. */
struct EXPLORED_API FSettlementReputation
{
	/** Ha habido primer contacto; sin él la reputación no existe. */
	bool bContacted = false;
	/** 0–100; solo tiene sentido con bContacted. */
	int32 Reputation = 0;
	/** El trueque está cerrado mientras Day < TradeCooldownUntilDay. */
	int32 TradeCooldownUntilDay = 0;
	/** Último día en que un trueque dio reputación (límite de uno al día); −1 = nunca. */
	int32 LastTradeReputationDay = -1;
	/** Último día que contó como jornada respetuosa; −1 = nunca. */
	int32 LastRespectfulDay = -1;
	/** Día con un trueque gratis pendiente por un encargo del tablón; −1 = ninguno. */
	int32 FreeTradeDay = -1;

	bool operator==(const FSettlementReputation& Other) const
	{
		return bContacted == Other.bContacted && Reputation == Other.Reputation &&
			TradeCooldownUntilDay == Other.TradeCooldownUntilDay &&
			LastTradeReputationDay == Other.LastTradeReputationDay && LastRespectfulDay == Other.LastRespectfulDay &&
			FreeTradeDay == Other.FreeTradeDay;
	}
};

/** Reputación de toda la partida (sección de guardado «reputation»). */
struct EXPLORED_API FReputationState
{
	FSettlementReputation Settlements[static_cast<int32>(ESettlement::Count)];
	/** Objetos rituales ya devueltos (ids de artifacts.json); cada uno suma una sola vez. */
	TArray<FName> ReturnedRituals;

	const FSettlementReputation& Get(ESettlement S) const { return Settlements[static_cast<int32>(S)]; }
	FSettlementReputation& Get(ESettlement S) { return Settlements[static_cast<int32>(S)]; }

	bool operator==(const FReputationState& Other) const;
};

/** Qué pasó al aplicar una acción. */
struct EXPLORED_API FReputationChange
{
	/** Cambio real aplicado (tras el recorte a 0–100 y los límites diarios). */
	int32 Delta = 0;
	int32 Before = 0;
	int32 After = 0;
	/** Esta acción fue el primer contacto con el asentamiento. */
	bool bFirstContact = false;
	/** Esta acción abrió o alargó el enfriamiento de 15 días. */
	bool bCooldownStarted = false;
	/** La acción no contó (límite diario, objeto ya devuelto, reputación insuficiente…). */
	bool bIgnored = false;
};

/** Reacción de un aldeano al que el jugador golpea: huye, nunca se hiere. */
struct EXPLORED_API FVillagerStrikeResult
{
	FReputationChange Change;
	/** El aldeano huye y no vuelve a comerciar ese día. */
	bool bFlees = true;
	/** Siempre 0: los aldeanos no tienen vida que perder (biblia 05 §1.5). */
	float HealthDamage = 0.0f;
};

struct EXPLORED_API FReputationModel
{
	static constexpr int32 MinReputation = 0;
	static constexpr int32 MaxReputation = 100;
	/** Biblia 05 §1.2: reputación al primer contacto (tramo Neutral). */
	static constexpr int32 FirstContactReputation = 50;
	/** Por debajo de esto el pueblo es Hostil y empieza el enfriamiento. */
	static constexpr int32 HostileBelow = 20;
	/** Biblia 05 §1.5: días de juego con el trueque cerrado tras caer por debajo de 20. */
	static constexpr int32 HostileCooldownDays = 15;
	/** Biblia 05 §1.5: ayudar a repeler un asalto solo cuenta con reputación ≥ 40. */
	static constexpr int32 DefendRaidMinReputation = 40;
	/** Biblia 05 §1.2: radio de detección del primer contacto (m). */
	static constexpr float FirstContactRadiusM = 80.0f;

	/** Límite inferior de cada tramo: 0, 20, 40, 70, 90. */
	static int32 TierMin(EReputationTier Tier);
	static EReputationTier TierOf(int32 Reputation);
	/** Tasa del trueque en cuartos (0, 3, 4, 5, 6 → ×0, ×0,75, ×1, ×1,25, ×1,5): aritmética entera exacta. */
	static int32 TradeRateQuarters(EReputationTier Tier);
	/** Cambio fijo de cada acción según la biblia (+3, +5, +4, +2, +10, −15, −10, −20, −25). */
	static int32 ActionDelta(EReputationAction Action);

	/**
	 * Primer contacto: crea la reputación con valor 50. Devuelve true solo la primera vez;
	 * volver a llamarla no toca nada.
	 */
	static bool Contact(FReputationState& State, ESettlement Settlement);

	/**
	 * Aplica una acción el día Day. Una acción sobre un asentamiento sin contacto hace antes
	 * el primer contacto (quien caza junto a la aldea ya está allí). Límites:
	 * - Trade: solo el primero de cada día suma;
	 * - RespectfulDay: una vez al día;
	 * - DefendRaid: solo con reputación ≥ 40;
	 * - ReturnRitual: usa ReturnRitualObject, que sabe qué objeto se devuelve (aquí se ignora).
	 */
	static FReputationChange Apply(FReputationState& State, ESettlement Settlement, EReputationAction Action, int32 Day);

	/**
	 * Devolver un objeto ritual a su marae (biblia 05 §1.4-1.5): +5 sin trueque de por medio,
	 * a cualquier hora y también con el trueque cerrado. Cada objeto cuenta una sola vez en
	 * toda la partida, se devuelva donde se devuelva. Rechaza los que no son rituales.
	 */
	static FReputationChange ReturnRitualObject(FReputationState& State, ESettlement Settlement, FName ArtifactId,
		EArtifactProvenance Provenance, int32 Day);

	/** Objetos que el pueblo reconoce como suyos: los de marae y cueva ritual (no los del pecio). */
	static bool IsRitualProvenance(EArtifactProvenance Provenance);

	/** Golpear a un aldeano: −25, huye y no se hiere. */
	static FVillagerStrikeResult StrikeVillager(FReputationState& State, ESettlement Settlement, int32 Day);

	/** Tramo actual; Neutral si no hay contacto (lo que habrá al llegar, sin efectos todavía). */
	static EReputationTier Tier(const FReputationState& State, ESettlement Settlement);

	/**
	 * El trueque está abierto por reputación (no mira la hora): hay contacto, el tramo no es
	 * Hostil y no hay enfriamiento en curso.
	 */
	static bool IsTradeOpenByReputation(const FReputationState& State, ESettlement Settlement, int32 Day);
	static bool IsInCooldown(const FReputationState& State, ESettlement Settlement, int32 Day);
	/** Días de enfriamiento que quedan en Day (0 si no hay). */
	static int32 CooldownDaysLeft(const FReputationState& State, ESettlement Settlement, int32 Day);

	/** Hay un trueque gratis del tablón pendiente ese día. */
	static bool HasFreeTrade(const FReputationState& State, ESettlement Settlement, int32 Day);

	/** Biblia 05 §1.6: el guardián enseña wayfinding con reputación Buena (≥ 70). */
	static bool CanTeachWayfinding(const FReputationState& State, ESettlement Settlement);
	/** Biblia 05 §1.5: animales domésticos, aviso de asaltos y ayuda en defensa con Alta (≥ 90). */
	static bool GrantsHighFavors(const FReputationState& State, ESettlement Settlement);

	/**
	 * Los navegantes nunca son enemigos: siempre false, con cualquier reputación. Existe para
	 * que la IA y el combate lo pregunten aquí en vez de deducirlo del tramo.
	 */
	static bool IsHostileCombatant(const FReputationState& State, ESettlement Settlement);

	// --- Guardado (sección «reputation») -------------------------------------------------
	/**
	 * Escribe cada asentamiento por su id y la lista de objetos devueltos. La carga parte del
	 * estado por defecto, recorta la reputación a 0–100, descarta un asentamiento ilegible
	 * sin tirar el otro y quita los ids repetidos o vacíos.
	 */
	static void Save(FSaveArchive& Ar, const FReputationState& State);
	static void Load(const FSaveArchive& Ar, FReputationState& OutState);
};
