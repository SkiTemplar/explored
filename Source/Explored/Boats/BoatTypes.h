#pragma once

#include "CoreMinimal.h"

struct FOceanWaves;

/**
 * Embarcaciones del jugador en orden de progresión (GDD §8.10): balsa (lenta,
 * solo aguas someras) → canoa (rápida) → canoa con balancín y vela (mar
 * abierto) → barco «Limón» (piezas del Albatros, objetivo final opcional).
 * Los nombres coinciden con el campo «type» de Content/Data/boats.json
 * (Tools/DataCheck lo comprueba).
 */
enum class EBoatType : uint8
{
	Raft,
	Canoe,
	Outrigger,
	Limon,
	Count
};

EXPLORED_API const TCHAR* LexToString(EBoatType Type);

/** Banda del barco: babor (izquierda) o estribor (derecha) mirando a proa. */
enum class EBoatSide : uint8
{
	Port,
	Starboard
};

/** Estado de la embarcación. Solo «Wrecked» se hunde de verdad. */
enum class EBoatCondition : uint8
{
	/** Navegando con normalidad. */
	Afloat,
	/** Anegado: el casco lleno de agua flota a ras (la madera no se hunde); hay que achicar. */
	Swamped,
	/** Volcado: quilla arriba, a la deriva; hay que adrizarlo. */
	Capsized,
	/** Casco destrozado (daño 1): se hunde hasta el fondo. */
	Wrecked
};

EXPLORED_API const TCHAR* LexToString(EBoatCondition Condition);

/**
 * Definición física y de juego de un tipo de embarcación. Unidades: cm, kg,
 * segundos y grados salvo donde se indique (fuerzas en newtons, áreas en m²).
 * Las medidas salen de las mallas de Tools/Blender/props/boats.py.
 */
struct EXPLORED_API FBoatDefinition
{
	EBoatType Type = EBoatType::Raft;
	/** Nombre de la malla generada (SM_*) o vacío si aún no existe (se usa una forma básica). */
	const TCHAR* MeshName = TEXT("");

	float LengthCm = 400.0f;
	float BeamCm = 70.0f;
	/** Altura del casco de la quilla a la borda: el francobordo es HullDepthCm menos el calado. */
	float HullDepthCm = 50.0f;
	float HullMassKg = 90.0f;
	/** Coeficiente de flotación: área de la flotación / (eslora × manga). */
	float WaterplaneCoefficient = 0.6f;
	/** Carga máxima además del tripulante (kg). */
	float MaxCargoKg = 150.0f;

	/** Velocidad máxima remando sin parar en agua quieta (cm/s). */
	float MaxPaddleSpeedCmS = 250.0f;
	/** Empuje medio de una palada (N). */
	float PaddleThrustN = 55.0f;
	/** Duración de una palada (s); la siguiente no empieza hasta que acaba. */
	float StrokeDurationS = 0.7f;
	/** Brazo de la palada respecto al eje del casco (cm): cuanto mayor, más gira una palada por un solo lado. */
	float StrokeLeverCm = 55.0f;
	/** Constante de tiempo de la deriva libre (s): cuánto tarda en frenar o en igualar la corriente. */
	float CoastTimeConstantS = 12.0f;
	/** Resistencia lateral frente a la longitudinal (quilla, orza, flotador). */
	float LateralResistance = 35.0f;
	/** Constante de tiempo del giro (s): cuánto sigue girando tras una palada o un golpe de timón. */
	float YawTimeConstantS = 2.0f;
	/** Velocidad de giro con el timón a fondo navegando a MaxPaddleSpeedCmS (°/s). */
	float RudderTurnRateDegS = 18.0f;

	/** Superficie vélica (m²); 0 = sin vela. */
	float SailAreaM2 = 0.0f;
	/** Altura del centro vélico sobre la flotación (cm): brazo del momento escorante de la vela. */
	float SailCenterOfEffortCm = 0.0f;
	/** Superficie expuesta al viento del casco y el tripulante (m²): deriva con la vela arriada. */
	float WindageAreaM2 = 1.0f;

	/** Altura metacéntrica efectiva (cm): resistencia a escorar por el viento. */
	float MetacentricHeightCm = 15.0f;
	/** Periodo natural de balance (s). */
	float RollPeriodS = 1.8f;
	/** Amortiguamiento del balance (fracción del crítico): bajo = entra en resonancia con las olas. */
	float RollDamping = 0.15f;
	/** Cuánto sigue el balance la pendiente transversal de la ola (0–1): el balancín y el casco doble lo frenan. */
	float WaveRollResponse = 1.0f;
	/** Escora a partir de la cual vuelca (°). */
	float CapsizeRollDeg = 30.0f;

	/** Entrada de agua con el casco destrozado del todo (kg/s); escala con el daño. */
	float MaxLeakKgS = 3.0f;
	/** Lo que achica el tripulante (kg/s). */
	float BailRateKgS = 4.0f;
	/** Balsa de troncos: el agua se escurre entre ellos y nunca se anega. */
	bool bSelfDraining = false;
	/** Solo aguas someras (la balsa): se niega a salir a más profundidad que MaxSafeDepthCm. */
	bool bShallowWaterOnly = false;
	float MaxSafeDepthCm = 0.0f;
	/** Multiplicador del daño por golpes contra el arrecife (menor = casco más duro). */
	float ImpactDamageScale = 1.0f;

	/** Área de la flotación (m²). */
	float WaterplaneAreaM2() const { return (LengthCm / 100.0f) * (BeamCm / 100.0f) * WaterplaneCoefficient; }
	bool HasSail() const { return SailAreaM2 > 0.0f; }
};

/** Mandos que el jugador mantiene de un fotograma a otro (las paladas van aparte: FBoatModel::TryStroke). */
struct EXPLORED_API FBoatControls
{
	/** Timón (espadilla): -1 caer a babor, 1 caer a estribor. Solo gobierna con arrancada. */
	float Rudder = 0.0f;
	/** Escota: 0 cazada (vela al centro), 1 largada (vela abierta 90°). */
	float SailTrim01 = 0.5f;
	/** Ajusta la escota sola al ángulo óptimo para el viento aparente (accesibilidad). */
	bool bAutoTrim = true;
	/** El tripulante achica agua en lugar de remar. */
	bool bBailing = false;
};

/**
 * Lo que el mundo aporta en cada paso. El modelo no conoce actores ni
 * subsistemas: quien llama inyecta olas, marea, corriente, viento y fondo.
 */
struct EXPLORED_API FBoatEnvironment
{
	/** Olas de Gerstner del océano (las mismas que el material); nulo = agua plana. */
	const FOceanWaves* Waves = nullptr;
	/** Instante de las olas al FINAL del paso (s, el mismo reloj que usa el océano). */
	float WaveTimeSeconds = 0.0f;
	/** Subida del nivel del mar por la marea (cm): afecta a la flotación y al fondo disponible. */
	float TideOffsetCm = 0.0f;
	/** Velocidad del agua (cm/s, mundo): corrientes de los estrechos (FOceanCurrents::CurrentAt). */
	FVector2D CurrentCmS = FVector2D::ZeroVector;
	/** Velocidad del viento verdadero (cm/s, mundo, hacia donde sopla): ver FBoatWind. */
	FVector2D WindCmS = FVector2D::ZeroVector;
	/**
	 * Profundidad del fondo bajo el nivel medio del mar en (X, Y) (cm; ≤ 0 es
	 * tierra firme). Sin función no hay varadas ni límite de mar abierto.
	 */
	TFunction<float(const FVector2D&)> DepthBelowSeaLevelCm;
};

/** Estado completo de una embarcación (lo que integra el modelo). */
struct EXPLORED_API FBoatState
{
	EBoatType Type = EBoatType::Raft;
	EBoatCondition Condition = EBoatCondition::Afloat;

	/** Punto más bajo de la quilla bajo el centro del casco (cm, mundo): el origen de la malla. */
	FVector LocationCm = FVector::ZeroVector;
	/** Rumbo: X es el norte e Y el este (TimeOfDaySubsystem), así que la guiñada coincide con el rumbo de brújula. */
	float YawDeg = 0.0f;
	/** Cabeceo (positivo = proa arriba) y balance (positivo = estribor abajo), convención FRotator. */
	float PitchDeg = 0.0f;
	float RollDeg = 0.0f;

	/** Velocidad sobre el fondo (cm/s, mundo). */
	FVector2D VelocityCmS = FVector2D::ZeroVector;
	float HeaveVelocityCmS = 0.0f;
	float PitchRateDegS = 0.0f;
	float RollRateDegS = 0.0f;
	float YawRateDegS = 0.0f;

	/** 0 intacto, 1 destrozado. */
	float HullDamage01 = 0.0f;
	float CargoKg = 0.0f;
	float WaterInHullKg = 0.0f;
	bool bCrewAboard = false;
	bool bSailRaised = false;

	/** La quilla toca fondo (encallado o varado con la bajamar). */
	bool bGrounded = false;
	/** La balsa ha llegado al borde del mar abierto y no avanza más. */
	bool bAtOpenOceanLimit = false;

	/** Amarrada a un poste o a un muelle: el cabo no deja que se aleje más de MooringLengthCm del punto de amarre. */
	bool bMoored = false;
	/** El cabo va tenso en este paso (el barco tira del amarre). */
	bool bMooringTaut = false;
	/** Punto de amarre (cm, mundo, XY). */
	FVector2D MooringAnchorCm = FVector2D::ZeroVector;
	/** Largo del cabo (cm). */
	float MooringLengthCm = 0.0f;

	/** Golpes contra el fondo que han dañado el casco desde la construcción (los consume el astillero para repartirlos en las uniones). */
	int32 ImpactCount = 0;
	/** Velocidad del último golpe (cm/s). */
	float LastImpactSpeedCmS = 0.0f;
	/** Dirección del último golpe en el marco del casco (X proa, Y estribor; unitaria). */
	FVector2D LastImpactDirection = FVector2D(1.0, 0.0);
	/** Trabajo de roce contra el fondo varado y en marcha (N·m acumulados): la arena y la roca gastan las uniones. */
	float GroundScrapeWorkNm = 0.0f;

	float StrokeTimeLeftS = 0.0f;
	EBoatSide StrokeSide = EBoatSide::Port;
	/** Tiempo pendiente de integrar (paso fijo); no se guarda. */
	float PendingTimeS = 0.0f;
};

/** Datos planos para la partida guardada (P-SAVE los envuelve en su propio formato). */
struct EXPLORED_API FBoatSaveData
{
	EBoatType Type = EBoatType::Raft;
	EBoatCondition Condition = EBoatCondition::Afloat;
	FVector LocationCm = FVector::ZeroVector;
	float YawDeg = 0.0f;
	float HullDamage01 = 0.0f;
	float CargoKg = 0.0f;
	float WaterInHullKg = 0.0f;
	bool bSailRaised = false;
	bool bMoored = false;
	FVector2D MooringAnchorCm = FVector2D::ZeroVector;
	float MooringLengthCm = 0.0f;
};

/**
 * Camino de estrellas (GDD §6.2–6.3) como dato plano: de una isla a otra
 * siguiendo el punto del horizonte por el que sale o se pone una estrella.
 * Lo rellenará el modelo de ruinas; aquí solo se navega con él.
 */
struct EXPLORED_API FStarPath
{
	FVector2D OriginCm = FVector2D::ZeroVector;
	FVector2D DestinationCm = FVector2D::ZeroVector;
	/** Rumbo de brújula (°, 0 = norte, 90 = este) del punto del horizonte que marca la estrella. */
	float StarBearingDeg = 0.0f;
};

/** Lectura de navegación nocturna frente a un camino de estrellas. */
struct EXPLORED_API FNightNavigationReading
{
	/** Error de la proa frente a la estrella (°, -180..180; positivo = hay que caer a estribor). */
	float HeadingErrorDeg = 0.0f;
	/** Error del rumbo real sobre el fondo (con abatimiento y corriente) frente a la estrella. */
	float CourseErrorDeg = 0.0f;
	/** Distancia lateral a la derrota origen→destino (cm; positivo = a estribor de la derrota). */
	float CrossTrackCm = 0.0f;
	float DistanceToGoCm = 0.0f;
	bool bOnCourse = false;
};
