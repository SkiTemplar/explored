#pragma once

#include "CoreMinimal.h"
#include "WorldGen/TerrainDeltaCodecModel.h"

/**
 * Cola de envío de deltas de terreno por cliente (biblia 08 «Cooperativo y red» §2.2):
 * una entrada por chunk (las ediciones repetidas del mismo chunk se fusionan antes de
 * salir), con presupuesto de bytes por segundo y prioridad por distancia al receptor.
 * Modelo puro: no conoce la red ni `FTerrainEditModel`, solo qué mandar y cuándo.
 */
class EXPLORED_API FTerrainDeltaQueueModel
{
public:
	/** Presupuesto sostenido de la capa de terreno (biblia 08 §2.2 y §3). */
	static constexpr double SustainedBytesPerSecond = 8000.0;
	/** Techo de ráfaga y ventana durante la que se puede mantener (biblia 08 §2.2). */
	static constexpr double BurstBytesPerSecond = 16000.0;
	static constexpr double BurstWindowSeconds = 5.0;
	/** Capacidad del cubo de fichas: lo máximo que se puede gastar de golpe tras estar inactivo. */
	static constexpr double BurstCapacityBytes = BurstBytesPerSecond * BurstWindowSeconds;
	/** Bajo esta distancia al receptor, el chunk tiene prioridad sobre el resto (biblia 08 §2.2). */
	static constexpr double PriorityDistanceM = 30.0;

	struct FEntry
	{
		FIntVector Chunk = FIntVector::ZeroValue;
		/** Muestras fusionadas, en forma canónica (ver FTerrainDeltaCodecModel::Canonicalize). */
		TArray<FTerrainDeltaCodecModel::FSample> Samples;
		double DistanceToReceiverM = 0.0;
		/** Orden de llegada a la cola: desempate FIFO dentro del mismo grupo de prioridad. */
		int64 SequenceNumber = 0;
	};

	/**
	 * Añade el parche de un chunk a la cola. Si el chunk ya estaba en cola (su paquete
	 * todavía no ha salido), las muestras se fusionan (biblia 08 §2.2: la edición más
	 * reciente gana, ver FTerrainDeltaCodecModel::Canonicalize) y la entrada NO cambia de
	 * posición en la cola; solo se refresca su distancia al receptor con la más reciente.
	 */
	void Enqueue(const FIntVector& Chunk, const TArray<FTerrainDeltaCodecModel::FSample>& Samples, double DistanceToReceiverM);

	int32 Num() const { return Entries.Num(); }
	bool IsEmpty() const { return Entries.IsEmpty(); }
	bool Contains(const FIntVector& Chunk) const;
	void Reset();

	/** Repone el presupuesto disponible al ritmo sostenido, sin pasar de BurstCapacityBytes. */
	void Accrue(double DeltaSeconds);
	double AvailableBudgetBytes() const { return AvailableBytes; }
	/** Deja el cubo lleno: para pruebas y para arrancar con la ráfaga completa disponible. */
	void FillBudget() { AvailableBytes = BurstCapacityBytes; }

	/**
	 * Saca la entrada de mayor prioridad (más cerca de PriorityDistanceM primero; dentro
	 * de cada grupo, la que lleva más tiempo en cola) SI su coste codificado (biblia 08
	 * §2.2, mismo formato que FTerrainDeltaCodecModel::Encode) cabe en el presupuesto
	 * disponible; en ese caso lo descuenta y la quita de la cola. Si no alcanza el
	 * presupuesto, devuelve false y la cola no cambia (no se salta a una entrada más
	 * barata: se espera a que se repongan más bytes, igual que un enlace real).
	 */
	bool TryPopWithinBudget(FEntry& Out);

private:
	int32 IndexOfHighestPriority() const;

	TArray<FEntry> Entries;
	double AvailableBytes = 0.0;
	int64 NextSequenceNumber = 0;
};
