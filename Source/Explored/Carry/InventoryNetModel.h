#pragma once

#include "CoreMinimal.h"

#include "Carry/InventoryModel.h"
#include "Items/ContentIdTableModel.h"

/**
 * Replicación del inventario propio (biblia 08 §2.4) como modelo puro.
 *
 * Lo que UCarryComponent mandará al dueño con un FFastArraySerializer y
 * COND_OwnerOnly: una entrada de 12 bytes por hueco ocupado, identificada por
 * el InstanceId del modelo (la ReplicationID del array rápido). En cada cambio
 * solo viajan las entradas nuevas o distintas y los ids que desaparecen
 * (FInventoryNetDelta); FInventoryNetCoalescer junta los cambios a 10 Hz.
 * Al resto de jugadores solo les llegan las dos manos (6 bytes) para la malla.
 *
 * Autoridad (biblia 08 §1.3): el servidor aplica las operaciones de
 * FInventoryModel y el cliente solo recibe el resultado; nunca al revés.
 */

/** Dónde está una entrada. Del 1 al 7, igual que EInventorySlot; del 8 al 10, el equipo puesto. */
enum class EInventoryNetSlot : uint8
{
	None = 0,
	HandLeft = 1,
	HandRight = 2,
	Pockets = 3,
	Belt = 4,
	Pouch = 5,
	Backpack = 6,
	Sledge = 7,
	EquippedBackpack = 8,
	EquippedBelt = 9,
	EquippedSledge = 10,
	Count
};

/** Bits de FInventoryNetEntry::Flags. */
namespace InventoryNetFlags
{
	/** Un objeto DosManos: la entrada de HandLeft ocupa también la derecha. */
	constexpr uint8 TwoHanded = 1 << 0;
	/** Recipiente: Count lleva centilitros de líquido, no unidades (los recipientes no apilan). */
	constexpr uint8 LiquidInCount = 1 << 1;
	/** Mojado (lo pone la capa de UE con FInventoryNetExtras). */
	constexpr uint8 Wet = 1 << 2;
	/** Encendido: antorcha, yesca con brasa (FInventoryNetExtras). */
	constexpr uint8 Lit = 1 << 3;
	/** Bits que la capa de UE puede pasar en FInventoryNetExtras::Flags. */
	constexpr uint8 ExtraMask = Wet | Lit;
}

/**
 * Entrada replicada (biblia 08 §2.4). Los campos suman 12 bytes: la biblia
 * decía 13 por un error de suma (1+1+2+4+1+1+1+1 = 12), ya corregido allí.
 */
struct EXPLORED_API FInventoryNetEntry
{
	uint8 Slot = 0;
	uint8 SlotIndex = 0;
	/** Índice en la tabla de items.json (FContentIdTableModel), no el FName. */
	uint16 DefinitionId = FContentIdTableModel::InvalidNetId;
	uint32 InstanceId = 0;
	uint8 Quality01x255 = 0;
	uint8 Durability01x255 = 255;
	/** Unidades de la pila (1–10) o centilitros si Flags lleva LiquidInCount. */
	uint8 Count = 1;
	uint8 Flags = 0;

	bool operator==(const FInventoryNetEntry& Other) const;
	bool operator!=(const FInventoryNetEntry& Other) const { return !(*this == Other); }
};

/** Las dos manos para todos los jugadores: 2 × (uint16 definición + uint8 calidad) = 6 bytes. */
struct EXPLORED_API FInventoryNetHands
{
	uint16 LeftDefinitionId = FContentIdTableModel::InvalidNetId;
	uint8 LeftQuality01x255 = 0;
	uint16 RightDefinitionId = FContentIdTableModel::InvalidNetId;
	uint8 RightQuality01x255 = 0;

	bool operator==(const FInventoryNetHands& Other) const;
};

/** Lo que se manda en un cambio: entradas nuevas o distintas y las que desaparecen. */
struct EXPLORED_API FInventoryNetDelta
{
	/** Ordenadas por InstanceId. */
	TArray<FInventoryNetEntry> Changed;
	/** InstanceId de las entradas que ya no están (también ordenados). */
	TArray<uint32> Removed;

	bool IsEmpty() const { return Changed.Num() == 0 && Removed.Num() == 0; }
	/** Bytes de carga útil: 12 por entrada y 4 por id quitado (la ReplicationID del array rápido). */
	int32 GetPayloadBytes() const;
};

/** Datos de la instancia completa que el modelo plano no tiene (los pone UCarryComponent). */
struct EXPLORED_API FInventoryNetExtras
{
	/** 0–1 sobre la durabilidad máxima; 1 en lo que no se gasta. */
	float Durability01 = 1.0f;
	/** Solo los bits de InventoryNetFlags::ExtraMask. */
	uint8 Flags = 0;
};

/** Por qué una entrada no se ha podido mandar (no debería pasar tras sanear la partida). */
struct EXPLORED_API FInventoryNetSkip
{
	int64 InstanceId = 0;
	FName DefinitionId;
};

class EXPLORED_API FInventoryNetModel
{
public:
	static constexpr int32 EntryBytes = 12;
	static constexpr int32 HandsBytes = 6;
	/** Bytes de un id quitado en el delta. */
	static constexpr int32 RemovedIdBytes = 4;
	/** Cabecera de EncodeDelta: dos uint16 con el número de entradas y de ids quitados. */
	static constexpr int32 DeltaHeaderBytes = 4;

	// ------------------------------------------------------------ cuantización

	/** Calidad 1–5 a 0–255 (1→0, 3→128, 5→255) y vuelta, sin pérdida. */
	static uint8 QualityToByte(uint8 Quality);
	static uint8 ByteToQuality(uint8 Byte);
	static uint8 Durability01ToByte(float Durability01);
	static float ByteToDurability01(uint8 Byte);
	/** Litros a centilitros en un byte (hasta 2,55 l; la cantimplora lleva 1). */
	static uint8 LitersToCentiliters(float Liters);

	// ---------------------------------------------------------------- estado

	/**
	 * Foto del inventario propio: una entrada por hueco ocupado (un DosManos,
	 * una sola) más el equipo puesto, ordenada por InstanceId. Lo que no tiene
	 * número de red (definición fuera de la tabla, id de más de 32 bits o hueco
	 * visible de más de 255) va a OutSkipped y no se manda.
	 */
	static void BuildSnapshot(const FInventoryState& State, const FContentIdTableModel& Table,
		const TMap<int64, FInventoryNetExtras>& Extras, TArray<FInventoryNetEntry>& OutEntries, TArray<FInventoryNetSkip>& OutSkipped);

	/** Lo que ven los demás: definición y calidad de cada mano. */
	static FInventoryNetHands BuildHands(const FInventoryState& State, const FContentIdTableModel& Table);

	/** Qué hay que mandar para pasar de Old a New (fotos de BuildSnapshot). */
	static FInventoryNetDelta Diff(const TArray<FInventoryNetEntry>& Old, const TArray<FInventoryNetEntry>& New);
	/**
	 * Aplica un delta a la copia del cliente. Todo o nada: falla sin tocar nada si
	 * quita un id que no está o si un id sale a la vez como cambiado y quitado.
	 */
	static bool ApplyDelta(TArray<FInventoryNetEntry>& InOut, const FInventoryNetDelta& Delta);

	// --------------------------------------------------------------- bytes

	/** 12 bytes little-endian en el orden de la biblia (Slot, SlotIndex, DefinitionId...). */
	static void EncodeEntry(const FInventoryNetEntry& Entry, TArray<uint8>& Out);
	/** Rechaza entradas con un hueco que no existe, Count 0 o bits de Flags desconocidos. */
	static bool DecodeEntry(const uint8* Data, int32 Num, FInventoryNetEntry& Out);
	static void EncodeHands(const FInventoryNetHands& Hands, TArray<uint8>& Out);
	static bool DecodeHands(const uint8* Data, int32 Num, FInventoryNetHands& Out);
	static TArray<uint8> EncodeDelta(const FInventoryNetDelta& Delta);
	/** Rechaza paquetes truncados, con bytes de más o con entradas no válidas. */
	static bool DecodeDelta(const TArray<uint8>& Bytes, FInventoryNetDelta& Out);
};

/**
 * Junta los cambios del inventario a 10 Hz (biblia 08 §2.4): fabricar a
 * máquina manda como mucho un delta cada 0,1 s y en reposo no manda nada.
 * Si algo aparece y desaparece dentro de la misma ventana, no viaja.
 */
class EXPLORED_API FInventoryNetCoalescer
{
public:
	static constexpr float IntervalSeconds = 0.1f;

	/**
	 * Avanza el reloj con la foto actual. Devuelve true (y el delta desde lo
	 * último enviado) si ha pasado la ventana y hay algo que mandar.
	 */
	bool Tick(float DeltaSeconds, const TArray<FInventoryNetEntry>& Current, FInventoryNetDelta& OutDelta);
	/** Todo el inventario para quien se acaba de unir (288 B con 24 huecos); reinicia lo enviado. */
	FInventoryNetDelta MakeFullSnapshot(const TArray<FInventoryNetEntry>& Current);
	const TArray<FInventoryNetEntry>& GetLastSent() const { return LastSent; }

private:
	TArray<FInventoryNetEntry> LastSent;
	float Accumulated = 0.0f;
};
