#pragma once

#include "CoreMinimal.h"

#include "Carry/InventoryModel.h"

/** Qué manda el servidor a un cliente sobre un contenedor que tiene abierto. */
enum class EContainerMessageKind : uint8
{
	/** Contenido entero: al abrir (o al reabrir). */
	Snapshot,
	/** Solo los huecos visibles que han cambiado desde el último envío. */
	Update,
};

struct EXPLORED_API FContainerMessage
{
	int32 ClientId = INDEX_NONE;
	FName ContainerId;
	EContainerMessageKind Kind = EContainerMessageKind::Snapshot;
	/** Huecos visibles cambiados (Update), en orden ascendente. Vacío en Snapshot. */
	TArray<int32> Slots;
};

/**
 * «El contenido del cofre no se replica hasta que se abre» (biblia 08 §2.4). Los
 * contenedores del mundo (`AExploredContainer`) no mandan nada a nadie mientras están
 * cerrados; al abrirlo, `Server_SubscribeContainer` suscribe al cliente, que recibe el
 * contenido entero y luego solo los huecos que cambian mientras siga abierto; al
 * cerrarlo (o al alejarse, o al desconectarse) se da de baja.
 *
 * Dos jugadores en el mismo cofre están permitidos y el servidor es el árbitro: las
 * operaciones de `FInventoryContainer` ya son atómicas, así que quien pierde la carrera
 * recibe `EInventoryFail::NotFound` y el hueco se le vuelve a mandar. Sin bloqueos ni
 * colas.
 *
 * Modelo puro: el actor solo lo alimenta con los RPC y serializa los huecos que diga
 * CollectOutgoing con las entradas de 12 B de §2.4 (1+1+2+4+1+1+1+1).
 */
class EXPLORED_API FContainerReplicationModel
{
public:
	/** Entrada de inventario en el cable (08 §2.4): 1+1+2+4+1+1+1+1 = 12 B. */
	static constexpr int32 EntryBytes = 12;
	/** Cabecera de un mensaje: id de contenedor (`uint16` de la tabla de ids) + tipo + cuenta. */
	static constexpr int32 MessageHeaderBytes = 4;
	/** Distancia máxima para abrir y para seguir con él abierto (08 §1.2: 250 cm + 50 de latencia). */
	static constexpr double MaxOpenDistanceCm = 300.0;

	/** ¿Se puede abrir (o seguir abierto) a esta distancia? NaN nunca. */
	static bool IsWithinReach(double DistanceCm);

	/**
	 * Abre: suscribe al cliente. False si el id no es válido, el cliente tampoco, ya estaba
	 * abierto por él o está fuera de alcance. El siguiente CollectOutgoing le manda el
	 * contenido entero.
	 */
	bool Subscribe(int32 ClientId, FName ContainerId, double DistanceCm);

	/** Cierra: da de baja. False si no lo tenía abierto. */
	bool Unsubscribe(int32 ClientId, FName ContainerId);

	/** Desconexión: da de baja de todo. */
	void DropClient(int32 ClientId);

	/** El contenedor desaparece (roto, recogido): se da de baja a todos. */
	void RemoveContainer(FName ContainerId);

	/**
	 * Revisión de distancias de los que lo tienen abierto; da de baja a quien se aleja más
	 * de MaxOpenDistanceCm. Devuelve los clientes dados de baja.
	 */
	TArray<int32> CloseOutOfReach(FName ContainerId, const TMap<int32, double>& DistanceByClient);

	bool IsSubscribed(int32 ClientId, FName ContainerId) const;

	/** Clientes con el contenedor abierto, en orden ascendente. */
	TArray<int32> Subscribers(FName ContainerId) const;

	/** Un hueco ha cambiado: se mandará a todos los que lo tengan abierto (a nadie si está cerrado). */
	void MarkSlotChanged(FName ContainerId, int32 SlotIndex);

	/** Solo a un cliente (el que perdió una carrera): se le vuelve a mandar ese hueco. */
	void MarkSlotStale(int32 ClientId, FName ContainerId, int32 SlotIndex);

	/**
	 * Saca lo pendiente: un Snapshot a cada suscripción nueva y un Update con los huecos
	 * cambiados a las demás. Orden determinista (contenedor, cliente).
	 */
	void CollectOutgoing(TArray<FContainerMessage>& OutMessages);

	/** Bytes de un mensaje en el cable con el contenido actual del contenedor. */
	static int32 MessageBytes(const FContainerMessage& Message, const FInventoryContainer& Container);

	/**
	 * Servidor: el cliente pide sacar el objeto que ve en SlotIndex. Solo si lo tiene
	 * abierto; si el hueco ya no tiene ese objeto (otro jugador fue más rápido), NotFound y
	 * el hueco se le refresca. Si sale bien, avisa a todos los que lo tengan abierto.
	 */
	EInventoryFail ServerTake(int32 ClientId, FInventoryContainer& Container, int32 SlotIndex, int64 InstanceId, FInventoryItem& OutItem);

	/** Servidor: el cliente guarda un objeto. Solo con el contenedor abierto; mismo aviso. */
	EInventoryFail ServerPut(int32 ClientId, FInventoryContainer& Container, const FInventoryItem& Item);

private:
	struct FSubscription
	{
		int32 ClientId = INDEX_NONE;
		FName ContainerId;
		bool bNeedsSnapshot = true;
		TArray<int32> DirtySlots;
	};

	FSubscription* FindSubscription(int32 ClientId, FName ContainerId);
	const FSubscription* FindSubscription(int32 ClientId, FName ContainerId) const;
	static void AddSlot(TArray<int32>& Slots, int32 SlotIndex);

	TArray<FSubscription> Subscriptions;
};
