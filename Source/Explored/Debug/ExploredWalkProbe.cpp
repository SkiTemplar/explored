#include "Debug/ExploredWalkProbe.h"

#include "AssetCompilingManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"

#include "Explored.h"
#include "UI/ExploredPlayerController.h"
#include "WorldGen/IslandLayout.h"
#include "WorldGen/IslandLayoutLoader.h"

namespace ExploredWalkProbeDetail
{
	/** Espera tras los shaders para que el personaje caiga al suelo y el mundo se asiente. */
	constexpr float WarmupSeconds = 4.0f;
	constexpr float LogEverySeconds = 5.0f;
	/** La meta se queda a esta distancia del centro de la torre (su base, fuera de su colisión). */
	constexpr float StopShortOfTowerMeters = 5.0f;

	FVector2D ToMeters(const FVector& Cm)
	{
		return FVector2D(Cm.X / 100.0, Cm.Y / 100.0);
	}

	/** Cala Varada → Palmar → base del faro por los caminos de autor. */
	TArray<FVector2D> BuildRoute(const FIslandLayout& Layout)
	{
		TArray<FVector2D> Route;
		for (const TCHAR* PathId : { TEXT("camino_cala_palmar"), TEXT("camino_palmar_faro") })
		{
			if (const FIslandPath* Path = Layout.FindPath(PathId))
			{
				Route.Append(Path->Points);
			}
		}
		const FIslandPoint* Tower = Layout.FindPoint(TEXT("lighthouse"));
		if (Tower && Route.Num() >= 2)
		{
			// El último tramo acaba en el centro de la torre: se recorta hasta su base.
			const FVector2D Before = Route[Route.Num() - 2];
			const FVector2D Dir = (Tower->Location - Before).GetSafeNormal();
			Route.Last() = Tower->Location - Dir * StopShortOfTowerMeters;
		}
		return Route;
	}
}

bool UExploredWalkProbe::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("ExploredWalkProbe"));
#endif
}

TStatId UExploredWalkProbe::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredWalkProbe, STATGROUP_Tickables);
}

void UExploredWalkProbe::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	OutputPath = FPaths::ProjectLogDir() / TEXT("walk_probe.json");
	FParse::Value(FCommandLine::Get(), TEXT("WalkProbeOut="), OutputPath, false);
	if (AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(&InWorld, 0)))
	{
		PC->SetUIMode(EExploredUIMode::Playing);
	}
	bActive = true;
	bWalking = false;
	Timer = -ExploredWalkProbeDetail::WarmupSeconds;
	UE_LOG(LogExplored, Display, TEXT("[WalkProbe] activo -> %s"), *OutputPath);
}

void UExploredWalkProbe::BeginWalk()
{
	const FIslandLayout& Layout = *ExploredIsland::Get();
	const ACharacter* Character = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (!Character)
	{
		UE_LOG(LogExplored, Error, TEXT("[WalkProbe] no hay personaje"));
		Finish();
		return;
	}
	StartLocation = Character->GetActorLocation();
	const FVector2D Start = ExploredWalkProbeDetail::ToMeters(StartLocation);
	const int32 Zone = Layout.FindZoneAt(Start);
	StartZone = Layout.Zones.IsValidIndex(Zone) ? Layout.Zones[Zone].Id.ToString() : TEXT("ninguna");
	Probe.Start(ExploredWalkProbeDetail::BuildRoute(Layout));
	bWalking = true;
	UE_LOG(LogExplored, Display, TEXT("[WalkProbe] salida en (%.1f, %.1f, %.2f) m, zona %s, %d puntos de paso"),
		Start.X, Start.Y, StartLocation.Z / 100.0, *StartZone, Probe.GetWaypointCount());
}

void UExploredWalkProbe::Tick(float DeltaTime)
{
	if ((GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) || FAssetCompilingManager::Get().GetNumRemainingAssets() > 0)
	{
		return;
	}
	Timer += DeltaTime;
	if (Timer < 0.0f)
	{
		return;
	}
	if (!bWalking)
	{
		BeginWalk();
		return;
	}
	ACharacter* Character = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (!Character)
	{
		Finish();
		return;
	}
	const FVector Location = Character->GetActorLocation();
	const FVector2D Here = ExploredWalkProbeDetail::ToMeters(Location);
	MaxHeightMeters = FMath::Max(MaxHeightMeters, static_cast<float>(Location.Z / 100.0));
	if (Probe.Step(Here, DeltaTime) != EWalkProbeStatus::Walking)
	{
		Finish();
		return;
	}
	const FVector2D Dir = Probe.DesiredDirection(Here);
	const FVector Dir3(Dir.X, Dir.Y, 0.0);
	Character->AddMovementInput(Dir3, 1.0f);
	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(Dir3.Rotation());
	}
	LogTimer += DeltaTime;
	if (LogTimer >= ExploredWalkProbeDetail::LogEverySeconds)
	{
		LogTimer = 0.0f;
		UE_LOG(LogExplored, Display, TEXT("[WalkProbe] t=%.0f s en (%.1f, %.1f, %.2f) m, punto %d/%d"), Probe.GetElapsedSeconds(),
			Here.X, Here.Y, Location.Z / 100.0, Probe.GetWaypointIndex(), Probe.GetWaypointCount());
	}
}

void UExploredWalkProbe::Finish()
{
	bActive = false;
	const ACharacter* Character = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	const FVector End = Character ? Character->GetActorLocation() : FVector::ZeroVector;
	const FString Json = FString::Printf(
		TEXT("{\n  \"status\": \"%s\",\n  \"start_zone\": \"%s\",\n  \"start_m\": [%.2f, %.2f, %.2f],\n  \"end_m\": [%.2f, %.2f, %.2f],\n")
		TEXT("  \"waypoints_reached\": %d,\n  \"waypoints\": %d,\n  \"elapsed_s\": %.1f,\n  \"travelled_m\": %.1f,\n  \"max_height_m\": %.2f\n}\n"),
		LexToString(Probe.GetStatus()), *StartZone, StartLocation.X / 100.0, StartLocation.Y / 100.0, StartLocation.Z / 100.0,
		End.X / 100.0, End.Y / 100.0, End.Z / 100.0, Probe.GetWaypointIndex(), Probe.GetWaypointCount(), Probe.GetElapsedSeconds(),
		Probe.GetTravelledMeters(), MaxHeightMeters);
	FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogExplored, Display, TEXT("[WalkProbe] resultado %s: %d/%d puntos en %.1f s (%.0f m)"), LexToString(Probe.GetStatus()),
		Probe.GetWaypointIndex(), Probe.GetWaypointCount(), Probe.GetElapsedSeconds(), Probe.GetTravelledMeters());
	FPlatformMisc::RequestExit(false);
}
