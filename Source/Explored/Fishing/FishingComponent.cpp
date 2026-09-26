#include "Fishing/FishingComponent.h"

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Carry/CarryComponent.h"
#include "Fishing/ExploredFishingSubsystem.h"
#include "Fishing/ExploredTrap.h"
#include "Items/ExploredItemActor.h"
#include "Items/ItemTypes.h"
#include "Player/SwimComponent.h"

namespace FishingComponentDetail
{
	/** Filo con que se despieza una legendaria al sacarla (la escena ya la da abierta). */
	constexpr float LegendaryButcherEdge = 0.6f;
	/** Alcance para colocar una trampa (cm). */
	constexpr float TrapPlaceDistanceCm = 400.0f;
}

UFishingComponent::UFishingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFishingComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);
}

UExploredFishingSubsystem* UFishingComponent::GetFishing() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UExploredFishingSubsystem>() : nullptr;
}

bool UFishingComponent::TraceWaterPoint(FVector& OutSurfacePoint) const
{
	const AActor* Owner = GetOwner();
	const UExploredFishingSubsystem* Fishing = GetFishing();
	UWorld* World = GetWorld();
	if (!Owner || !Fishing || !World)
	{
		return false;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	Owner->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FVector Direction = EyeRotation.Vector();
	if (Direction.Z > -0.02)
	{
		// Mirando al horizonte o hacia arriba no se llega al agua.
		return false;
	}

	float WaterZ = 0.0f;
	if (!Fishing->GetWaterHeightAt(EyeLocation + Direction * (MaxCastDistanceCm * 0.5f), WaterZ))
	{
		return false;
	}
	const double Distance = (WaterZ - EyeLocation.Z) / Direction.Z;
	if (Distance <= 0.0 || Distance > MaxCastDistanceCm)
	{
		return false;
	}
	OutSurfacePoint = EyeLocation + Direction * Distance;

	// Si algo sólido corta el lance antes del agua (roca, orilla), no vale.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredFishingCast), false, Owner);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, EyeLocation, OutSurfacePoint, ECC_WorldStatic, Params))
	{
		return false;
	}
	return true;
}

float UFishingComponent::ComputeNoise() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0.0f;
	}
	float WeightRatio = 0.0f;
	if (const UCarryComponent* Carry = Owner->FindComponentByClass<UCarryComponent>())
	{
		WeightRatio = ComfortableWeightKg > 0.0f ? Carry->GetTotalWeight() / ComfortableWeightKg : 0.0f;
	}
	const USwimComponent* Swim = Owner->FindComponentByClass<USwimComponent>();
	const bool bWading = Swim && Swim->IsInWater();
	const float Speed01 = NoisySpeedCmS > 0.0f ? static_cast<float>(Owner->GetVelocity().Size()) / NoisySpeedCmS : 0.0f;
	return FFishingModel::PlayerNoise(WeightRatio, Speed01, 0.0f, bWading);
}

bool UFishingComponent::StartCast()
{
	UExploredFishingSubsystem* Fishing = GetFishing();
	if (SessionState != EFishingSessionState::Idle || !Fishing)
	{
		return false;
	}
	FVector Surface;
	if (!TraceWaterPoint(Surface))
	{
		return false;
	}
	const float DepthM = Fishing->MeasureDepthM(Surface, GetOwner());
	if (DepthM < 0.3f)
	{
		return false;
	}

	FFishingConditions Conditions = Fishing->MakeConditions(Surface, DepthM);
	Conditions.Method = ECatchMethod::Rod;
	Conditions.Bait = Bait;
	Conditions.Tackle = Tackle;
	Conditions.SpotTag = SpotTag;
	Conditions.Noise01 = ComputeNoise();

	const float Now = Fishing->GetNowDays();
	FFishingSaveState& SaveState = Fishing->GetState();
	for (const FLegendaryCatch& Legend : FFishingModel::Legendaries())
	{
		if (!SpotTag.IsNone() && Legend.SpotTag == SpotTag)
		{
			FFishingModel::NoteLegendaryPresence(SaveState, Legend.Id, Conditions.Noise01, Now);
		}
	}

	CastPoint = Surface;
	bHasBite = FFishingModel::WaitForBite(Conditions, &SaveState, Fishing->GetSeed(),
		FFishingModel::SpotKeyAt(FVector2D(Surface)), Now, MaxBiteWaitSeconds, PendingBite);
	if (bHasBite)
	{
		PendingBite.Fight.StartDistanceM = FMath::Max(3.0f, static_cast<float>(FVector::Dist(GetOwner()->GetActorLocation(), Surface)) / 100.0f);
	}
	BiteTimer = bHasBite ? PendingBite.WaitSeconds : MaxBiteWaitSeconds;
	SessionState = EFishingSessionState::Waiting;
	ReelInput = 0.0f;
	SetComponentTickEnabled(true);
	return true;
}

void UFishingComponent::Cancel()
{
	if (SessionState != EFishingSessionState::Idle)
	{
		Finish(EFishingResult::Cancelled, NAME_None);
	}
}

void UFishingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (SessionState == EFishingSessionState::Waiting)
	{
		BiteTimer -= DeltaTime;
		if (BiteTimer > 0.0f)
		{
			return;
		}
		if (!bHasBite)
		{
			Finish(EFishingResult::NoBite, NAME_None);
			return;
		}
		Fight.Emplace(PendingBite.Fight, PendingBite.FightSeed);
		SessionState = EFishingSessionState::Fighting;
		return;
	}

	if (SessionState == EFishingSessionState::Fighting && Fight.IsSet())
	{
		switch (Fight->Tick(DeltaTime, ReelInput))
		{
		case EFishFightOutcome::Caught:
			HandleCatch();
			Finish(EFishingResult::Caught, PendingBite.Id);
			break;
		case EFishFightOutcome::Escaped:
			Finish(EFishingResult::Escaped, PendingBite.Id);
			break;
		case EFishFightOutcome::LineSnapped:
			// TODO(P-CARRY): gastar el sedal y el anzuelo del aparejo equipado.
			Finish(EFishingResult::LineSnapped, PendingBite.Id);
			break;
		default:
			break;
		}
	}
}

void UFishingComponent::HandleCatch()
{
	UExploredFishingSubsystem* Fishing = GetFishing();
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Fishing || !Owner || !World)
	{
		return;
	}
	const float Now = Fishing->GetNowDays();
	FFishingModel::RegisterCatch(Fishing->GetState(), FFishingModel::ZoneKeyAt(FVector2D(CastPoint)), Now);

	const FVector DropLocation = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 80.0f + FVector(0.0, 0.0, 20.0);
	if (!PendingBite.bLegendary)
	{
		SpawnCatchActor(World, PendingBite.Id, DropLocation);
		return;
	}

	// Una legendaria no es un objeto: sale ya abierta, con su recompensa exclusiva.
	Fishing->GetState().MarkLegendaryCaught(PendingBite.Id);
	FButcherResult Butchered;
	if (FFishingModel::Butcher(PendingBite.Id, PendingBite.WeightKg, FishingComponentDetail::LegendaryButcherEdge, Now, Butchered))
	{
		int32 Index = 0;
		for (const FButcherYield& Yield : Butchered.Yields)
		{
			for (int32 I = 0; I < Yield.Count; ++I)
			{
				SpawnCatchActor(World, Yield.ItemId, DropLocation + FVector(0.0, 0.0, 15.0 * Index++));
			}
		}
	}
}

void UFishingComponent::Finish(EFishingResult Result, FName CatchId)
{
	SessionState = EFishingSessionState::Idle;
	Fight.Reset();
	bHasBite = false;
	ReelInput = 0.0f;
	SetComponentTickEnabled(false);
	OnFishingFinished.Broadcast(Result, CatchId);
}

float UFishingComponent::GetTension01() const
{
	return Fight.IsSet() ? Fight->GetState().Tension01 : 0.0f;
}

float UFishingComponent::GetLineOutM() const
{
	return Fight.IsSet() ? Fight->GetState().DistanceM : 0.0f;
}

AExploredItemActor* UFishingComponent::SpawnCatchActor(UWorld* World, FName ItemId, const FVector& Location)
{
	if (!World || ItemId.IsNone())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AExploredItemActor* Actor = World->SpawnActor<AExploredItemActor>(AExploredItemActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (Actor)
	{
		FItemInstance Instance;
		Instance.DefinitionId = ItemId;
		Actor->InitializeFromInstance(Instance);
	}
	return Actor;
}

AExploredTrap* UFishingComponent::PlaceTrap(FName KindName, FName BaitItemId)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}
	FVector EyeLocation;
	FRotator EyeRotation;
	Owner->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FVector End = EyeLocation + EyeRotation.Vector() * FishingComponentDetail::TrapPlaceDistanceCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredPlaceTrap), false, Owner);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, EyeLocation, End, ECC_WorldStatic, Params))
	{
		return nullptr;
	}
	const FTransform Transform(FRotator(0.0, EyeRotation.Yaw, 0.0), Hit.ImpactPoint);
	AExploredTrap* Trap = World->SpawnActorDeferred<AExploredTrap>(AExploredTrap::StaticClass(), Transform, Owner);
	if (Trap)
	{
		Trap->Configure(KindName, BaitItemId);
		Trap->FinishSpawning(Transform);
	}
	return Trap;
}
