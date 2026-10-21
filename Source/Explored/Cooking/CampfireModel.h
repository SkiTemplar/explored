#pragma once

#include "CoreMinimal.h"

/**
 * Hoguera de Baliza Siete (GDD v3 §5.2; Content/Data/fuels.json): arde 5 min reales al encenderse,
 * cada leña suma 1,7 min y un tarro de aceite 6 min; calienta a 4 m o menos. Solo el ciclón apaga
 * un fuego al aire libre (el techo de un refugio lo protege). Cocina un trabajo a la vez (asar un
 * pez, hervir aceite) que se pausa si el fuego se apaga y queda listo para recoger al terminar.
 * Modelo puro e inmutable; AExploredFire es la capa fina.
 */
struct EXPLORED_API FCampfireConfig
{
	static constexpr float DefaultBurnSeconds = 300.0f;
	static constexpr float DefaultMaxBurnSeconds = 900.0f;
	static constexpr float DefaultWoodSeconds = 102.0f;
	static constexpr float DefaultOilJarSeconds = 360.0f;
	static constexpr float DefaultHeatRadiusMeters = 4.0f;
	static constexpr float DefaultWarmthPerMinute = 25.0f;

	float BurnSeconds = DefaultBurnSeconds;
	float MaxBurnSeconds = DefaultMaxBurnSeconds;
	float HeatRadiusMeters = DefaultHeatRadiusMeters;
	float WarmthPerMinute = DefaultWarmthPerMinute;
	/** Combustible → segundos que suma (lena, tarro_aceite). */
	TMap<FName, float> FuelSeconds;

	static FCampfireConfig Default();
};

/** Trabajo de cocina en marcha o terminado sobre la hoguera. */
struct EXPLORED_API FCampfireJob
{
	FName RecipeId;
	FName OutputItem;
	int32 OutputCount = 0;
	float RemainingSeconds = 0.0f;

	bool IsActive() const { return !RecipeId.IsNone(); }
	bool IsDone() const { return IsActive() && RemainingSeconds <= 0.0f; }
	bool operator==(const FCampfireJob& Other) const
	{
		return RecipeId == Other.RecipeId && OutputItem == Other.OutputItem && OutputCount == Other.OutputCount
			&& RemainingSeconds == Other.RemainingSeconds;
	}
};

struct EXPLORED_API FCampfireState
{
	bool bLit = false;
	float BurnSeconds = 0.0f;
	FCampfireJob Job;

	bool operator==(const FCampfireState& Other) const
	{
		return bLit == Other.bLit && BurnSeconds == Other.BurnSeconds && Job == Other.Job;
	}
};

enum class ECampfireEvent : uint8
{
	/** Se acabó el combustible. */
	BurnedOut,
	/** El ciclón lo apagó (estaba al aire libre). */
	Quenched,
	/** El trabajo de cocina ha terminado. */
	JobDone,
};

struct EXPLORED_API FCampfireModel
{
	/** Enciende con la carga inicial. Encender uno ya encendido no cambia nada. */
	static FCampfireState Ignite(const FCampfireState& In, const FCampfireConfig& Config);

	/** Suma combustible a un fuego encendido. false si está apagado o el objeto no arde. */
	static bool AddFuel(const FCampfireState& In, const FCampfireConfig& Config, FName Item, FCampfireState& Out);

	/** Avanza el tiempo. bStormExposed: muro del ciclón y sin techo. */
	static FCampfireState Tick(const FCampfireState& In, float DeltaSeconds, bool bStormExposed, TArray<ECampfireEvent>& OutEvents);

	/** Empieza a cocinar (pide fuego encendido y sin otro trabajo). */
	static bool StartJob(const FCampfireState& In, FName RecipeId, FName OutputItem, int32 OutputCount, float Seconds, FCampfireState& Out);

	/** Recoge un trabajo terminado: devuelve el estado sin trabajo y lo que sale. */
	static bool CollectJob(const FCampfireState& In, FCampfireState& Out, FName& OutItem, int32& OutCount);

	/** ¿Calienta a esta distancia? (encendido y dentro del radio). */
	static bool Warms(const FCampfireState& State, const FCampfireConfig& Config, float DistanceMeters);

	/** Calor 0-1 a esta distancia: 1 dentro del radio, cae a 0 en el doble del radio (para la luz y el cuerpo). */
	static float HeatAt(const FCampfireState& State, const FCampfireConfig& Config, float DistanceMeters);
};
