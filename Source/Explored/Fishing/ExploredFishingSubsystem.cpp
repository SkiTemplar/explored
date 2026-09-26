#include "Fishing/ExploredFishingSubsystem.h"

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/SubsystemCollection.h"

#include "Ocean/ExploredOcean.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"

namespace ExploredFishingSubsystemDetail
{
	/** Profundidad máxima que se mide con una traza (m); más allá, mar abierto. */
	constexpr float MaxDepthProbeM = 300.0f;
	/** Hasta esta profundidad se asume arrecife (sin máscara de arrecife en WorldGen todavía). */
	constexpr float ReefMinDepthM = 3.0f;
}

bool UExploredFishingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExploredFishingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTimeOfDaySubsystem>();
	Seed = FArchipelagoLayout::OfficialSeed ^ 0xF15B0Au;
}

float UExploredFishingSubsystem::GetNowDays() const
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	return Time ? Time->GetTotalDays() : 0.0f;
}

AExploredOcean* UExploredFishingSubsystem::FindOcean() const
{
	if (!CachedOcean.IsValid())
	{
		CachedOcean = Cast<AExploredOcean>(UGameplayStatics::GetActorOfClass(GetWorld(), AExploredOcean::StaticClass()));
	}
	return CachedOcean.Get();
}

bool UExploredFishingSubsystem::GetWaterHeightAt(const FVector& Location, float& OutWaterZ) const
{
	if (const AExploredOcean* Ocean = FindOcean())
	{
		OutWaterZ = Ocean->GetWaterHeightAt(Location);
		return true;
	}
	return false;
}

float UExploredFishingSubsystem::MeasureDepthM(const FVector& SurfacePoint, const AActor* IgnoredActor) const
{
	using namespace ExploredFishingSubsystemDetail;
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredFishingDepth), false);
	Params.AddIgnoredActor(IgnoredActor);
	if (const AExploredOcean* Ocean = FindOcean())
	{
		Params.AddIgnoredActor(Ocean);
	}
	FHitResult Hit;
	const FVector End = SurfacePoint - FVector(0.0, 0.0, MaxDepthProbeM * 100.0);
	if (World->LineTraceSingleByChannel(Hit, SurfacePoint, End, ECC_WorldStatic, Params))
	{
		return FMath::Max(0.0f, static_cast<float>(SurfacePoint.Z - Hit.ImpactPoint.Z) / 100.0f);
	}
	return MaxDepthProbeM;
}

FFishingConditions UExploredFishingSubsystem::MakeConditions(const FVector& SurfacePoint, float DepthM) const
{
	using namespace ExploredFishingSubsystemDetail;
	FFishingConditions Conditions;
	const float Now = GetNowDays();
	Conditions.SetTime(Now);
	if (const UExploredWeatherSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UExploredWeatherSubsystem>() : nullptr)
	{
		Conditions.SetWeather(Weather->GetCurrent());
	}
	Conditions.DepthM = DepthM;
	Conditions.Habitat = FFishingModel::HabitatForDepth(DepthM, DepthM >= ReefMinDepthM);
	Conditions.Depletion01 = FFishingModel::ZoneDepletion(State, FFishingModel::ZoneKeyAt(FVector2D(SurfacePoint)), Now);
	return Conditions;
}
