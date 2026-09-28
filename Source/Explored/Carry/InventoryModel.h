#pragma once

#include "CoreMinimal.h"

/**
 * Inventario diegético (GDD §8.2, biblia §3.8 y §3.9) como modelo puro.
 *
 * Solo conoce registros planos de objetos (FInventoryItem): id de definición,
 * peso, volumen, tamaño, etiquetas y un id de instancia estable. La capa de
 * Unreal (UCarryComponent) traduce FItemDefinition/FItemInstance a estos
 * registros y guarda aparte el resto de la instancia (calidad, piezas...).
 *
 * Aquí viven todas las reglas de capacidad y peso: manos (los objetos DosManos
 * ocupan las dos y nunca se combinan consigo mismos, revisión H3), bolsillos,
 * cinturón, bolsa estanca, mochila, angarillas y contenedores del mundo.
 * Toda operación que mueve objetos es atómica: si falla, el estado no cambia y
 * se devuelve el motivo (EInventoryFail).
 *
 * Pilas (biblia 03 §1.3): un registro puede llevar hasta MaxStackSize unidades
 * iguales en un solo hueco. WeightKg y VolumeLiters son de UNA unidad; el peso y
 * el volumen que cuentan para la mochila y el límite de carga son unidad × Count.
 */

/** Tamaño a efectos de transporte. Espejo de EItemSize (Items/ItemTypes.h), mismo orden. */
enum class EInventorySize : uint8
{
	Pequeno,
	Mediano,
	Grande,
	DosManos
};

/** Dónde puede estar un objeto que lleva el jugador. */
enum class EInventorySlot : uint8
{
	None,
	HandLeft,
	HandRight,
	Pockets,
	Belt,
	/** Bolsa estanca: bolsillo impermeable de la mochila o bolsa colgada del cinturón. */
	Pouch,
	Backpack,
	/** Angarillas enganchadas detrás del jugador (se arrastran). */
	Sledge,
	Count
};

/** Motivo por el que una operación no se ha podido hacer. La capa de UE lo convierte en texto localizado. */
enum class EInventoryFail : uint8
{
	None,
	InvalidItem,
	NotFound,
	AlreadyThere,
	HandOccupied,
	NeedBothHands,
	HandsFull,
	/** Lo que se tiene en las dos manos es un único objeto DosManos (H3). */
	SameItem,
	TooBig,
	WrongKind,
	ContainerFull,
	TooHeavy,
	NoRoom,
	NoBackpack,
	NoPouch,
	NoSledge,
	SledgeAttached,
	NotEquippable,
	ContainerNotEmpty,
	OverCarryLimit,
	NotALiquidContainer,
	DuplicateId,
	CorruptState,
	/** Las dos pilas no son del mismo objeto (definición, calidad o tamaño distintos) o no apilan. */
	NotStackable,
	/** La pila de destino ya tiene MaxStackSize unidades. */
	StackFull,
	/** Cantidad fuera de rango al partir o gastar una pila. */
	InvalidCount
};

/** Descripción corta en español para registros y mensajes de depuración. */
EXPLORED_API const TCHAR* LexToString(EInventoryFail Fail);

/** Registro plano de un objeto transportable (o de una pila de objetos iguales). */
struct EXPLORED_API FInventoryItem
{
	/** Estable durante toda la partida; 0 = registro vacío. */
	int64 InstanceId = 0;
	FName DefinitionId;
	/** Peso efectivo de UNA unidad (ya con sus piezas), sin el líquido que contenga. */
	float WeightKg = 0.0f;
	/** Volumen de UNA unidad. */
	float VolumeLiters = 0.0f;
	EInventorySize Size = EInventorySize::Pequeno;
	TArray<FName> Tags;
	/** Agua u otro líquido que lleva dentro (cantimplora, coco...). Pesa 1 kg/l. */
	float LiquidLiters = 0.0f;
	/** 0 = no guarda líquido. Ver FInventoryModel::LiquidCapacityFromRecipiente. */
	float LiquidCapacityLiters = 0.0f;
	/** Unidades de la pila (1 = objeto suelto). */
	int32 Count = 1;
	/** Tope de la pila para este objeto: 1 = no apila; ver FInventoryModel::ComputeMaxStack. */
	int32 MaxStack = 1;
	/** Calidad 1–5 (biblia §2.4): dos pilas de calidad distinta no se funden. */
	uint8 Quality = 3;

	bool IsValid() const { return InstanceId != 0 && !DefinitionId.IsNone(); }
	bool IsTwoHanded() const { return Size == EInventorySize::DosManos; }
	bool HasTag(FName Tag) const { return Tags.Contains(Tag); }
	/** Peso de toda la pila más el líquido que lleve dentro. */
	float GetTotalWeightKg() const { return WeightKg * static_cast<float>(Count) + LiquidLiters; }
	float GetTotalVolumeLiters() const { return VolumeLiters * static_cast<float>(Count); }
	bool IsStackable() const { return MaxStack > 1; }
	/** Unidades que aún caben en esta pila. */
	int32 GetStackRoom() const { return FMath::Max(0, MaxStack - Count); }
	/** Misma definición, calidad, tamaño, peso y volumen por unidad, y los dos apilan (sin líquido). */
	bool CanStackWith(const FInventoryItem& Other) const;
	/** Copia con otra cantidad (mismo id); la usa el modelo al partir y fundir pilas. */
	FInventoryItem WithCount(int32 NewCount) const;

	bool operator==(const FInventoryItem& Other) const;
	bool operator!=(const FInventoryItem& Other) const { return !(*this == Other); }
};

/** Objeto dentro de un contenedor con su hueco visible (gancho, balda, posición en el arcón). */
struct EXPLORED_API FInventoryEntry
{
	FInventoryItem Item;
	/**
	 * Índice del hueco donde se ve el objeto. Se asigna el primero libre al
	 * guardar y no cambia al sacar otros: el actor del contenedor lo traduce
	 * a una transformación (lo guardado se ve colocado, GDD §8.2).
	 */
	int32 SlotIndex = 0;

	bool operator==(const FInventoryEntry& Other) const { return Item == Other.Item && SlotIndex == Other.SlotIndex; }
};

/** Reglas de capacidad de un contenedor. Un 0 en un límite significa «sin límite». */
struct EXPLORED_API FInventoryContainerSpec
{
	int32 MaxSlots = 0;
	float MaxVolumeLiters = 0.0f;
	float MaxWeightKg = 0.0f;
	EInventorySize MaxSize = EInventorySize::DosManos;
	/** Vacío = acepta cualquier objeto; si no, el objeto necesita al menos una de estas etiquetas. */
	TArray<FName> AcceptedTags;
	/** Lo que se guarda aquí no se moja (mapa, cerillas, yesca). */
	bool bWaterproof = false;

	bool operator==(const FInventoryContainerSpec& Other) const;

	/** 4 objetos pequeños (GDD §8.2). */
	static FInventoryContainerSpec Pockets();
	/** Enganches para herramientas, recipientes e instrumentos, hasta tamaño Mediano. */
	static FInventoryContainerSpec Belt(int32 Hooks);
	/** Bolsa estanca: 2 objetos pequeños que no se mojan. */
	static FInventoryContainerSpec Pouch();
	/** Mochila por volumen y peso; admite hasta tamaño Mediano. */
	static FInventoryContainerSpec Backpack(float VolumeLiters, float WeightKg);
	/** Angarillas: mucha carga de madera, piedra y material de obra, incluidos troncos DosManos. */
	static FInventoryContainerSpec Sledge();
	/** Contenedores del mundo (biblia §3.9). */
	static FInventoryContainerSpec Basket();
	static FInventoryContainerSpec Shelf();
	static FInventoryContainerSpec Chest();
};

/** Contenedor con su contenido: cualquiera del cuerpo o uno del mundo (cesta, estante, arcón). */
struct EXPLORED_API FInventoryContainer
{
	/** Nombre estable para el guardado (p. ej. «arcon_base_01»); vacío en los del cuerpo. */
	FName Id;
	FInventoryContainerSpec Spec;
	TArray<FInventoryEntry> Entries;

	int32 Num() const { return Entries.Num(); }
	bool IsEmpty() const { return Entries.Num() == 0; }
	float GetUsedWeightKg() const;
	float GetUsedVolumeLiters() const;

	/** Comprueba tamaño, etiquetas, huecos, volumen y peso sin modificar nada. */
	EInventoryFail CanAccept(const FInventoryItem& Item) const;
	/**
	 * Unidades de Item que caben en la pila IntoInstanceId de este contenedor por
	 * tope de pila, volumen y peso (no usa un hueco nuevo). 0 si no es compatible.
	 */
	int32 UnitsThatFitInStack(const FInventoryItem& Item, int64 IntoInstanceId) const;
	/** Guarda en el primer hueco libre. Devuelve false (y no cambia nada) si no cabe. */
	bool Add(const FInventoryItem& Item, EInventoryFail& OutFail);
	bool RemoveById(int64 InstanceId, FInventoryItem& OutItem);

	int32 FindIndexById(int64 InstanceId) const;
	const FInventoryItem* FindById(int64 InstanceId) const;
	/** Hueco visible del objeto o INDEX_NONE. */
	int32 GetSlotIndexOf(int64 InstanceId) const;
	/** Objeto que ocupa un hueco visible o nullptr. */
	const FInventoryItem* FindBySlotIndex(int32 SlotIndex) const;
	int32 FirstFreeSlotIndex() const;
	bool HasItemWithTag(FName Tag) const;

	bool operator==(const FInventoryContainer& Other) const { return Id == Other.Id && Spec == Other.Spec && Entries == Other.Entries; }
};

/** Qué da cada pieza de equipo al ponérsela (tabla de FInventoryModel::FindEquipmentSpec). */
enum class EInventoryEquipment : uint8
{
	None,
	Backpack,
	Belt,
	Sledge
};

struct EXPLORED_API FInventoryEquipmentSpec
{
	EInventoryEquipment Kind = EInventoryEquipment::None;
	float BackpackVolumeLiters = 0.0f;
	float BackpackWeightKg = 0.0f;
	/** Kilos de más que se llevan cómodos (armazón, faja). */
	float ComfortBonusKg = 0.0f;
	/** La mochila trae un bolsillo impermeable (abre la bolsa estanca). */
	bool bWaterproofPocket = false;
	int32 BeltHooks = 0;
};

/** Resultado de guardar fundiendo con las pilas que ya hay (FInventoryModel::StowMerging). */
struct EXPLORED_API FInventoryStowResult
{
	/** Unidades que han llegado al destino. */
	int32 MovedUnits = 0;
	/** Pilas del destino que han crecido, con cuántas unidades cada una. */
	TArray<TPair<int64, int32>> ToppedUp;
	/** El registro original ha ocupado un hueco nuevo en el destino (conserva su id). */
	bool bMovedWhole = false;
	/** El registro original ha desaparecido: todas sus unidades se fundieron en otras pilas. */
	bool bSourceRemoved = false;
	/**
	 * Id nuevo si solo parte del resto cabía en un hueco nuevo (peso o volumen al
	 * límite): esa parte es otra pila y el original sigue en el origen. 0 si no.
	 */
	int64 NewStackId = 0;
	/** Unidades que siguen en el origen (la pila original, más pequeña). */
	int32 LeftAtSource = 0;
};

/**
 * Qué ha quitado FInventoryModel::SanitizeUnknownDefinitions de una partida
 * guardada. Nada se pierde en silencio: lo desconocido se registra y lo conocido
 * que se queda sin sitio (la mochila desconocida desaparece) cae al suelo.
 */
struct EXPLORED_API FInventoryLoadReport
{
	/** Objetos cuya definición ya no existe (contenido retirado en una actualización). */
	TArray<FInventoryItem> Unknown;
	/** Objetos conocidos que se han quedado sin sitio: la capa de UE los suelta a los pies. */
	TArray<FInventoryItem> Orphaned;

	bool IsClean() const { return Unknown.Num() == 0 && Orphaned.Num() == 0; }
};

/** Lo que queda en el suelo al soltar las angarillas (al nadar o a voluntad). */
struct EXPLORED_API FInventorySledgeDrop
{
	FInventoryItem SledgeItem;
	FInventoryContainer Load;
};

/**
 * Estado completo del inventario del jugador, en datos planos para el guardado
 * (P-SAVE). Los contenedores del mundo se guardan aparte, cada uno con su Id.
 */
struct EXPLORED_API FInventoryState
{
	/** Manos: InstanceId 0 = vacía. Con un DosManos, las dos guardan el mismo registro. */
	FInventoryItem HandLeft;
	FInventoryItem HandRight;
	bool bHandsHoldTwoHanded = false;

	FInventoryContainer Pockets;
	FInventoryContainer Belt;
	FInventoryContainer Pouch;
	FInventoryContainer Backpack;
	FInventoryContainer Sledge;

	bool bHasBackpack = false;
	/** Registro de la mochila puesta; vacío si la capacidad se fijó a mano (SetCustomBackpack). */
	FInventoryItem BackpackItem;
	float BackpackComfortBonusKg = 0.0f;
	bool bBackpackWaterproofPocket = false;

	/** Cinturón de cuero puesto (vacío = el cinturón básico de 3 enganches). */
	FInventoryItem BeltItem;

	bool bHasSledge = false;
	FInventoryItem SledgeItem;

	int64 NextInstanceId = 1;

	bool operator==(const FInventoryState& Other) const;
	bool operator!=(const FInventoryState& Other) const { return !(*this == Other); }
};

class EXPLORED_API FInventoryModel
{
public:
	/** Capacidad cómoda sin equipo (la misma que usaba USwimComponent). */
	static constexpr float BaseComfortableKg = 15.0f;
	/** Por encima de este múltiplo de la capacidad cómoda no se puede cargar más en el cuerpo. */
	static constexpr float MaxLoadRatio = 2.0f;
	/** Fracción del peso de las angarillas que cuenta como carga (se arrastran, no se levantan). */
	static constexpr float SledgeDragFactor = 0.3f;
	static constexpr int32 PocketSlots = 4;
	static constexpr int32 BaseBeltHooks = 3;
	static constexpr int32 PouchSlots = 2;
	/** Unidades por hueco para lo que apila (biblia 03 §1.3). Espejo en Tools/DataCheck. */
	static constexpr int32 MaxStackSize = 10;

	/**
	 * Tope de pila de una definición (biblia 03 §1.3): MaxStackSize para recursos
	 * sin durabilidad ni líquido propio; 1 para herramientas y armas (durabilidad),
	 * recipientes (líquido), objetos DosManos, objetos compuestos (cada uno lleva
	 * sus piezas) y lo que tiene una etiqueta de NonStackableTags (equipo,
	 * contenedores, piezas legendarias únicas).
	 */
	static int32 ComputeMaxStack(float MaxDurability, float LiquidCapacityLiters, EInventorySize Size, const TArray<FName>& Tags, bool bComposite);
	/** Etiquetas que nunca apilan. Tools/DataCheck lee esta lista del .cpp. */
	static const TArray<FName>& GetNonStackableTags();
	FInventoryModel();

	/** Ids nuevos para objetos que entran en el inventario (recogidos, fabricados...). */
	int64 AllocateInstanceId();

	// ----------------------------------------------------------------- manos

	/** A la primera mano libre (izquierda antes que derecha); un DosManos exige las dos libres. */
	bool PickUp(const FInventoryItem& Item, EInventoryFail& OutFail);
	/** A una mano concreta (HandLeft/HandRight); un DosManos va a las dos. */
	bool PlaceInHand(const FInventoryItem& Item, EInventorySlot Hand, EInventoryFail& OutFail);
	/** Saca lo de una mano (un DosManos libera las dos). */
	bool RemoveFromHand(EInventorySlot Hand, FInventoryItem& OutItem, EInventoryFail& OutFail);
	/** Vacía las dos manos y devuelve lo que había (un DosManos una sola vez). */
	TArray<FInventoryItem> ClearHands();
	bool SwapHands();
	/** nullptr si la mano está vacía. */
	const FInventoryItem* GetHandItem(EInventorySlot Hand) const;
	bool IsHandEmpty(EInventorySlot Hand) const { return GetHandItem(Hand) == nullptr; }
	bool IsHoldingTwoHanded() const { return State.bHandsHoldTwoHanded; }
	/** Hay dos piezas distintas para combinar. Un DosManos no se combina consigo mismo (H3). */
	bool CanCombineHands(EInventoryFail& OutFail) const;

	// ------------------------------------------------------------- traslados

	/** A la mano: si una mano ya lleva una pila compatible con sitio para todo, se funde en ella (OutMergedInto = su id). */
	bool PickUpMerging(const FInventoryItem& Item, int64& OutMergedInto, EInventoryFail& OutFail);

	// ----------------------------------------------------------------- pilas

	/**
	 * Separa Count unidades de una pila (1..Count-1) y las pone en To (una mano
	 * libre o un contenedor del cuerpo con un hueco libre) con un id nuevo.
	 */
	bool SplitStack(int64 InstanceId, int32 Count, EInventorySlot To, int64& OutNewId, EInventoryFail& OutFail);
	/**
	 * Pasa unidades de FromId a IntoId (dos pilas compatibles que lleva el jugador):
	 * tantas como quepan por tope, volumen, peso y límite de carga. Si FromId se
	 * queda sin unidades, desaparece. Falla (sin cambios) si no cabe ninguna.
	 */
	bool MergeStacks(int64 FromId, int64 IntoId, int32& OutMoved, EInventoryFail& OutFail);
	/**
	 * Guarda en To completando primero las pilas compatibles que ya hay (por orden
	 * de hueco visible) y, si sobra algo, en un hueco nuevo. Si no cabe todo, se
	 * guarda lo que quepa y el resto sigue en el origen. Falla sin cambios si no
	 * cabe ni una unidad. Un objeto que no apila se comporta como Move.
	 */
	bool StowMerging(int64 InstanceId, EInventorySlot To, FInventoryStowResult& OutResult, EInventoryFail& OutFail);
	/** Gasta Count unidades de una pila (comer, echar al fuego, construir); con todas, la quita. */
	bool RemoveUnits(int64 InstanceId, int32 Count, EInventoryFail& OutFail);

	/** Dónde está un objeto del jugador (None si no lo lleva). */
	EInventorySlot FindItem(int64 InstanceId) const;
	const FInventoryItem* FindItemById(int64 InstanceId) const;
	/** Contenedor del cuerpo (Pockets, Belt, Pouch, Backpack, Sledge); nullptr para las manos. */
	const FInventoryContainer* GetContainer(EInventorySlot Slot) const;

	/** ¿Se podría mover? No cambia nada. */
	EInventoryFail CanMove(int64 InstanceId, EInventorySlot To) const;
	bool Move(int64 InstanceId, EInventorySlot To, EInventoryFail& OutFail);

	/** Del jugador a un contenedor del mundo. */
	bool StoreInWorld(int64 InstanceId, FInventoryContainer& World, EInventoryFail& OutFail);
	/** De un contenedor del mundo al jugador (mano o contenedor del cuerpo). */
	bool TakeFromWorld(FInventoryContainer& World, int64 InstanceId, EInventorySlot To, EInventoryFail& OutFail);

	/**
	 * Dónde conviene guardar un objeto: bolsa estanca para lo que no debe
	 * mojarse, cinturón para herramientas e instrumentos, bolsillos para lo
	 * pequeño, mochila para el resto y angarillas para troncos y piedras.
	 * Solo propone destinos con sitio; None si no cabe en ninguno.
	 */
	EInventorySlot SuggestStowSlot(const FInventoryItem& Item) const;
	/** Guarda lo de una mano donde propone SuggestStowSlot. */
	bool AutoStowFromHand(EInventorySlot Hand, EInventorySlot& OutWhere, EInventoryFail& OutFail);
	/** Como AutoStowFromHand, pero fundiendo con las pilas del destino (StowMerging). */
	bool AutoStowMergingFromHand(EInventorySlot Hand, EInventorySlot& OutWhere, FInventoryStowResult& OutResult, EInventoryFail& OutFail);

	// ----------------------------------------------------------------- equipo

	/** Qué equipo es una definición (mochilas, cinturón de cuero, angarillas). */
	static bool FindEquipmentSpec(const FInventoryItem& Item, FInventoryEquipmentSpec& OutSpec);

	/**
	 * Se pone el objeto que hay en una mano: mochila (si ya había otra, el
	 * contenido pasa a la nueva y la vieja queda en esa mano), cinturón de
	 * cuero o angarillas (se enganchan detrás y liberan las manos).
	 */
	bool EquipFromHand(EInventorySlot Hand, EInventoryFail& OutFail);
	/** La mochila tiene que estar vacía; va a la mano indicada. */
	bool UnequipBackpack(EInventorySlot Hand, EInventoryFail& OutFail);
	/** Vuelve al cinturón básico: los enganches que sobran tienen que estar vacíos. */
	bool UnequipBelt(EInventorySlot Hand, EInventoryFail& OutFail);
	/** Mochila sin objeto con capacidad fija (UCarryComponent::SetBackpack, tests). */
	bool SetCustomBackpack(bool bEquipped, float VolumeLiters, float WeightKg, EInventoryFail& OutFail);
	bool HasBackpack() const { return State.bHasBackpack; }
	bool HasPouch() const;
	int32 GetBeltHooks() const { return State.Belt.Spec.MaxSlots; }

	/** Engancha unas angarillas que estaban en el suelo, con su carga. */
	bool AttachSledge(const FInventoryItem& SledgeItem, const FInventoryContainer& Load, EInventoryFail& OutFail);
	bool DetachSledge(FInventorySledgeDrop& OutDrop);
	bool HasSledge() const { return State.bHasSledge; }

	/**
	 * Política al entrar en el agua: las angarillas no flotan con el jugador,
	 * se sueltan en la orilla con su carga. Devuelve true si ha soltado algo.
	 */
	bool ApplyEnterWater(FInventorySledgeDrop& OutDrop);
	/** Con las angarillas enganchadas no se nada ni se trepa. */
	bool CanSwim() const { return !State.bHasSledge; }
	bool CanClimb() const { return !State.bHasSledge; }

	// ------------------------------------------------------------------- peso

	/** Lo que se lleva encima (manos, bolsillos, cinturón, bolsa, mochila y el equipo puesto). */
	float GetBodyWeightKg() const;
	/** Angarillas más su carga. */
	float GetSledgeWeightKg() const;
	float GetComfortableCapacityKg() const;
	/** Peso efectivo / capacidad cómoda; alimenta FSurvivalInputs::CarriedWeightRatio. */
	float GetCarriedWeightRatio() const;
	/** Solo lo que va encima: lo que cansa al nadar. */
	float GetSwimLoadRatio() const;
	/** 0–1: ruido al moverse (asusta a los peces, P-FISH). */
	float GetNoiseLevel() const;
	/** Multiplicador de la velocidad de andar por sobrecarga y arrastre. */
	float GetMoveSpeedMultiplier() const;

	// ---------------------------------------------------------------- consultas

	/** En cualquier sitio del jugador (manos, cuerpo, mochila y, si se pide, angarillas). */
	bool HasItemWithTag(FName Tag, bool bIncludeSledge = false) const;
	/** A mano: manos, bolsillos y cinturón (lo que se puede consultar sin quitarse la mochila). */
	bool HasItemWithTagAtHand(FName Tag) const;
	/** Brújula a mano: el trazo de costa se corrige con el rumbo (GDD §5.5). */
	bool HasCompassAtHand() const;
	/** En la bolsa estanca (o en las angarillas no, que se mojan). */
	bool IsStoredDry(int64 InstanceId) const;
	/** Todos los objetos con esa etiqueta van secos (true si no se lleva ninguno). */
	bool AreTaggedItemsDry(FName Tag) const;

	/** Agua que se lleva en cantimploras y recipientes. */
	float GetCarriedWaterLiters() const;
	/** Llena hasta su capacidad (y hasta el peso libre de la mochila si va dentro). Devuelve los litros añadidos. */
	float FillLiquid(int64 InstanceId, float Liters, EInventoryFail& OutFail);
	/** Devuelve los litros que realmente se han bebido. */
	float DrinkFrom(int64 InstanceId, float Liters);

	/**
	 * Gasta un objeto que lleva el jugador (materiales al construir): lo quita de
	 * donde esté, también de las angarillas. No quita el equipo puesto ni deja la
	 * bolsa estanca sin soporte con cosas dentro (ContainerNotEmpty).
	 */
	bool ConsumeItem(int64 InstanceId, FInventoryItem& OutItem, EInventoryFail& OutFail);

	/**
	 * Sustituye el registro de un objeto por otro con el mismo id y la misma
	 * definición que no pese ni ocupe más (una pila que mengua): se queda en su sitio.
	 */
	bool ShrinkItem(const FInventoryItem& Updated, EInventoryFail& OutFail);
	/** Litros que caben en un recipiente según su propiedad Recipiente (0–5, biblia §2.1). */
	static float LiquidCapacityFromRecipiente(float RecipienteValue) { return FMath::Max(0.0f, RecipienteValue) * 0.25f; }

	// ---------------------------------------------------------------- guardado

	const FInventoryState& GetState() const { return State; }
	/** Carga un estado guardado; si no es coherente, no cambia nada y devuelve false. */
	bool LoadState(const FInventoryState& InState, EInventoryFail& OutFail);
	/** Comprueba ids únicos, manos, capacidades, equipo y pilas (1..MaxStack unidades). */
	static bool ValidateState(const FInventoryState& InState, EInventoryFail& OutFail);
	/**
	 * Quita de una partida guardada los objetos cuya definición ya no existe
	 * (IsKnown devuelve false). Si lo desconocido era equipo (mochila, cinturón,
	 * angarillas), lo que llevaba y ya no cabe pasa a OutReport.Orphaned. Llamar
	 * antes de LoadState. Devuelve true si ha cambiado algo.
	 */
	static bool SanitizeUnknownDefinitions(FInventoryState& InOut, TFunctionRef<bool(FName)> IsKnown, FInventoryLoadReport& OutReport);

private:
	/** Recalcula las capacidades del cinturón, la bolsa, la mochila y las angarillas según el equipo. */
	static void RebuildSpecs(FInventoryState& InOut);
	static bool StateProvidesPouch(const FInventoryState& InState);

	FInventoryContainer* GetMutableContainer(EInventorySlot Slot);
	/** ¿Cabe en el destino? bAlreadyOnBody = su peso ya cuenta en el cuerpo. */
	EInventoryFail CanPlace(const FInventoryItem& Item, EInventorySlot To, bool bAlreadyOnBody) const;
	/** Coloca sin comprobar (llamar solo tras CanPlace). */
	void PlaceUnchecked(const FInventoryItem& Item, EInventorySlot To);
	/** Saca un objeto de donde esté (manos o contenedor del cuerpo). */
	bool RemoveUnchecked(int64 InstanceId, FInventoryItem& OutItem);
	/** Quitar este objeto de donde está dejaría la bolsa estanca sin soporte con cosas dentro. */
	bool WouldOrphanPouch(int64 InstanceId, EInventorySlot From, EInventorySlot To) const;
	FInventoryItem* FindMutableItemById(int64 InstanceId);
	/** Escribe el registro en su sitio (y en las dos manos si es el DosManos). */
	void CommitItem(const FInventoryItem& Updated);
	/** Unidades de Item que caben en la pila IntoId del jugador, con el límite de carga del cuerpo. */
	int32 UnitsThatFitOnPlayerStack(const FInventoryItem& Item, int64 IntoId, bool bAlreadyOnBody) const;

	FInventoryState State;
};
