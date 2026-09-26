#pragma once

#include "CoreMinimal.h"
#include "Fauna/BoidsModel.h"
#include "Fauna/FaunaTypes.h"

/** Banco de arrecife (se dispersa y se rehace) o de mar abierto (migra). */
enum class EFishSchoolKind : uint8
{
	Reef,
	OpenSea,
};

enum class EFishSchoolState : uint8
{
	Schooling,   // arrecife: dando vueltas junto a su casa
	Scattered,   // arrecife: disperso por una amenaza
	Regrouping,  // arrecife: pasada la amenaza, se rehace
	Migrating,   // mar abierto: gran banco en ruta
};

struct EXPLORED_API FFishSchoolConfig
{
	EFishSchoolKind Kind = EFishSchoolKind::Reef;
	FVector HomeCm = FVector::ZeroVector;
	float HomeRadiusCm = 800.0f;
	int32 Count = 30;
	uint32 Seed = 1;
	/** Rumbo inicial de la migración (mar abierto). */
	FVector2D MigrationDirection = FVector2D(1.0, 0.0);
	/** Distancia a la que el jugador asusta al banco con ruido máximo (se reduce si se mueve en silencio). */
	float ScatterRadiusCm = 600.0f;
	/** Segundos sin amenaza antes de rehacerse. */
	float RegroupDelaySeconds = 4.0f;
	/** Dispersión mínima (cm) con la que el banco se da por rehecho (o 1,25 veces la habitual en calma si es mayor). */
	float RegroupedSpreadCm = 180.0f;
	/** Holgura bajo la superficie y sobre el fondo. */
	float SurfaceClearanceCm = 40.0f;
	float SeabedClearanceCm = 40.0f;
};

/**
 * Banco de peces sobre FBoidsModel (GDD §10): la franja vertical la da el
 * mundo (nunca rompen la superficie ni tocan el fondo, nunca entran en
 * tierra), el jugador y los depredadores son amenazas y la hora escala el
 * ritmo (ciclo diario de actividad).
 */
class EXPLORED_API FFishSchoolModel
{
public:
	void Init(const FFishSchoolConfig& InConfig, const FFaunaWorldQuery& World);
	void Tick(float DeltaSeconds, const FFaunaStimuli& Stimuli, const FFaunaWorldQuery& World);

	EFishSchoolState GetState() const { return State; }
	const FFishSchoolConfig& GetConfig() const { return Config; }
	const FBoidsModel& GetBoids() const { return Boids; }
	/** Para sacar peces capturados (pesca) del banco. */
	FBoidsModel& GetBoidsMutable() { return Boids; }
	FVector2D GetMigrationDirection() const { return MigrationDirection; }
	/** Dispersión por debajo de la cual un banco disperso se da por rehecho. */
	float GetRegroupedSpread() const { return FMath::Max(Config.RegroupedSpreadCm, CalmSpread * 1.25f); }

	/** ¿Asusta el jugador (o un depredador) al banco ahora? */
	bool IsThreatened(const FFaunaStimuli& Stimuli) const;

	/** Franja de agua transitable en un punto: [fondo + holgura, superficie − holgura]. */
	static FBoidBand WaterBand(const FVector& P, const FFaunaWorldQuery& World, float SurfaceClearanceCm, float SeabedClearanceCm);

	/** Parámetros de boids por tipo de banco. */
	static FBoidsParams ParamsFor(EFishSchoolKind Kind);

private:
	FBoidsEnvironment BuildEnvironment(const FFaunaStimuli& Stimuli, const FFaunaWorldQuery& World) const;

	FFishSchoolConfig Config;
	FBoidsModel Boids;
	EFishSchoolState State = EFishSchoolState::Schooling;
	FVector2D MigrationDirection = FVector2D(1.0, 0.0);
	float CalmSeconds = 0.0f;
	float StateSeconds = 0.0f;
	float CalmSpread = 0.0f;
	double LocalTime = 0.0;
};

enum class EBirdFlockState : uint8
{
	Foraging,         // sobrevuela su costa
	Fleeing,          // el jugador está demasiado cerca
	FollowingBoat,    // sigue a la canoa que lleva pescado (biblia §4.5)
	Stealing,         // va a por pescado dejado al aire (GDD §10)
	ReturningToLand,  // atardecer: vuela hacia la isla más cercana (GDD §6.2)
	CirclingLand,     // de noche: vuelta lenta en lo alto sobre la isla, sin posarse
};

struct EXPLORED_API FBirdFlockConfig
{
	EFaunaSpecies Species = EFaunaSpecies::Gull;
	FVector HomeCm = FVector::ZeroVector;
	float HomeRadiusCm = 4000.0f;
	int32 Count = 12;
	uint32 Seed = 1;
	/** Altura mínima sobre el agua o el terreno (siempre en vuelo, GDD §12). */
	float MinAltitudeCm = 600.0f;
	float MaxAltitudeCm = 4000.0f;
	/** Se apartan del jugador («a distancia», GDD §10). */
	float PlayerFleeRadiusCm = 1500.0f;
	float StealSearchRadiusCm = 6000.0f;
	/** Solo roban si el jugador está más lejos que esto del objeto. */
	float PlayerSafeRadiusCm = 1200.0f;
	/** Distancia horizontal a la que un ave agarra el objeto. */
	float GrabRadiusCm = 250.0f;
	/** Distancia a la que siguen a una canoa con pescado. */
	float FollowBoatRadiusCm = 8000.0f;
};

struct EXPLORED_API FBirdFlockEvents
{
	/** Índice en FFaunaStimuli::Stealables del objeto robado en este paso, o INDEX_NONE. */
	int32 StolenItemIndex = INDEX_NONE;
};

/** Consulta de robo de las gaviotas (GDD §10). */
struct EXPLORED_API FGullTheft
{
	/**
	 * Objeto robable más cercano a Center dentro de SearchRadiusCm: pescado,
	 * al aire, y sin el jugador a menos de PlayerSafeRadiusCm de él. Si no hay
	 * jugador (bHasPlayer = false) nadie lo protege. INDEX_NONE si no hay.
	 */
	static int32 FindStealable(const TArray<FStealableItem>& Items, const FVector& CenterCm, float SearchRadiusCm,
		bool bHasPlayer, const FVector& PlayerCm, float PlayerSafeRadiusCm);
};

/**
 * Bandada de aves marinas sobre FBoidsModel: siempre en vuelo (velocidad
 * mínima y altura mínima garantizadas), se apartan del jugador, siguen a la
 * canoa con pescado, roban pescado al aire y al atardecer vuelan hacia la
 * isla más cercana, lo que el jugador puede leer para orientarse.
 */
class EXPLORED_API FBirdFlockModel
{
public:
	void Init(const FBirdFlockConfig& InConfig, const FFaunaWorldQuery& World);
	FBirdFlockEvents Tick(float DeltaSeconds, const FFaunaStimuli& Stimuli, const FFaunaWorldQuery& World);

	EBirdFlockState GetState() const { return State; }
	const FBirdFlockConfig& GetConfig() const { return Config; }
	const FBoidsModel& GetBoids() const { return Boids; }

	/** Franja de aire: [max(agua, terreno) + mínima, max(agua, terreno) + máxima]. */
	static FBoidBand AirBand(const FVector& P, const FFaunaWorldQuery& World, float MinAltitudeCm, float MaxAltitudeCm);

	static FBoidsParams ParamsFor(EFaunaSpecies Species);

private:
	FBirdFlockConfig Config;
	FBoidsModel Boids;
	EBirdFlockState State = EBirdFlockState::Foraging;
	double LocalTime = 0.0;
};
