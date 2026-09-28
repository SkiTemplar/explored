#pragma once

#include "CoreMinimal.h"

/**
 * Códec de red de los deltas de terreno por chunk (biblia 08 «Cooperativo y red» §2.2):
 * serializa las muestras editadas de un chunk de `FTerrainEditModel` en uno o más
 * paquetes de tamaño acotado para replicarlos servidor → cliente, y los decodifica de
 * vuelta. Modelo puro: no conoce `FTerrainEditModel` ni la red, solo el formato de cable.
 *
 * Formato de un paquete (todo little-endian):
 * ```
 *   uint8   Version           // CurrentVersion
 *   int16   ChunkX, ChunkY, ChunkZ
 *   uint16  NumRuns
 *   por tramo:  uint16 FirstSample (0..MaxLocalIndex)
 *               uint8  Count (1..255)
 *               int16  Delta[Count]           // milímetros, mismo entero que el guardado
 * ```
 * Cabecera de HeaderBytes (9) + RunOverheadBytes (3) por tramo + 2 B por muestra, con
 * tope duro de MaxPacketBytes por paquete. Una edición más grande se parte en varios
 * paquetes del mismo chunk, en orden.
 */
class EXPLORED_API FTerrainDeltaCodecModel
{
public:
	/** Versión de formato que produce y acepta este códec. */
	static constexpr uint8 CurrentVersion = 1;
	/** Tope duro por paquete (biblia 08 §2.2): por debajo del umbral de fragmentación. */
	static constexpr int32 MaxPacketBytes = 512;
	/** Cabecera: versión (1) + chunk X/Y/Z (2 cada uno) + número de tramos (2). */
	static constexpr int32 HeaderBytes = 9;
	/** Coste fijo de un tramo: primera muestra (2) + cuenta (1). */
	static constexpr int32 RunOverheadBytes = 3;
	/** Cuenta máxima de un tramo que permite el formato de cable (cabe en un uint8). */
	static constexpr int32 MaxRunCountWire = 255;
	/**
	 * Cuenta máxima real de un tramo: nunca se parte a la mitad, así que tiene que caber
	 * él solo en un paquete vacío (HeaderBytes + RunOverheadBytes + 2·Count).
	 */
	static constexpr int32 MaxRunCount =
		(MaxPacketBytes - HeaderBytes - RunOverheadBytes) / 2 < MaxRunCountWire
			? (MaxPacketBytes - HeaderBytes - RunOverheadBytes) / 2
			: MaxRunCountWire;
	/** Índice local máximo: chunk de edición de 32³ = 32768 muestras (biblia 08 §2.2). */
	static constexpr int32 MaxLocalIndex = 32767;
	/** Delta máximo y mínimo representables en el cable (int16 con signo). */
	static constexpr int32 MaxDeltaMm = 32767;
	static constexpr int32 MinDeltaMm = -32768;

	/** Una muestra editada dentro de un chunk. */
	struct FSample
	{
		/** Índice lineal dentro del chunk: X + N·(Y + N·Z), 0..MaxLocalIndex. */
		int32 LocalIndex = 0;
		/** Delta en milímetros, el mismo entero que guarda FTerrainEditModel. */
		int32 DeltaMm = 0;

		bool operator==(const FSample& Other) const { return LocalIndex == Other.LocalIndex && DeltaMm == Other.DeltaMm; }
		bool operator!=(const FSample& Other) const { return !(*this == Other); }
	};

	/** Parche a enviar: coordenada de chunk + muestras (en cualquier orden, pueden repetir índice). */
	struct FChunkPatch
	{
		FIntVector Chunk = FIntVector::ZeroValue;
		TArray<FSample> Samples;

		bool IsEmpty() const { return Samples.Num() == 0; }
	};

	/** Resultado de decodificar un paquete. */
	struct FPacket
	{
		FIntVector Chunk = FIntVector::ZeroValue;
		/** Muestras en orden ascendente de LocalIndex, sin repetir. */
		TArray<FSample> Samples;
	};

	/**
	 * Normaliza una lista de muestras a la forma canónica: ordenadas ascendentemente por
	 * `LocalIndex` y sin índices repetidos (si dos entradas comparten índice, gana la que
	 * viene después en `Samples`, que es la edición más reciente). Es la base tanto de
	 * `Encode` como de la fusión de la cola de envío (biblia 08 §2.2: reenviar la misma
	 * muestra dos veces no acumula error, y dos ediciones sobre el mismo chunk se pueden
	 * fusionar quedándose con el valor final).
	 */
	static void Canonicalize(const TArray<FSample>& Samples, TArray<FSample>& OutSorted);

	/** true si las tres coordenadas del chunk caben en el `int16` de la cabecera. */
	static bool IsChunkEncodable(const FIntVector& Chunk);

	/** true si la muestra cabe en el formato de cable (índice en [0, MaxLocalIndex] y delta en int16). */
	static bool IsSampleEncodable(const FSample& Sample);

	/**
	 * Cuantiza un delta de densidad en metros al entero de milímetros del cable, redondeando
	 * al más cercano (los medios, lejos de cero, igual que `FMath::RoundToInt`). Devuelve
	 * false si no es finito o no cabe en int16 (±32,767 m); entonces `OutMm` queda saturado
	 * al extremo con el mismo signo (0 si no es finito) para que el llamante decida.
	 */
	static bool QuantizeDeltaMm(double DeltaMeters, int32& OutMm);

	/**
	 * Serializa un parche en uno o más paquetes de como mucho MaxPacketBytes, en orden.
	 * Las muestras que no cumplen `IsSampleEncodable`, o todas si el chunk no cumple
	 * `IsChunkEncodable`, se descartan y se cuentan en `OutRejected` (nunca en silencio:
	 * truncar un `int16` mandaría otro chunk u otro valor y desincronizaría al cliente).
	 * Un parche vacío, o que se queda vacío tras filtrar, produce cero paquetes.
	 */
	static TArray<TArray<uint8>> Encode(const FChunkPatch& Patch, int32* OutRejected = nullptr);

	/**
	 * Codifica solo el primer paquete de `CanonicalSamples` (ya en forma canónica y todas
	 * codificables: la cola lo garantiza) y dice cuántas muestras del principio ha metido
	 * en `OutConsumed`. Es lo que usa la cola para gastar el presupuesto paquete a paquete
	 * sin volver a codificar el chunk entero. Devuelve un array vacío (y 0) si no hay
	 * muestras, si el chunk no es codificable o si la entrada no es canónica.
	 */
	static TArray<uint8> EncodeFirstPacket(const FIntVector& Chunk, const TArray<FSample>& CanonicalSamples, int32& OutConsumed);

	/**
	 * Decodifica un paquete. Devuelve false sin tocar `Out` si los bytes están truncados,
	 * sobran al final del paquete, tienen una versión que no es `CurrentVersion`, ningún
	 * tramo (este códec nunca manda un paquete vacío), un tramo con cuenta 0, un tramo que
	 * se sale de [0, MaxLocalIndex], o tramos que no vienen en orden estrictamente creciente
	 * (solapados o desordenados: indicio de manipulación).
	 */
	static bool Decode(const TArray<uint8>& Bytes, FPacket& Out);

	/**
	 * Estado replicado de un chunk tal como lo guarda `FTerrainEditModel`: índice local →
	 * delta en mm, sin ceros (un delta 0 es «muestra sin tocar» y se borra).
	 */
	using FChunkState = TMap<int32, int32>;

	/**
	 * Aplica un paquete ya decodificado al estado del chunk: cada muestra fija su valor
	 * (el último gana; 0 borra la entrada). Idempotente: aplicarlo dos veces es igual que
	 * una. Dos paquetes con índices disjuntos conmutan.
	 */
	static void ApplyPacket(const FPacket& Packet, FChunkState& InOutState);

	/**
	 * Decodifica y aplica de forma atómica al mundo del cliente (chunk → estado). Si el
	 * paquete no pasa `Decode`, devuelve false y `InOutWorld` no cambia en nada. Un chunk
	 * que se queda sin muestras desaparece del mapa, igual que en `FTerrainEditModel`.
	 */
	static bool DecodeAndApply(const TArray<uint8>& Bytes, TMap<FIntVector, FChunkState>& InOutWorld);

	/** Muestras canónicas de un estado de chunk: lo que se manda como «chunk completo» (biblia 08 §2.2). */
	static TArray<FSample> SamplesOf(const FChunkState& State);

	/** Bytes que ocuparía un único paquete con un tramo de Count muestras (para tests y para dimensionar la cola). */
	static constexpr int32 PacketBytesForRun(int32 Count) { return HeaderBytes + RunOverheadBytes + Count * 2; }

	/** Bytes totales que produciría Encode para este parche (suma de todos los paquetes). */
	static int32 EncodedByteCount(const FChunkPatch& Patch);
};
