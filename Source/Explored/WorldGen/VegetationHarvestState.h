#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

#include "WorldGen/VegetationStateModel.h"

class UHierarchicalInstancedStaticMeshComponent;

/**
 * Estado en tiempo de ejecución de la recolección de vegetación y rocas
 * (UExploredWiringSubsystem). Con UObject (TWeakObjectPtr): no es un modelo
 * puro y no se compila en Tools/HostTests. Sin generated.h a propósito: es un
 * fichero corriente que UnrealHeaderTool no necesita parsear en profundidad,
 * a diferencia de ExploredWiringSubsystem.h, cuyo parser no admite bien una
 * función friend con cuerpo inline (ver GetTypeHash más abajo).
 */

/** Instancia golpeada: celda + componente HISM (una celda agrupa varias especies, cada una en su propio HISM) + índice dentro de él. */
struct EXPLORED_API FVegetationInstanceKey
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	FName Component;
	int32 Index = INDEX_NONE;

	bool operator==(const FVegetationInstanceKey& Other) const
	{
		return Cell == Other.Cell && Component == Other.Component && Index == Other.Index;
	}
};

EXPLORED_API uint32 GetTypeHash(const FVegetationInstanceKey& Key);

/**
 * Estado de una instancia golpeada o talada (UExploredWiringSubsystem::HarvestInstance).
 * Instance es la parte pura (FVegetationStateModel): golpes, hora de tala, brote y
 * rebrote en minutos de juego. Talada queda en «Stump» (tocón) hasta SproutAtMinute,
 * luego «Sapling» (brote que crece, todavía no talable) y a RegrowAtMinute vuelve a
 * ser un ejemplar entero (biblia 02 §1.2). La hora de tala se guarda en
 * FSaveWorldDeltas::VegetationClock, así que el rebrote atraviesa guardar y cargar.
 */
struct EXPLORED_API FVegetationRuntimeState
{
	FVegetationInstanceState Instance;
	/** Oculta a escala 0 (tocón) o reducida (brote): no se puede golpear hasta que rebrote. */
	bool bHidden = false;
	/** Especie (FScatterRule::Species): perfil de tala para la escala del brote. */
	FName Species;
	/** Última escala aplicada a la instancia oculta (0 tocón, SaplingStartScale → 1 brote). */
	float AppliedScale = 0.0f;
	FTransform OriginalTransform;
	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;

	EVegetationStage StageAt(int64 NowMinute) const { return FVegetationStateModel::StageAt(Instance, NowMinute); }
};
