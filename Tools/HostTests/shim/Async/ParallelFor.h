// Shim: ParallelFor secuencial. El determinismo del código real no debe depender del orden
// de ejecución de los hilos, así que ejecutar en orden es una prueba válida (y más estricta
// si además se compara con una ejecución en orden inverso: ver EXPLORED_HOST_PARALLEL_REVERSE).
#pragma once
#include "CoreMinimal.h"

enum class EParallelForFlags { None = 0, ForceSingleThread = 1, Unbalanced = 2 };

inline bool& HostParallelForReverse()
{
	static bool bReverse = false;
	return bReverse;
}

template <typename F>
void ParallelFor(int32 Num, F&& Body, bool = false)
{
	if (HostParallelForReverse()) { for (int32 I = Num - 1; I >= 0; --I) { Body(I); } }
	else { for (int32 I = 0; I < Num; ++I) { Body(I); } }
}
template <typename F>
void ParallelFor(int32 Num, F&& Body, EParallelForFlags) { ParallelFor(Num, Body); }
