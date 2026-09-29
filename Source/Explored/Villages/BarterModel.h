#pragma once

#include "CoreMinimal.h"
#include "Villages/ReputationModel.h"

/**
 * Trueque con el pueblo del arrecife [F3] (GDD v2 §5, biblia 05 §1.4, 06 §2.11). Modelo
 * puro: decide si un intercambio se acepta y qué reputación da. Nunca hay moneda ni precio:
 * cada objeto que ofrece el jugador vale de 1 a 5 según su categoría, la suma se multiplica
 * por la tasa del tramo de reputación y tiene que cubrir el valor de lo que pide.
 *
 * La tasa va en cuartos (×0,75 = 3/4) y todo se calcula con enteros de 64 bits: el mismo
 * intercambio da el mismo resultado en el servidor y en cualquier cliente.
 *
 * Red (biblia 08 §5.5, §2.4): el cliente manda el intercambio por RPC; el servidor valida el
 * inventario de quien lo hace, llama a Execute y aplica los objetos. El cliente puede llamar
 * a Evaluate para el gesto del aldeano (asentir o negar), pero no decide nada.
 */

/** Categorías de lo que el jugador ofrece (tabla de biblia 05 §1.4). */
enum class EBarterCategory : uint8
{
	/** No se trueca: objetos rituales (se devuelven), tesoros del museo y lo que no está en la tabla. */
	None,
	CommonFood,     // 1: fruta común, pescado sin conservar.
	PreparedFood,   // 2: comida preparada o conservada.
	FiberCeramic,   // 2: cerámica, tela de fibra, cordelería.
	Leather,        // 3: cuero curtido, piel en bruto.
	GoodTool,       // 3: herramienta de nivel 2-3.
	Medicine,       // 4: medicina.
	WorkedMetal,    // 5: metal trabajado.
	Count
};

EXPLORED_API const TCHAR* LexToString(EBarterCategory Category);

/** Una línea de lo que da el jugador: Count objetos de una categoría. */
struct EXPLORED_API FBarterGive
{
	EBarterCategory Category = EBarterCategory::None;
	int32 Count = 1;
};

/** Algo que ofrece el pueblo (fila de «offers» en fases_futuras.json). */
struct EXPLORED_API FBarterOffer
{
	FName Id;
	/** 1–5. */
	int32 Value = 1;
	/** Tramo mínimo para que lo ofrezcan. */
	EReputationTier MinTier = EReputationTier::Neutral;
};

struct EXPLORED_API FBarterRequest
{
	ESettlement Settlement = ESettlement::WhiteSands;
	/** Día de juego absoluto y hora (0–24). */
	int32 Day = 0;
	float Hour = 12.0f;
	TArray<FBarterGive> Given;
	FBarterOffer Wanted;
	int32 WantedCount = 1;
	/** Gastar el trueque gratis del encargo del tablón: no se paga nada. */
	bool bUseFreeTrade = false;
};

enum class EBarterResult : uint8
{
	Accepted,
	/** Sin primer contacto no hay con quién hacer trueque. */
	NotContacted,
	/** Fuera de 8:00-18:00: los trocadores están en su rutina. */
	OutsideHours,
	/** Tramo Hostil: trueque cerrado. */
	Hostile,
	/** Reputación ya recuperada, pero siguen los 15 días de enfriamiento. */
	Cooldown,
	/** Lo pedido exige un tramo más alto. */
	TierTooLow,
	/** Con tramo Cauta solo aceptan comida, fibra y cerámica. */
	NotBasicWhileWary,
	/** Alguna línea es un objeto que no se trueca (ritual, tesoro, fuera de tabla). */
	NotTradeable,
	/** Cantidades o valores imposibles (≤ 0, valor fuera de 1-5). */
	Invalid,
	/** Lo ofrecido no cubre lo pedido. */
	NotEnough,
	/** Se pidió el trueque gratis y no hay ninguno pendiente hoy. */
	NoFreeTrade,
	Count
};

EXPLORED_API const TCHAR* LexToString(EBarterResult Result);

struct EXPLORED_API FBarterOutcome
{
	EBarterResult Result = EBarterResult::Invalid;
	/** Valor de lo ofrecido tras la tasa, en cuartos. */
	int64 OfferedQuarters = 0;
	/** Valor de lo pedido, en cuartos (Value × Count × 4; 0 con el trueque gratis). */
	int64 RequiredQuarters = 0;
	/** Reputación ganada al ejecutarlo (+3 el primero del día, 0 el resto y el gratis). */
	int32 ReputationGained = 0;
	bool bUsedFreeTrade = false;

	bool IsAccepted() const { return Result == EBarterResult::Accepted; }
};

struct EXPLORED_API FBarterModel
{
	/** Biblia 05 §1.4: ventana horaria [8:00, 18:00). A las 18:00 en punto ya no atienden. */
	static constexpr float OpenHour = 8.0f;
	static constexpr float CloseHour = 18.0f;
	/** Tope de objetos por línea: más que esto no cabe en una mano ni en una mochila. */
	static constexpr int32 MaxCountPerLine = 999;
	/** Tope de líneas por intercambio. */
	static constexpr int32 MaxLines = 16;

	/** Valor 1-5 de una categoría; 0 para None. */
	static int32 CategoryValue(EBarterCategory Category);
	/** Comida, fibra y cerámica: lo único que aceptan con tramo Cauta. */
	static bool IsBasic(EBarterCategory Category);
	static bool IsWithinHours(float Hour);

	/**
	 * Decide sin tocar nada. El orden de comprobación es el que ve el jugador: primero si hay
	 * alguien que atienda (contacto, hora, enfriamiento, tramo) y luego qué se ofrece.
	 */
	static FBarterOutcome Evaluate(const FReputationState& State, const FBarterRequest& Request);

	/**
	 * Evalúa y, si se acepta, aplica la reputación: +3 al primer trueque del día (biblia 05
	 * §1.4) o consume el trueque gratis, que no cuenta para ese límite.
	 */
	static FBarterOutcome Execute(FReputationState& State, const FBarterRequest& Request);

	/**
	 * Cuántas unidades de Request.Wanted cubre lo que se ofrece (Request.WantedCount no se
	 * mira): lo que enseña el prompt «Ofrecer {objeto} → {oferta} ×N» de biblia 06 §2.11.
	 * 0 si el trueque no se acepta ni por una unidad.
	 */
	static int32 AffordableCount(const FReputationState& State, const FBarterRequest& Request);
};
