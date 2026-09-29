#pragma once

#include "CoreMinimal.h"
#include "WorldGen/TerrainDeltaCodecModel.h"
#include "WorldGen/TerrainEditModel.h"

/**
 * Pegamento puro entre `FTerrainEditModel` y el formato de cable de los deltas de terreno
 * (`FTerrainDeltaCodecModel`, biblia 08 §2.2). Autoridad del servidor:
 *
 * - El servidor edita su modelo y convierte `FTerrainEditResult::ChangedSamples` en un
 *   parche por chunk de guardado con el valor FINAL de cada muestra (0 si ha vuelto a estar
 *   sin tocar). Reenviarlo dos veces no acumula error y dos parches del mismo chunk se
 *   fusionan en la cola quedándose con el último valor.
 * - Un cliente que llega tarde recibe el estado completo de cada chunk editado
 *   (`FullChunkPatch`).
 * - El cliente aplica cada paquete a su copia del modelo (`ApplyPacket`) y remalla los
 *   chunks que leen alguna muestra cambiada. Nunca edita por su cuenta.
 */
class EXPLORED_API FTerrainNetSyncModel
{
public:
	/**
	 * Un parche por chunk de guardado (`ChunkOfSample`) con el valor actual de cada muestra
	 * cambiada, ordenados por (Z, Y, X) y con las muestras en forma canónica. Ignora
	 * muestras repetidas.
	 */
	static TArray<FTerrainDeltaCodecModel::FChunkPatch> PatchesForSamples(const FTerrainEditModel& Model,
		const TArray<FIntVector>& ChangedSamples);

	/** Estado completo de un chunk (vacío si no tiene ediciones), para un cliente que entra tarde. */
	static FTerrainDeltaCodecModel::FChunkPatch FullChunkPatch(const FTerrainEditModel& Model, const FIntVector& Chunk);

	/**
	 * Aplica un paquete ya decodificado. Devuelve false sin tocar nada si alguna muestra no
	 * cabe en el modelo (índice fuera del chunk o delta fuera de rango). En OutDirty añade,
	 * ordenados por (Z, Y, X) y sin repetir, los chunks de edición que leen alguna muestra
	 * cuyo valor ha cambiado de verdad; en OutChangedSamples, esas muestras.
	 */
	static bool ApplyPacket(FTerrainEditModel& Model, const FTerrainDeltaCodecModel::FPacket& Packet,
		TArray<FIntVector>& OutDirty, TArray<FIntVector>* OutChangedSamples = nullptr);

	/** Caja (metros) que contiene las muestras; vacía (IsValid = false) sin muestras. */
	static FBox SamplesBounds(const FTerrainEditModel& Model, const TArray<FIntVector>& Samples);

	/** Orden (Z, Y, X) de chunks que usan todos los modelos de terreno. */
	static bool ChunkLess(const FIntVector& A, const FIntVector& B);
	/** Ordena y quita repetidos. */
	static void SortUnique(TArray<FIntVector>& InOut);
};
