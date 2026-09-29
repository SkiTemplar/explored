#pragma once

#include "CoreMinimal.h"

#include "Boats/BoatModel.h"

/**
 * Piezas con las que el jugador arma un casco (GDD v2 §3.14). No hay «construir
 * barco»: cada pieza aporta masa, volumen y posición, y el modelo hidrostático
 * decide si lo armado flota, escora, vuelca o se hunde.
 */
enum class EHullPieceType : uint8
{
	/** Tronco para balsa: flota por sí mismo y es pesado. */
	Log,
	/** Tablón fino: cubierta o larguero, flota poco. */
	Plank,
	/** Haz de cañas de bambú gruesas: huecas, flotan mucho para lo que pesan. */
	Bamboo,
	/** Flotador sellado (calabaza, barril): casi todo aire; balancín o reserva de flotación. */
	Float,
	/** Mástil vertical: habilita la vela y sube el centro de masas. */
	Mast,
	/** Vela: sin volumen; su superficie es Y × Z del tamaño (m²). Solo empuja con un mástil. */
	Sail,
	/** Par de remos: propulsión para un tripulante. */
	Oars,
	/** Pala (canalete): propulsión para un tripulante, menos que los remos. */
	Paddle,
	Count
};

EXPLORED_API const TCHAR* LexToString(EHullPieceType Type);

/** Datos físicos de cada tipo de pieza. Medidas en cm, densidad en kg/m³. */
struct EXPLORED_API FHullPieceSpec
{
	EHullPieceType Type = EHullPieceType::Log;
	/** Tamaño por defecto: X eslora, Y manga, Z altura. */
	FVector DefaultSizeCm = FVector(100.0, 10.0, 10.0);
	/** Densidad efectiva del volumen de la pieza (0 = masa fija, sin volumen que flote). */
	float DensityKgM3 = 0.0f;
	/** Masa de las piezas sin volumen (remos, pala) o por m² de vela. */
	float FixedMassKg = 0.0f;
	/** Su volumen desplaza agua. */
	bool bBuoyant = false;
};

/** Una pieza colocada. Marco del casco en cm: X hacia proa, Y hacia estribor, Z arriba. */
struct EXPLORED_API FHullPiece
{
	EHullPieceType Type = EHullPieceType::Log;
	/** Centro de la pieza (cm). */
	FVector CenterCm = FVector::ZeroVector;
	/** Tamaño alineado con los ejes (cm); cualquier componente ≤ 0 toma la del tipo. */
	FVector SizeCm = FVector::ZeroVector;
};

/** Carga o pasajero: masa puntual en el marco del casco. */
struct EXPLORED_API FHullLoad
{
	float MassKg = 0.0f;
	/** Centro de masas de la carga (cm). Un pasajero de pie tiene el suyo ~90 cm sobre la cubierta. */
	FVector CenterCm = FVector::ZeroVector;
	/** Un pasajero además puede remar, palear o llevar la vela. */
	bool bPassenger = false;
};

/** Qué le pasa a lo armado al echarlo al agua, del mejor al peor caso. */
enum class EHullVerdict : uint8
{
	/** Flota nivelada (escora y asiento por debajo de LevelToleranceDeg). */
	Floats,
	/** Flota, pero escorada o con asiento. */
	Lists,
	/** La borda queda a menos de MinFreeboardCm del agua en algún punto: embarca agua. */
	Swamps,
	/** No hay equilibrio estable por debajo de CapsizeHeelDeg: vuelca. */
	Capsizes,
	/** Pesa más que el agua que puede desplazar entera: se hunde. */
	Sinks,
	/** No hay ninguna pieza que flote. */
	Empty
};

EXPLORED_API const TCHAR* LexToString(EHullVerdict Verdict);

/** Resultado hidrostático en agua quieta. Alturas desde la quilla (Z mínima de las piezas que flotan). */
struct EXPLORED_API FHullHydrostatics
{
	EHullVerdict Verdict = EHullVerdict::Empty;

	/** Piezas + cargas (kg). */
	float TotalMassKg = 0.0f;
	/** Solo piezas (kg). */
	float StructureMassKg = 0.0f;
	/** Peso de agua que desplazaría todo el volumen sumergido (kg). */
	float MaxBuoyancyKg = 0.0f;
	/** TotalMassKg / MaxBuoyancyKg: por encima de 1 se hunde. */
	float LoadRatio = 0.0f;

	/** Volumen sumergido en equilibrio (m³): desplazamiento / densidad del agua. */
	float DisplacementM3 = 0.0f;
	/** Calado adrizado: de la quilla a la flotación (cm). */
	float DraftCm = 0.0f;
	/** Francobordo mínimo en la escora y el asiento de equilibrio (cm); negativo = borda bajo el agua. */
	float FreeboardCm = 0.0f;
	/** Escora de equilibrio (°, positiva = estribor abajo). */
	float HeelDeg = 0.0f;
	/** Asiento de equilibrio (°, positivo = proa abajo). */
	float TrimDeg = 0.0f;

	FVector CenterOfMassCm = FVector::ZeroVector;
	/** Centro de carena adrizado (cm). */
	FVector CenterOfBuoyancyCm = FVector::ZeroVector;
	/** Altura del centro de masas sobre la quilla (cm). */
	float KGCm = 0.0f;
	/** Altura del centro de carena adrizado sobre la quilla (cm). */
	float KBCm = 0.0f;
	/** Altura del metacentro transversal sobre la quilla (cm): KG + GM. */
	float KMCm = 0.0f;
	/** Altura metacéntrica transversal adrizada (cm); negativa = el centro de masas está por encima del metacentro. */
	float GMCm = 0.0f;
	/** Altura metacéntrica longitudinal adrizada (cm). */
	float GMLongCm = 0.0f;
	/** Ángulo de estabilidad nula: a partir de ahí la escora ya no se endereza sola (°, valor absoluto). */
	float VanishingStabilityDeg = 0.0f;

	/** Flotación adrizada: área (m²), eslora y manga (cm). */
	float WaterplaneAreaM2 = 0.0f;
	float WaterlineLengthCm = 0.0f;
	float WaterlineBeamCm = 0.0f;
	/** Manga efectiva: suma de las franjas en Y que están bajo el agua, sin los huecos (un balancín no ensancha el casco) (cm). */
	float EffectiveBeamCm = 0.0f;
	/** Área frontal sumergida adrizada (m²): lo que empuja agua al avanzar. */
	float FrontalAreaM2 = 0.0f;
	/** Altura de la cubierta (lo más alto de las piezas que flotan, sin contar el mástil) sobre la quilla (cm). */
	float DeckHeightCm = 0.0f;

	bool IsAfloat() const { return Verdict == EHullVerdict::Floats || Verdict == EHullVerdict::Lists || Verdict == EHullVerdict::Swamps; }
};

/** Cómo se propulsa lo armado. */
enum class EHullPropulsion : uint8
{
	None,
	Paddle,
	Oars,
	Sail
};

EXPLORED_API const TCHAR* LexToString(EHullPropulsion Propulsion);

/** Velocidad y maniobra que salen de la propulsión y de la forma del casco. */
struct EXPLORED_API FHullPerformance
{
	EHullPropulsion Propulsion = EHullPropulsion::None;
	/** Empuje de la propulsión elegida (N). */
	float ThrustN = 0.0f;
	/** Superficie vélica útil (m²). */
	float SailAreaM2 = 0.0f;
	/** Eslora en la flotación / manga efectiva. */
	float Slenderness = 0.0f;
	/** Coeficiente de resistencia de forma: los cascos chatos empujan agua. */
	float DragCoefficient = 0.0f;
	/** Velocidad de casco (Froude 0,4): pasarla cuesta mucho más empuje (cm/s). */
	float HullSpeedCmS = 0.0f;
	/** Velocidad sostenida (cm/s). */
	float MaxSpeedCmS = 0.0f;
	/** Velocidad de giro a MaxSpeedCmS (°/s): cuanto más corto, más gira. */
	float TurnRateDegS = 0.0f;
	/** Cuánto mantiene el rumbo solo (0–1): cuanto más esbelto, más recto. */
	float CourseStability01 = 0.0f;
};

/**
 * Modelo puro del casco por piezas (GDD v2 §3.14). Cada pieza es una caja
 * alineada con los ejes del casco con su masa y su volumen; las cargas son
 * masas puntuales.
 *
 * Hidrostática exacta para cajas: para una escora α (o un asiento) se busca
 * por bisección la flotación que desplaza el peso total, recortando la
 * sección de cada caja con el plano del agua (polígono exacto, no muestreo).
 * El brazo adrizante GZ(α) es la distancia horizontal del centro de masas al
 * centro de carena; la escora de equilibrio es el cero estable de GZ más
 * cercano a 0 y la altura metacéntrica es dGZ/dα en 0. Escora y asiento se
 * resuelven por separado (desacoplados), como en la estabilidad clásica.
 *
 * Determinista y sin estado oculto: Evaluate() no depende del orden de las
 * piezas salvo por redondeo.
 */
class EXPLORED_API FHullAssemblyModel
{
public:
	/** Agua de mar (kg/m³), aire y gravedad: los mismos que FBoatModel, para que el casco no cambie al botarlo. */
	static constexpr float WaterDensity = FBoatModel::WaterDensity;
	static constexpr float AirDensity = FBoatModel::AirDensity;
	static constexpr float Gravity = FBoatModel::Gravity;
	/** Por debajo de esta escora y este asiento se considera nivelada (°). */
	static constexpr float LevelToleranceDeg = 2.0f;
	/** Francobordo mínimo (cm): por debajo, cualquier ola entra y la embarcación va anegada. */
	static constexpr float MinFreeboardCm = 2.0f;
	/** Anegada arrastra el agua embarcada: fracción de la velocidad que conserva. */
	static constexpr float SwampedSpeedFactor = 0.5f;
	/** Escora estática a partir de la cual vuelca (°, biblia 02 §8.2). */
	static constexpr float CapsizeHeelDeg = 55.0f;
	/** Masa de un pasajero tipo (kg): la del tripulante de FBoatModel. */
	static constexpr float PassengerMassKg = FBoatModel::CrewMassKg;
	/** Empuje sostenido de un tripulante con pala (N) y con un par de remos (N). */
	static constexpr float PaddleThrustN = 35.0f;
	static constexpr float OarsThrustN = 70.0f;
	/** Coeficiente medio de empuje de la vela en los rumbos navegables. */
	static constexpr float SailThrustCoefficient = 0.8f;
	/** Superficie vélica máxima que aguanta un mástil (m²). */
	static constexpr float MaxSailAreaPerMastM2 = 12.0f;
	/** Número de Froude de la velocidad de casco. */
	static constexpr float HullSpeedFroude = 0.4f;
	/** Radio de giro en esloras. */
	static constexpr float TurnRadiusLengths = 1.5f;
	/** Lo que se pueden meter una en otra dos piezas con volumen (cm, en los tres ejes): encajes y redondeo, no apilar. */
	static constexpr float MaxOverlapCm = 2.0f;

	static const FHullPieceSpec& Spec(EHullPieceType Type);

	/**
	 * Añade una pieza; devuelve su índice, o INDEX_NONE si su centro o su tamaño no son
	 * finitos o si tiene volumen y se mete más de MaxOverlapCm en otra pieza con volumen.
	 */
	int32 AddPiece(const FHullPiece& Piece);
	/** Quita una pieza; false si el índice no existe. */
	bool RemovePiece(int32 Index);
	/** Añade una carga; una con masa o centro no finitos se ignora. */
	void AddLoad(const FHullLoad& Load);
	void ClearLoads() { Loads.Reset(); }

	const TArray<FHullPiece>& GetPieces() const { return Pieces; }
	const TArray<FHullLoad>& GetLoads() const { return Loads; }

	/** Tamaño efectivo (cm) de una pieza, con los valores por defecto del tipo. */
	static FVector EffectiveSizeCm(const FHullPiece& Piece);
	/** Masa de una pieza (kg). */
	static float PieceMassKg(const FHullPiece& Piece);

	/** Hidrostática en agua quieta con las piezas y las cargas actuales. */
	FHullHydrostatics Evaluate() const;

	/** Velocidad y maniobra con un viento dado (m/s) para la vela. */
	FHullPerformance Performance(const FHullHydrostatics& Hydro, float WindSpeedMS) const;

	/**
	 * Traduce lo armado a los números con los que navega FBoatModel (eslora,
	 * manga, calado, altura metacéntrica, vela, velocidad de remo, carga
	 * máxima). Parte de la definición de la balsa para lo que las piezas no
	 * deciden (amortiguamientos, daño).
	 */
	FBoatDefinition ToBoatDefinition(const FHullHydrostatics& Hydro) const;

	/** Brazo adrizante (cm) a una escora (°): positivo endereza hacia babor. Expuesto para la curva de estabilidad. */
	float RightingArmCm(float HeelDeg) const;

private:
	TArray<FHullPiece> Pieces;
	TArray<FHullLoad> Loads;
};
