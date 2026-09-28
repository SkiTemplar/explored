#pragma once

#include "CoreMinimal.h"

#include "Survival/SurvivalModel.h"

/**
 * Combate de H1 (biblia 05 §3, 00-TODO «Combate y fauna peligrosa»).
 *
 * No hay barra de vida nueva: el golpe quita Salud de FSurvivalState y, si corta, abre un
 * corte con FBodyModel::AddCut, que sangra, se infecta y se cura como cualquier otro
 * (biblia 05 §3.0). Este modelo solo decide cuánto, cuándo y si llega:
 *
 * - Daño instantáneo a partir de la propiedad del objeto (items.json, escala 0–5):
 *   Filo o Punta × 3 con un corte de profundidad propiedad ÷ 5; Contundente × 4 sin
 *   corte, con aturdimiento si la propiedad es ≥ 4.
 * - Golpe rápido (× 0,7, hasta 3 seguidos y luego 0,4 s de pausa) y golpe cargado
 *   (× 1,6 tras 1,2 s de aviso visible y 0,3 s de recuperación).
 * - Esquiva: 0,3 s de invulnerabilidad desde que arranca y 1,2 s de reutilización.
 * - Precisión del arco por tramos de distancia: 100/70/40/0 %.
 * - Estadísticas de cerdo salvaje, cabra montés, cangrejo de los cocoteros y tiburón
 *   de arrecife, reflejadas en Content/Data/combat.json (Tools/DataCheck las compara).
 * - Red (biblia 08 §1.2 y §2.7): el servidor resuelve los impactos. Los mensajes que
 *   viajan tienen tamaño fijo y se codifican y validan aquí (ver «Red» abajo).
 *
 * Determinismo: el tiempo va en milisegundos enteros (int64) y el daño en aritmética
 * entera sobre décimas de propiedad, así que 3 × 5 × 0,7 da 10 en todas las máquinas
 * (con float, 10,4999… o 10,5000… según el compilador) y las ventanas de tiempo se
 * pueden probar al milisegundo.
 */

/** Tipo de daño de un golpe. Coincide con la propiedad que lo produce. */
enum class ECombatDamageKind : uint8
{
	None,    // sin propiedad ofensiva: no hace daño
	Cut,     // Filo
	Pierce,  // Punta
	Blunt,   // Contundente
	Count
};

EXPLORED_API const TCHAR* LexToString(ECombatDamageKind Kind);

/** Variante de la acción de ataque (biblia 05 §3.1). Plain = sin multiplicador (flecha, animal, trampa). */
enum class ECombatSwing : uint8
{
	Plain,
	Quick,
	Charged,
	Count
};

EXPLORED_API const TCHAR* LexToString(ECombatSwing Swing);

/** Propiedad ofensiva elegida de un objeto, en décimas (Filo 3 = 30). */
struct EXPLORED_API FCombatStrike
{
	ECombatDamageKind Kind = ECombatDamageKind::None;
	int32 PropertyTenths = 0;
};

/** Lo que hace un golpe al llegar. */
struct EXPLORED_API FCombatHit
{
	ECombatDamageKind Kind = ECombatDamageKind::None;
	ECombatSwing Swing = ECombatSwing::Plain;
	/** Salud que quita (entera, como la tabla de la biblia 05 §3.1). */
	int32 HealthDamage = 0;
	/** Profundidad del corte que abre (0–1); 0 = ninguno. */
	float CutDepth = 0.0f;
	/** Aturdimiento en milisegundos; 0 = ninguno. */
	int32 StunMs = 0;
};

/** Lo que tiene en marcha un combatiente (jugador o animal). */
enum class ECombatPhase : uint8
{
	Idle,
	QuickSwing,  // golpe rápido ejecutándose (0,45 s)
	ChainPause,  // pausa obligatoria tras 3 golpes rápidos (0,4 s)
	Charging,    // golpe cargado con aviso: el arma se echa atrás (1,2 s)
	Recovery,    // recuperación tras soltar el cargado (0,3 s)
	Count
};

EXPLORED_API const TCHAR* LexToString(ECombatPhase Phase);

/** Por qué no se acepta una acción. */
enum class ECombatReject : uint8
{
	None,
	Busy,          // hay un golpe o una recuperación en marcha
	ChainPause,    // ya van 3 golpes rápidos: toca la pausa de 0,4 s
	DodgeCooldown, // la esquiva aún no se ha recuperado
	NotCharging,   // cancelar o soltar sin carga en marcha
	Count
};

EXPLORED_API const TCHAR* LexToString(ECombatReject Reject);

/**
 * Estado de tiempos de un combatiente. Lo guarda el servidor por jugador o animal; no
 * se guarda en disco (al cargar todos están en reposo).
 */
struct EXPLORED_API FCombatTimingState
{
	/** Último instante visto: una llamada con un tiempo anterior se trata como este (el reloj no retrocede). */
	int64 NowMs = 0;
	ECombatPhase Phase = ECombatPhase::Idle;
	int64 PhaseStartMs = 0;
	int64 PhaseEndMs = 0;
	/** Golpes rápidos de la racha actual (0–3). */
	int32 ChainCount = 0;
	/** Cuándo acabó el último golpe rápido: si el siguiente empieza antes de NowMs + pausa, encadena. */
	int64 LastQuickEndMs = 0;
	bool bHasQuick = false;
	/** Arranque de la última esquiva. */
	int64 DodgeStartMs = 0;
	bool bHasDodged = false;
	/** Golpe cargado que ya ha llegado y que Advance aún no ha entregado (una acción puede cerrar la carga). */
	bool bChargedImpactPending = false;
	int64 ChargedImpactMs = 0;
};

/** Resultado de pedir una acción. */
struct EXPLORED_API FCombatActionResult
{
	bool bAccepted = false;
	ECombatReject Reject = ECombatReject::None;
	ECombatSwing Swing = ECombatSwing::Plain;
	/** Golpe rápido: posición en la racha (1–3). */
	int32 ComboIndex = 0;
	/** Instante en que el golpe llega (el rápido, al pedirlo; el cargado, al acabar el aviso). */
	int64 ImpactMs = 0;
};

/** Qué ha pasado al recibir un golpe. */
struct EXPLORED_API FCombatReceiveResult
{
	/** Estaba en la ventana de invulnerabilidad de la esquiva: no ha pasado nada. */
	bool bDodged = false;
	/** Salud que ha quitado de verdad (en Explorador no baja de 10, como las caídas). */
	float HealthLost = 0.0f;
	bool bCutOpened = false;
	bool bDied = false;
};

/** Animales con estadísticas de combate (biblia 05 §5). */
enum class ECombatCreature : uint8
{
	WildBoar,     // cerdo salvaje (jabalí)
	WildGoat,     // cabra montés salvaje
	CoconutCrab,  // cangrejo de los cocoteros grande
	ReefShark,    // tiburón de arrecife genérico (no el tigre legendario «Sombra»)
	Count
};

EXPLORED_API const TCHAR* LexToString(ECombatCreature Creature);

/** Ficha de combate de un animal. Espejo de Content/Data/combat.json («creatures»). */
struct EXPLORED_API FCombatCreatureStats
{
	/** Id de datos (fauna.json o, para el tiburón, combat.json). */
	const TCHAR* Id = TEXT("");
	int32 HealthPoints = 0;
	/** Ataque: propiedad equivalente (mismo formulario que las armas). */
	ECombatDamageKind AttackKind = ECombatDamageKind::None;
	int32 AttackProperty = 0;
	/** Daño del ataque (= fórmula de la propiedad equivalente). */
	int32 AttackDamage = 0;
	/** Corte del ataque; la tabla de la biblia manda sobre la fórmula (tiburón: 0,6, no 4 ÷ 5). */
	float AttackCutDepth = 0.0f;
	/** Aturdimiento si no se esquiva (jabalí: 1 s, no los 1,5 s genéricos). */
	int32 AttackStunMs = 0;
	/** Huye al quedar con esta fracción de vida o menos; 0 = nunca por vida. */
	float FleeHealthFraction = 0.0f;
	/** Huye tras este número de golpes recibidos; 0 = nunca por golpes. */
	int32 FleeAfterHits = 0;
	/** Probabilidad (%) de volcar una balsa o una canoa sin balancín por embestida. */
	int32 CapsizeChancePct = 0;
};

/** Vida y huida de un animal durante una pelea (servidor). */
struct EXPLORED_API FCombatCreatureState
{
	int32 Health = 0;
	int32 HitsTaken = 0;
	bool bFleeing = false;
	bool bDead = false;
};

/** Qué ha pasado al golpear a un animal. */
struct EXPLORED_API FCombatCreatureHitResult
{
	int32 DamageDealt = 0;
	bool bStartedFleeing = false;
	bool bKilled = false;
};

// --- Red (biblia 08) ------------------------------------------------------------------

/** Acción que el cliente pide al servidor (RPC de servidor, fiable). */
enum class ECombatNetAction : uint8
{
	QuickStrike,
	ChargeStart,
	ChargeCancel,
	Dodge,
	BowShot,
	Count
};

EXPLORED_API const TCHAR* LexToString(ECombatNetAction Action);

/**
 * Cliente → servidor, 7 bytes: acción (1), reloj del cliente en ms módulo 65 536 (2),
 * rumbo (2, 360° / 65 536) y cabeceo (2, con signo, 90° / 32 767). El cliente no dice a
 * quién golpea ni cuánto daño hace: el servidor traza desde su copia del personaje con
 * ese rumbo y lee el arma de su copia del inventario (biblia 08 §1.2).
 */
struct EXPLORED_API FCombatActionMsg
{
	ECombatNetAction Action = ECombatNetAction::QuickStrike;
	uint16 ClientTimeMs = 0;
	uint16 AimYaw = 0;
	int16 AimPitch = 0;
};

/**
 * Servidor → cliente, 8 bytes: atacante (2) y víctima (2) como id corto de red, banderas
 * (1: tipo de daño en 2 bits, variante en 2 bits, esquivado, muerto, huye, 1 bit
 * reservado a 0), salud quitada (1), profundidad del corte en 1/255 (1) y aturdimiento
 * en décimas de segundo (1). Va fiable al dueño de la víctima y sin fiabilidad como
 * multicast cosmético a quien esté a menos de 15 m (biblia 08 §2.9).
 */
struct EXPLORED_API FCombatImpactMsg
{
	uint16 AttackerId = 0;
	uint16 VictimId = 0;
	ECombatDamageKind Kind = ECombatDamageKind::None;
	ECombatSwing Swing = ECombatSwing::Plain;
	bool bDodged = false;
	bool bKilled = false;
	bool bFleeing = false;
	uint8 HealthDamage = 0;
	uint8 CutDepth255 = 0;
	uint8 StunDeciseconds = 0;
};

/**
 * Estado de combate replicado de cada personaje a los demás (OnRep, solo al cambiar),
 * 2 bytes: fase (3 bits) y racha (2 bits) en el primero, 3 bits reservados a 0; décimas
 * de segundo transcurridas en la fase, saturadas a 255, en el segundo. Con esto los
 * demás ven el aviso del golpe cargado y la esquiva sin recibir cada fotograma.
 */
struct EXPLORED_API FCombatNetState
{
	ECombatPhase Phase = ECombatPhase::Idle;
	uint8 ChainCount = 0;
	uint8 ElapsedDeciseconds = 0;
};

struct EXPLORED_API FCombatModel
{
	// --- Constantes (biblia 05 §3, espejo de Content/Data/combat.json) ----------------

	static constexpr int32 CutDamagePerPoint = 3;      // Filo y Punta
	static constexpr int32 BluntDamagePerPoint = 4;    // Contundente
	static constexpr int32 CutDepthDivisor = 5;        // profundidad = propiedad ÷ 5
	static constexpr int32 MaxProperty = 5;
	static constexpr int32 BluntStunMinProperty = 4;
	static constexpr int32 BluntStunMs = 1500;

	static constexpr int32 QuickMultiplierPct = 70;
	static constexpr int32 ChargedMultiplierPct = 160;
	static constexpr int32 QuickExecuteMs = 450;
	static constexpr int32 QuickChainMax = 3;
	static constexpr int32 ChainPauseMs = 400;
	static constexpr int32 ChargeTelegraphMs = 1200;
	static constexpr int32 ChargeRecoveryMs = 300;

	static constexpr int32 DodgeInvulnerableMs = 300;
	static constexpr int32 DodgeCooldownMs = 1200;

	static constexpr float ReachShortM = 1.2f;  // cuchillo, hacha, machete
	static constexpr float ReachSpearM = 2.2f;  // lanza

	static constexpr float BowFullBandM = 15.0f;
	static constexpr float BowMidBandM = 30.0f;
	static constexpr float BowFarBandM = 45.0f;
	static constexpr int32 BowFullAccuracyPct = 100;
	static constexpr int32 BowMidAccuracyPct = 70;
	static constexpr int32 BowFarAccuracyPct = 40;

	/** Validación del servidor (biblia 08 §1.2): margen de alcance por latencia y ventana de gracia. */
	static constexpr float ServerReachMarginM = 0.5f;
	static constexpr int32 ServerGraceMs = 250;
	/** Tolerancia de cadencia: un golpe rápido llega como pronto al 85 % de su ejecución. */
	static constexpr int32 CadenceTolerancePct = 15;

	static constexpr int32 ActionMsgBytes = 7;
	static constexpr int32 ImpactMsgBytes = 8;
	static constexpr int32 NetStateBytes = 2;

	// --- Daño -------------------------------------------------------------------------

	/**
	 * Valor en décimas de una propiedad del objeto (FItemDefinition::Properties). Ausente,
	 * negativa, NaN o infinita = 0; por encima de 5 se recorta a 5.
	 */
	static int32 PropertyTenths(const TMap<FName, float>& Properties, FName Name);

	/** La propiedad de ese tipo del objeto (Cut = Filo, Pierce = Punta, Blunt = Contundente). */
	static FCombatStrike StrikeOfKind(const TMap<FName, float>& Properties, ECombatDamageKind Kind);

	/**
	 * La propiedad ofensiva que más daño hace. A igual daño gana la que corta (Filo, luego
	 * Punta), porque además abre herida. Sin ninguna: Kind = None.
	 */
	static FCombatStrike BestStrike(const TMap<FName, float>& Properties);

	/** Daño, corte y aturdimiento de un golpe con esa propiedad y variante. */
	static FCombatHit HitFor(const FCombatStrike& Strike, ECombatSwing Swing);

	/** Atajo: el mejor golpe del objeto con esa variante. */
	static FCombatHit HitWithItem(const TMap<FName, float>& Properties, ECombatSwing Swing);

	/** Porcentaje de la variante (Plain 100, Quick 70, Charged 160). */
	static int32 SwingMultiplierPct(ECombatSwing Swing);

	/**
	 * Alcance cuerpo a cuerpo según la definición de items.json: 2,2 m la lanza, 1,2 m el
	 * resto. No sale de «Largo»: un hacha con mango de bambú es larga y no alcanza más.
	 */
	static float MeleeReachM(FName DefinitionId);

	// --- Tiempos del cuerpo a cuerpo y la esquiva --------------------------------------

	/** Golpe rápido. Llega al instante (el efecto sobre el mundo, del servidor) y ocupa 0,45 s. */
	static FCombatActionResult TryQuick(FCombatTimingState& State, int64 NowMs);

	/** Empieza a cargar: 1,2 s de aviso visible; el golpe llega solo al acabar (ver Advance). */
	static FCombatActionResult StartCharge(FCombatTimingState& State, int64 NowMs);

	/** Suelta la carga antes de tiempo: no hay golpe ni recuperación. */
	static bool CancelCharge(FCombatTimingState& State, int64 NowMs);

	/**
	 * Esquiva: se puede en reposo, en la pausa de la racha o cargando (cancela la carga);
	 * no a mitad de un golpe rápido ni en la recuperación del cargado.
	 */
	static FCombatActionResult TryDodge(FCombatTimingState& State, int64 NowMs);

	/**
	 * Avanza el reloj hasta NowMs cerrando las fases vencidas. Devuelve true si en ese
	 * tramo ha llegado un golpe cargado (OutImpact con su instante exacto).
	 */
	static bool Advance(FCombatTimingState& State, int64 NowMs, FCombatActionResult& OutImpact);

	/** ¿Esquivando? Ventana [arranque, arranque + 300 ms). */
	static bool IsInvulnerable(const FCombatTimingState& State, int64 NowMs);

	/** ¿Se ve el aviso del golpe cargado? */
	static bool IsTelegraphing(const FCombatTimingState& State, int64 NowMs);

	/** Milisegundos que faltan para poder esquivar otra vez (0 = ya). */
	static int32 DodgeCooldownRemainingMs(const FCombatTimingState& State, int64 NowMs);

	// --- Recibir un golpe (enganche con las heridas) -----------------------------------

	/**
	 * Aplica un golpe al cuerpo del jugador si no lo esquiva: quita Salud, abre el corte
	 * con FBodyModel::AddCut (el mismo sistema `wounds` de survival_needs.json) y dispara
	 * el ánimo de «herido». En Explorador no mata (como las caídas: se queda en 10).
	 */
	static FCombatReceiveResult ReceiveHit(FSurvivalState& Body, const FCombatTimingState& Defender, int64 NowMs,
		const FCombatHit& Hit, const FSurvivalModeSettings& Mode, TArray<ESurvivalEvent>& OutEvents);

	// --- Arco ------------------------------------------------------------------------

	/**
	 * Precisión del arco (%) por tramos [0, 15) = 100, [15, 30) = 70, [30, 45) = 40,
	 * ≥ 45 = 0: cada límite pertenece al tramo más lejano. NaN o infinito = 0; negativa
	 * (entrada corrupta) = 0 m.
	 */
	static int32 BowAccuracyPct(float DistanceM);

	/** ¿Acierta el disparo? Tirada del servidor, determinista por semilla y número de disparo. */
	static bool BowShotHits(float DistanceM, uint32 Seed, uint32 ShotIndex);

	/** Golpe de una flecha: Punta de la flecha × 3, sin variante. */
	static FCombatHit ArrowHit(const TMap<FName, float>& ArrowProperties);

	// --- Animales --------------------------------------------------------------------

	static const FCombatCreatureStats& CreatureStats(ECombatCreature Creature);

	/** El golpe que da el animal (con su corte y su aturdimiento de tabla). */
	static FCombatHit CreatureAttack(ECombatCreature Creature);

	static FCombatCreatureState NewCreature(ECombatCreature Creature);

	/** Golpea a un animal: baja su vida, decide si huye o muere. Un animal muerto no recibe más. */
	static FCombatCreatureHitResult HitCreature(FCombatCreatureState& State, ECombatCreature Creature, const FCombatHit& Hit);

	// --- Red: validación del servidor ---------------------------------------------------

	/** ¿Llega el golpe? Alcance del arma + 0,5 m de margen de latencia. */
	static bool ServerAcceptsReach(float DistanceM, float WeaponReachM);

	/** ¿Está el reloj del cliente dentro de la ventana de gracia de 250 ms? (módulo 65 536). */
	static bool ServerAcceptsClientTime(uint16 ServerTimeMs, uint16 ClientTimeMs);

	/** Tiempo mínimo entre dos golpes rápidos pedidos por el cliente (450 ms − 15 %). */
	static int32 MinQuickCadenceMs();

	// --- Red: codificación ------------------------------------------------------------

	static void EncodeAction(const FCombatActionMsg& Msg, TArray<uint8>& OutBytes);
	/** false (sin tocar OutMsg) si el tamaño no es exacto o la acción no existe. */
	static bool DecodeAction(const TArray<uint8>& Bytes, FCombatActionMsg& OutMsg);

	/** El mensaje de impacto de un golpe ya resuelto. Satura la salud a 255 y cuantiza el corte. */
	static FCombatImpactMsg MakeImpact(uint16 AttackerId, uint16 VictimId, const FCombatHit& Hit, bool bDodged,
		bool bKilled, bool bFleeing);
	static void EncodeImpact(const FCombatImpactMsg& Msg, TArray<uint8>& OutBytes);
	/** false si el tamaño no es exacto, el tipo o la variante no existen o el bit reservado no es 0. */
	static bool DecodeImpact(const TArray<uint8>& Bytes, FCombatImpactMsg& OutMsg);

	static FCombatNetState MakeNetState(const FCombatTimingState& State, int64 NowMs);
	static void EncodeNetState(const FCombatNetState& Msg, TArray<uint8>& OutBytes);
	static bool DecodeNetState(const TArray<uint8>& Bytes, FCombatNetState& OutMsg);
};
