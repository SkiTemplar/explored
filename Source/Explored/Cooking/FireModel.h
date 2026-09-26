#pragma once

#include "CoreMinimal.h"

/** Niveles del fuego como estación (GDD §8.5): fogata → hoguera → horno de arcilla. */
enum class EFireLevel : uint8
{
	Fogata,
	Hoguera,
	HornoArcilla,
	Count
};

/** Estado del hogar. */
enum class EFireStatus : uint8
{
	Unlit,      // Apagado: hace falta encenderlo otra vez.
	Burning,    // Encendido: da calor, luz y humo.
	Embers,     // Brasas: se reaviva echando combustible, sin volver a encender.
};

/** Formas de encender (biblia §5.3). */
enum class EIgnitionMethod : uint8
{
	Matches,    // Cerillas del Albatros (se gastan).
	Flint,      // Pedernal contra metal rescatado.
	Friction,   // Arco de fuego (minijuego en la capa de UE).
	Count
};

/** Sucesos para que el juego reaccione (sonido, partículas, textos). */
enum class EFireEvent : uint8
{
	Ignited,
	IgnitionFailed,
	FuelAdded,
	Revived,        // Brasas que vuelven a arder al echar combustible.
	BurnedDown,     // Se acabó el combustible: quedan brasas.
	Extinguished,   // Lo apagan la lluvia o el viento.
	WentOut,        // Las brasas se enfrían del todo.
	Count
};

EXPLORED_API const TCHAR* LexToString(EFireLevel Level);
EXPLORED_API const TCHAR* LexToString(EIgnitionMethod Method);

/** Un nivel de fuego (Content/Data/fuels.json, «levels»). */
struct EXPLORED_API FFireLevelDef
{
	EFireLevel Level = EFireLevel::Fogata;
	FName Id;
	FString NameEs;
	/** Pieza de building_pieces.json que construye este nivel. */
	FName PieceId;
	float MaxFuelHours = 4.0f;
	float BurnRate = 1.0f;
	float HeatScale = 0.7f;
	float EmberHours = 2.0f;
	float RainQuench = 2.5f;
	float WindTolerance = 0.6f;
	/** Cerrado (horno): la lluvia y el viento no lo apagan aunque esté a la intemperie. */
	bool bEnclosed = false;
	float BaseSmoke = 0.3f;
};

/** Un combustible (fuels.json, «fuels»). */
struct EXPLORED_API FFuelDef
{
	FName ItemId;
	float BurnHours = 0.5f;
	/** Calor que da (0–1) comparado con la mejor leña. */
	float Heat = 0.5f;
	/** Yesca: ayuda a prender, apenas dura. */
	bool bTinder = false;
	/** Verde: humo denso y blanco (fuego de señal, biblia §5.3). */
	bool bGreen = false;
};

/** Una forma de encender (fuels.json, «ignition»). */
struct EXPLORED_API FIgnitionDef
{
	EIgnitionMethod Method = EIgnitionMethod::Matches;
	FName Id;
	FString NameEs;
	/** Objeto que hay que tener en la mano. */
	FName ToolItemId;
	/** Si cada intento gasta una unidad del objeto (una cerilla). */
	bool bConsumesTool = false;
	float BaseChance = 0.5f;
	float Minutes = 1.0f;
};

/** Tablas del fuego. Default() se genera desde Content/Data/fuels.json (FireData.inl). */
struct EXPLORED_API FFireData
{
	TArray<FFireLevelDef> Levels;
	TArray<FFuelDef> Fuels;
	TArray<FIgnitionDef> Ignitions;

	static const FFireData& Default();

	/** Nivel pedido; si falta en los datos, el primero (los datos siempre traen los tres). */
	const FFireLevelDef& GetLevel(EFireLevel Level) const;
	const FFuelDef* FindFuel(FName ItemId) const;
	const FIgnitionDef* FindIgnition(EIgnitionMethod Method) const;
	/** Forma de encender que usa este objeto en la mano; nullptr si ninguna. */
	const FIgnitionDef* FindIgnitionByTool(FName ItemId) const;
};

/** Lo que rodea al fuego en este intervalo. */
struct EXPLORED_API FFireEnvironment
{
	float Rain = 0.0f;      // 0–1
	float Wind = 0.2f;      // 0–1
	bool bSheltered = false; // bajo techo (la capa de UE lo detecta con una traza hacia arriba)
};

/**
 * Estado de un fuego. Datos planos: es lo que se guarda en la partida
 * (combustible, calor, humedad, brasas) y lo que compara el test de
 * determinismo.
 */
struct EXPLORED_API FFireState
{
	EFireLevel Level = EFireLevel::Fogata;
	EFireStatus Status = EFireStatus::Unlit;
	/** Horas de combustible cargadas (a ritmo 1). */
	float FuelHours = 0.0f;
	/** Calor medio (0–1) del combustible cargado, ponderado por horas. */
	float FuelHeat = 0.0f;
	/** Puñados de yesca listos para prender. */
	int32 TinderCharges = 0;
	/** Humedad del hogar y la leña (0–1): a 1 se apaga. */
	float Dampness = 0.0f;
	/** Horas de brasas que quedan. */
	float EmberHours = 0.0f;
	/** Horas de humo denso de señal que quedan (combustible verde). */
	float SignalSmokeHours = 0.0f;
	/** Calor que da ahora (0–1.2 aprox.); lo leen la supervivencia y la cocina. */
	float Heat = 0.0f;
	/** Humo que echa ahora (0–1); lo lee el ahumadero. */
	float Smoke = 0.0f;

	bool operator==(const FFireState& Other) const;
	bool operator!=(const FFireState& Other) const { return !(*this == Other); }
};

/** Resultado de un intento de encender. */
struct EXPLORED_API FIgnitionResult
{
	bool bLit = false;
	float Chance = 0.0f;
	float MinutesSpent = 0.0f;
	/** Se gastó una unidad de la herramienta (una cerilla). */
	bool bConsumedTool = false;
};

/**
 * Reglas del fuego. Funciones puras y deterministas: el azar entra como
 * RandomRoll en [0, 1), igual que en FSurvivalModel.
 */
struct EXPLORED_API FFireModel
{
	static constexpr int32 MaxTinderCharges = 3;
	/** Paso máximo de simulación (horas) para que un Tick largo dé lo mismo que muchos cortos. */
	static constexpr float MaxStepHours = 0.05f;

	/**
	 * Echa una unidad de combustible. Devuelve false si el objeto no arde o
	 * si ya no cabe. Sobre brasas lo reaviva sin volver a encender.
	 */
	static bool AddFuel(FFireState& State, const FFireData& Data, FName ItemId, TArray<EFireEvent>& OutEvents);

	/** Probabilidad de prender con este método ahora (0 si no hay combustible o ya arde). */
	static float IgnitionChance(const FFireState& State, const FFireData& Data, EIgnitionMethod Method, const FFireEnvironment& Env);

	/** Intenta encender. Gasta un puñado de yesca por intento, prenda o no. */
	static FIgnitionResult TryIgnite(FFireState& State, const FFireData& Data, EIgnitionMethod Method, const FFireEnvironment& Env,
		float RandomRoll, TArray<EFireEvent>& OutEvents);

	/** Avanza el fuego DeltaHours de juego (se subdivide en pasos de MaxStepHours). */
	static void Tick(FFireState& State, const FFireData& Data, const FFireEnvironment& Env, float DeltaHours, TArray<EFireEvent>& OutEvents);

	/** Mejora el hogar (solo hacia arriba); conserva el combustible y el estado. */
	static bool Upgrade(FFireState& State, EFireLevel NewLevel);

	/** Horas que durará el combustible cargado con este tiempo (sin contar las brasas). */
	static float RemainingBurnHours(const FFireState& State, const FFireData& Data, const FFireEnvironment& Env);

	/** Fuego de señal para el barco del horizonte (P-EVENTS): hoguera con humo verde. */
	static bool IsSignalFire(const FFireState& State);

	/** Las fogatas encendidas (o en brasas) son puntos de reaparición (GDD §8.6). */
	static bool IsRespawnPoint(const FFireState& State);

	/** Calor que recibe algo a DistanceMeters del fuego (0 a partir de 5 m). */
	static float HeatAtDistance(const FFireState& State, float DistanceMeters);
};
