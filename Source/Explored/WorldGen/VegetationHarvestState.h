#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

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

/** Progreso de golpes y, si está talada, cómo devolverla (rebrote de sesión, ver UExploredWiringSubsystem::HarvestInstance). */
struct EXPLORED_API FVegetationRuntimeState
{
	int32 Hits = 0;
	bool bHidden = false;
	/** < 0 = no rebrota en esta sesión (o no está oculta). Cuenta atrás en segundos reales. */
	float RegrowRemainingSeconds = -1.0f;
	FTransform OriginalTransform;
	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;
};
