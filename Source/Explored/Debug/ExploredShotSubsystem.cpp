#include "Debug/ExploredShotSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

#include "Explored.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"

namespace
{
	constexpr uint32 WorldSeed = 20260926;
	/** Segundos de espera en cada vista para que Lumen y la exposición converjan. */
	constexpr float SettleSeconds = 4.0f;
	/** Espera inicial tras terminar los shaders (streaming de Nanite y distance fields). */
	constexpr float WarmupSeconds = 8.0f;
}

bool UExploredShotSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("ExploredShots")) ||
		FString(FCommandLine::Get()).Contains(TEXT("-ExploredShots="));
#endif
}

void UExploredShotSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}

	FString Set = TEXT("all");
	FParse::Value(FCommandLine::Get(), TEXT("ExploredShots="), Set);
	OutputDir = FPaths::ProjectSavedDir() / TEXT("Shots");
	FParse::Value(FCommandLine::Get(), TEXT("ShotsDir="), OutputDir);
	BuildShotList(Set);

	bActive = Shots.Num() > 0;
	Current = INDEX_NONE;
	Timer = -WarmupSeconds;
	UE_LOG(LogExplored, Display, TEXT("[Shots] %d vistas en el conjunto «%s» -> %s"), Shots.Num(), *Set, *OutputDir);
}

void UExploredShotSubsystem::BuildShotList(const FString& Set)
{
	const FTerrainDensity Density(FArchipelagoLayout::Generate(WorldSeed));
	const bool bAll = Set == TEXT("all");

	if (bAll || Set == TEXT("spawn"))
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			FExploredShot Shot;
			Shot.Name = TEXT("spawn");
			Shot.Location = It->GetActorLocation() + FVector(0, 0, 70);
			// Mira hacia el centro de la isla de inicio.
			const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
			const FVector Target(Landing->Center.X * 100.0, Landing->Center.Y * 100.0, Shot.Location.Z);
			Shot.Rotation = (Target - Shot.Location).Rotation();
			Shot.Rotation.Pitch = -3.0f;
			Shot.Hours = 9.5f;
			Shots.Add(Shot);
			break;
		}
	}

	if (bAll || Set == TEXT("islands"))
	{
		for (const FIslandDesc& Island : Density.GetLayout().Islands)
		{
			// Cámara en el mar, a 1,6 radios del centro, mirando a la isla desde 40 m.
			const float Angle = Island.Rotation + 0.9f;
			const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			const FVector2D Cam2D = Island.Center + Dir * Island.Radius * 1.45f;
			const float Height = FMath::Max(Density.SampleColumn(Cam2D.X, Cam2D.Y).Height, 0.0f) + 25.0f + Island.MaxHeight * 0.25f;
			FExploredShot Shot;
			Shot.Name = FString::Printf(TEXT("island_%s"), LexToString(Island.Archetype));
			Shot.Location = FVector(Cam2D.X, Cam2D.Y, Height) * 100.0;
			const FVector Target(Island.Center.X * 100.0, Island.Center.Y * 100.0, Island.MaxHeight * 35.0);
			Shot.Rotation = (Target - Shot.Location).Rotation();
			Shot.Hours = 16.0f;
			Shots.Add(Shot);
		}
	}

	if (bAll || Set == TEXT("day"))
	{
		const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
		const FVector2D Cam2D = Landing->Center + FVector2D(Landing->Radius * 1.3f, -Landing->Radius * 0.4f);
		const FVector Location(Cam2D.X * 100.0, Cam2D.Y * 100.0, 1500.0);
		const FRotator Rotation = (FVector(Landing->Center.X * 100.0, Landing->Center.Y * 100.0, 800.0) - Location).Rotation();
		for (const float Hours : {6.2f, 9.0f, 13.0f, 17.6f, 18.4f, 22.0f})
		{
			FExploredShot Shot;
			Shot.Name = FString::Printf(TEXT("day_%04.1f"), Hours);
			Shot.Location = Location;
			Shot.Rotation = Rotation;
			Shot.Hours = Hours;
			Shots.Add(Shot);
		}
	}

	if (bAll || Set == TEXT("aerial"))
	{
		FExploredShot Shot;
		Shot.Name = TEXT("aerial");
		Shot.Location = FVector(0.0, -4200.0 * 100.0, 1800.0 * 100.0);
		Shot.Rotation = FRotator(-32.0f, 90.0f, 0.0f);
		Shot.Hours = 15.5f;
		Shots.Add(Shot);
	}
}

void UExploredShotSubsystem::BeginShot(int32 Index)
{
	const FExploredShot& Shot = Shots[Index];
	UWorld* World = GetWorld();

	if (!Camera)
	{
		Camera = World->SpawnActor<ACameraActor>(Shot.Location, Shot.Rotation);
		Camera->GetCameraComponent()->SetFieldOfView(80.0f);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	Camera->SetActorLocationAndRotation(Shot.Location, Shot.Rotation);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		PC->SetViewTarget(Camera);
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorHiddenInGame(true);
		}
	}
	if (UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Time->SetTime(Time->GetDay(), Shot.Hours);
		Time->SetTimeScale(0.0f);
	}
	Timer = 0.0f;
	bRequested = false;
}

void UExploredShotSubsystem::Tick(float DeltaTime)
{
	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
	{
		return;
	}

	Timer += DeltaTime;
	if (Current == INDEX_NONE)
	{
		if (Timer >= 0.0f)
		{
			Current = 0;
			BeginShot(Current);
		}
		return;
	}

	if (!bRequested && Timer >= SettleSeconds)
	{
		const FString File = OutputDir / (Shots[Current].Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		bRequested = true;
		UE_LOG(LogExplored, Display, TEXT("[Shots] %s"), *File);
		return;
	}

	// Un fotograma después de pedir la captura pasa a la siguiente vista.
	if (bRequested && !FScreenshotRequest::IsScreenshotRequested())
	{
		++Current;
		if (Current >= Shots.Num())
		{
			bActive = false;
			UE_LOG(LogExplored, Display, TEXT("[Shots] Terminado"));
			FPlatformMisc::RequestExit(false, TEXT("ExploredShots"));
			return;
		}
		BeginShot(Current);
	}
}

TStatId UExploredShotSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredShotSubsystem, STATGROUP_Tickables);
}
