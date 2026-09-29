#include "Mining/TerrainToolComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

#include "Carry/CarryComponent.h"
#include "Explored.h"
#include "Mining/TerrainEditSubsystem.h"
#include "Mining/TerrainRuntimeMesher.h"
#include "WorldGen/TerrainToolModel.h"

namespace TerrainToolComponentDetail
{
	/**
	 * Objeto de las manos que hace de herramienta para ese botón, o NAME_None. Principal: la
	 * mano derecha. Secundario: la izquierda; con la izquierda libre, la de la derecha.
	 */
	FName HeldToolItem(const UCarryComponent& Carry, bool bSecondary)
	{
		FItemInstance Item;
		ETerrainDigTool Tool = ETerrainDigTool::Count;
		const EHand First = bSecondary ? EHand::Left : EHand::Right;
		if (Carry.GetHandItem(First, Item) && FTerrainToolModel::ToolFromItem(Item.DefinitionId, Tool))
		{
			return Item.DefinitionId;
		}
		if (bSecondary && Carry.IsHandEmpty(EHand::Left) && Carry.GetHandItem(EHand::Right, Item)
			&& FTerrainToolModel::ToolFromItem(Item.DefinitionId, Tool))
		{
			return Item.DefinitionId;
		}
		return NAME_None;
	}

	EExploredTerrainAction FromModel(ETerrainToolAction Action)
	{
		switch (Action)
		{
		case ETerrainToolAction::ShovelFlatten: return EExploredTerrainAction::ShovelFlatten;
		case ETerrainToolAction::PlaceSoil: return EExploredTerrainAction::PlaceSoil;
		default: return EExploredTerrainAction::Pick;
		}
	}

	EExploredTerrainCue FromModel(EMineHitCue Cue)
	{
		switch (Cue)
		{
		case EMineHitCue::Rebound: return EExploredTerrainCue::Rebound;
		case EMineHitCue::Miss: return EExploredTerrainCue::Miss;
		default: return EExploredTerrainCue::Hit;
		}
	}

	bool IsFinite(const FVector& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
	}
}

UTerrainToolComponent::UTerrainToolComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTerrainToolComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UTerrainToolComponent, CarriedSoilM3, COND_OwnerOnly);
}

bool UTerrainToolComponent::TryUseFromHands(const UCarryComponent* Carry, bool bSecondary)
{
	if (!Carry)
	{
		return false;
	}
	const FName ItemId = TerrainToolComponentDetail::HeldToolItem(*Carry, bSecondary);
	return !ItemId.IsNone() && TryUseHeldTool(ItemId, bSecondary);
}

bool UTerrainToolComponent::TryUseHeldTool(FName ItemId, bool bSecondary)
{
	using namespace TerrainToolComponentDetail;
	ETerrainDigTool Tool = ETerrainDigTool::Count;
	if (!FTerrainToolModel::ToolFromItem(ItemId, Tool))
	{
		return false;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UTerrainEditSubsystem* Terrain = UTerrainEditSubsystem::Get(this);
	const UWorld* World = GetWorld();
	if (!Pawn || !Pawn->IsLocallyControlled() || !Terrain || !World)
	{
		return true;
	}
	const ETerrainToolAction Action = FTerrainToolModel::ActionFor(Tool, bSecondary);
	const double Now = World->GetTimeSeconds();
	// Mientras dura el golpe anterior no se repite (el servidor lo descartaría igualmente).
	if (Now - LastLocalUseSeconds < FTerrainToolModel::SecondsPerUse(Action, Tool))
	{
		return true;
	}
	FHitResult Hit;
	if (!TraceTerrain(Hit))
	{
		return true;
	}
	LastLocalUseSeconds = Now;
	const FVector ImpactMeters = Hit.ImpactPoint / 100.0;
	const ETerrainMaterial Material = Terrain->MaterialAt(ImpactMeters);
	const EMineHitCue Predicted = FTerrainToolModel::PredictCue(Action, Tool, Material, CarriedSoilM3);
	PlayCue(FromModel(Action), FromModel(Predicted), Hit.ImpactPoint);
	ServerUseTool(bSecondary, FVector_NetQuantize10(Hit.ImpactPoint));
	return true;
}

bool UTerrainToolComponent::TraceTerrain(FHitResult& OutHit) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	const UWorld* World = GetWorld();
	if (!Controller || !World)
	{
		return false;
	}
	FVector Eye;
	FRotator View;
	Controller->GetPlayerViewPoint(Eye, View);
	const FVector End = Eye + View.Vector() * (FTerrainToolModel::ReachMeters * 100.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredTerrainTool), false, Pawn);
	if (!World->LineTraceSingleByChannel(OutHit, Eye, End, ECC_Visibility, Params))
	{
		return false;
	}
	const AActor* HitActor = OutHit.GetActor();
	return HitActor && HitActor->ActorHasTag(UTerrainRuntimeMesher::TerrainTag);
}

void UTerrainToolComponent::PlayCue(EExploredTerrainAction Action, EExploredTerrainCue Cue, const FVector& LocationCm)
{
	OnToolCue.Broadcast(Action, Cue, LocationCm);
	USoundBase* Sound = Cue == EExploredTerrainCue::Rebound ? ReboundSound.Get() : (Cue == EExploredTerrainCue::Hit ? HitSound.Get() : nullptr);
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, LocationCm);
	}
}

void UTerrainToolComponent::ServerUseTool_Implementation(bool bSecondary, FVector_NetQuantize10 ImpactCm)
{
	using namespace TerrainToolComponentDetail;
	UTerrainEditSubsystem* Terrain = UTerrainEditSubsystem::Get(this);
	const UWorld* World = GetWorld();
	FTerrainToolRequest Request;
	if (!Terrain || !World || !MakeServerRequest(bSecondary, FVector(ImpactCm) / 100.0, Request))
	{
		return;
	}
	// Lo barato primero (acción, alcance, cadencia): una ráfaga descartada no evalúa la densidad.
	ETerrainToolVerdict Verdict = FTerrainToolModel::ValidateUse(Request);
	if (Verdict == ETerrainToolVerdict::Accepted)
	{
		FTerrainToolRequest WithDensity = Request;
		WithDensity.DensityAtImpact = Terrain->Density(Request.ImpactPoint);
		Verdict = FTerrainToolModel::Validate(WithDensity);
	}
	if (Verdict != ETerrainToolVerdict::Accepted)
	{
		UE_LOG(LogExplored, Verbose, TEXT("[Terreno] Uso descartado por el servidor (%d)"), static_cast<int32>(Verdict));
		return;
	}
	LastServerUseSeconds = World->GetTimeSeconds();
	const EExploredTerrainAction Action = FromModel(Request.Action);
	const EExploredTerrainCue Cue = ApplyOnServer(Action, static_cast<uint8>(Request.Tool), Request.ImpactPoint);
	MulticastToolCue(Action, Cue, ImpactCm);
}

bool UTerrainToolComponent::MakeServerRequest(bool bSecondary, const FVector& ImpactMeters, FTerrainToolRequest& OutRequest) const
{
	using namespace TerrainToolComponentDetail;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const UWorld* World = GetWorld();
	// La herramienta sale de las manos en el servidor, nunca de lo que diga el cliente.
	const UCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<UCarryComponent>() : nullptr;
	ETerrainDigTool Tool = ETerrainDigTool::Count;
	if (!Carry || !World || !IsFinite(ImpactMeters) || !FTerrainToolModel::ToolFromItem(HeldToolItem(*Carry, bSecondary), Tool))
	{
		return false;
	}
	OutRequest.Action = FTerrainToolModel::ActionFor(Tool, bSecondary);
	OutRequest.Tool = Tool;
	OutRequest.ImpactPoint = ImpactMeters;
	OutRequest.EyeLocation = Pawn->GetPawnViewLocation() / 100.0;
	OutRequest.SecondsSinceLastUse = static_cast<float>(World->GetTimeSeconds() - LastServerUseSeconds);
	return true;
}

EExploredTerrainCue UTerrainToolComponent::ApplyOnServer(EExploredTerrainAction Action, uint8 Tool, const FVector& ImpactMeters)
{
	using namespace TerrainToolComponentDetail;
	UTerrainEditSubsystem* Terrain = UTerrainEditSubsystem::Get(this);
	const ETerrainDigTool DigTool = static_cast<ETerrainDigTool>(Tool);
	const ETerrainMaterial Material = Terrain->MaterialAt(ImpactMeters);
	FTerrainEditResult Edit;
	switch (Action)
	{
	case EExploredTerrainAction::ShovelFlatten:
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		const double HalfHeight = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0;
		const double FeetZ = (GetOwner()->GetActorLocation().Z - HalfHeight) / 100.0;
		const int32 Tier = FTerrainEdits::ToolInfo(DigTool).ToolTier;
		Edit = Terrain->Shovel(FTerrainToolModel::MakeShovelStroke(ImpactMeters, FeetZ, Material, Tier, CarriedSoilM3));
		break;
	}
	case EExploredTerrainAction::PlaceSoil:
		Edit = Terrain->PlaceSoil(FTerrainToolModel::MakeSoilPlacement(ImpactMeters, CarriedSoilM3));
		break;
	default:
	{
		FTerrainDigHit Hit;
		Hit.ImpactPoint = ImpactMeters;
		Hit.Material = Material;
		Hit.Tool = DigTool;
		Edit = Terrain->Dig(Hit).Edit;
		break;
	}
	}
	CarriedSoilM3 = static_cast<float>(FTerrainToolModel::UpdateCarriedSoil(CarriedSoilM3, Material, Edit.VolumeRemoved, Edit.VolumeAdded));
	return FromModel(FTerrainToolModel::CueFromResult(Edit));
}

void UTerrainToolComponent::MulticastToolCue_Implementation(EExploredTerrainAction Action, EExploredTerrainCue Cue, FVector_NetQuantize10 LocationCm)
{
	// Quien golpea ya lo ha oído al pulsar (predicción local).
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		return;
	}
	PlayCue(Action, Cue, LocationCm);
}
