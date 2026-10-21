#include "WorldGen/IslandDecorModel.h"

bool FIslandDecor::Validate(FString& OutError) const
{
	if (Meshes.IsEmpty())
	{
		OutError = TEXT("sin mallas");
		return false;
	}
	for (const FString& Path : Meshes)
	{
		if (!Path.StartsWith(TEXT("/Game/Art/")))
		{
			OutError = FString::Printf(TEXT("ruta de malla fuera de /Game/Art: %s"), *Path);
			return false;
		}
	}
	if (Instances.Num() > MaxInstances)
	{
		OutError = FString::Printf(TEXT("%d instancias (máximo %d)"), Instances.Num(), MaxInstances);
		return false;
	}
	for (int32 I = 0; I < Instances.Num(); ++I)
	{
		const FIslandDecorInstance& Inst = Instances[I];
		const bool bFinite = FMath::IsFinite(Inst.Location.X) && FMath::IsFinite(Inst.Location.Y)
			&& FMath::IsFinite(Inst.Location.Z) && FMath::IsFinite(Inst.YawDeg) && FMath::IsFinite(Inst.Scale);
		if (!Meshes.IsValidIndex(Inst.Mesh) || !bFinite || Inst.Scale <= 0.0f || Inst.Scale > 10.0f)
		{
			OutError = FString::Printf(TEXT("instancia %d no válida (malla %d, escala %.2f)"), I, Inst.Mesh, Inst.Scale);
			return false;
		}
	}
	return true;
}

TArray<int32> FIslandDecor::CountPerMesh() const
{
	TArray<int32> Counts;
	Counts.Init(0, Meshes.Num());
	for (const FIslandDecorInstance& Inst : Instances)
	{
		if (Counts.IsValidIndex(Inst.Mesh))
		{
			++Counts[Inst.Mesh];
		}
	}
	return Counts;
}
