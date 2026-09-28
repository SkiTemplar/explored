#include "Debug/ExploredPlaytestAuditor.h"

#include "AssetCompilingManager.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "WorldCollision.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#include "DynamicRHI.h"
#include "RHIStats.h"

#include "Debug/ExploredPlaytestBot.h"
#include "Explored.h"
#include "Items/ExploredItemActor.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"

namespace
{
	/** Espera tras terminar shaders/streaming a que la escena (física incluida) se asiente. */
	constexpr float WarmupSeconds = 6.0f;
	/** Nº máximo de instancias de un mismo componente HISM que se muestrean con traza vertical
	 * (vegetación y rocas pueden tener miles; una traza por instancia sería demasiado coste en
	 * una pasada que corre una sola vez, así que se toma una muestra determinista repartida). */
	constexpr int32 MaxGroundSamplesPerComponent = 40;
	/** Techo de defectos de material acumulados, por si algo deja el mundo en un estado extremo. */
	constexpr int32 MaxMaterialIssues = 2000;
}

bool UExploredPlaytestAuditor::ShouldCreateSubsystem(UObject* Outer) const
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

void UExploredPlaytestAuditor::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}

	bStandalone = !FString(FCommandLine::Get()).Contains(TEXT("-ExploredShots="));
	OutputDir = FPaths::ProjectSavedDir() / TEXT("Shots");
	FParse::Value(FCommandLine::Get(), TEXT("ShotsDir="), OutputDir);

	bActive = true;
	Phase = EPhase::Warmup;
	Timer = 0.0f;
	UE_LOG(LogExplored, Display, TEXT("[Playtest] Auditor activo (standalone=%s) -> %s"),
		bStandalone ? TEXT("true") : TEXT("false"), *OutputDir);
}

void UExploredPlaytestAuditor::Tick(float DeltaTime)
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

	Timer += DeltaTime;

	if (Phase == EPhase::Warmup)
	{
		if (Timer >= WarmupSeconds)
		{
			AuditMaterials();
			AuditGroundClearance();
			AuditSpawnPoints();
			BeginPhysicsTracking();
			Timer = 0.0f;
			Phase = EPhase::WaitingPhysics;
		}
		return;
	}

	if (Phase == EPhase::WaitingPhysics)
	{
		if (Timer >= FPlaytestReportModel::PhysicsSampleSeconds)
		{
			SamplePhysicsDrift();
			Phase = EPhase::WaitingForBot;
		}
		return;
	}

	if (Phase == EPhase::WaitingForBot)
	{
		// Si UExploredPlaytestBot está activo (mismas condiciones de ShouldCreateSubsystem que
		// este auditor), se espera a que termine su ruta antes de escribir el informe: si no,
		// en solitario ("-ExploredPlaytestAudit" sin capturas) se cerraría el proceso a mitad
		// de la ruta del bot y sus pasos se quedarían fuera del JSON.
		const UExploredPlaytestBot* Bot = GetWorld()->GetSubsystem<UExploredPlaytestBot>();
		if (Bot && !Bot->IsDone())
		{
			return;
		}

		Phase = EPhase::Done;
		bActive = false;
		if (bStandalone)
		{
			WriteReport();
			UE_LOG(LogExplored, Display, TEXT("[Playtest] Auditor en solitario: informe escrito, cerrando"));
			FPlatformMisc::RequestExit(false, TEXT("ExploredPlaytestAudit"));
		}
	}
}

void UExploredPlaytestAuditor::AuditMaterials()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Found = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Found >= MaxMaterialIssues)
		{
			continue;
		}

		TArray<UStaticMeshComponent*> Comps;
		Actor->GetComponents(Comps);
		for (UStaticMeshComponent* Comp : Comps)
		{
			if (!Comp || !Comp->GetStaticMesh())
			{
				continue;
			}

			int32 InstanceCount = 1;
			if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Comp))
			{
				InstanceCount = ISM->GetInstanceCount();
				if (InstanceCount <= 0)
				{
					continue;
				}
			}

			const int32 NumMats = Comp->GetNumMaterials();
			for (int32 Slot = 0; Slot < NumMats; ++Slot)
			{
				UMaterialInterface* Mat = Comp->GetMaterial(Slot);
				const bool bMissing = Mat == nullptr;
				const bool bDefault = GEngine && Mat == GEngine->DefaultMaterial;
				const bool bGrid = GEngine && GEngine->WorldGridMaterial && Mat == GEngine->WorldGridMaterial;
				if (!bMissing && !bDefault && !bGrid)
				{
					continue;
				}

				const FVector LocationMeters = Comp->GetComponentLocation() / 100.0;
				FPlaytestReportModel::AppendMaterialIssue(Report, Comp->GetStaticMesh()->GetName(),
					FString::FromInt(Slot), InstanceCount, LocationMeters);
				++Found;
				if (Found >= MaxMaterialIssues)
				{
					UE_LOG(LogExplored, Warning, TEXT("[Playtest] Tope de %d defectos de material alcanzado; se deja de auditar"), MaxMaterialIssues);
					break;
				}
			}
		}
	}
	UE_LOG(LogExplored, Display, TEXT("[Playtest] Materiales: %d defecto(s)"), Found);
}

void UExploredPlaytestAuditor::AuditGroundClearance()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Found = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		TArray<UStaticMeshComponent*> Comps;
		Actor->GetComponents(Comps);
		for (UStaticMeshComponent* Comp : Comps)
		{
			if (!Comp || !Comp->GetStaticMesh())
			{
				continue;
			}

			FCollisionQueryParams Params(SCENE_QUERY_STAT(PlaytestGroundClearance), false);
			Params.AddIgnoredActor(Actor);

			// Asunción: el pivote de las mallas de props/vegetación está en la base (convención del
			// pipeline de arte de este proyecto). Si algún día deja de cumplirse, este muestreo
			// empezaría a marcar falsos "enterrado"/"flotando" sistemáticos y sería la primera pista.
			if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Comp))
			{
				const int32 InstanceCount = ISM->GetInstanceCount();
				const int32 Stride = FMath::Max(1, InstanceCount / MaxGroundSamplesPerComponent);
				for (int32 Index = 0; Index < InstanceCount; Index += Stride)
				{
					FTransform InstanceTransform;
					if (!ISM->GetInstanceTransform(Index, InstanceTransform, true))
					{
						continue;
					}
					const FVector Base = InstanceTransform.GetLocation();
					FHitResult Hit;
					if (World->LineTraceSingleByChannel(Hit, Base + FVector(0, 0, 300.0f), Base - FVector(0, 0, 1000.0f), ECC_WorldStatic, Params))
					{
						bool bFloating = false, bBuried = false;
						const float ClearanceCm = Base.Z - Hit.Location.Z;
						if (FPlaytestReportModel::IsFloatingOrBuried(ClearanceCm, bFloating, bBuried))
						{
							FPlaytestReportModel::AppendFloatingOrBuriedIssue(Report,
								FString::Printf(TEXT("%s#%d"), *Comp->GetStaticMesh()->GetName(), Index), Base / 100.0, ClearanceCm, bFloating);
							++Found;
						}
					}
				}
			}
			else
			{
				const FVector Base = Comp->GetComponentLocation();
				FHitResult Hit;
				if (World->LineTraceSingleByChannel(Hit, Base + FVector(0, 0, 300.0f), Base - FVector(0, 0, 1000.0f), ECC_WorldStatic, Params))
				{
					bool bFloating = false, bBuried = false;
					const float ClearanceCm = Base.Z - Hit.Location.Z;
					if (FPlaytestReportModel::IsFloatingOrBuried(ClearanceCm, bFloating, bBuried))
					{
						FPlaytestReportModel::AppendFloatingOrBuriedIssue(Report, Actor->GetName(), Base / 100.0, ClearanceCm, bFloating);
						++Found;
					}
				}
			}
		}
	}
	UE_LOG(LogExplored, Display, TEXT("[Playtest] Flotando/enterrado: %d defecto(s)"), Found);
}

void UExploredPlaytestAuditor::AuditSpawnPoints()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Valores por defecto de ACharacter si no hay ningún pawn del que medir la cápsula real.
	float CapsuleRadius = 34.0f;
	float CapsuleHalfHeight = 88.0f;
	if (const ACharacter* Character = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(World, 0)))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			CapsuleRadius = Capsule->GetScaledCapsuleRadius();
			CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	int32 Found = 0;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!Start)
		{
			continue;
		}

		const FVector Location = Start->GetActorLocation() + FVector(0, 0, CapsuleHalfHeight + 2.0f);
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PlaytestSpawnOverlap), false);

		TArray<FOverlapResult> Overlaps;
		if (!World->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_Pawn, Shape, Params))
		{
			continue;
		}

		TArray<FString> Names;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			const AActor* OverlapActor = Overlap.GetActor();
			if (OverlapActor && !OverlapActor->IsA<ACharacter>())
			{
				Names.AddUnique(OverlapActor->GetName());
			}
		}
		if (Names.Num() == 0)
		{
			continue;
		}

		FString Csv;
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			Csv += (Index > 0 ? TEXT(", ") : TEXT("")) + Names[Index];
		}
		FPlaytestReportModel::AppendSpawnBlockedIssue(Report, Start->GetName(), Start->GetActorLocation() / 100.0, Csv);
		++Found;
	}
	UE_LOG(LogExplored, Display, TEXT("[Playtest] Puntos de aparición bloqueados: %d"), Found);
}

void UExploredPlaytestAuditor::BeginPhysicsTracking()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
		const bool bSimulatingPhysics = Root && Root->IsSimulatingPhysics();
		const bool bIsItem = Actor->IsA<AExploredItemActor>();
		if (!bSimulatingPhysics && !bIsItem)
		{
			continue;
		}
		FTrackedPhysicsActor Tracked;
		Tracked.Actor = Actor;
		Tracked.StartLocationMeters = Actor->GetActorLocation() / 100.0;
		TrackedPhysicsActors.Add(Tracked);
	}
	UE_LOG(LogExplored, Display, TEXT("[Playtest] Siguiendo %d actor(es) con física/objetos sueltos"), TrackedPhysicsActors.Num());
}

void UExploredPlaytestAuditor::SamplePhysicsDrift()
{
	const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));

	int32 Found = 0;
	for (const FTrackedPhysicsActor& Tracked : TrackedPhysicsActors)
	{
		AActor* Actor = Tracked.Actor.Get();
		if (!Actor)
		{
			// Se destruyó (recogido, consumido...) en la ventana de muestreo: no es un defecto.
			continue;
		}

		const FVector NowMeters = Actor->GetActorLocation() / 100.0;
		if (!FPlaytestReportModel::DidPhysicsDrift(Tracked.StartLocationMeters * 100.0, NowMeters * 100.0))
		{
			continue;
		}

		const float GroundHeight = Density.SampleColumn(NowMeters.X, NowMeters.Y).Height;
		const bool bBelowTerrain = NowMeters.Z < GroundHeight - 0.1f;
		const bool bBelowWater = NowMeters.Z < FArchipelagoLayout::SeaLevel - 0.1f;
		FPlaytestReportModel::AppendPhysicsDriftIssue(Report, Actor->GetName(), Tracked.StartLocationMeters, NowMeters, bBelowTerrain, bBelowWater);
		++Found;
	}
	UE_LOG(LogExplored, Display, TEXT("[Playtest] Física a la deriva: %d defecto(s)"), Found);
}

void UExploredPlaytestAuditor::RecordFrameSample(const FString& ShotName, float AvgFPS, float MinFPS)
{
	FTextureMemoryStats Stats;
	RHIGetTextureMemoryStats(Stats);
	const double VRAMUsedMB = (Stats.StreamingMemorySize + Stats.NonStreamingMemorySize) / (1024.0 * 1024.0);

	FPlaytestFrameSample Sample;
	Sample.ShotName = ShotName;
	Sample.AvgFPS = AvgFPS;
	Sample.MinFPS = MinFPS;
	Sample.VRAMUsedMB = VRAMUsedMB;
	Report.FrameSamples.Add(Sample);
}

void UExploredPlaytestAuditor::RecordBotStep(const FString& WaypointName, const FVector& LocationMeters,
	const FString& FocusedActorName, bool bInteracted, const FString& InventoryDelta)
{
	FPlaytestReportModel::AppendBotStep(Report, WaypointName, LocationMeters, FocusedActorName, bInteracted, InventoryDelta);
}

void UExploredPlaytestAuditor::WriteReport()
{
	IFileManager::Get().MakeDirectory(*OutputDir, true);
	const FString JsonPath = OutputDir / TEXT("playtest_report.json");
	const FString TxtPath = OutputDir / TEXT("playtest_report.txt");
	FFileHelper::SaveStringToFile(FPlaytestReportModel::ToJson(Report), *JsonPath);
	FFileHelper::SaveStringToFile(FPlaytestReportModel::ToReadableSummary(Report), *TxtPath);
	UE_LOG(LogExplored, Display, TEXT("[Playtest] Informe: %d defecto(s), %d muestra(s) de rendimiento -> %s"),
		Report.Issues.Num(), Report.FrameSamples.Num(), *JsonPath);
}

TStatId UExploredPlaytestAuditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredPlaytestAuditor, STATGROUP_Tickables);
}
