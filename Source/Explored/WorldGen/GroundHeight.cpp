#include "WorldGen/GroundHeight.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"

#include "Explored.h"
#include "WorldGen/GrassConfigLoader.h"
#include "WorldGen/GroundField.h"
#include "WorldGen/IslandLayoutLoader.h"

namespace GroundHeightDetail
{
	constexpr double MetersToCm = 100.0;
	/** Tramo del trazado de respaldo (cm): por encima del faro y por debajo del fondo de la malla (−6 m). */
	constexpr double TraceTopCm = 20000.0;
	constexpr double TraceBottomCm = -2000.0;

	TSharedPtr<const FGroundField> Shared;
	bool bTriedShared = false;

	bool TraceLowestStatic(const UWorld& World, double X, double Y, float& OutHeightMeters)
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredGroundHeight), false);
		const FVector Top(X * MetersToCm, Y * MetersToCm, TraceTopCm);
		const FVector Bottom(X * MetersToCm, Y * MetersToCm, TraceBottomCm);
		// Las consultas por tipo de objeto devuelven todos los impactos: el más bajo es el terreno.
		World.LineTraceMultiByObjectType(Hits, Top, Bottom, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
		if (Hits.IsEmpty())
		{
			return false;
		}
		double Lowest = TNumericLimits<double>::Max();
		for (const FHitResult& Hit : Hits)
		{
			Lowest = FMath::Min(Lowest, Hit.ImpactPoint.Z);
		}
		OutHeightMeters = static_cast<float>(Lowest / MetersToCm);
		return true;
	}
}

TSharedPtr<const FGroundField> ExploredGround::LoadField(FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *ExploredGrass::GroundFieldPath()) || Bytes.IsEmpty())
	{
		OutError = TEXT("no existe");
		return nullptr;
	}
	TSharedPtr<FGroundField> Field = MakeShared<FGroundField>();
	if (!Field->Parse(Bytes, OutError))
	{
		return nullptr;
	}
	return Field;
}

TSharedPtr<const FGroundField> ExploredGround::GetShared()
{
	using namespace GroundHeightDetail;
	if (!bTriedShared)
	{
		FString Error;
		ReloadShared(Error);
		if (!Shared)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Suelo] ground_field.bin no se puede usar (%s): la altura sale de un trazado"), *Error);
		}
	}
	return Shared;
}

TSharedPtr<const FGroundField> ExploredGround::ReloadShared(FString& OutError)
{
	using namespace GroundHeightDetail;
	bTriedShared = true;
	Shared = LoadField(OutError);
	return Shared;
}

float ExploredGround::HeightAtMeters(const UWorld* World, double X, double Y)
{
	using namespace GroundHeightDetail;
	if (const TSharedPtr<const FGroundField> Field = GetShared())
	{
		FGroundSample Sample;
		if (Field->Sample(X, Y, Sample))
		{
			return Sample.Height;
		}
	}
	float Traced = 0.0f;
	if (World && TraceLowestStatic(*World, X, Y, Traced))
	{
		return Traced;
	}
	return ExploredIsland::Get()->SampleColumn(static_cast<float>(X), static_cast<float>(Y)).Height;
}
