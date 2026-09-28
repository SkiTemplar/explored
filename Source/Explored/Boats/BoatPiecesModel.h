#pragma once

#include "CoreMinimal.h"

#include "Boats/BoatTypes.h"

/**
 * Piezas de casco del catálogo de barcos por piezas (biblia 02 §8.1). Los ids
 * de Content/Data/boat_pieces.json («type») son estos nombres; Tools/DataCheck
 * los compara.
 */
enum class EBoatPieceType : uint8
{
	/** Columna del casco: primera pieza obligatoria; ancla las cuadernas y el mástil. */
	Keel,
	/** Costilla transversal: da la manga y el puntal, y sostiene los tablones. */
	Frame,
	/** Piel exterior: el hueco que encierra es la flotabilidad. */
	HullPlank,
	/** Suelo interior: espacio de carga. */
	Deck,
	/** Palo: sin él no se iza la vela. */
	Mast,
	/** Superficie vélica (Y × Z del tamaño, m²). */
	Sail,
	/** Flotador del balancín: flotabilidad aparte y menos escora hacia su banda. */
	Outrigger,
	/** Timón o espadilla. */
	Rudder,
	/** Banco de remo: un tripulante rema desde él. */
	RowingBench,
	/** Amarre o noray: sin él no se puede atar el barco. */
	Mooring,
	Count
};

EXPLORED_API const TCHAR* LexToString(EBoatPieceType Type);
/** Nombre del enum a tipo (el campo «type» de boat_pieces.json). False si no existe. */
EXPLORED_API bool LexFromString(EBoatPieceType& OutType, const TCHAR* Name);

/**
 * Datos físicos de cada tipo de pieza a su tamaño por defecto. La masa y lo
 * que aporta la pieza escalan con su dimensión principal (ver
 * FBoatPiecesModel::ScaleOf): un tablón el doble de largo pesa y encierra el
 * doble.
 */
struct EXPLORED_API FBoatPieceSpec
{
	EBoatPieceType Type = EBoatPieceType::Keel;
	/** Id de Content/Data/boat_pieces.json. */
	const TCHAR* Id = TEXT("");
	/** Tamaño por defecto (cm): X eslora, Y manga, Z altura. */
	FVector DefaultSizeCm = FVector(100.0, 10.0, 10.0);
	float MassKg = 0.0f;
	/** Volumen de flotación (L, biblia 02 §8.2 «VolumenFlotacionLitros»): tablones y flotador. */
	float BuoyancyLiters = 0.0f;
	/** Espacio de carga (kg): la cubierta. */
	float CargoKg = 0.0f;
	/** Superficie vélica (m²): la vela. */
	float SailAreaM2 = 0.0f;
	/** Integridad inicial (1–100) de las uniones de la pieza, como cualquier pieza de construcción. */
	int32 BaseIntegrity = 100;
};

/** Una pieza colocada. Marco del casco en cm: X hacia proa, Y hacia estribor, Z arriba desde la quilla. */
struct EXPLORED_API FBoatPiece
{
	EBoatPieceType Type = EBoatPieceType::Keel;
	FVector CenterCm = FVector::ZeroVector;
	/** Tamaño (cm); cualquier componente ≤ 0 o no finita toma la del tipo. */
	FVector SizeCm = FVector::ZeroVector;
};

/** Unión cuaderna–tablón (biblia 02 §8.3): lo que un golpe rompe y abre una vía de agua. */
struct EXPLORED_API FBoatHullJoint
{
	/** Índices en FBoatPiecesModel::GetPieces. */
	int32 Frame = INDEX_NONE;
	int32 Plank = INDEX_NONE;
	/** Punto de la unión en el marco del casco (cm): la cuaderna en X, el tablón en Y y Z. */
	FVector LocationCm = FVector::ZeroVector;
	/** 0 rota (brecha abierta) … MaxIntegrity. */
	float Integrity = 100.0f;
	float MaxIntegrity = 100.0f;

	bool IsBreached() const { return Integrity <= 0.0f; }
};

/** Qué le falta a un casco para ser un barco (máscara de bits). */
enum class EBoatHullIssue : uint32
{
	None = 0,
	/** Sin quilla: no hay casco que empezar. */
	NoKeel = 1u << 0,
	/** Sin cuadernas. */
	NoFrames = 1u << 1,
	/** Sin tablones: nada encierra aire. */
	NoPlanks = 1u << 2,
	/** Una cuaderna no toca la quilla ni ningún tablón (un botalón se ata sobre los tablones). */
	FrameUnattached = 1u << 3,
	/** Un tablón no descansa en ninguna cuaderna. */
	PlankUnsupported = 1u << 4,
	/** Un mástil no está plantado sobre la quilla ni sobre un travesaño. */
	MastUnstepped = 1u << 5,
	/** Vela sin mástil. */
	SailWithoutMast = 1u << 6,
	/** Un balancín no está unido a ninguna cuaderna (el botalón que lo sujeta). */
	OutriggerUnattached = 1u << 7
};

EXPLORED_API const TCHAR* LexToString(EBoatHullIssue Issue);

/** Lo que pasa con lo armado en agua quieta, del mejor al peor caso. */
enum class EBoatHullVerdict : uint8
{
	/** Flota con margen. */
	Floats,
	/** Pasa del 95 % de la flotabilidad: embarca agua (2 kg/s) mientras dure el exceso. */
	TakingWater,
	/** Escora estática por encima de 55° (vuelca a los 4 s) o sin estabilidad inicial. */
	Capsizes,
	/** Pasa del 115 % de la flotabilidad: se hunde. */
	Sinks,
	/** Al casco le falta algo (ver EBoatHullIssue): no se puede botar. */
	Incomplete
};

EXPLORED_API const TCHAR* LexToString(EBoatHullVerdict Verdict);

/** Masa puntual a bordo: tripulante, carga o agua. Marco del casco (cm). */
struct EXPLORED_API FBoatLoadMass
{
	float MassKg = 0.0f;
	FVector CenterCm = FVector::ZeroVector;
};

/** Lo que lleva el barco en un momento dado. */
struct EXPLORED_API FBoatLoadout
{
	TArray<FBoatLoadMass> Masses;
	/** Agua embarcada (kg): en el fondo, sobre el eje. */
	float WaterInHullKg = 0.0f;
	bool bSailRaised = false;
	/** Viento de través sobre la vela (m/s, positivo = empuja hacia estribor). */
	float BeamWindMS = 0.0f;
};

/** Hidrostática y estabilidad de lo armado (biblia 02 §8.2). Alturas desde la quilla. */
struct EXPLORED_API FBoatHullReport
{
	EBoatHullVerdict Verdict = EBoatHullVerdict::Incomplete;
	/** Máscara de EBoatHullIssue. */
	uint32 Issues = 0;

	float StructureMassKg = 0.0f;
	/** Estructura + cargas + agua embarcada. */
	float TotalMassKg = 0.0f;
	/** Σ litros de flotación × 1,025 (kg). */
	float MaxBuoyancyKg = 0.0f;
	/** TotalMassKg / MaxBuoyancyKg. */
	float LoadRatio = 0.0f;
	/** Espacio de carga de las cubiertas (kg). */
	float DeckCargoKg = 0.0f;

	float LengthCm = 0.0f;
	float BeamCm = 0.0f;
	/** Puntal: la cuaderna más alta (cm). */
	float DepthCm = 0.0f;
	/** Área de flotación (m²): volumen de flotación / puntal (casco prismático). */
	float WaterplaneAreaM2 = 0.0f;
	/** Calado de diseño: el 60 % del puntal (cm). */
	float DesignDraftCm = 0.0f;
	/** Calado de equilibrio con la masa total, sin pasar del puntal (cm). */
	float EquilibriumDraftCm = 0.0f;
	/** Agua embarcada que deja la borda a ras (kg). */
	float SwampWaterKg = 0.0f;
	/** Entrada de agua por sobrecarga (kg/s): 2 kg/s por encima del 95 %. */
	float OverloadIngressKgS = 0.0f;

	float SailAreaM2 = 0.0f;
	float KGCm = 0.0f;
	float KBCm = 0.0f;
	float BMCm = 0.0f;
	/** Altura metacéntrica transversal del casco sin balancín (cm); ≤ 0 = sin estabilidad inicial. */
	float GMCm = 0.0f;
	/** Lo que suma el flotador hundido: inercia de su flotación a su brazo / volumen desplazado (cm). */
	float OutriggerBMCm = 0.0f;
	/** Altura metacéntrica con la que aguanta la escora actual: con el flotador hundido o sin él. */
	float EffectiveGMCm = 0.0f;
	/** Momento escorante tras el balancín (N·m, positivo = estribor abajo), sin el peso del flotador. */
	float HeelingMomentNm = 0.0f;
	/** Escora estática (°, positiva = estribor abajo). */
	float HeelDeg = 0.0f;

	bool IsComplete() const { return Issues == 0; }
	bool IsAfloat() const { return Verdict == EBoatHullVerdict::Floats || Verdict == EBoatHullVerdict::TakingWater; }
};

/** Lo que ha hecho un golpe a las uniones. */
struct EXPLORED_API FBoatImpactReport
{
	int32 JointsDamaged = 0;
	/** Uniones que han llegado a 0 con este golpe (brechas nuevas). */
	int32 NewBreaches = 0;
	float IntegrityLost = 0.0f;
};

/**
 * Vuelco por escora sostenida (biblia 02 §8.2): por encima de 55° durante más
 * de 4 s, o de inmediato por encima de 75°. Bajar de 55° reinicia la cuenta.
 */
struct EXPLORED_API FBoatCapsizeTimer
{
	static constexpr float SustainedHeelDeg = 55.0f;
	static constexpr float SustainedSeconds = 4.0f;
	static constexpr float InstantHeelDeg = 75.0f;

	float OverSeconds = 0.0f;
	bool bCapsized = false;

	/** Avanza DeltaSeconds con la escora actual; true si vuelca (y se queda volcado hasta Reset). */
	bool Step(float HeelDeg, float DeltaSeconds);
	void Reset() { OverSeconds = 0.0f; bCapsized = false; }
};

/**
 * Modelo puro del barco por piezas (biblia 02 §8): el casco es la suma de
 * sus piezas y de ahí salen la flotación, la estabilidad y la ficha con la
 * que navega FBoatModel. Las cuatro embarcaciones canónicas son planos
 * (Blueprint) sobre el mismo catálogo.
 *
 * - Flotabilidad máxima: Σ litros de tablones y flotadores × 1,025.
 * - El casco es prismático: área de flotación = volumen / puntal, así que el
 *   calado es masa / (ρ · área) igual que en FBoatModel.
 * - Estabilidad inicial de un casco de caja: KB = T/2, BM = A·B² / (12·∇),
 *   GM = KB + BM − KG; escora estática atan(M / (Δ·g·GM)).
 * - Balancín: escorando hacia su banda lo hunde (suma la inercia de su
 *   flotación, A·brazo²) y quita el 60 % del momento (biblia 02 §8.2); hacia
 *   la otra se levanta y solo su peso sujeta hasta despegarse. Un casco que
 *   solo no aguanta de pie (GM < 0) navega con él mientras el momento no
 *   pase del peso del flotador por su brazo.
 * - Integridad: una unión por cada par cuaderna–tablón que se toca. Un golpe
 *   por encima de FBoatModel::SafeImpactSpeedCmS daña la unión más cercana
 *   al punto de impacto y, menos, las de alrededor; cada unión a 0 es una
 *   vía de agua de 0,5 L/s.
 *
 * Determinista y sin azar. No incluye nada de Unreal más allá de CoreMinimal.
 */
class EXPLORED_API FBoatPiecesModel
{
public:
	/** Agua de mar (kg/L) y gravedad. */
	static constexpr float SeaWaterKgPerLiter = 1.025f;
	static constexpr float Gravity = 9.81f;
	static constexpr float AirDensity = 1.225f;
	/** Calado de diseño de un plano canónico: fracción del puntal. */
	static constexpr float DesignDraftFraction = 0.6f;
	/** Carga a partir de la cual embarca agua y a partir de la cual se hunde (fracción de la flotabilidad). */
	static constexpr float TakingWaterLoadRatio = 0.95f;
	static constexpr float SinkLoadRatio = 1.15f;
	/** Entrada de agua mientras dura la sobrecarga (kg/s). */
	static constexpr float OverloadIngressKgS = 2.0f;
	/** Vía de agua por cada unión rota (L/s). */
	static constexpr float BreachLeakLitersPerS = 0.5f;
	/** Fracción del momento escorante hacia la banda del balancín que se queda (quita el 60 %). */
	static constexpr float OutriggerHeelFactor = 0.4f;
	/** Hueco máximo entre dos piezas para que se toquen (cm). */
	static constexpr float ContactToleranceCm = 5.0f;
	/** Integridad que quita un golpe en la unión más cercana por cada m/s por encima de la velocidad segura. */
	static constexpr float ImpactIntegrityPerMS = 60.0f;
	/** Radio (cm) en el que un golpe reparte daño alrededor de la unión más cercana. */
	static constexpr float ImpactRadiusCm = 120.0f;
	/** Altura del centro de masas de un tripulante sentado sobre la quilla (cm). */
	static constexpr float CrewSeatedHeightCm = 30.0f;
	/** Coeficiente de fuerza lateral de la vela con viento de través. */
	static constexpr float SailSideForceCoefficient = 1.0f;
	/** Tamaño mínimo y máximo de una dimensión de pieza (cm): lo demás se recorta. */
	static constexpr float MinPieceSizeCm = 1.0f;
	static constexpr float MaxPieceSizeCm = 5000.0f;
	/** Tope de piezas de un casco (un plano canónico lleva menos de 40). */
	static constexpr int32 MaxPieces = 256;

	static const FBoatPieceSpec& Spec(EBoatPieceType Type);

	/** Plano canónico de una embarcación (biblia 02 §8.4). */
	static FBoatPiecesModel Blueprint(EBoatType Type);
	/** Cuántas piezas de un tipo lleva un plano (lo que lista boats.json). */
	static int32 BlueprintCount(EBoatType Type, EBoatPieceType Piece);
	/** Piezas del Albatros que exige un plano (GDD §4.3): máscara de EShipPart (bit i = parte i). */
	static uint32 RequiredShipPartsMask(EBoatType Type);
	/** Todas las piezas del Albatros (las cuatro de EShipPart). */
	static constexpr uint32 AllShipPartsMask = 0xFu;
	/** Se puede empezar el plano con las partes del Albatros recuperadas (misma máscara). */
	static bool HasShipPartsFor(EBoatType Type, uint32 RecoveredShipPartsMask);

	// --- Montaje

	/** Añade una pieza (tamaño saneado) y rehace las uniones; INDEX_NONE si el tipo no existe o se pasa de MaxPieces. */
	int32 AddPiece(const FBoatPiece& Piece);
	/** Quita una pieza; las uniones que no la tocan conservan su integridad. */
	bool RemovePiece(int32 Index);

	const TArray<FBoatPiece>& GetPieces() const { return Pieces; }
	const TArray<FBoatHullJoint>& GetJoints() const { return Joints; }
	int32 CountOf(EBoatPieceType Type) const;
	/** Con mástil y vela se puede izar (biblia 02 §8.1: el mástil habilita SetSailRaised). */
	bool CanRaiseSail() const { return CountOf(EBoatPieceType::Mast) > 0 && CountOf(EBoatPieceType::Sail) > 0; }
	/** Sin amarre no se puede atar el barco (biblia 02 §8.4). */
	bool CanMoor() const { return CountOf(EBoatPieceType::Mooring) > 0; }
	/** Tripulantes que pueden remar a la vez: uno por banco. */
	int32 RowingSeats() const { return CountOf(EBoatPieceType::RowingBench); }

	/** Tamaño efectivo (cm) con los valores por defecto del tipo. */
	static FVector EffectiveSizeCm(const FBoatPiece& Piece);
	/** Factor por el que escala todo lo que aporta la pieza (masa, litros, carga, vela). */
	static float ScaleOf(const FBoatPiece& Piece);
	static float PieceMassKg(const FBoatPiece& Piece);
	static float PieceBuoyancyLiters(const FBoatPiece& Piece);

	/** Máscara de EBoatHullIssue: 0 = casco completo. */
	uint32 FindIssues() const;

	/** Hidrostática y estabilidad con una carga dada. */
	FBoatHullReport Evaluate(const FBoatLoadout& Loadout) const;
	/** Con un tripulante sentado en el eje, sin carga ni vela. */
	FBoatHullReport EvaluateWithCrew() const;

	/**
	 * Ficha de navegación para FBoatModel: la física (eslora, manga, puntal,
	 * masa, flotación, carga, vela, altura metacéntrica) sale de las piezas; el
	 * tacto de mando (paladas, amortiguamientos, daño) se copia de Tuning.
	 */
	FBoatDefinition ToBoatDefinition(const FBoatDefinition& Tuning) const;

	// --- Integridad (biblia 02 §8.3)

	/** Golpe a SpeedCmS en un punto del casco (cm, marco del casco). Por debajo de la velocidad segura no hace nada. */
	FBoatImpactReport ApplyImpact(float SpeedCmS, const FVector& PointCm);
	/** Repara una unión (puntos de integridad). False si no existe, ya está entera o Points no es positivo. */
	bool RepairJoint(int32 JointIndex, float Points);
	/** Uniones rotas: cada una es una vía de agua. */
	int32 BreachCount() const;
	/** Entrada de agua por las brechas (kg/s). */
	float LeakKgS() const;
	/** Integridad media de las uniones (0–1; 1 sin uniones). */
	float Integrity01() const;

private:
	void RebuildJoints();

	TArray<FBoatPiece> Pieces;
	TArray<FBoatHullJoint> Joints;
};
