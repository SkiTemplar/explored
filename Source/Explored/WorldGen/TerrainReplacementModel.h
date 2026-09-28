#pragma once

#include "CoreMinimal.h"

/** Estado de un chunk horneado de 64 m frente a la malla fina en tiempo de ejecución. */
enum class ETerrainReplacementState : uint8
{
	/** Se ve y colisiona el chunk horneado (Nanite). */
	Baked,
	/** Una tarea está buscando qué chunks de edición de dentro tienen superficie. */
	Surveying,
	/** Se están mallando los chunks de edición; el horneado sigue visible. */
	Building,
	/** Todos los chunks de edición tienen malla: el horneado se oculta. */
	Replaced,
};

/**
 * Sustitución de chunks horneados por mallas finas, pura (docs/tecnico/terreno-editable.md):
 *
 * Baked → Surveying (se pide) → Building (se sabe qué chunks de edición hacen falta) →
 * Replaced (todos tienen malla). El horneado se oculta solo al final, de golpe, para que
 * nunca se vea un agujero; mientras tanto la malla fina crece debajo sin notarse.
 * Una vez Replaced, un chunk no vuelve a Baked salvo con Reset (cargar otra partida).
 */
class EXPLORED_API FTerrainReplacementModel
{
public:
	explicit FTerrainReplacementModel(int32 InEditChunksPerRender = 8);

	/** Pide sustituir un chunk horneado. true si estaba en Baked (hay que lanzar el sondeo). */
	bool Request(const FIntVector& RenderChunk);
	/**
	 * Resultado del sondeo: los chunks de edición que necesitan malla (los de otro chunk de
	 * render se ignoran). Si ya tienen todos malla (o no hace falta ninguno), pasa a Replaced
	 * y lo añade a OutCompleted. Sin efecto si el chunk no estaba en Surveying.
	 */
	void SetRequired(const FIntVector& RenderChunk, const TArray<FIntVector>& EditChunks, TArray<FIntVector>& OutCompleted);
	/**
	 * Un chunk de edición ya tiene malla (aunque sea vacía). Si con él su chunk de render
	 * completa la sustitución, lo añade a OutCompleted.
	 */
	void OnEditChunkMeshed(const FIntVector& EditChunk, TArray<FIntVector>& OutCompleted);

	ETerrainReplacementState GetState(const FIntVector& RenderChunk) const;
	/** Chunks de edición que aún faltan en un chunk en Building (0 en cualquier otro estado). */
	int32 NumMissing(const FIntVector& RenderChunk) const;
	/** Chunks de render que no están en Baked, en orden (Z, Y, X). */
	TArray<FIntVector> ActiveRenderChunks() const;
	bool IsMeshed(const FIntVector& EditChunk) const { return Meshed.Contains(EditChunk); }
	FIntVector RenderChunkOf(const FIntVector& EditChunk) const;
	void Reset();

private:
	struct FEntry
	{
		ETerrainReplacementState State = ETerrainReplacementState::Baked;
		TSet<FIntVector> Missing;
	};

	int32 EditChunksPerRender = 8;
	TMap<FIntVector, FEntry> Entries;
	/** Chunks de edición que ya tienen malla. */
	TSet<FIntVector> Meshed;
};
