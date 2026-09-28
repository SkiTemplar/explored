#pragma once

#include "CoreMinimal.h"

#include "Survival/BodyModel.h"

/**
 * Escalada (biblia 02 §13): trepar palmeras y escalar roca por salientes.
 *
 * Modelo puro sin UObject. La capa UE (movimiento personalizado del personaje)
 * le pasa la ruta que tiene delante y el eje vertical de cada tick, y aplica lo
 * que devuelve: altura sobre la base de la ruta, Energía gastada y, al soltarse,
 * la caída que resuelve FBodyModel::FallDamage (biblia 01 §6.13).
 *
 * Red (biblia 08 §1.2): el movimiento propio se predice. El cliente ejecuta el
 * mismo Tick con su copia replicada de la Energía; el servidor lo vuelve a
 * ejecutar con la Energía autoritativa y compara con ValidateClientMove. La
 * Energía, el daño y el esguince solo los aplica el servidor.
 */

/** Sobre qué se trepa. */
enum class EClimbSurface : uint8
{
	Palm, // tronco de palmera (especie «Palm» de FHarvestModel)
	Rock, // pared de roca por salientes, pendiente > 60°
};

/** Estados de la escalada. None = con los pies en el suelo, fuera del modelo. */
enum class EClimbState : uint8
{
	None,
	Grabbing, // agarrado y quieto
	Climbing, // subiendo o bajando
	Resting,  // en un anclaje (copa, clavija, cuerda fija): recupera Energía
	Falling,  // se ha soltado; espera a aterrizar
};

/** Por qué se rechaza una orden. None = aceptada. */
enum class EClimbReject : uint8
{
	None,
	IllegalTransition, // la orden no existe desde el estado actual
	InvalidRoute,      // alturas no finitas, negativas o nulas
	SlopeTooGentle,    // roca con pendiente <= 60°: se sube andando o no se sube
	CarryingSledge,    // con las angarillas enganchadas no se trepa (FInventoryModel::CanClimb)
	NoEnergy,          // sin Energía no se empieza a trepar
	NoAnchor,          // descansar solo en un anclaje
	NotRock,           // las clavijas solo van en roca
};

/** Avisos al jugador (biblia 02, «Textos de feedback»). El tope de 3 m no tiene aviso (§13.2). */
enum class EClimbNotice : uint8
{
	None,
	Exhausted,      // «No llego más arriba así.»
	NoPitonSpot,    // «Aquí no hay donde clavar nada.»
	CarryingSledge, // «Con las angarillas no puedo trepar.»
};

/** Ajustes. Los valores de biblia 02 §13 y 01 §6.3; el resto, §13.6. */
struct EXPLORED_API FClimbTuning
{
	float PalmSpeedMps = 0.7f;
	float PalmFootSpeedMps = 1.3f;   // con pie_de_palmera
	float RockSpeedMps = 0.7f;
	float FixedRopeSpeedMps = 1.3f;  // igual que con pie de palmera (§13.3)

	float ClimbDrainPerSecond = 9.0f;     // × peso, a pulso
	float PalmFootDrainPerSecond = 6.0f;  // × peso, con pie_de_palmera
	float HoldDrainFraction = 1.0f / 3.0f; // agarrado y quieto: un tercio de lo que cuesta subir
	float RestRecoveryPerSecond = 6.0f;   // en un anclaje

	float MinRockSlopeDeg = 60.0f;  // estrictamente mayor
	float FreeReachM = 3.0f;        // tramo sin clavijas; también el que añade cada clavija
	float AnchorSnapM = 0.3f;       // distancia a un anclaje para poder descansar en él

	/** Error de altura (m) que el servidor tolera sin corregir al cliente (8 cm, biblia 08 §1.2). */
	float CorrectionToleranceM = 0.08f;
};

/** Lo que hay delante del jugador al pulsar «Trepar». Alturas en metros sobre la base. */
struct EXPLORED_API FClimbRoute
{
	EClimbSurface Surface = EClimbSurface::Palm;

	/** Altura de la copa (palmera) o de la cima de la pared (roca). */
	float TopHeightM = 0.0f;

	/** Pendiente de la pared en grados (solo roca). */
	float SlopeDeg = 90.0f;

	/** Clavijas ya clavadas en esta pared, en metros (persisten con el terreno, §13.4). */
	TArray<float> PitonHeightsM;

	/** Cuerda fija desde la base hasta esta altura; 0 = sin cuerda (§13.3). */
	float FixedRopeTopM = 0.0f;
};

/** Datos del jugador que el servidor lee de su propia copia (biblia 08 §1.2). */
struct EXPLORED_API FClimberInfo
{
	/** Peso cargado / capacidad cómoda, igual que FSurvivalInputs::CarriedWeightRatio. */
	float CarriedWeightRatio = 0.0f;
	bool bHasPalmFoot = false;  // pie_de_palmera en el inventario
	bool bHasSledge = false;    // angarillas enganchadas
};

/** Entrada de un tick. */
struct EXPLORED_API FClimbInput
{
	/** Eje vertical: 1 sube, −1 baja, 0 quieto. Se recorta a [−1, 1]; NaN cuenta como 0. */
	float Vertical = 0.0f;

	/** Energía actual del jugador (0–100). */
	float Energy = 100.0f;
};

/** Resultado de un tick. */
struct EXPLORED_API FClimbStep
{
	float HeightM = 0.0f;
	float EnergyDelta = 0.0f; // a sumar a la Energía (negativo = gasto)
	EClimbNotice Notice = EClimbNotice::None;

	bool bReachedCrown = false;  // ha llegado a la copa de la palmera: coco_verde y «palms_climbed»
	bool bToppedOut = false;     // ha coronado la pared: vuelve a andar arriba
	bool bReachedBase = false;   // ha bajado hasta el suelo
	bool bAtReachLimit = false;  // no hay saliente para seguir; se para sin mensaje (§13.2)
	bool bStartedFalling = false;
};

/** Estado replicado: un byte y la altura en centímetros (biblia 08 §3). */
struct EXPLORED_API FClimbSnapshot
{
	EClimbState State = EClimbState::None;
	int32 HeightCm = 0;

	bool operator==(const FClimbSnapshot& O) const { return State == O.State && HeightCm == O.HeightCm; }
};

/** Veredicto del servidor sobre un movimiento predicho por el cliente. */
enum class EClimbValidation : uint8
{
	Accept,
	Correct, // el servidor manda su FClimbSnapshot al cliente
};

class EXPLORED_API FClimbModel
{
public:
	FClimbModel() = default;
	explicit FClimbModel(const FClimbTuning& InTuning) : Tuning(InTuning) {}

	/** Especies de FHarvestModel a las que se puede trepar. */
	static bool IsClimbableSpecies(FName Species);

	/** Pendiente de roca escalable: estrictamente más de 60°. */
	static bool IsClimbableSlope(float SlopeDeg, const FClimbTuning& Tuning);

	/** Energía por segundo que cuesta subir (positiva) por esta superficie. */
	static float ClimbDrainPerSecond(EClimbSurface Surface, const FClimberInfo& Who, const FClimbTuning& Tuning);

	/**
	 * Altura máxima alcanzable en la ruta: la copa en palmera; en roca, el tramo libre
	 * desde la base y desde cada anclaje alcanzable (clavija o final de cuerda fija).
	 */
	static float ReachM(const FClimbRoute& Route, const FClimbTuning& Tuning);

	/** Aviso de apuntar el pico a una pared para clavar: NoPitonSpot si no pasa de 60°. */
	static EClimbNotice PitonNotice(float SlopeDeg, const FClimbTuning& Tuning);

	/** Aviso que acompaña a un rechazo (la mayoría se rechazan en silencio). */
	static EClimbNotice NoticeFor(EClimbReject Reject);

	/** Texto del aviso en el idioma activo (ES fuente, EN en Tools/Localization). */
	static FText NoticeText(EClimbNotice Notice);

	// --- Órdenes (el servidor las valida igual que el cliente) --------------------

	/** None → Grabbing en la base de la ruta. */
	EClimbReject Start(const FClimbRoute& Route, const FClimberInfo& Who, float Energy);

	/** Descansar en un anclaje (Grabbing/Climbing → Resting). */
	EClimbReject Rest();

	/** Volver a agarrarse para seguir (Resting → Grabbing). */
	EClimbReject Resume();

	/** Soltarse a propósito (Grabbing/Climbing/Resting → Falling). */
	EClimbReject Release();

	/**
	 * Clavar una clavija a la altura actual, agarrado o descansando en roca. El
	 * servidor comprueba antes el pico en mano y la clavija en su inventario.
	 */
	EClimbReject DrivePiton();

	/**
	 * Falling → None al tocar el suelo. ExtraDropM es lo que queda por debajo de la
	 * base de la ruta (una ladera, un saliente); la altura de caída es la suma.
	 * Devuelve el daño y esguince de FBodyModel::FallDamage; si no se estaba
	 * cayendo, no hace nada y devuelve un resultado vacío.
	 */
	FFallResult Land(ELandingSurface Surface, float ExtraDropM = 0.0f);

	/** Avanza DeltaSeconds. Solo mueve en Grabbing/Climbing/Resting. */
	FClimbStep Tick(const FClimbInput& Input, float DeltaSeconds);

	// --- Red -----------------------------------------------------------------

	FClimbSnapshot Snapshot() const;

	/** Compara lo que dice el cliente con el resultado del servidor tras el mismo tick. */
	static EClimbValidation ValidateClientMove(const FClimbSnapshot& Server, const FClimbSnapshot& Client,
		const FClimbTuning& Tuning);

	// --- Consulta ------------------------------------------------------------

	EClimbState GetState() const { return State; }
	float GetHeightM() const { return Height; }
	float GetFallHeightM() const { return FallFromM; }
	const FClimbRoute& GetRoute() const { return Route; }
	const FClimbTuning& GetTuning() const { return Tuning; }
	bool IsOnWall() const { return State == EClimbState::Grabbing || State == EClimbState::Climbing || State == EClimbState::Resting; }

	/** Hay un anclaje (copa, clavija, cuerda fija) a la altura actual. */
	bool IsAtAnchor() const;

private:
	float SpeedMps() const;
	bool OnFixedRope() const;
	void BeginFall(FClimbStep& Out);

	FClimbTuning Tuning;
	FClimbRoute Route;
	FClimberInfo Climber;
	EClimbState State = EClimbState::None;
	float Height = 0.0f;
	float FallFromM = 0.0f;
	bool bCrownReported = false;
};
