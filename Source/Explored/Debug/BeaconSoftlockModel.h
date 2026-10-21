#pragma once

#include "CoreMinimal.h"
#include "Narrative/BeaconProgressModel.h"
#include "Narrative/SupplyCrateModel.h"

/**
 * Auditor de softlocks de Baliza Siete (plan F3, criterio 2; GDD §5.3 y riesgo R7): decide si,
 * desde una foto de la partida, la baliza aún se puede encender con lo que lleva Iria, lo que
 * queda en la isla y la caja de suministros. Lo usa UExploredPlaytestAuditor con los recursos
 * reales del mapa; el pescado reaparece y el resto no (GDD §5.3).
 *
 * También recorre la máquina de etapas (FBeaconDirectorModel) en todos los órdenes de los hitos
 * (lente, aceite, mecha), con y sin la página 1, y con la lente antes o después del ciclón.
 *
 * Modelo puro (solo CoreMinimal.h); Tests/BeaconSoftlockSpec.cpp lo prueba en el host.
 */

/** Foto de la partida para el análisis. */
struct EXPLORED_API FSoftlockSnapshot
{
	/** Mochila y carga pesada (id de objeto → cantidad). */
	TMap<FName, int32> Pack;
	/** Lo que aún se puede recoger en la isla. */
	TMap<FName, int32> World;
	FBeaconProgressState Progress;
	FSupplyCrateState Crate;
	int32 RaftLogsPlaced = 0;
	/** Horas absolutas (día 2 a las 19:00 = 43). */
	double AbsHours = 7.0;
};

/** Reglas del juego que importan para terminar (valores de storm_curve.json y recipes.json). */
struct EXPLORED_API FSoftlockRules
{
	double RaftLastHour = 42.0;
	/** Reapertura del mar tras el ciclón; TNumericLimits<double>::Max() = no reabre nunca. */
	double RaftReopenHour = 54.0;
	int32 LogsForRaft = 4;
	int32 RopesForRaft = 2;
	int32 FibersPerRope = 2;
	int32 FibersPerWick = 3;
	int32 WoodPerFire = 3;
	int32 FishPerJar = 2;
	bool bFishRespawns = true;
};

struct EXPLORED_API FSoftlockVerdict
{
	bool bCompletable = false;
	/** La caja entregaba algo en esta foto (se cuenta como abierta antes de seguir). */
	TMap<FName, int32> CrateItems;
	/** Qué impide terminar («leña: faltan 3»). Vacío si se puede. */
	TArray<FString> Missing;
};

struct EXPLORED_API FSoftlockScenario
{
	FString Name;
	FSoftlockSnapshot Snapshot;
};

struct EXPLORED_API FBeaconSoftlockModel
{
	static FSoftlockVerdict Evaluate(const FSoftlockSnapshot& Snapshot, const FSoftlockRules& Rules = FSoftlockRules());

	/**
	 * Escenarios alcanzables del plan F3 a partir de la isla entera al empezar (IslandAtStart:
	 * fibra, leña, tronco, cana_bambu, coco...): orden distinto de tareas, página 1 sin leer y
	 * recursos agotados por los sumideros reales del juego (hogueras y leña de más, cañas de
	 * pescar de más, tarros quemados como combustible, cocos bebidos), más la lente sin recoger
	 * al cerrarse el mar.
	 */
	static TArray<FSoftlockScenario> StandardScenarios(const TMap<FName, int32>& IslandAtStart);

	/**
	 * Recorre FBeaconDirectorModel en los 6 órdenes de lente/aceite/mecha × página 1 leída o no ×
	 * lente antes o después del ciclón (24 partidas). Devuelve una línea por partida que no llega a
	 * la etapa Final; vacío = ninguna se bloquea.
	 */
	static TArray<FString> StoryOrderFailures();

	/** Texto corto de un veredicto («se puede; la caja da 3 lena» / «faltan: ...»). */
	static FString Describe(const FSoftlockVerdict& Verdict);
};
