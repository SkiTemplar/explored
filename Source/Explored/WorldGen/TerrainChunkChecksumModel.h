#pragma once

#include "CoreMinimal.h"
#include "WorldGen/TerrainDeltaCodecModel.h"

/**
 * Verificación de integridad del terreno replicado (biblia 08 «Cooperativo y red» §2.2):
 * suma de comprobación FNV-1a de 32 bits por chunk editado, y un rastreador que decide
 * cuándo toca volver a mandarla (cada 30 s por chunk visible) y compara la que confirma
 * el cliente. Modelo puro: no conoce la red ni `FTerrainEditModel`.
 */
class EXPLORED_API FTerrainChunkChecksumModel
{
public:
	/** Cada cuánto se vuelve a comprobar un chunk editado y visible (biblia 08 §2.2). */
	static constexpr double VerificationIntervalSeconds = 30.0;

	/**
	 * FNV-1a de 32 bits sobre las muestras de un chunk. Determinista: aplica
	 * `FTerrainDeltaCodecModel::Canonicalize` antes de plegar los bytes, así que el
	 * resultado no depende del orden de `Samples` ni de índices repetidos. Un chunk sin
	 * muestras da la base de FNV-1a (0x811C9DC5).
	 */
	static uint32 Compute(const TArray<FTerrainDeltaCodecModel::FSample>& Samples);

	/** Registro de la última comprobación mandada de un chunk. */
	struct FChunkRecord
	{
		uint32 LastChecksum = 0;
		double LastSentAtSeconds = 0.0;
		bool bEverSent = false;
	};

	/**
	 * Rastreador por cliente de cuándo tocan las comprobaciones periódicas y de si la que
	 * confirma el cliente coincide con la última mandada.
	 */
	class EXPLORED_API FTracker
	{
	public:
		/**
		 * Chunks a los que hay que mandarles la comprobación ahora: los que nunca se han
		 * mandado, o llevan >= VerificationIntervalSeconds desde la última vez. Marca los
		 * elegidos como mandados a NowSeconds con el checksum dado. `CurrentChecksums` son
		 * todos los chunks editados visibles para este cliente en este instante.
		 */
		void DueChunks(const TMap<FIntVector, uint32>& CurrentChecksums, double NowSeconds, TArray<FIntVector>& OutDue);

		/**
		 * Compara el checksum que confirma el cliente contra el último que se le mandó a
		 * ese chunk. Devuelve false (desincronización: pedir el chunk completo) si no
		 * coinciden o si nunca se le mandó ninguno a ese chunk.
		 */
		bool ConfirmAndCheck(const FIntVector& Chunk, uint32 ClientChecksum) const;

		/** El chunk deja de estar cargado en el cliente (sale de rango): se olvida su historial. */
		void ForgetChunk(const FIntVector& Chunk) { Records.Remove(Chunk); }

		int32 NumTracked() const { return Records.Num(); }
		void Reset() { Records.Reset(); }

	private:
		TMap<FIntVector, FChunkRecord> Records;
	};
};
