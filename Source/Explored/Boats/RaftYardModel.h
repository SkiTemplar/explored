#pragma once

#include "CoreMinimal.h"

#include "Boats/BoatModel.h"
#include "Boats/HullAssemblyModel.h"

/**
 * Con qué se unen dos piezas de la balsa (GDD v2 §3.17). La unión es lo que se
 * rompe: la madera aguanta, lo que cede es la atadura o los clavos.
 */
enum class ERaftJointKind : uint8
{
	/** Cordel de fibra: barato y de las primeras horas; se gasta pronto. */
	Fiber,
	/** Cuerda: flexible; absorbe bien los golpes. */
	Rope,
	/** Clavos: rígidos; resisten el roce, pero un golpe seco raja la madera alrededor. */
	Nails,
	Count
};

EXPLORED_API const TCHAR* LexToString(ERaftJointKind Kind);

/** Datos de juego de cada tipo de unión. */
struct EXPLORED_API FRaftJointSpec
{
	ERaftJointKind Kind = ERaftJointKind::Rope;
	/** Objeto que se gasta al hacer la unión y al repararla (Content/Data/items.json). */
	const TCHAR* ItemId = TEXT("");
	int32 ItemsPerJoint = 1;
	int32 ItemsPerRepair = 1;
	/** Divisor del desgaste por roce (arrastre sin rodillos, varadas). */
	float WearToughness = 1.0f;
	/** Divisor del daño por golpe (encallar, chocar). */
	float ImpactToughness = 1.0f;
	/** Salud que devuelve cada reparación (0–1). */
	float RepairPerAction = 0.5f;
};

/** Unión entre dos piezas del casco (índices en FHullAssemblyModel::GetPieces). */
struct EXPLORED_API FRaftJoint
{
	int32 PieceA = INDEX_NONE;
	int32 PieceB = INDEX_NONE;
	ERaftJointKind Kind = ERaftJointKind::Rope;
	/** 1 intacta, 0 rota. Una unión rota que no suelta nada (hay otras que sujetan) se puede reparar. */
	float Health01 = 1.0f;

	bool IsBroken() const { return Health01 <= 0.0f; }
};

/** Suelo del camino de botadura. */
enum class ELaunchSurface : uint8
{
	/** Arena seca de la playa alta. */
	Sand,
	/** Arena mojada de la franja intermareal: más firme. */
	WetSand,
	Grass,
	Rock,
	/** Rampa de tablones: poco roce y casi no gasta. */
	PlankRamp,
	Count
};

EXPLORED_API const TCHAR* LexToString(ELaunchSurface Surface);

struct EXPLORED_API FLaunchSurfaceSpec
{
	ELaunchSurface Surface = ELaunchSurface::Sand;
	/** Coeficiente de rozamiento de la madera arrastrada. */
	float Friction = 0.5f;
	/** Desgaste de las uniones del fondo por kN·m de carga × distancia arrastrada (salud / kN·m, repartida entre ellas). */
	float WearPerKNm = 0.1f;
};

/** Tramo recto del camino de botadura. */
struct EXPLORED_API FLaunchSegment
{
	ELaunchSurface Surface = ELaunchSurface::Sand;
	/** Largo medido a lo largo del suelo en planta (cm). */
	float LengthCm = 100.0f;
	/** Lo que baja el suelo en el tramo (cm; positivo = cuesta abajo hacia el mar). */
	float DropCm = 0.0f;
};

/**
 * Camino de la balsa desde donde se construye hasta el agua: una línea en
 * planta con tramos de suelo. La posición S (cm) es la del centro del casco a
 * lo largo del camino; S = 0 es el principio.
 */
struct EXPLORED_API FLaunchPath
{
	/** Principio del camino (cm, mundo); Z es la altura del suelo ahí. */
	FVector StartCm = FVector::ZeroVector;
	/** Rumbo del camino (°): la proa del casco mira hacia aquí (hacia el mar). */
	float YawDeg = 0.0f;
	TArray<FLaunchSegment> Segments;
	/** Altura del agua (cm, mundo) con la marea del momento. */
	float WaterLevelZCm = -1.0e6f;

	float TotalLengthCm() const;
	/** Altura del suelo en S (cm, mundo), interpolada dentro del tramo. */
	float GroundZAt(float S) const;
	/** Índice del tramo que contiene S (el siguiente en un borde exacto); INDEX_NONE si no hay tramos. */
	int32 SegmentIndexAt(float S) const;
	/** Pendiente en S (radianes; positiva = cuesta abajo hacia el mar). */
	float SlopeRadAt(float S) const;
	/** Profundidad del agua sobre el suelo en S (cm; ≤ 0 en seco). */
	float WaterDepthAt(float S) const { return WaterLevelZCm - GroundZAt(S); }
	FVector WorldAt(float S) const;
};

/** Dónde está la balsa del astillero. */
enum class ERaftYardState : uint8
{
	/** En tierra: estable, no le afectan las olas. Solo se mueve empujándola. */
	Ashore,
	/** A flote: navega con FBoatModel (deriva y cabecea salvo que se amarre). */
	Afloat
};

EXPLORED_API const TCHAR* LexToString(ERaftYardState State);

/** Lo que ha pasado con las uniones por un golpe, un roce o un daño directo. */
struct EXPLORED_API FRaftDamageReport
{
	int32 JointsDamaged = 0;
	int32 JointsBroken = 0;
	/** Piezas que se han soltado del casco (ya no están en GetHull): el motor las deja flotar o caer. */
	TArray<FHullPiece> Released;

	void Append(const FRaftDamageReport& Other);
};

/** Resultado de un empujón. */
struct EXPLORED_API FRaftPushReport
{
	float MovedCm = 0.0f;
	/** La balsa ha pasado todo el empujón (o su final) sobre rodillos. */
	bool bOnRollers = false;
	/** Ha llegado al final del camino sin flotar. */
	bool bAtPathEnd = false;
	/** Ha flotado: el estado pasa a Afloat y hay que crear el barco (MakeBoat). */
	bool bLaunched = false;
	/** Fracción del peso que sostiene el agua al final del empujón (0–1). */
	float WaterSupport01 = 0.0f;
	FRaftDamageReport Damage;
};

/**
 * Casco por piezas tal como se guarda (GDD v2 §3.14 y §3.17): piezas y uniones.
 * Las cargas y los pasajeros no se guardan, porque salen del inventario del barco
 * y de quién va a bordo al cargar.
 */
struct EXPLORED_API FRaftHullSaveData
{
	TArray<FHullPiece> Pieces;
	/** Índices de pieza en Pieces. */
	TArray<FRaftJoint> Joints;

	bool IsEmpty() const { return Pieces.Num() == 0; }
};

/**
 * Modelo puro del astillero de balsas (GDD v2 §3.17): uniones entre piezas,
 * botadura desde tierra (arrastre, rodillos, rampa) y daño por roce y golpes.
 *
 * - La forma y la flotación son de FHullAssemblyModel (lo lleva dentro); la
 *   navegación, la deriva, el oleaje y el amarre, de FBoatModel. Este modelo
 *   solo añade lo que ocurre entre las piezas y en tierra.
 * - En tierra la balsa es estable. Se mueve con Push: rozamiento de Coulomb
 *   contra el suelo del tramo, la pendiente ayuda o frena y el agua sostiene
 *   una fracción del peso según la profundidad frente al calado. Flota cuando
 *   el agua sostiene todo el peso.
 * - Los rodillos son troncos atravesados en el camino. Si hay al menos uno
 *   bajo cada mitad del casco, rueda (resistencia de rodadura, sin desgaste) y
 *   los rodillos de debajo avanzan la mitad que el casco, así que se quedan
 *   atrás y hay que volver a ponerlos delante.
 * - El desgaste es de Archard: carga × distancia, repartido entre las uniones
 *   que tocan el suelo. Un golpe daña las uniones cercanas al punto de impacto.
 * - Una unión rota suelta las piezas que ya no están unidas al grupo principal
 *   (el de más masa). Las sueltas salen del casco y el motor las deja flotar.
 *
 * Paso fijo de 1/60 s como FBoatModel: el resultado no depende del ritmo de
 * fotogramas. Sin aleatoriedad.
 */
class EXPLORED_API FRaftYardModel
{
public:
	/** Los mismos que FBoatModel: gravedad y paso fijo. */
	static constexpr float Gravity = FBoatModel::Gravity;
	static constexpr float FixedStepS = FBoatModel::FixedStepS;
	static constexpr float MaxFrameS = FBoatModel::MaxFrameS;
	/** Empuje horizontal sostenido de una persona (N). */
	static constexpr float PushForcePerPersonN = 300.0f;
	/** Resistencia a la rodadura sobre rodillos de tronco en arena. */
	static constexpr float RollingResistance = 0.05f;
	/** Hueco máximo entre dos piezas para poder unirlas (cm). */
	static constexpr float MaxJointGapCm = 5.0f;
	/** Una pieza toca el suelo si su cara inferior está a menos de esto de la quilla (cm). */
	static constexpr float BottomContactToleranceCm = 2.0f;
	/** Daño de un golpe por cada m/s por encima de FBoatModel::SafeImpactSpeedCmS en la unión más cercana. */
	static constexpr float ImpactDamagePerMS = 0.25f;
	/** Frenado del agua en la orilla (1/s por unidad de peso sostenido). */
	static constexpr float ShallowWaterDragPerS = 0.5f;

	/** Tope de piezas de un casco guardado: un guardado manipulado no puede dejar Evaluate calculando minutos. */
	static constexpr int32 MaxSavedPieces = 256;
	/** Tope de cada coordenada del centro y de cada lado de una pieza guardada (cm). */
	static constexpr float MaxSavedExtentCm = 5000.0f;

	static const FRaftJointSpec& JointSpec(ERaftJointKind Kind);
	static const FLaunchSurfaceSpec& SurfaceSpec(ELaunchSurface Surface);
	static float PushForceN(int32 People) { return PushForcePerPersonN * FMath::Max(People, 0); }

	// --- Construcción

	int32 AddPiece(const FHullPiece& Piece);
	/** Desmonta una pieza: se quitan sus uniones y se renumeran las demás. */
	bool RemovePiece(int32 Index);
	/**
	 * Une dos piezas. INDEX_NONE si alguna no existe, son la misma, ya están
	 * unidas o hay más de MaxJointGapCm de hueco entre ellas.
	 */
	int32 AddJoint(int32 PieceA, int32 PieceB, ERaftJointKind Kind);
	void AddLoad(const FHullLoad& Load);
	void ClearLoads();

	const FHullAssemblyModel& GetHull() const { return Hull; }
	const TArray<FRaftJoint>& GetJoints() const { return Joints; }
	/** Hidrostática del casco actual (se recalcula solo cuando cambia). */
	const FHullHydrostatics& GetHydrostatics() const;
	/** Masa de piezas y cargas (kg). */
	float TotalMassKg() const;
	/** Hueco entre las cajas de dos piezas (cm; 0 si se tocan o se solapan). */
	static float GapCm(const FHullPiece& A, const FHullPiece& B);

	// --- Integridad

	/** Salud media de las uniones (1 sin uniones). */
	float Integrity01() const;
	/** Daño equivalente para FBoatModel (1 − integridad): se sincroniza con Repair/ApplyDamage. */
	float HullDamage01() const { return 1.0f - Integrity01(); }
	/** La pieza toca el suelo por debajo (su cara inferior está en la quilla de las piezas con volumen): solo sus uniones sufren el roce. Remos, pala y vela nunca. */
	bool IsBottomPiece(int32 Index) const;

	/** Roce: carga × distancia (N·m) contra un suelo. FBoatModel lo acumula varado en GroundScrapeWorkNm. */
	FRaftDamageReport ApplyScrapeWork(float WorkNm, ELaunchSurface Surface);
	/** Golpe a SpeedCmS en la dirección (marco del casco, X proa) de FBoatState::LastImpactDirection. */
	FRaftDamageReport ApplyImpact(float SpeedCmS, const FVector2D& DirectionHull);
	/** Daño directo a una unión (tiburón, ciclón, hacha). */
	FRaftDamageReport DamageJoint(int32 JointIndex, float Amount01);
	/** Una reparación: devuelve RepairPerAction de salud. False si no existe o ya está intacta. Quien llama gasta ItemsPerRepair. */
	bool RepairJoint(int32 JointIndex);
	/** Suelta las piezas que no están unidas al grupo principal (también al botar: una pieza sin atar se va flotando). */
	TArray<FHullPiece> ReleaseLoosePieces();

	// --- Astillero y botadura

	/** Pone la balsa en tierra sobre el camino con el centro en S (se recorta al camino). */
	void PlaceOnPath(const FLaunchPath& InPath, float CenterS);
	/** La balsa se ha construido en el agua: ya está a flote. */
	void SetAfloat();
	ERaftYardState GetState() const { return State; }
	const FLaunchPath& GetPath() const { return Path; }
	float GetCenterS() const { return CenterS; }
	/** Velocidad a lo largo del camino (cm/s). */
	float GetVelocityCmS() const { return VelocityCmS; }
	/** Eslora del casco a lo largo del camino (cm): caja de todas las piezas. */
	float HullLengthCm() const;

	/** Pone un rodillo atravesado en S. False si no está en el camino o la balsa ya no está en tierra. */
	bool PlaceRoller(float S);
	/** Recoge un rodillo. False si no existe o está bajo el casco (lo aplasta la balsa). */
	bool TakeRoller(int32 Index);
	const TArray<float>& GetRollers() const { return Rollers; }
	/** Hay rodillo bajo cada mitad del casco: rueda en lugar de arrastrarse. */
	bool IsOnRollers() const;

	/** Fracción del peso que sostiene el agua en la posición actual (0–1). */
	float WaterSupport01() const;
	/** Fuerza de empuje para arrancarla desde parada ahora mismo (N; 0 si se desliza sola). */
	float RequiredPushForceN() const;

	/** Empuja (N, positivo hacia el mar) durante DeltaSeconds. Sin efecto si no está en tierra. */
	FRaftPushReport Push(float PushForceN, float DeltaSeconds);

	// --- Guardado

	/** Piezas y uniones en el orden actual, con la salud de cada unión. */
	FRaftHullSaveData ToHullSaveData() const;
	/**
	 * Rehace el casco de un guardado, en tierra y sin camino (quien llama pone
	 * SetAfloat o PlaceOnPath). Descarta, sin tocar el resto, las piezas con
	 * valores no finitos o más allá de MaxSavedExtentCm, las que pasan de
	 * MaxSavedPieces y las uniones que ya no se podrían hacer (pieza descartada o
	 * inexistente, repetida, consigo misma, tipo desconocido o piezas separadas).
	 * Una salud no finita cuenta como unión rota; las demás se recortan a 0–1.
	 * Con un guardado válido, ToHullSaveData devuelve exactamente lo guardado.
	 */
	static FRaftYardModel FromHullSaveData(const FRaftHullSaveData& Data, int32* OutDiscarded = nullptr);

	/** Ficha de navegación del casco actual (FHullAssemblyModel::ToBoatDefinition). */
	FBoatDefinition ToBoatDefinition() const;
	/** El barco que sale de la botadura: en el punto del camino, flotando en su calado y con la arrancada del empujón. */
	FBoatModel MakeBoat() const;

private:
	void Substep(float H, float PushForceN, FRaftPushReport& Report);
	void Invalidate() { bHydroDirty = true; }
	/** Reparte Damage01 entre las uniones con pesos (mismo tamaño que Joints) y suelta lo que quede suelto. */
	FRaftDamageReport ApplyJointDamage(const TArray<float>& Damage01);
	float BottomZ() const;
	void HullBoundsX(double& OutMinX, double& OutMaxX) const;

	FHullAssemblyModel Hull;
	TArray<FRaftJoint> Joints;
	TArray<float> Rollers;
	FLaunchPath Path;
	ERaftYardState State = ERaftYardState::Ashore;
	float CenterS = 0.0f;
	float VelocityCmS = 0.0f;
	float PendingTimeS = 0.0f;

	mutable FHullHydrostatics CachedHydro;
	mutable bool bHydroDirty = true;
};
