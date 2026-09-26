#pragma once

#include "CoreMinimal.h"

#include "Fishing/FishingTension.h"

struct FWeatherSample;

/**
 * Hábitats marinos (GDD §10). La orilla es la zona intermareal de rocas y
 * arena donde van las trampas de cangrejo, el corral de piedras y las pozas.
 */
enum class EFishHabitat : uint8
{
	Shore,    // Orilla e intermareal
	Lagoon,   // Laguna y bajíos de arena
	Reef,     // Arrecife
	Slope,    // Talud
	Deep,     // Aguas profundas, mar abierto
	Count
};

/** Franjas del día que marcan la actividad de cada especie (biblia §4.1: amanecer y atardecer, los mejores). */
enum class EDayPeriod : uint8
{
	Dawn,
	Day,
	Dusk,
	Night,
	Count
};

/** Fase de la marea (ver FOceanTide). */
enum class ETidePhase : uint8
{
	Low,
	Rising,
	High,
	Falling,
	Count
};

/** Cuartos de la Luna (ciclo de 12 días, GDD §9.1). */
enum class EMoonQuarter : uint8
{
	New,
	Waxing,
	Full,
	Waning,
	Count
};

/** Cebos (biblia §3.5). Sin cebo el anzuelo desnudo casi no pica. */
enum class EFishBait : uint8
{
	None,
	Lombriz,
	Visceras,
	FrutaFermentada,
	Cangrejo,
	Senuelo,
	Count
};

/** Formas de capturar (GDD §8.9). Se usan como bits en las máscaras. */
enum class ECatchMethod : uint8
{
	Rod,
	Spear,
	Net,
	Trap,
	Hand,
	Count
};

/** Trampas fijas (biblia §3.5): nasa, trampa de cangrejos y corral de piedras intermareal. */
enum class ETrapKind : uint8
{
	Nasa,
	CrabTrap,
	StoneCorral,
	Count
};

/** Golpe con lanza o arpón (biblia §4.3): empuje seguro y corto, o lanzamiento. */
enum class ESpearStrike : uint8
{
	Thrust,
	Throw,
};

constexpr uint8 FishBit(EFishHabitat H) { return static_cast<uint8>(1u << static_cast<uint8>(H)); }
constexpr uint8 FishBit(ECatchMethod M) { return static_cast<uint8>(1u << static_cast<uint8>(M)); }
constexpr uint8 FishBit(EDayPeriod P) { return static_cast<uint8>(1u << static_cast<uint8>(P)); }

/**
 * Una especie capturable (11 peces + langosta de arrecife, GDD §8.8). Los
 * pesos de franja, marea, luna y cebo multiplican la tasa de picada: 1 es
 * neutro, 0 nunca. Espejo de `Content/Data/fish.json` (lo comprueba DataCheck).
 */
struct EXPLORED_API FFishSpecies
{
	/** Id del objeto en items.json que se obtiene al capturarlo. */
	FName Id;
	FString NameEs;
	uint8 HabitatMask = 0;
	uint8 MethodMask = 0;
	float MinDepthM = 0.0f;
	float MaxDepthM = 10.0f;
	/** Picadas por minuto en condiciones ideales (población llena, cebo favorito, silencio). */
	float BitesPerMinute = 0.5f;
	float PeriodWeight[static_cast<int32>(EDayPeriod::Count)] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float TideWeight[static_cast<int32>(ETidePhase::Count)] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float MoonWeight[static_cast<int32>(EMoonQuarter::Count)] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float BaitWeight[static_cast<int32>(EFishBait::Count)] = { 0.15f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	/** 0–1: cuánto le quita la mar gruesa. */
	float SeaStateSensitivity = 0.5f;
	/** -1..1: agua turbia por la lluvia (la barracuda caza mejor en ella). */
	float RainAffinity = 0.0f;
	/** 0–1: cuánto le espanta el ruido del pescador (biblia §4.2). */
	float Wariness = 0.5f;
	float MinWeightKg = 0.5f;
	float MaxWeightKg = 2.0f;
	/** Tirón típico (kgf) de un ejemplar mediano en una arrancada. */
	float StrengthKgf = 3.0f;
	float StaminaSeconds = 10.0f;
	float Aggression = 0.5f;
	/** Despiece (biblia §4.4): fracción del peso en filetes y en aceite. */
	float FilletRatio = 0.4f;
	float OilRatio = 0.01f;

	bool LivesIn(EFishHabitat H) const { return (HabitatMask & FishBit(H)) != 0; }
	bool CaughtBy(ECatchMethod M) const { return (MethodMask & FishBit(M)) != 0; }
	bool IsFish() const { return CaughtBy(ECatchMethod::Rod); }
};

/** Una de las cinco capturas legendarias (biblia §4.6): únicas, con lugar y reto propios. */
struct EXPLORED_API FLegendaryCatch
{
	FName Id;
	FString NameEs;
	/** Lugar con nombre donde vive (lo marca el mundo en FFishingConditions::SpotTag). */
	FName SpotTag;
	uint8 MethodMask = 0;
	/** El Rey de Plata solo se pesca desde la canoa, lejos de tierra. */
	bool bRequiresBoat = false;
	/** El Errante huye unos días si te ha oído antes (ver FFishingModel::NoteLegendaryPresence). */
	bool bSpooksEasily = false;
	/** Encuentros por minuto en su sitio y su hora: raros incluso bien hecho. */
	float PerMinute = 0.01f;
	float PeriodWeight[static_cast<int32>(EDayPeriod::Count)] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float MinWeightKg = 30.0f;
	float MaxWeightKg = 60.0f;
	float StrengthKgf = 12.0f;
	float StaminaSeconds = 40.0f;
	float Aggression = 0.6f;
	/** Roce contra la roca: deshilacha los sedales de fibra (El Viejo). */
	float RockAbrasionPerSecond = 0.0f;
	/** Impactos de arpón para rendirlo (capturas con arpón). */
	int32 HarpoonHits = 1;
	float Wariness = 0.5f;
	float FilletRatio = 0.35f;
	/** Recompensa exclusiva (ids de items.json). */
	TArray<FName> Rewards;

	bool CaughtBy(ECatchMethod M) const { return (MethodMask & FishBit(M)) != 0; }
};

/** Una trampa colocada en el mundo y lo que ha atrapado (datos planos para guardar). */
struct EXPLORED_API FTrapCatch
{
	FName ItemId;
	float WeightKg = 0.0f;
	float CaughtAtDays = 0.0f;
};

struct EXPLORED_API FPlacedTrap
{
	int32 Id = 0;
	ETrapKind Kind = ETrapKind::Nasa;
	EFishHabitat Habitat = EFishHabitat::Shore;
	/** Posición en el mundo (cm); el modelo no la usa, la guarda para el actor. */
	FVector Location = FVector::ZeroVector;
	EFishBait Bait = EFishBait::None;
	/** 0–1: el cebo se consume en un día. */
	float BaitLeft01 = 0.0f;
	float PlacedAtDays = 0.0f;
	/** Hasta cuándo está simulada. */
	float SimulatedToDays = 0.0f;
	TArray<FTrapCatch> Contents;
};

struct EXPLORED_API FTidePoolRecord
{
	int32 PoolId = 0;
	/** Última bajamar (ver FFishingModel::LowTideIndex) en la que se vació. */
	int32 LastLowTideIndex = MIN_int32;
};

/** Presión de pesca en una zona (biblia §4.5): se vacía al sobrepescar y se recupera sola. */
struct EXPLORED_API FZonePopulation
{
	int32 ZoneKey = 0;
	float Depletion01 = 0.0f;
	float UpdatedAtDays = 0.0f;
};

struct EXPLORED_API FLegendarySpook
{
	FName Id;
	float UntilDays = 0.0f;
};

/** Todo lo que la pesca necesita guardar (P-SAVE lo serializa tal cual). */
struct EXPLORED_API FFishingSaveState
{
	int32 NextTrapId = 1;
	TArray<FPlacedTrap> Traps;
	TArray<FName> CaughtLegendaries;
	TArray<FLegendarySpook> Spooked;
	TArray<FTidePoolRecord> TidePools;
	TArray<FZonePopulation> Zones;

	FPlacedTrap& PlaceTrap(ETrapKind Kind, EFishHabitat Habitat, const FVector& Location, EFishBait Bait, float NowDays);
	FPlacedTrap* FindTrap(int32 TrapId);
	bool RemoveTrap(int32 TrapId);

	bool IsLegendaryCaught(FName Id) const { return CaughtLegendaries.Contains(Id); }
	void MarkLegendaryCaught(FName Id) { CaughtLegendaries.AddUnique(Id); }
	bool IsLegendarySpooked(FName Id, float NowDays) const;
};

/** Dónde y cuándo se pesca. Lo rellena la capa de UE (o el test) con el mundo real. */
struct EXPLORED_API FFishingConditions
{
	EFishHabitat Habitat = EFishHabitat::Reef;
	float DepthM = 8.0f;
	/** Hora local (0–24). */
	float Hours = 7.0f;
	/** FOceanTide::Level y ::Flow. */
	float TideLevel = 0.0f;
	float TideFlow = 1.0f;
	/** 0 luna nueva, 0.5 llena (ExploredSky::MoonPhase). */
	float MoonPhase01 = 0.25f;
	float Rain = 0.0f;
	float SeaState = 0.15f;
	/** Ruido del pescador (ver FFishingModel::PlayerNoise). */
	float Noise01 = 0.0f;
	EFishBait Bait = EFishBait::None;
	ECatchMethod Method = ECatchMethod::Rod;
	/** Sitio con nombre (legendarias): «cueva_arenas_blancas», «canal_profundo»... */
	FName SpotTag;
	bool bFromBoat = false;
	/** Sobrepesca de la zona (FFishingModel::ZoneDepletion). */
	float Depletion01 = 0.0f;
	FFishingTackle Tackle;

	/** Hora, marea y luna a partir de los días totales de juego. */
	void SetTime(float TotalDays);
	void SetWeather(const FWeatherSample& Weather);
};

/** Una picada (o, con arpón, un pez a tiro). */
struct EXPLORED_API FFishBite
{
	/** Especie o legendaria. */
	FName Id;
	bool bLegendary = false;
	float WaitSeconds = 0.0f;
	float WeightKg = 0.0f;
	/** Parámetros de la pelea con la caña para este ejemplar. */
	FFishFightParams Fight;
	uint32 FightSeed = 0;
};

/** Pieza que sale del despiece. */
struct EXPLORED_API FButcherYield
{
	FName ItemId;
	int32 Count = 0;
	/** Días hasta pudrirse desde FButcherResult::FreshSinceDays; < 0 si no se pudre. */
	float SpoilAfterDays = -1.0f;
};

/**
 * Resultado del despiece (biblia §4.4). La conservación (ahumar, salar...) es
 * del modelo de cocina: aquí solo salen los ids y desde cuándo cuenta el frescor.
 */
struct EXPLORED_API FButcherResult
{
	TArray<FButcherYield> Yields;
	float Seconds = 0.0f;
	float FreshSinceDays = 0.0f;
};

/**
 * Pesca y marisqueo (GDD §8.9, biblia §4). Funciones puras y deterministas:
 * misma semilla, mismo sitio y misma hora dan el mismo resultado. La fauna
 * marina visible (bancos, tiburones) es otro sistema: aquí la disponibilidad
 * de peces es abstracta, por tablas de especie, para que P-FAUNA pueda
 * visualizarla después con FFishingModel::BiteRatePerSecond.
 */
struct EXPLORED_API FFishingModel
{
	/** Una pieza sin aprovechar se pudre en un día (biblia §4.4). */
	static constexpr float RawSpoilDays = 1.0f;
	/** Bajo este nivel de marea se abren las pozas (bajamar). */
	static constexpr float TidePoolOpenLevel = -0.55f;
	/** Profundidad máxima a la que se ve y se alcanza un pez con arpón desde la superficie. */
	static constexpr float SpearMaxDepthM = 5.0f;
	/** Índice de refracción del agua de mar. */
	static constexpr float WaterRefractiveIndex = 1.34f;

	// --- Tablas -----------------------------------------------------------

	static const TArray<FFishSpecies>& Species();
	static const FFishSpecies* FindSpecies(FName Id);
	static const TArray<FLegendaryCatch>& Legendaries();
	static const FLegendaryCatch* FindLegendary(FName Id);

	static FName BaitItemId(EFishBait Bait);
	static EFishBait BaitFromItemId(FName ItemId);
	static int32 TrapCapacity(ETrapKind Kind);

	// --- Lectura del entorno ----------------------------------------------

	static EDayPeriod DayPeriod(float Hours);
	static ETidePhase TidePhase(float TideLevel, float TideFlow);
	static EMoonQuarter MoonQuarter(float MoonPhase01);
	/** Hábitat por profundidad del fondo; bReefNearby distingue arrecife de laguna en lo somero. */
	static EFishHabitat HabitatForDepth(float DepthM, bool bReefNearby);

	/**
	 * Ruido que hace el pescador (0–1, GDD §8.2 y biblia §4.2): el peso que
	 * lleva (FSurvivalInputs::CarriedWeightRatio), moverse, chapotear y estar
	 * metido en el agua.
	 */
	static float PlayerNoise(float CarriedWeightRatio, float MoveSpeed01, float SplashesPerMinute, bool bWading);

	// --- Picadas ----------------------------------------------------------

	/** Picadas por segundo real de una especie en unas condiciones (0 si no puede estar ahí). */
	static float BiteRatePerSecond(const FFishSpecies& Species, const FFishingConditions& Conditions);

	/** Encuentros por segundo con una legendaria (0 si ya se capturó, fuera de su sitio o espantada). */
	static float LegendaryRatePerSecond(const FLegendaryCatch& Legend, const FFishingConditions& Conditions,
		const FFishingSaveState* State, float NowDays);

	/** Suma de todas las tasas: lo que el HUD o la fauna pueden usar como «¿hay vida aquí?». */
	static float TotalBiteRatePerSecond(const FFishingConditions& Conditions, const FFishingSaveState* State, float NowDays);

	/**
	 * Espera de picada segundo a segundo, determinista por semilla, sitio
	 * (SpotKey) e instante de inicio. False si no pica en MaxWaitSeconds.
	 */
	static bool WaitForBite(const FFishingConditions& Conditions, const FFishingSaveState* State, uint32 Seed,
		int32 SpotKey, float StartDays, float MaxWaitSeconds, FFishBite& OutBite);

	// --- Arpón y red --------------------------------------------------------

	/**
	 * Profundidad real de un pez que se ve a ApparentDepthM bajo la superficie,
	 * a HorizontalDistM en horizontal, con los ojos a EyeHeightM sobre el agua.
	 * Siempre mayor que la aparente (el agua «sube» los peces): hay que
	 * apuntar por debajo (en vertical, n veces más hondo).
	 */
	static float TrueDepthFromApparent(float EyeHeightM, float HorizontalDistM, float ApparentDepthM);

	/**
	 * Probabilidad de acertar. DistanceM hasta el pez; AimErrorM, distancia
	 * entre donde se apunta y donde está de verdad. Bajo el agua el
	 * lanzamiento pierde alcance.
	 */
	static float SpearHitChance(ESpearStrike Strike, float DistanceM, float AimErrorM, float Wariness, bool bUnderwater);

	/** Lance de red de mano en lo somero: de 0 a 3 peces pequeños. */
	static TArray<FTrapCatch> CastNet(const FFishingConditions& Conditions, uint32 Seed, int32 SpotKey, float NowDays);

	// --- Trampas y pozas (sin animal visible, GDD §10) -------------------------

	/** Simula la trampa hasta ToDays, hora a hora, sin pasar de su capacidad. */
	static void AdvanceTrap(FPlacedTrap& Trap, float ToDays, uint32 WorldSeed);

	/** Revisa la trampa: la simula hasta NowDays y se lleva lo que haya. */
	static TArray<FTrapCatch> CollectTrap(FPlacedTrap& Trap, float NowDays, uint32 WorldSeed);

	/** Índice de la bajamar más cercana (dos al día). */
	static int32 LowTideIndex(float TotalDays);

	/** Marisquea una poza: solo en bajamar y una vez por bajamar; las mareas vivas dan más. */
	static TArray<FTrapCatch> GatherTidePool(FFishingSaveState& State, int32 PoolId, float TotalDays, uint32 Seed);

	// --- Despiece -----------------------------------------------------------

	/**
	 * Despieza un pez o una legendaria (biblia §4.4). KnifeEdge01 = Filo / 5
	 * del cuchillo; sin filo no se puede. El marisco se cocina entero: false.
	 */
	static bool Butcher(FName CatchId, float WeightKg, float KnifeEdge01, float NowDays, FButcherResult& OutResult);

	// --- Ecosistema -----------------------------------------------------------

	/** Clave de zona (celdas de 100 m) y de sitio de pesca (celdas de 5 m) a partir de una posición en cm. */
	static int32 ZoneKeyAt(const FVector2D& LocationCm);
	static int32 SpotKeyAt(const FVector2D& LocationCm);

	static float ZoneDepletion(const FFishingSaveState& State, int32 ZoneKey, float NowDays);
	static void RegisterCatch(FFishingSaveState& State, int32 ZoneKey, float NowDays);

	/** Si un pescador ruidoso se acerca a El Errante, este desaparece unos días. */
	static void NoteLegendaryPresence(FFishingSaveState& State, FName LegendId, float Noise01, float NowDays);
};
