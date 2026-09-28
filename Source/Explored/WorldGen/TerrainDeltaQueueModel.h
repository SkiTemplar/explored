#pragma once

#include "CoreMinimal.h"
#include "WorldGen/TerrainDeltaCodecModel.h"

/**
 * Cola de envío de deltas de terreno por cliente (biblia 08 «Cooperativo y red» §2.2):
 * una entrada por chunk (las ediciones repetidas del mismo chunk se fusionan antes de
 * salir), con presupuesto de bytes por segundo, prioridad por distancia al receptor y
 * relevancia acotada. Modelo puro: no conoce la red ni `FTerrainEditModel`, solo qué
 * mandar y cuándo.
 *
 * Presupuesto en bytes con dos límites que se cumplen a la vez:
 * - **Sostenido:** cubo de fichas que se llena a SustainedBytesPerSecond (8 KB/s) y guarda
 *   como mucho BurstCreditBytes = (16 − 8) KB/s × 5 s = 40 KB. Tras estar parada, la cola
 *   puede ir a 16 KB/s durante 5 s exactos (gasta 16 y repone 8 cada segundo) y luego
 *   vuelve a 8 KB/s. En cualquier ventana de T s salen como mucho 40 000 + 8 000·T bytes.
 * - **Pico:** ventana deslizante de 1 s: en cualquier intervalo de un segundo salen como
 *   mucho BurstBytesPerSecond (16 KB). Es lo que impide soltar los 40 KB de golpe en un
 *   solo tick, y es exacto (un segundo cubo de fichas dejaría pasar su capacidad de más).
 *
 * Se envía paquete a paquete (como mucho MaxPacketBytes): un chunk grande nunca se queda
 * atascado por no caber entero en el presupuesto, y lo que queda de él sigue en la cola
 * (y se sigue fusionando con ediciones nuevas).
 */
class EXPLORED_API FTerrainDeltaQueueModel
{
public:
	/** Presupuesto sostenido de la capa de terreno (biblia 08 §2.2 y §3): 64 kbps. */
	static constexpr double SustainedBytesPerSecond = 8000.0;
	/** Techo de ráfaga y ventana durante la que se puede mantener (biblia 08 §2.2): 128 kbps × 5 s. */
	static constexpr double BurstBytesPerSecond = 16000.0;
	static constexpr double BurstWindowSeconds = 5.0;
	/** Capacidad del cubo sostenido: lo que permite ir a ritmo de ráfaga durante BurstWindowSeconds. */
	static constexpr double BurstCreditBytes = (BurstBytesPerSecond - SustainedBytesPerSecond) * BurstWindowSeconds;
	/** Bajo esta distancia al receptor, el chunk tiene prioridad sobre el resto (biblia 08 §2.2). */
	static constexpr double PriorityDistanceM = 30.0;
	/**
	 * Más allá de esta distancia el chunk no es relevante para el receptor y no se manda
	 * (biblia 08 §2.2): se queda en la cola hasta que el receptor se acerque. La distancia
	 * que pasa el llamante es la menor entre la del personaje y la de cualquier chunk que
	 * ese cliente tenga cargado por World Partition.
	 */
	static constexpr double RelevanceDistanceM = 120.0;

	struct FEntry
	{
		FIntVector Chunk = FIntVector::ZeroValue;
		/** Muestras pendientes, fusionadas y en forma canónica (ver FTerrainDeltaCodecModel::Canonicalize). */
		TArray<FTerrainDeltaCodecModel::FSample> Samples;
		double DistanceToReceiverM = 0.0;
		/** Orden de llegada a la cola: desempate FIFO dentro del mismo grupo de prioridad. */
		int64 SequenceNumber = 0;
	};

	/** Un paquete listo para la RPC fiable servidor → cliente. */
	struct FOutgoingPacket
	{
		FIntVector Chunk = FIntVector::ZeroValue;
		TArray<uint8> Bytes;
		/** true si con este paquete el chunk ha salido entero de la cola. */
		bool bLastOfChunk = false;
	};

	/**
	 * Añade el parche de un chunk a la cola. Si el chunk ya estaba en cola (le quedan
	 * muestras por salir), se fusionan (biblia 08 §2.2: la edición más reciente gana, ver
	 * FTerrainDeltaCodecModel::Canonicalize) y la entrada NO cambia de posición; solo se
	 * refresca su distancia al receptor con la más reciente. Las muestras que no caben en
	 * el formato de cable, o todas si el chunk no cabe, se descartan y se devuelven
	 * contadas (el llamante debe registrarlo: es una desincronización segura). Un parche
	 * sin muestras válidas no crea entrada. Una distancia negativa cuenta como 0 y una no
	 * finita como «no relevante».
	 */
	int32 Enqueue(const FIntVector& Chunk, const TArray<FTerrainDeltaCodecModel::FSample>& Samples, double DistanceToReceiverM);

	/** Actualiza la distancia al receptor de un chunk en cola (el receptor se ha movido). false si no está. */
	bool UpdateDistance(const FIntVector& Chunk, double DistanceToReceiverM);

	int32 Num() const { return Entries.Num(); }
	bool IsEmpty() const { return Entries.IsEmpty(); }
	bool Contains(const FIntVector& Chunk) const;
	/** Muestras pendientes de un chunk (nullptr si no está en cola). */
	const TArray<FTerrainDeltaCodecModel::FSample>* PendingSamples(const FIntVector& Chunk) const;
	void Reset();

	/**
	 * Avanza el reloj de la cola: repone el cubo sostenido y deja caer de la ventana de pico
	 * lo enviado hace un segundo o más. Ignora tiempos nulos, negativos o no finitos (un
	 * tick roto no regala presupuesto).
	 */
	void Accrue(double DeltaSeconds);
	/** Bytes que se pueden gastar ahora mismo: el menor de los dos límites. */
	double AvailableBudgetBytes() const;
	double SustainedBudgetBytes() const { return SustainedBytes; }
	/** Bytes enviados dentro de la ventana de pico (el último segundo). */
	double BytesInPeakWindow() const;
	/** Llena el cubo sostenido: el estado tras un rato sin editar. */
	void FillBudget() { SustainedBytes = BurstCreditBytes; }

	/**
	 * Codifica el siguiente paquete de la entrada de mayor prioridad entre las relevantes
	 * (a menos de PriorityDistanceM primero; dentro de cada grupo, la que lleva más tiempo
	 * en cola) SI cabe en el presupuesto; en ese caso lo descuenta del cubo sostenido, lo apunta en la ventana de pico y quita
	 * de la entrada las muestras enviadas (la entrada sale de la cola cuando se vacía). Si
	 * no alcanza el presupuesto o no hay nada relevante, devuelve false y nada cambia (no se
	 * salta a una entrada más barata: se espera a que se repongan más bytes).
	 */
	bool TryPopPacket(FOutgoingPacket& Out);

private:
	int32 IndexOfHighestPriority() const;
	static double SanitizeDistance(double DistanceToReceiverM);

	struct FSent
	{
		double AtSeconds = 0.0;
		int32 Bytes = 0;
	};

	TArray<FEntry> Entries;
	double SustainedBytes = 0.0;
	/** Reloj interno: suma de los `Accrue` válidos. */
	double ClockSeconds = 0.0;
	/** Envíos de la ventana de pico, en orden de envío. */
	TArray<FSent> RecentSends;
	int64 NextSequenceNumber = 0;
};
