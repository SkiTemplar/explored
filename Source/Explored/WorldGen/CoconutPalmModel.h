#pragma once

#include "CoreMinimal.h"

/**
 * Los cocos caen al sacudir (GDD v2 §3.18, biblia 02 §1.2 y §13.1).
 *
 * La copa de cada palmera adulta tiene unos huecos para cocos. En cada hueco
 * cuaja un coco verde, que madura, cuelga maduro unos días y cae solo al suelo,
 * donde se pudre si nadie lo recoge; entonces el hueco vuelve a cuajar. Sobre
 * ese ciclo natural actúan cuatro cosas:
 *   - Sacudir el tronco suelta cocos maduros con una probabilidad que crece con
 *     lo flojos que están y con la fuerza de la sacudida. El verde no cae nunca.
 *   - Las rachas de un temporal sacuden igual que el jugador, una vez por hora.
 *   - Trepar (biblia 02 §13.1) coge un coco de la copa, verde o maduro.
 *   - Talar suelta lo que queda en la copa (parte del maduro se abre con el golpe)
 *     y la copa se queda vacía: sacudir y luego talar no da cocos de más.
 *
 * Todo es entero (minutos de juego) y cada momento del ciclo sale de un hash de
 * (semilla, hueco, generación), así que avanzar 30 días de golpe o 43 200 veces
 * un minuto da exactamente los mismos cocos en las mismas posiciones. Una
 * palmera que nadie ha tocado no necesita guardar nada: se reconstruye con
 * Initialize y Advance desde el principio del mundo.
 */

enum class ECoconutStage : uint8
{
	Empty,	// hueco sin coco (esperando a cuajar)
	Green,	// coco verde: no cae solo ni al sacudir
	Mature	// coco maduro: se suelta al sacudir y acaba cayendo solo
};

/** Números de la especie. Se sanean al usarlos (ver FCoconutPalmModel::Sanitize). */
struct EXPLORED_API FCoconutPalmProfile
{
	/** Huecos para cocos en la copa. */
	int32 Slots = 6;
	/** Días desde que un hueco se vacía hasta que cuaja otro coco (mínimo y máximo, se sortea por coco). */
	int32 RefillMinDays = 2;
	int32 RefillMaxDays = 4;
	/** Días que el coco pasa verde. */
	int32 GreenDays = 5;
	/** Días que cuelga maduro antes de caer solo (mínimo y máximo, se sortea por coco). */
	int32 HangMinDays = 3;
	int32 HangMaxDays = 8;
	/** Días que un coco caído aguanta en el suelo antes de pudrirse. */
	int32 GroundLifeDays = 6;
	/** Altura del tronco, en metros: cuanto más alta, más cuesta sacudirla a mano. */
	float TrunkHeightMeters = 9.0f;
	/** Radio de la copa, en metros (el mismo que FFellingProfile::CrownRadiusMeters). */
	float CrownRadiusMeters = 3.0f;
	/** Fracción de los maduros que se abren al caer la palmera talada (se quedan en cáscara). */
	float CrackChanceOnFell = 0.3f;
};

/** Un hueco de la copa. Su estado a cualquier hora se deduce de estos dos números. */
struct EXPLORED_API FCoconutSlot
{
	/** Cuántos cocos han salido ya de este hueco (caídos, sacudidos, cogidos o talados). */
	int32 Generation = 0;
	/** Minuto en que el hueco se quedó vacío por última vez: empieza el ciclo de la generación actual. */
	int64 CycleStartMinute = 0;
};

/** Un coco en el suelo bajo la palmera. */
struct EXPLORED_API FFallenCoconut
{
	/** Identifica el coco para recogerlo y guardarlo: (hueco << 24) | generación. */
	uint32 Id = 0;
	/** Posición en el plano, en centímetros (unidades de Unreal). */
	FVector2D Position = FVector2D::ZeroVector;
	int64 LandedMinute = 0;
};

/** Qué ha pasado en una sacudida, una racha o una tala (para sonido, animación y daño). */
struct EXPLORED_API FCoconutDrop
{
	/** coco_maduro, coco_verde o cascara_coco. */
	FName ItemId;
	FVector2D Position = FVector2D::ZeroVector;
	/** Id del coco en el suelo; 0 si no queda en la lista de la palmera (los de la tala). */
	uint32 Id = 0;
	/** Ha caído encima de quien sacudía. */
	bool bHitsShaker = false;
};

/** Cuentas para la persistencia, los logros y comprobar que no se crean ni se pierden cocos. */
struct EXPLORED_API FCoconutCounters
{
	int32 NaturalFalls = 0;
	int32 Shaken = 0;
	int32 GustFalls = 0;
	int32 Climbed = 0;
	int32 Felled = 0;
	int32 PickedFromGround = 0;
	int32 Rotted = 0;
};

struct EXPLORED_API FCoconutPalmState
{
	uint32 Seed = 0;
	/** Posición del tronco en el plano, en centímetros. */
	FVector2D TrunkPosition = FVector2D::ZeroVector;
	int64 LastUpdateMinute = 0;
	/** Cuántas sacudidas ha recibido: entra en el hash de cada tirada. */
	uint32 ShakeSerial = 0;
	/** Última hora de juego en la que se aplicó una racha (una por hora como mucho). */
	int64 LastGustHour = -1;
	/** Talada: la copa ya no da cocos (los del suelo siguen pudriéndose). */
	bool bFelled = false;
	TArray<FCoconutSlot> Slots;
	/** Ordenados por (LandedMinute, Id): no depende de cómo se haya avanzado el tiempo. */
	TArray<FFallenCoconut> Ground;
	FCoconutCounters Counters;
};

struct EXPLORED_API FCoconutPalmModel
{
	static constexpr int64 MinutesPerDay = 1440;
	/** Fuerza mínima que tiene cualquier maduro al sacudir, aunque acabe de madurar. */
	static constexpr float BaseLooseness = 0.35f;
	/** Una sacudida a mano de una palmera de esta altura (en metros) tiene fuerza 1. */
	static constexpr float HandShakeReferenceMeters = 4.5f;
	/** Fuerza mínima a mano, por alta que sea la palmera. */
	static constexpr float MinHandShakeStrength = 0.15f;
	/** Viento (0–1) a partir del cual las rachas sueltan cocos. */
	static constexpr float GustWindThreshold = 0.6f;
	/** Fuerza de la racha con viento 1 (ciclón). */
	static constexpr float MaxGustStrength = 0.5f;
	/** Los cocos caen entre este radio y FallRadiusCrownFraction × copa, alrededor del tronco. */
	static constexpr float MinFallRadiusMeters = 0.5f;
	static constexpr float FallRadiusCrownFraction = 0.6f;
	/** Un coco que cae a menos de esto de quien sacude le da en la cabeza. */
	static constexpr float HeadHitRadiusMeters = 0.35f;

	static const FName MatureItem;
	static const FName GreenItem;
	static const FName ShellItem;

	/** Perfil de la palmera de coco (`Palm` de FFellingModel: 9 m de tronco y 3 m de copa). */
	static FCoconutPalmProfile DefaultProfile();

	/** Corrige números imposibles: huecos negativos, días en cero o mínimos mayores que los máximos, NaN. */
	static FCoconutPalmProfile Sanitize(const FCoconutPalmProfile& Profile);

	/**
	 * Copa nueva. bStocked = true al generar el mundo: cada hueco empieza en un
	 * punto distinto de su ciclo, así que hay verdes y maduros, pero ninguno cae
	 * en el mismo minuto de la creación. bStocked = false para una palmera que
	 * acaba de hacerse adulta al rebrotar: todos los huecos empiezan vacíos.
	 */
	static FCoconutPalmState Initialize(uint32 Seed, const FVector2D& TrunkPosition, const FCoconutPalmProfile& Profile, int64 NowMinute, bool bStocked);

	/**
	 * Avanza hasta NowMinute: los maduros que cumplen su tiempo caen al suelo y
	 * los caídos que cumplen el suyo se pudren. Devuelve cuántos han caído. Una
	 * hora anterior a LastUpdateMinute no hace nada.
	 */
	static int32 Advance(FCoconutPalmState& State, const FCoconutPalmProfile& Profile, int64 NowMinute);

	/** Etapa del hueco a la hora de la última actualización. */
	static ECoconutStage StageOf(const FCoconutPalmState& State, const FCoconutPalmProfile& Profile, int32 SlotIndex);

	/** Cocos en la copa a la hora de la última actualización. */
	static int32 CountOnTree(const FCoconutPalmState& State, const FCoconutPalmProfile& Profile, ECoconutStage Stage);

	/** Fuerza de una sacudida a mano: 1 hasta HandShakeReferenceMeters y después baja con la altura. */
	static float HandShakeStrength(const FCoconutPalmProfile& Profile);

	/** Fuerza de la racha con ese viento (0–1): nada por debajo del umbral. */
	static float GustStrength(float Wind);

	/**
	 * Sacude el tronco en NowMinute con fuerza Strength (0–1). Cada maduro cae con
	 * probabilidad Strength × (BaseLooseness + (1 − BaseLooseness) × flojera),
	 * donde la flojera va de 0 al madurar a 1 al caer solo. Los que caen van al
	 * suelo y se añaden a OutDrops; bHitsShaker marca los que caen encima de
	 * ShakerPosition. Devuelve cuántos han caído.
	 */
	static int32 Shake(FCoconutPalmState& State, const FCoconutPalmProfile& Profile, float Strength, const FVector2D& ShakerPosition, int64 NowMinute, TArray<FCoconutDrop>& OutDrops);

	/**
	 * Racha de viento de la hora HourIndex (minutos HourIndex·60 …). Se aplica una
	 * sola vez por hora y nunca hacia atrás; el motor la llama al cruzar cada hora
	 * con el viento de esa hora, también al ponerse al día al cargar.
	 */
	static int32 ApplyGust(FCoconutPalmState& State, const FCoconutPalmProfile& Profile, float Wind, int64 HourIndex, TArray<FCoconutDrop>& OutDrops);

	/**
	 * Coge un coco de la copa tras trepar (biblia 02 §13.1): el verde si
	 * bWantGreen, si no el maduro. Devuelve el objeto o NAME_None si no hay.
	 */
	static FName PickFromCrown(FCoconutPalmState& State, const FCoconutPalmProfile& Profile, bool bWantGreen, int64 NowMinute);

	/** Recoge el coco caído con ese Id. Devuelve false si no está (ya recogido, podrido o inexistente). */
	static bool PickFromGround(FCoconutPalmState& State, uint32 Id, FFallenCoconut* OutCoconut = nullptr);

	/**
	 * La palmera cae talada hacia FallDirection (en el plano): lo que queda en la
	 * copa acaba en el suelo alrededor de la copa caída. Cada maduro se abre con
	 * CrackChanceOnFell y queda en cascara_coco; los verdes aguantan. La copa se
	 * queda vacía y deja de dar cocos. Devuelve cuántos cocos había en la copa.
	 * Estos drops sustituyen a los coco_* de Palm.FellDrops (ver la nota técnica).
	 */
	static int32 Fell(FCoconutPalmState& State, const FCoconutPalmProfile& Profile, const FVector2D& FallDirection, int64 NowMinute, TArray<FCoconutDrop>& OutDrops);

	/** Minutos del ciclo de la generación del hueco: cuaja, madura y cae solo, contados desde CycleStartMinute. */
	static void CycleOf(const FCoconutPalmState& State, const FCoconutPalmProfile& Profile, int32 SlotIndex, int64& OutSetMinute, int64& OutMatureMinute, int64& OutFallMinute);

	/** Dónde cae el coco de esa generación de ese hueco (en centímetros, absoluto). Determinista. */
	static FVector2D FallPosition(const FCoconutPalmState& State, const FCoconutPalmProfile& Profile, int32 SlotIndex, int32 Generation);

	static uint32 MakeId(int32 SlotIndex, int32 Generation);
};
