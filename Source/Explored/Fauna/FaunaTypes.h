#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

/**
 * Tipos comunes de la fauna (GDD §10): solo vida marina y aves siempre en
 * vuelo. Todo en centímetros y espacio de mundo (nivel del mar Z = 0), como
 * Ocean y Player; WorldGen trabaja en metros y quien consulta convierte.
 */

/** Especies con criatura visible. Las mallas salen de Tools/Blender/animals (fish.py, birds.py, jellyfish.py, turtle.py). */
enum class EFaunaSpecies : uint8
{
	ReefFish,       // bancos de arrecife (ClownFish, ButterflyFish, ParrotFish, SurgeonFish, Grouper, Wrasse)
	OpenSeaFish,    // bancos migratorios de mar abierto (Tuna)
	Stingray,
	Jellyfish,
	ReefShark,
	TigerShark,
	Dolphin,
	SeaTurtle,
	HumpbackWhale,
	Gull,           // gaviota / charrán
	Frigatebird,
	Count
};

EXPLORED_API const TCHAR* LexToString(EFaunaSpecies Species);

/** Cómo anima el shader de vértices a la especie (sin esqueleto, GDD §10 y §12). */
enum class EFaunaAnimStyle : uint8
{
	SpineWave,   // onda lateral de la columna (peces, tiburones)
	DiscWave,    // ondulación de las pectorales de la raya
	FlukeWave,   // onda vertical de mamífero (delfín, ballena)
	Flipper,     // aletas delanteras de la tortuga (remada lenta)
	Pulse,       // pulsación de la campana de la medusa
	Flap,        // aleteo de ave (nunca plegada: siempre en vuelo)
};

/** Perfil de actividad diaria. */
enum class EFaunaActivityProfile : uint8
{
	Diurnal,      // activo de día, casi nada de noche
	Crepuscular,  // picos al amanecer y al atardecer, activo de noche
	Constant,     // indiferente a la hora
};

/** Datos fijos de cada especie. */
struct EXPLORED_API FFaunaSpeciesInfo
{
	/** Nombre de la especie en las mallas (SM_<Mesh>_<Pieza>) de la primera variante. */
	const TCHAR* Mesh = TEXT("");
	EFaunaAnimStyle AnimStyle = EFaunaAnimStyle::SpineWave;
	EFaunaActivityProfile Activity = EFaunaActivityProfile::Diurnal;
	/** Actividad mínima (de noche para las diurnas). */
	float ActivityFloor = 0.2f;
	float BodyLengthCm = 20.0f;
	float CruiseSpeedCmS = 60.0f;
	float MaxSpeedCmS = 150.0f;
	/** Profundidad mínima del agua (cm) en la que se mueve; 0 para las aves. */
	float MinWaterDepthCm = 100.0f;
	bool bBird = false;

	static const FFaunaSpeciesInfo& Get(EFaunaSpecies Species);
};

/**
 * Consultas del mundo que inyecta quien usa la fauna (capa de UE o tests).
 * Cualquier función sin fijar toma un valor por defecto: fondo a −70 m, mar
 * en calma a nivel 0, sin corriente y tierra en el origen.
 */
struct EXPLORED_API FFaunaWorldQuery
{
	/** Altura del terreno (fondo o tierra) en cm; > 0 es tierra emergida. */
	TFunction<float(const FVector2D&)> Seabed;
	/** Altura de la superficie del mar en cm (olas y marea). */
	TFunction<float(const FVector2D&)> Surface;
	/** Corriente horizontal en cm/s (FOceanCurrents::CurrentAt). */
	TFunction<FVector2D(const FVector2D&)> Current;
	/** Punto de tierra más cercano (centro de la isla más próxima). */
	TFunction<FVector2D(const FVector2D&)> NearestLand;

	static constexpr float DefaultSeabedCm = -7000.0f;

	float SeabedZ(const FVector2D& P) const { return Seabed ? Seabed(P) : DefaultSeabedCm; }
	float SurfaceZ(const FVector2D& P) const { return Surface ? Surface(P) : 0.0f; }
	FVector2D CurrentAt(const FVector2D& P) const { return Current ? Current(P) : FVector2D::ZeroVector; }
	FVector2D NearestLandTo(const FVector2D& P) const { return NearestLand ? NearestLand(P) : FVector2D::ZeroVector; }

	/** Profundidad del agua (cm, ≥ 0): de la superficie al fondo. */
	float WaterDepth(const FVector2D& P) const { return FMath::Max(0.0f, SurfaceZ(P) - SeabedZ(P)); }
};

/** Mancha de sangre o cebo en el agua: el olor deriva con la corriente (biblia §4.2). */
struct EXPLORED_API FBloodSource
{
	FVector OriginCm = FVector::ZeroVector;
	float AgeSeconds = 0.0f;
	/** Cantidad relativa: 0.3 un pescado despiezado, 1 una herida grave. */
	float Amount01 = 1.0f;
};

/** Objeto del mundo que una bandada puede robar (GDD §10: «roban pescado dejado al aire»). */
struct EXPLORED_API FStealableItem
{
	FVector PositionCm = FVector::ZeroVector;
	bool bIsFish = true;
	/** Al aire: ni en un contenedor ni tapado ni en la mano. */
	bool bExposed = true;
};

/** Lo que la fauna percibe del jugador y del entorno en un instante. */
struct EXPLORED_API FFaunaStimuli
{
	/** Hora local [0, 24) (UTimeOfDaySubsystem::GetHours). */
	float Hours = 12.0f;
	/** Segundos de juego acumulados (para órbitas y ritmos deterministas). */
	double TimeSeconds = 0.0;

	bool bHasPlayer = false;
	FVector PlayerCm = FVector::ZeroVector;
	FVector PlayerVelocityCmS = FVector::ZeroVector;
	bool bPlayerInWater = false;
	bool bPlayerInBoat = false;
	bool bPlayerCarriesFish = false;
	/** Arrastra los pies en el fondo: la raya lo nota antes y se aparta (biblia §4.3). */
	bool bPlayerShuffling = false;
	/** Ruido que hace el jugador: 0 quieto, 0.3 nadar despacio, 1 chapoteo o salto al agua. */
	float PlayerNoise01 = 0.0f;

	/** Modo Explorador (GDD §11): fauna pacífica, nadie ataca. */
	bool bPeaceful = false;

	TArray<FBloodSource> Blood;
	/** Depredadores cercanos (tiburones): asustan a los bancos. */
	TArray<FVector> Predators;
	/** Objetos que las aves pueden robar. */
	TArray<FStealableItem> Stealables;
};

/** Ciclo diario de actividad (GDD §10) y ventanas del día. */
struct EXPLORED_API FFaunaActivity
{
	/** Luz diurna aproximada en [0, 1] (0 de noche, 1 de día; rampas al amanecer y al atardecer). */
	static float Daylight(float Hours);

	/** Ventana del atardecer: las bandadas vuelven a tierra (GDD §6.2, «aves al atardecer»). */
	static bool IsDusk(float Hours) { return Hours >= DuskStartHours && Hours <= DuskEndHours; }

	/** Actividad de la especie a esa hora en [ActivityFloor, 1]. */
	static float Level(EFaunaSpecies Species, float Hours);

	static constexpr float DuskStartHours = 17.0f;
	static constexpr float DuskEndHours = 19.5f;
};

/** Percepción: vista en cono, oído por ruido y olfato que deriva con la corriente. */
struct EXPLORED_API FFaunaPerception
{
	/** Vista: dentro del cono (semiángulo en grados) y del alcance, que se reduce con poca luz. */
	static bool CanSee(const FVector& EyeCm, const FVector& Forward, const FVector& TargetCm,
		float RangeCm, float HalfAngleDeg, float Light01);

	/** Oído: el alcance crece con el ruido de la fuente (0 no se oye nada). */
	static bool CanHear(const FVector& EarCm, const FVector& SourceCm, float Noise01, float HearingRangeCm);

	/**
	 * Olor de sangre en un punto: cada mancha deriva con la corriente del
	 * punto de origen (centro = origen + corriente · edad), se ensancha con el
	 * tiempo y se desvanece. Devuelve la intensidad total en [0, ∞).
	 */
	static float SmellAt(const FVector& PointCm, const TArray<FBloodSource>& Blood, const FFaunaWorldQuery& World);

	/** Centro de la mancha de olor tras derivar. */
	static FVector PlumeCenter(const FBloodSource& Source, const FFaunaWorldQuery& World);
};
