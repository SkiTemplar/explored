#pragma once

#include "CoreMinimal.h"

/**
 * Logros y estadísticas de juego (GDD §16). Modelo puro: solo CoreMinimal.
 *
 * Cualquier sistema informa de lo que pasa con eventos genéricos de estadística
 * (Report) y el modelo decide qué logros se desbloquean. Las definiciones llegan
 * ya parseadas (la capa de Unreal lee Content/Data/achievements.json) y el estado
 * es un struct plano que el guardado copia tal cual.
 *
 * Catálogo de estadísticas y contrato para los demás sistemas:
 * docs/tecnico/estadisticas.md.
 */

/** Tipo de una estadística: cómo acumula lo que se le informa. */
enum class EAchievementStatKind : uint8
{
	Counter,   // Suma cantidades positivas (hogueras encendidas, metros navegados).
	Maximum,   // Se queda con el mayor valor informado (profundidad, días vividos).
	Set,       // Conjunto de ids distintos (islas pisadas, comidas probadas).
	Flag       // Se marca una vez y ya no se desmarca (costa dibujada).
};

/** Ámbito de una estadística. */
enum class EAchievementStatScope : uint8
{
	Profile,   // Vida entera del jugador, a través de todas sus partidas.
	Run        // Solo la partida actual: se vacía con BeginRun y viaja con su guardado.
};

/** Fase del acceso anticipado en que el logro se puede conseguir (biblia 07 §2). Ordenadas. */
enum class EAchievementPhase : uint8
{
	EarlyAccess,   // «AA»: acceso anticipado.
	Phase2,        // «F2»: raíles, granja, murallas, resto de islas.
	Phase3         // «F3»: pueblo, piratas, isla oculta.
};

/** Rareza estimada para el icono de Steam (biblia 07 §2); no es un dato medido. */
enum class EAchievementRarity : uint8
{
	Common,     // «comun»: más del 40 % de quienes empiezan a jugar.
	Uncommon,   // «infrecuente»: 15–40 %.
	Rare,       // «raro»: 4–15 %.
	VeryRare    // «muy_raro»: menos del 4 %.
};

/**
 * A quién llega el logro en cooperativo (biblia 08 §5.7). El servidor decide el hecho y
 * avisa a cada cliente que corresponda; cada cliente desbloquea en su propia cuenta.
 */
enum class EAchievementCoopScope : uint8
{
	Actor,     // «actor»: solo quien hace la acción.
	World,     // «world»: todos los conectados en ese momento.
	Witness    // «witness»: quien esté a menos de FAchievementsModel::WitnessRadiusMeters del hecho.
};

EXPLORED_API const TCHAR* LexToString(EAchievementStatKind Kind);
EXPLORED_API const TCHAR* LexToString(EAchievementStatScope Scope);
EXPLORED_API const TCHAR* LexToString(EAchievementPhase Phase);
EXPLORED_API const TCHAR* LexToString(EAchievementRarity Rarity);
EXPLORED_API const TCHAR* LexToString(EAchievementCoopScope Scope);

/** «AA», «F2», «F3» → fase; false si no la reconoce. */
EXPLORED_API bool ParseAchievementPhase(const FString& Text, EAchievementPhase& OutPhase);
/** «comun», «infrecuente», «raro», «muy_raro» → rareza; false si no la reconoce. */
EXPLORED_API bool ParseAchievementRarity(const FString& Text, EAchievementRarity& OutRarity);
/** «actor», «world», «witness» → alcance; false si no lo reconoce. */
EXPLORED_API bool ParseAchievementCoopScope(const FString& Text, EAchievementCoopScope& OutScope);

struct EXPLORED_API FAchievementStatDef
{
	FName Id;
	EAchievementStatKind Kind = EAchievementStatKind::Counter;
	EAchievementStatScope Scope = EAchievementStatScope::Profile;
};

enum class EAchievementConditionKind : uint8
{
	Compare,   // Stat (contador, máximo o tamaño de conjunto) Op Value.
	Contains,  // El conjunto Stat contiene Item.
	Flag,      // La marca Stat está puesta.
	All,       // Todas las condiciones hijas.
	Any,       // Al menos una de las hijas.
	Not        // Negación de su única hija.
};

enum class EAchievementCompareOp : uint8
{
	GreaterEqual,
	Greater,
	LessEqual,
	Less,
	Equal
};

/** Convierte «>=», «>», «<=», «<», «==» en operador; false si no lo reconoce. */
EXPLORED_API bool ParseAchievementCompareOp(const FString& Text, EAchievementCompareOp& OutOp);

/** Nodo del lenguaje de condiciones de achievements.json. */
struct EXPLORED_API FAchievementCondition
{
	EAchievementConditionKind Kind = EAchievementConditionKind::Compare;
	FName Stat;
	EAchievementCompareOp Op = EAchievementCompareOp::GreaterEqual;
	double Value = 0.0;
	FName Item;
	TArray<FAchievementCondition> Children;

	static FAchievementCondition Compare(FName InStat, EAchievementCompareOp InOp, double InValue);
	static FAchievementCondition AtLeast(FName InStat, double InValue) { return Compare(InStat, EAchievementCompareOp::GreaterEqual, InValue); }
	static FAchievementCondition Contains(FName InStat, FName InItem);
	static FAchievementCondition Flag(FName InStat);
	static FAchievementCondition All(TArray<FAchievementCondition> InChildren);
	static FAchievementCondition Any(TArray<FAchievementCondition> InChildren);
	static FAchievementCondition Not(FAchievementCondition InChild);
};

/** Un logro tal como viene de achievements.json. Los textos se convierten a FText en la capa de Unreal. */
struct EXPLORED_API FAchievementDef
{
	FName Id;
	FString NameEs;
	FString NameEn;
	FString DescriptionEs;
	FString DescriptionEn;
	bool bHidden = false;
	/** Pista de icono (palabra genérica: «fuego», «mapa»…); la UI decide el dibujo. */
	FName Icon;
	/** Modos de juego en que se puede conseguir («Explorer», «Survivor», «Castaway», «Custom»). Vacío: todos. */
	TArray<FName> Modes;
	EAchievementPhase Phase = EAchievementPhase::EarlyAccess;
	EAchievementRarity Rarity = EAchievementRarity::Common;
	EAchievementCoopScope CoopScope = EAchievementCoopScope::Actor;
	FAchievementCondition Condition;
};

/** Valores de un ámbito de estadísticas. Datos planos para el guardado. */
struct EXPLORED_API FAchievementStatValues
{
	/** Contadores y máximos. */
	TMap<FName, double> Numbers;
	/** Conjuntos de ids, sin repetidos y en orden de llegada. */
	TMap<FName, TArray<FName>> Sets;
	/** Marcas puestas. */
	TArray<FName> Flags;

	/** Igualdad por contenido (el orden de los mapas no importa; el de cada conjunto, sí). */
	bool operator==(const FAchievementStatValues& Other) const;
};

/**
 * Estado completo y copiable. El guardado de perfil lleva Profile y Unlocked;
 * el de cada partida lleva Run y RunMode.
 */
struct EXPLORED_API FAchievementsState
{
	FAchievementStatValues Profile;
	FAchievementStatValues Run;
	/** Modo de la partida en curso; NAME_None fuera de partida (menú principal). */
	FName RunMode;
	/** Logros desbloqueados, en orden de desbloqueo. */
	TArray<FName> Unlocked;

	bool operator==(const FAchievementsState& Other) const;
};

class EXPLORED_API FAchievementsModel
{
public:
	/**
	 * Carga el catálogo de estadísticas y los logros. Valida ids repetidos,
	 * estadísticas desconocidas y tipos incompatibles (Contains exige un
	 * conjunto, Flag una marca, Compare cualquier cosa menos una marca).
	 * Si falla deja el modelo vacío y explica el primer error.
	 */
	bool Configure(const TArray<FAchievementStatDef>& InStats, const TArray<FAchievementDef>& InAchievements, FString& OutError);

	/** Empieza una partida nueva: vacía las estadísticas de partida y fija el modo. */
	void BeginRun(FName Mode);

	/**
	 * Evento genérico de estadística. Según el tipo de Stat:
	 * contador suma Amount (> 0), máximo guarda Amount si supera lo anterior,
	 * conjunto añade Item (Amount se ignora) y marca se pone (ambos se ignoran).
	 * Devuelve los logros que se desbloquean por este evento, cada uno una sola vez
	 * en toda la vida del perfil. Una estadística desconocida no hace nada.
	 */
	TArray<FName> Report(FName Stat, double Amount = 1.0, FName Item = NAME_None);

	/** Atajo para conjuntos. */
	TArray<FName> ReportItem(FName Stat, FName Item) { return Report(Stat, 1.0, Item); }

	/** Revisa todos los logros pendientes (tras cargar, o si cambian los datos). */
	TArray<FName> Evaluate();

	/** Progreso 0–1 para la UI (1 si ya está desbloqueado). Ver AchievementsModel.cpp para las reglas. */
	float GetProgress(FName AchievementId) const;

	bool IsUnlocked(FName AchievementId) const { return State.Unlocked.Contains(AchievementId); }
	/** Si el logro se puede conseguir en el modo de la partida actual. */
	bool IsAvailableInCurrentMode(const FAchievementDef& Def) const;

	/**
	 * Fase publicada del juego. Los logros de fases posteriores no se desbloquean ni cuentan
	 * en la lista, pero sus estadísticas se siguen acumulando: al subir la fase, Evaluate
	 * desbloquea lo que ya se hubiera cumplido. Por defecto todo está publicado.
	 */
	void SetReleasedPhase(EAchievementPhase Phase) { ReleasedPhase = Phase; }
	EAchievementPhase GetReleasedPhase() const { return ReleasedPhase; }
	bool IsReleased(const FAchievementDef& Def) const { return Def.Phase <= ReleasedPhase; }

	/** Radio de «witness» en cooperativo (biblia 08 §5.7), en metros. */
	static constexpr float WitnessRadiusMeters = 50.0f;

	/**
	 * Si un logro que decide el servidor llega a un jugador concreto (biblia 08 §5.7).
	 * bIsActor: ese jugador hizo la acción. DistanceMeters: distancia del jugador al hecho
	 * (solo cuenta para «witness»; un valor no finito o negativo no llega).
	 */
	static bool ReachesPlayer(EAchievementCoopScope Scope, bool bIsActor, float DistanceMeters);

	bool IsKnownStat(FName Stat) const { return StatIndex.Contains(Stat); }
	const FAchievementStatDef* FindStat(FName Stat) const;
	const FAchievementDef* FindAchievement(FName AchievementId) const;
	const TArray<FAchievementDef>& GetAchievements() const { return Achievements; }
	const TArray<FAchievementStatDef>& GetStats() const { return Stats; }

	double GetNumber(FName Stat) const;
	int32 GetSetSize(FName Stat) const;
	bool SetContains(FName Stat, FName Item) const;
	bool HasFlag(FName Stat) const;

	/** Estado plano para guardar. */
	const FAchievementsState& GetState() const { return State; }
	/** Restaura un estado guardado sin anunciar nada; llama a Evaluate después si procede. */
	void RestoreState(const FAchievementsState& InState) { State = InState; }

private:
	const FAchievementStatValues& ValuesFor(const FAchievementStatDef& Def) const;
	FAchievementStatValues& ValuesFor(const FAchievementStatDef& Def);
	bool Holds(const FAchievementCondition& Condition) const;
	float Progress(const FAchievementCondition& Condition) const;
	double MeasureOf(const FAchievementCondition& Condition) const;
	bool ValidateCondition(const FAchievementCondition& Condition, const FString& Where, FString& OutError) const;
	void CollectStats(const FAchievementCondition& Condition, int32 AchievementIndex);
	void TryUnlock(int32 AchievementIndex, TArray<FName>& OutUnlocked);

	TArray<FAchievementStatDef> Stats;
	TMap<FName, int32> StatIndex;
	TArray<FAchievementDef> Achievements;
	TMap<FName, int32> AchievementIndex;
	/** Qué logros dependen de cada estadística (para no evaluar los 30 en cada evento). */
	TMap<FName, TArray<int32>> Dependents;
	FAchievementsState State;
	EAchievementPhase ReleasedPhase = EAchievementPhase::Phase3;
};
