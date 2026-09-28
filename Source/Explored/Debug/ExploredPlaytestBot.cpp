#include "Debug/ExploredPlaytestBot.h"

#include "AssetCompilingManager.h"
#include "Camera/CameraComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#include "Carry/CarryComponent.h"
#include "Debug/ExploredPlaytestAuditor.h"
#include "Explored.h"
#include "Interaction/InteractionComponent.h"
#include "Items/ExploredItemActor.h"
#include "Player/ExploredCharacter.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/PointsOfInterest.h"
#include "WorldGen/TerrainDensity.h"

namespace
{
	/** Espera tras el asentamiento de shaders/streaming antes de empezar a moverse. */
	constexpr float WarmupSeconds = 4.0f;
	/** Tiempo mirando al punto para que UInteractionComponent (20 Hz) fije el foco. */
	constexpr float FocusWaitSeconds = 2.5f;
	/** Tiempo tras interactuar antes de comparar el inventario (animación, spawn de recursos...). */
	constexpr float EffectWaitSeconds = 1.5f;
}

bool UExploredPlaytestBot::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const bool bStandaloneFlag = FParse::Param(FCommandLine::Get(), TEXT("ExploredPlaytestAudit"));
	FString ShotsSet;
	FParse::Value(FCommandLine::Get(), TEXT("ExploredShots="), ShotsSet);
	return bStandaloneFlag || ShotsSet == TEXT("playtest");
#endif
}

void UExploredPlaytestBot::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}

	OutputDir = FPaths::ProjectSavedDir() / TEXT("Shots");
	FParse::Value(FCommandLine::Get(), TEXT("ShotsDir="), OutputDir);

	Character = Cast<AExploredCharacter>(UGameplayStatics::GetPlayerCharacter(&InWorld, 0));
	if (!Character.IsValid())
	{
		UE_LOG(LogExplored, Warning, TEXT("[PlaytestBot] Sin AExploredCharacter en el mundo; no hay ruta que recorrer"));
		bDone = true;
		return;
	}

	BuildRoute();
	bActive = true;
	UE_LOG(LogExplored, Display, TEXT("[PlaytestBot] Activo, %d punto(s) en la ruta"), Route.Num());
}

void UExploredPlaytestBot::BuildRoute()
{
	const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
	const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
	if (!Landing)
	{
		UE_LOG(LogExplored, Warning, TEXT("[PlaytestBot] No se encontró la isla Landing; ruta vacía"));
		return;
	}

	FVector Beach = FVector::ZeroVector;
	bool bHasBeach = false;
	for (int32 Step = 0; Step < 12 && !bHasBeach; ++Step)
	{
		bHasBeach = FPoiLayout::FindBeach(Density, *Landing, Landing->Rotation + Step * (UE_PI / 6.0f), Beach);
	}

	FVector Inland = FVector::ZeroVector;
	bool bHasInland = false;
	for (int32 Step = 0; Step < 12 && !bHasInland; ++Step)
	{
		bHasInland = FPoiLayout::FindInland(Density, *Landing, Landing->Rotation + Step * (UE_PI / 6.0f), 0.5f, Inland);
	}

	auto AddWaypoint = [this, Landing](const FString& Name, const FVector& PointMeters)
	{
		FWaypoint Waypoint;
		Waypoint.Name = Name;
		Waypoint.LocationMeters = PointMeters + FVector(0, 0, 1.7f);
		const FVector Target(Landing->Center.X, Landing->Center.Y, Waypoint.LocationMeters.Z);
		Waypoint.AimRotation = (Target - Waypoint.LocationMeters).Rotation();
		Route.Add(Waypoint);
	};

	if (bHasBeach)
	{
		AddWaypoint(TEXT("Landing_playa"), Beach);
	}
	if (bHasInland)
	{
		AddWaypoint(TEXT("Landing_jungla"), Inland);
	}
}

void UExploredPlaytestBot::BeginWaypoint(int32 Index)
{
	CurrentWaypoint = Index;
	const FWaypoint& Waypoint = Route[Index];

	if (AExploredCharacter* Char = Character.Get())
	{
		// Mismo ETeleportType que usa UExploredShotSubsystem para el pawn: sin física a medias
		// entre el punto anterior y este.
		Char->SetActorLocation(Waypoint.LocationMeters * 100.0, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCameraComponent* Camera = Char->GetCamera())
		{
			// UInteractionComponent traza desde esta cámara (ver InteractionComponent.cpp);
			// apuntarla es lo único que hace falta para que "mire hacia" el punto de interés.
			Camera->SetWorldRotation(Waypoint.AimRotation);
		}
	}

	SnapshotInventory(CountsBeforeStep);
	Timer = 0.0f;
	StepPhase = EStepPhase::WaitFocus;
	FocusedActorNameAtStep.Reset();
	bInteractedAtStep = false;
	UE_LOG(LogExplored, Display, TEXT("[PlaytestBot] %s"), *Waypoint.Name);
}

void UExploredPlaytestBot::SnapshotInventory(TMap<FName, int32>& OutCounts) const
{
	OutCounts.Reset();
	if (const AExploredCharacter* Char = Character.Get())
	{
		if (const UCarryComponent* Carry = Char->GetCarryComponent())
		{
			TSet<FName> Tools;
			Carry->CountMaterials(OutCounts, Tools);
		}
	}
}

FString UExploredPlaytestBot::DescribeInventoryDelta(const TMap<FName, int32>& Before, const TMap<FName, int32>& After) const
{
	TSet<FName> Keys;
	for (const TPair<FName, int32>& Pair : Before)
	{
		Keys.Add(Pair.Key);
	}
	for (const TPair<FName, int32>& Pair : After)
	{
		Keys.Add(Pair.Key);
	}

	TArray<FString> Parts;
	for (const FName& Key : Keys)
	{
		const int32 Delta = After.FindRef(Key) - Before.FindRef(Key);
		if (Delta != 0)
		{
			Parts.Add(FString::Printf(TEXT("%+d %s"), Delta, *Key.ToString()));
		}
	}

	FString Result;
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		Result += (Index > 0 ? TEXT(", ") : TEXT("")) + Parts[Index];
	}
	return Result;
}

void UExploredPlaytestBot::Tick(float DeltaTime)
{
	if ((GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) || FAssetCompilingManager::Get().GetNumRemainingAssets() > 0)
	{
		return;
	}
	if (const UWorldPartitionSubsystem* Partition = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>())
	{
		if (!Partition->IsStreamingCompleted())
		{
			return;
		}
	}

	if (!bWarmedUp)
	{
		WarmupTimer += DeltaTime;
		if (WarmupTimer < WarmupSeconds)
		{
			return;
		}
		bWarmedUp = true;
		if (Route.Num() == 0)
		{
			bDone = true;
			bActive = false;
			return;
		}
		BeginWaypoint(0);
		return;
	}

	AExploredCharacter* Char = Character.Get();
	if (!Char)
	{
		bDone = true;
		bActive = false;
		return;
	}

	Timer += DeltaTime;

	switch (StepPhase)
	{
	case EStepPhase::WaitFocus:
		if (Timer >= FocusWaitSeconds)
		{
			if (const UInteractionComponent* Interaction = Char->GetInteractionComponent())
			{
				if (const AActor* Focus = Interaction->GetFocusedActor())
				{
					FocusedActorNameAtStep = Focus->GetName();
				}
			}
			StepPhase = EStepPhase::RequestBefore;
		}
		break;

	case EStepPhase::RequestBefore:
	{
		const FString File = OutputDir / (Route[CurrentWaypoint].Name + TEXT("_bot_before.png"));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		StepPhase = EStepPhase::WaitBeforeDone;
		break;
	}

	case EStepPhase::WaitBeforeDone:
		if (!FScreenshotRequest::IsScreenshotRequested())
		{
			StepPhase = EStepPhase::Interact;
		}
		break;

	case EStepPhase::Interact:
	{
		UInteractionComponent* Interaction = Char->GetInteractionComponent();
		UCarryComponent* Carry = Char->GetCarryComponent();
		if (Interaction && !FocusedActorNameAtStep.IsEmpty())
		{
			// Mismo despacho que AExploredCharacter::HandleInteract: un objeto suelto
			// (AExploredItemActor) se coge con UCarryComponent::TryPickUp; el resto usa el
			// evento genérico Interact (talar, recolectar de una celda de vegetación...).
			if (AExploredItemActor* ItemActor = Cast<AExploredItemActor>(Interaction->GetFocusedActor()))
			{
				if (Carry)
				{
					FText FailReason;
					bInteractedAtStep = Carry->TryPickUp(ItemActor, FailReason);
				}
			}
			else
			{
				bInteractedAtStep = Interaction->InteractWithFocus();
			}
		}
		Timer = 0.0f;
		StepPhase = EStepPhase::WaitEffect;
		break;
	}

	case EStepPhase::WaitEffect:
		if (Timer >= EffectWaitSeconds)
		{
			StepPhase = EStepPhase::RequestAfter;
		}
		break;

	case EStepPhase::RequestAfter:
	{
		TMap<FName, int32> CountsAfter;
		SnapshotInventory(CountsAfter);
		const FString Delta = DescribeInventoryDelta(CountsBeforeStep, CountsAfter);
		if (UExploredPlaytestAuditor* Auditor = GetWorld()->GetSubsystem<UExploredPlaytestAuditor>())
		{
			Auditor->RecordBotStep(Route[CurrentWaypoint].Name, Route[CurrentWaypoint].LocationMeters,
				FocusedActorNameAtStep, bInteractedAtStep, Delta);
		}
		const FString File = OutputDir / (Route[CurrentWaypoint].Name + TEXT("_bot_after.png"));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		StepPhase = EStepPhase::WaitAfterDone;
		break;
	}

	case EStepPhase::WaitAfterDone:
		if (!FScreenshotRequest::IsScreenshotRequested())
		{
			const int32 Next = CurrentWaypoint + 1;
			if (Next >= Route.Num())
			{
				bDone = true;
				bActive = false;
				UE_LOG(LogExplored, Display, TEXT("[PlaytestBot] Ruta terminada"));
			}
			else
			{
				BeginWaypoint(Next);
			}
		}
		break;
	}
}

TStatId UExploredPlaytestBot::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredPlaytestBot, STATGROUP_Tickables);
}
