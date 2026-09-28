#pragma once

#include "CoreMinimal.h"
#include "Save/SaveValue.h"

/**
 * Animales domésticos y corrales [F2] (GDD v2 §3.6, biblia 02 §10.2 y §11.5, borrador
 * `livestock` de Content/Data/fases_futuras.json). Modelo puro: un `FLivestockModel` es
 * la granja de una base, con sus corrales y los animales que viven en ellos. El tiempo
 * va en días de juego enteros: el subsistema llama a `EndDay` al cerrar cada día.
 *
 * Reglas:
 * - Tres especies: gallina (gallinero), cerdo (pocilga) y cabra. El corral genérico
 *   admite cualquiera de las tres; el gallinero solo gallinas y la pocilga solo cerdos.
 * - Tope: `MaxAnimalsPerPen` (8) por corral y `MaxAlivePerBase` (8) vivos en toda la
 *   base (biblia 02 §10.2, mitigación de rendimiento). Un corral o una base llenos no
 *   admiten animales nuevos ni crías.
 * - Comida: el comedero guarda unidades (una por fruta o tubérculo); al cerrar el día
 *   cada animal, por orden de id, come `FeedPerDay`. Si no hay, pasa el día sin comer.
 * - Doma: un animal capturado llega sin domar y se doma tras `DaysFedToTame` días
 *   seguidos comiendo en el corral. Uno domado que pasa `DaysUnfedToWild` días seguidos
 *   sin comer vuelve a salvaje: no muere, pero huye si se abre el corral. Los nacidos en
 *   el corral y los del trueque llegan domados.
 * - Cría: en cada corral y especie se emparejan machos y hembras adultos, domados y que
 *   han comido ese día (por orden de id). Cada pareja tiene `BreedingChancePerDay` (15 %)
 *   de dar una cría ese día. Si no hay sitio para todas, los partos entran en un orden
 *   que sale del hash del día, no del id del corral. La cría es adulta a los
 *   `DaysToAdult` (6) días cerrados.
 * - Productos: una gallina adulta que ha comido pone un huevo al día; una cabra adulta
 *   que ha comido da leche si su última cría aún no es adulta. Se acumulan en el corral
 *   hasta `MaxStoredProducts` y se recogen con `Collect`.
 * - Determinismo: las tiradas salen de un hash de semilla + corral + día + pareja, así
 *   que la misma partida da las mismas crías en cualquier máquina y en cualquier orden
 *   de carga.
 *
 * Red (biblia 08 §2.7 b y §5.5): el modelo vive solo en el servidor. Los clientes piden
 * echar comida, recoger o abrir la puerta por RPC y el servidor llama a este modelo. Cada
 * animal es fauna terrestre replicada (`FLivestockAnimalNet`, dentro del tope duro de
 * §2.7 b) y, lejos del corral, basta con el estado agregado `FLivestockPenNet`.
 */

enum class ELivestockSpecies : uint8
{
	Chicken,
	Pig,
	Goat,
	Count,
};

enum class ELivestockSex : uint8
{
	Female,
	Male,
};

/** Estructura del kit de construcción que hace de corral (ids de building_pieces.json). */
enum class ELivestockPenKind : uint8
{
	/** `gallinero`: solo gallinas. */
	Coop,
	/** `pocilga`: solo cerdos. */
	Sty,
	/** `corral`: cualquier especie doméstica. */
	Pen,
	Count,
};

/** De dónde sale un animal que entra en el corral. */
enum class ELivestockOrigin : uint8
{
	/** Capturado con trampa: llega sin domar. */
	Captured,
	/** Trueque con el pueblo del arrecife [F3]: llega domado. */
	Traded,
	/** Nacido en el corral: domado y cría. */
	Born,
};

enum class ELivestockResult : uint8
{
	Ok,
	UnknownPen,
	UnknownAnimal,
	/** La especie no vive en ese tipo de corral. */
	WrongPen,
	/** El corral ya tiene MaxAnimalsPerPen animales. */
	PenFull,
	/** La base ya tiene MaxAlivePerBase animales vivos. */
	BaseFull,
	/** No se retira un corral con animales dentro. */
	PenNotEmpty,
	/** El comedero está lleno. */
	TroughFull,
	InvalidArgument,
};

EXPLORED_API const TCHAR* LexToString(ELivestockSpecies Species);
EXPLORED_API const TCHAR* LexToString(ELivestockPenKind Kind);
EXPLORED_API const TCHAR* LexToString(ELivestockResult Result);

struct EXPLORED_API FLivestockSettings
{
	/** Biblia 02 §10.2: tope de animales vivos por base. */
	int32 MaxAlivePerBase = 8;
	/** Encargo de F2: tope por corral (con un solo corral coincide con el de la base). */
	int32 MaxAnimalsPerPen = 8;
	/** Biblia 02 §10.2: probabilidad diaria de cría por pareja. */
	float BreedingChancePerDay = 0.15f;
	/** Biblia 02 §10.2: días cerrados que tarda una cría en ser adulta. */
	int32 DaysToAdult = 6;
	/** Biblia 02 §10.2: unidades de comida por animal y día. */
	int32 FeedPerDay = 1;
	/** Biblia 02 §11.5: días seguidos comiendo para domar un animal capturado. */
	int32 DaysFedToTame = 3;
	/** Biblia 02 §11.5: días seguidos sin comer para que un animal domado vuelva a salvaje. */
	int32 DaysUnfedToWild = 2;
	/** Propuesta: el comedero guarda dos días de comida para un corral lleno. */
	int32 TroughCapacity = 16;
	/** Propuesta: huevos o leche que se acumulan en el corral antes de perderse. */
	int32 MaxStoredProducts = 12;
	/** Días que `EndDay` recupera de golpe si se salta alguno (acota datos corruptos). */
	int32 MaxCatchUpDays = 60;

	/** Devuelve una copia con cada valor en su rango (nunca topes negativos ni NaN). */
	FLivestockSettings Sanitized() const;
};

struct EXPLORED_API FLivestockAnimal
{
	int32 Id = INDEX_NONE;
	ELivestockSpecies Species = ELivestockSpecies::Chicken;
	ELivestockSex Sex = ELivestockSex::Female;
	int32 PenId = INDEX_NONE;
	/** Días cerrados desde que nació; los animales que llegan de fuera entran adultos. */
	int32 AgeDays = 0;
	bool bAdult = true;
	bool bTame = true;
	/** Días seguidos comiendo sin estar domado (hacia DaysFedToTame). */
	int32 TameDays = 0;
	/** Días seguidos sin comer. */
	int32 UnfedDays = 0;
	/** Comió en el último día cerrado. */
	bool bFedToday = false;
	/** Día en que tuvo su última cría (INDEX_NONE = nunca). */
	int32 LastBirthDay = INDEX_NONE;

	bool operator==(const FLivestockAnimal& Other) const;
};

struct EXPLORED_API FLivestockPen
{
	int32 Id = INDEX_NONE;
	ELivestockPenKind Kind = ELivestockPenKind::Pen;
	/** Unidades de comida en el comedero. */
	int32 Feed = 0;
	int32 Eggs = 0;
	int32 Milk = 0;

	bool operator==(const FLivestockPen& Other) const;
};

/** Una cría nacida al cerrar un día. */
struct EXPLORED_API FLivestockBirth
{
	int32 AnimalId = INDEX_NONE;
	int32 MotherId = INDEX_NONE;
	int32 PenId = INDEX_NONE;
	ELivestockSpecies Species = ELivestockSpecies::Chicken;
	int32 Day = 0;
};

/** Lo que pasó al cerrar uno o varios días; el subsistema lo convierte en estadísticas y avisos. */
struct EXPLORED_API FLivestockDayReport
{
	TArray<FLivestockBirth> Births;
	/** Animales que se han domado. */
	TArray<int32> Tamed;
	/** Animales domados que han vuelto a salvaje. */
	TArray<int32> WentWild;
	/** Crías que se han hecho adultas. */
	TArray<int32> GrewUp;
	int32 EggsLaid = 0;
	int32 MilkGiven = 0;
	/** Días cerrados de verdad (0 si el día ya estaba cerrado). */
	int32 DaysClosed = 0;
};

/** Lo recogido de un corral. */
struct EXPLORED_API FLivestockCollect
{
	int32 Eggs = 0;
	int32 Milk = 0;
};

/**
 * Estado replicado de un animal (biblia 08 §2.7 b): especie y banderas caben en los
 * bytes `uint8` especie y `uint8` banderas de los 14 B de la fauna terrestre.
 */
struct EXPLORED_API FLivestockAnimalNet
{
	uint8 Species = 0;
	/** Bit 0 macho, 1 adulto, 2 domado, 3 comió hoy. */
	uint8 Flags = 0;

	static constexpr uint8 FlagMale = 1 << 0;
	static constexpr uint8 FlagAdult = 1 << 1;
	static constexpr uint8 FlagTame = 1 << 2;
	static constexpr uint8 FlagFed = 1 << 3;

	bool operator==(const FLivestockAnimalNet& Other) const { return Species == Other.Species && Flags == Other.Flags; }
};

/**
 * Estado agregado de un corral para los clientes que están lejos (propuesta de
 * fases_futuras.json: fuera de 60 m se replica el corral, no cada animal). Son 7 B:
 * adultos y crías por especie (4 bits cada uno, hasta 15), comida, huevos, leche (hasta
 * 255 cada uno) y si está lleno.
 */
struct EXPLORED_API FLivestockPenNet
{
	uint8 Adults[static_cast<int32>(ELivestockSpecies::Count)] = {};
	uint8 Young[static_cast<int32>(ELivestockSpecies::Count)] = {};
	uint8 Feed = 0;
	uint8 Eggs = 0;
	uint8 Milk = 0;
	bool bFull = false;

	/** Bytes que viajan por la red. */
	static constexpr int32 PackedBytes = 7;
	void Pack(uint8 (&Out)[PackedBytes]) const;
	static FLivestockPenNet Unpack(const uint8 (&In)[PackedBytes]);

	bool operator==(const FLivestockPenNet& Other) const;
};

class EXPLORED_API FLivestockModel
{
public:
	/** Estadísticas de achievements.json que informa la granja (biblia 07 §2.1). */
	static const TCHAR* StatSpeciesRaised;
	static const TCHAR* StatEggsCollected;
	/** Objetos que produce (items.json / pendingItems de fases_futuras.json). */
	static const TCHAR* EggItem;
	static const TCHAR* MilkItem;
	/** Distancia a partir de la cual un cliente recibe el corral agregado y no cada animal. */
	static constexpr float AggregateNetDistanceCm = 6000.0f;

	FLivestockModel() = default;
	explicit FLivestockModel(const FLivestockSettings& InSettings, uint32 InSeed = 0);

	void Reset();
	const FLivestockSettings& GetSettings() const { return Settings; }
	uint32 GetSeed() const { return Seed; }

	// --- Nombres de datos ---

	/** Id de la especie en fases_futuras.json y en el stat `livestock_species_raised`. */
	static FName SpeciesId(ELivestockSpecies Species);
	static bool ParseSpecies(FName Id, ELivestockSpecies& Out);
	/** Id de la pieza en building_pieces.json. */
	static FName PenPieceId(ELivestockPenKind Kind);
	static bool ParsePenPiece(FName Id, ELivestockPenKind& Out);
	static bool PenAccepts(ELivestockPenKind Kind, ELivestockSpecies Species);

	// --- Corrales ---

	/** Da de alta un corral al colocar la pieza. Devuelve su id. */
	int32 AddPen(ELivestockPenKind Kind);
	/** Retira un corral vacío (con animales dentro devuelve PenNotEmpty). */
	ELivestockResult RemovePen(int32 PenId);
	const FLivestockPen* FindPen(int32 PenId) const;
	const TArray<FLivestockPen>& GetPens() const { return Pens; }

	// --- Animales ---

	/** Mete un animal adulto en el corral. OutId recibe su id si todo va bien. */
	ELivestockResult AddAnimal(int32 PenId, ELivestockSpecies Species, ELivestockSex Sex, ELivestockOrigin Origin,
		int32& OutId);
	ELivestockResult CanAddAnimal(int32 PenId, ELivestockSpecies Species) const;
	/** Sacrificio, venta o muerte: el animal desaparece de la granja. */
	ELivestockResult RemoveAnimal(int32 AnimalId);
	/** Pasa un animal a otro corral de la base (respeta tipo y tope del destino). */
	ELivestockResult MoveAnimal(int32 AnimalId, int32 ToPenId);
	const FLivestockAnimal* FindAnimal(int32 AnimalId) const;
	const TArray<FLivestockAnimal>& GetAnimals() const { return Animals; }
	int32 NumAnimals() const { return Animals.Num(); }
	int32 NumAnimalsInPen(int32 PenId) const;
	/** Lleno se ve lleno (biblia 02 §10.2): el corral no admite ni un animal más. */
	bool IsPenFull(int32 PenId) const;
	bool IsBaseFull() const { return Animals.Num() >= Settings.MaxAlivePerBase; }

	// --- Acciones del jugador (RPC al servidor) ---

	/** Echa Count unidades de comida al comedero. OutAccepted es lo que cabe; el resto vuelve a la mano. */
	ELivestockResult AddFeed(int32 PenId, int32 Count, int32& OutAccepted);
	/** Recoge huevos y leche del corral. */
	ELivestockResult Collect(int32 PenId, FLivestockCollect& Out);
	/** Abre la puerta: los animales sin domar se escapan y salen de la granja. */
	ELivestockResult OpenGate(int32 PenId, TArray<int32>& OutEscaped);

	// --- Tiempo ---

	/**
	 * Cierra el día Day. Si faltan días entre el último cerrado y Day, los cierra todos
	 * por orden (como mucho MaxCatchUpDays). Un día ya cerrado no hace nada.
	 */
	FLivestockDayReport EndDay(int32 Day);
	int32 GetLastEndedDay() const { return LastEndedDay; }

	// --- Red ---

	static FLivestockAnimalNet MakeAnimalNet(const FLivestockAnimal& Animal);
	FLivestockPenNet MakePenNet(int32 PenId) const;

	// --- Guardado ---

	FSaveValue ToValue() const;
	/** Carga un guardado. Si el dato está roto devuelve false y deja la granja vacía. */
	bool FromValue(const FSaveValue& Value);

	bool operator==(const FLivestockModel& Other) const;

private:
	void CloseOneDay(int32 Day, FLivestockDayReport& Report);
	FLivestockPen* FindPenMutable(int32 PenId);
	FLivestockAnimal* FindAnimalMutable(int32 AnimalId);
	bool BreedingRoll(int32 PenId, int32 Day, int32 PairIndex) const;
	ELivestockSex OffspringSex(int32 PenId, int32 Day, int32 PairIndex) const;

	FLivestockSettings Settings;
	uint32 Seed = 0;
	int32 LastEndedDay = INDEX_NONE;
	int32 NextPenId = 1;
	int32 NextAnimalId = 1;
	TArray<FLivestockPen> Pens;
	TArray<FLivestockAnimal> Animals;
};
