#include "Debug/ExploredShotSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "AssetCompilingManager.h"
#include "ContentStreaming.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#include "Debug/ExploredPlaytestAuditor.h"
#include "Explored.h"
#include "Narrative/ExploredVoyageSubsystem.h"
#include "Interaction/InteractionComponent.h"
#include "Player/ExploredCharacter.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "UI/ExploredPlayerController.h"
#include "WorldGen/IslandLayout.h"
#include "WorldGen/IslandLayoutLoader.h"

// Contadores de RenderCore/RHI que alimentan el HUD de «stat unit»/«stat gpu» (ciclos del último
// fotograma, ver RenderTimer.h): se leen directamente para volcarlos a texto en -ExploredBench sin
// depender de capturar el overlay en pantalla. La API de GPU en crudo (GGPUFrameTime) está
// deprecada desde 5.6 a favor de RHIGetGPUFrameCycles().
#include "DynamicRHI.h"
#include "RenderTimer.h"
#include "RHIStats.h"

namespace
{
	/** Segundos de espera en cada vista para que Lumen y la exposición converjan. */
	constexpr float SettleSeconds = 4.0f;
	/** Espera inicial tras terminar los shaders (streaming de Nanite y distance fields). */
	constexpr float WarmupSeconds = 8.0f;
	/** Espera máxima adicional a que el streaming de texturas alcance los mips pedidos. */
	constexpr float MaxStreamingWaitSeconds = 20.0f;
}

bool UExploredShotSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("ExploredShots")) ||
		FString(FCommandLine::Get()).Contains(TEXT("-ExploredShots=")) ||
		FParse::Param(FCommandLine::Get(), TEXT("ExploredBench"));
#endif
}

void UExploredShotSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}

	bBenchMode = FParse::Param(FCommandLine::Get(), TEXT("ExploredBench"));

	FString Set = bBenchMode ? TEXT("bench") : TEXT("all");
	FParse::Value(FCommandLine::Get(), TEXT("ExploredShots="), Set);
	OutputDir = FPaths::ProjectSavedDir() / TEXT("Shots");
	FParse::Value(FCommandLine::Get(), TEXT("ShotsDir="), OutputDir);
	BuildShotList(Set);

	// El conjunto «menu» quiere ver el frontend (menú, ajustes, pausa) tal
	// cual lo vería el jugador; el resto de conjuntos son verificación del
	// mundo y no deben quedar tapados por el menú principal que se muestra
	// por defecto al arrancar el mapa.
	if (Set != TEXT("menu"))
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(&InWorld, 0))
		{
			if (AExploredPlayerController* ExploredPC = Cast<AExploredPlayerController>(PC))
			{
				ExploredPC->SetUIMode(EExploredUIMode::Playing);
			}
		}
	}

	bActive = Shots.Num() > 0;
	Current = INDEX_NONE;
	Timer = -WarmupSeconds;
	UE_LOG(LogExplored, Display, TEXT("[Shots] %d vistas en el conjunto «%s» -> %s"), Shots.Num(), *Set, *OutputDir);
}

void UExploredShotSubsystem::BuildShotList(const FString& Set)
{
	// Vistas sobre la disposición de autor (island.json). Las alturas salen del relieve
	// analítico de FIslandLayout, el mismo con el que Tools/Island genera la malla del terreno.
	const FIslandLayout& Layout = *ExploredIsland::Get();
	const bool bAll = Set == TEXT("all");
	const bool bBench = Set == TEXT("bench");
	const FIslandDesc* Main = Layout.Islands.IsEmpty() ? nullptr : &Layout.Islands[0];
	auto GroundAt = [&Layout](const FVector2D& P)
	{
		return FMath::Max(Layout.SampleColumn(P.X, P.Y).Height, FIslandLayout::SeaLevel);
	};
	auto AddShot = [this](const FString& Name, const FVector& EyeMeters, const FVector& LookAtMeters, float ShotHours)
	{
		FExploredShot Shot;
		Shot.Name = Name;
		Shot.Location = EyeMeters * 100.0;
		Shot.Rotation = ((LookAtMeters - EyeMeters) * 100.0).Rotation();
		Shot.Hours = ShotHours;
		Shots.Add(Shot);
	};

	if ((bAll || bBench || Set == TEXT("spawn")) && Main)
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			const FVector Eye = It->GetActorLocation() / 100.0 + FVector(0, 0, 0.7f);
			AddShot(TEXT("spawn"), Eye, FVector(Main->Center.X, Main->Center.Y, Eye.Z - 1.0f), 9.5f);
			break;
		}
	}

	if (bAll || Set == TEXT("islands"))
	{
		// Cada tierra desde el mar, a 1,45 radios del centro y por encima de su meseta.
		for (const FIslandDesc& Island : Layout.Islands)
		{
			const FVector2D Dir(FMath::Cos(Island.Rotation + 0.9f), FMath::Sin(Island.Rotation + 0.9f));
			const FVector2D Cam2D = Island.Center + Dir * Island.RadiusAlong(Dir) * 1.45f;
			const FVector Eye(Cam2D.X, Cam2D.Y, GroundAt(Cam2D) + 25.0f + Island.MaxHeight);
			AddShot(FString::Printf(TEXT("island_%s"), *Island.Id.ToString()), Eye,
				FVector(Island.Center.X, Island.Center.Y, Island.MaxHeight), 16.0f);
		}
	}

	if (bAll || Set == TEXT("playtest"))
	{
		// Recorrido a pie: una vista por zona a varias horas y una por punto de autor a mediodía.
		struct FHourNamed { const TCHAR* Name; float Hours; };
		const FHourNamed Hours[] = {
			{ TEXT("amanecer"), 6.2f },
			{ TEXT("mediodia"), 13.0f },
			{ TEXT("atardecer"), 17.6f },
			{ TEXT("noche"), 22.0f },
		};
		for (const FIslandZone& Zone : Layout.Zones)
		{
			const FIslandColumn Column = Layout.SampleColumn(Zone.Center.X, Zone.Center.Y);
			const FVector2D LandCenter = Layout.Islands.IsValidIndex(Column.IslandIndex) ? Layout.Islands[Column.IslandIndex].Center : Zone.Center;
			// Un paso hacia el exterior de la tierra para mirar hacia dentro, a la zona.
			const FVector2D Out = (Zone.Center - LandCenter).GetSafeNormal();
			const FVector2D Stand2D = Zone.Center + (Out.IsNearlyZero() ? FVector2D(-1.0, 0.0) : Out) * Zone.Radius * 0.5f;
			const FVector Eye(Stand2D.X, Stand2D.Y, GroundAt(Stand2D) + 1.7f);
			for (const FHourNamed& Hour : Hours)
			{
				AddShot(FString::Printf(TEXT("%s_%s"), *Zone.Id.ToString(), Hour.Name), Eye,
					FVector(Zone.Center.X, Zone.Center.Y, GroundAt(Zone.Center) + 1.7f), Hour.Hours);
			}
		}
		for (const FIslandPoint& Point : Layout.Points)
		{
			const FVector Eye(Point.Location.X, Point.Location.Y, GroundAt(Point.Location) + 1.7f);
			const FVector Ahead = Eye + FRotator(0.0f, Point.Yaw, 0.0f).Vector() * 10.0;
			AddShot(FString::Printf(TEXT("punto_%s_mediodia"), *Point.Id.ToString()), Eye, Ahead, 13.0f);
		}
	}

	if ((bAll || Set == TEXT("day")) && Main)
	{
		const FVector2D Dir = FVector2D(1.0, -0.3).GetSafeNormal();
		const FVector2D Cam2D = Main->Center + Dir * Main->RadiusAlong(Dir) * 1.3f;
		const FVector Eye(Cam2D.X, Cam2D.Y, 15.0f);
		for (const float Hours : {6.2f, 9.0f, 13.0f, 17.6f, 18.4f, 22.0f})
		{
			AddShot(FString::Printf(TEXT("day_%04.1f"), Hours), Eye, FVector(Main->Center.X, Main->Center.Y, 8.0f), Hours);
		}
	}

	if ((bAll || bBench || Set == TEXT("water")) && Main)
	{
		// Orilla de cerca, casi a ras de agua, y (solo en «water») una vista bajo el agua.
		const FVector2D ShoreDir = FVector2D(1.0, -0.3).GetSafeNormal();
		const float ShoreR = Main->RadiusAlong(ShoreDir);
		const FVector2D CamXY = Main->Center + ShoreDir * (ShoreR + 2.5f);
		const FVector2D TargetXY = Main->Center + ShoreDir * (ShoreR - 5.0f);
		const FVector Eye(CamXY.X, CamXY.Y, FMath::Max(Layout.SampleColumn(CamXY.X, CamXY.Y).Height, -0.3f) + 0.6f);
		AddShot(TEXT("shore_closeup"), Eye, FVector(TargetXY.X, TargetXY.Y, GroundAt(TargetXY)), 10.5f);
		if (bAll || Set == TEXT("water"))
		{
			const FVector2D DeepXY = Main->Center + ShoreDir * (ShoreR + 50.0f);
			const float Floor = Layout.SampleColumn(DeepXY.X, DeepXY.Y).Height;
			const float SafeZ = FMath::Clamp(-1.6f, FMath::Min(Floor + 0.5f, -0.6f), -0.6f);
			FExploredShot Shot;
			Shot.Name = TEXT("underwater");
			Shot.Location = FVector(DeepXY.X, DeepXY.Y, SafeZ) * 100.0;
			Shot.Rotation = FRotator(20.0f, 200.0f, 0.0f);
			Shot.Hours = 12.5f;
			Shots.Add(Shot);
		}
	}

	if (bAll || bBench || Set == TEXT("aerial"))
	{
		// Toda la isla y el islote desde el suroeste.
		AddShot(TEXT("aerial"), FVector(-320.0f, -260.0f, 180.0f), FVector(40.0f, 20.0f, 0.0f), 15.5f);
	}

	if (Set == TEXT("f3a"))
	{
		// Terreno de F3a (docs/v3/capturas/f3a-*.png): cenital con el norte arriba, Cala Varada desde
		// el mar, la Colina del Faro desde el Palmar y el islote del carguero desde Playa Norte.
		AddShot(TEXT("f3a-cenital"), FVector(52.0f, 0.0f, 430.0f), FVector(52.5f, 0.0f, 0.0f), 13.0f);
		AddShot(TEXT("f3a-cala"), FVector(-178.0f, -28.0f, 12.0f), FVector(-100.0f, 12.0f, 1.0f), 10.5f);
		const FVector2D Palmar(-22.0f, -28.0f);
		AddShot(TEXT("f3a-faro"), FVector(Palmar.X, Palmar.Y, GroundAt(Palmar) + 6.0f), FVector(45.0f, 15.0f, 32.0f), 15.0f);
		const FVector2D North(112.0f, 58.0f);
		AddShot(TEXT("f3a-islote"), FVector(North.X, North.Y, GroundAt(North) + 8.0f), FVector(232.0f, 150.0f, 4.0f), 16.5f);
	}

	if (Set == TEXT("menu"))
	{
		// El menú principal ya está en pantalla por defecto al arrancar el
		// mapa (AExploredPlayerController::BeginPlay); aquí solo se pide a
		// Ajustes y a Pausa que se abran para el resto de capturas (ver
		// BeginShot). La cámara y la hora no importan: se ven detrás del
		// panel de UI, que es lo que hay que verificar.
		for (const TCHAR* Name : { TEXT("menu_main"), TEXT("menu_settings"), TEXT("menu_pause"), TEXT("menu_credits"), TEXT("menu_newgame") })
		{
			FExploredShot Shot;
			Shot.Name = Name;
			Shot.Location = FVector(-300.0 * 100.0, -200.0 * 100.0, 120.0 * 100.0);
			Shot.Rotation = FRotator(-20.0f, 35.0f, 0.0f);
			Shot.Hours = 12.0f;
			Shot.bShowUI = true;
			Shots.Add(Shot);
		}
	}
}

void UExploredShotSubsystem::PlacePawnFacing(const FVector& Target)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	// A 2,5 m del pecio, del lado de la cámara de la vista, con los ojos a la altura del centro.
	const FVector Center = Target + FVector(0.0, 0.0, 60.0);
	const FVector Away = FVector(-900.0, -600.0, 0.0).GetSafeNormal();
	const FVector Location = Center + Away * 250.0 + FVector(0.0, 0.0, 40.0);
	Pawn->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation((Center - Location).Rotation());
}

void UExploredShotSubsystem::InteractWithFocus(const FString& ShotName)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	const AExploredCharacter* Character = PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
	UInteractionComponent* Interaction = Character ? Character->GetInteractionComponent() : nullptr;
	const AActor* Focus = Interaction ? Interaction->GetFocusedActor() : nullptr;
	const bool bDone = Interaction && Interaction->InteractWithFocus();
	UE_LOG(LogExplored, Display, TEXT("[Shots] %s: foco %s, interacción %s"), *ShotName,
		Focus ? *Focus->GetClass()->GetName() : TEXT("ninguno"), bDone ? TEXT("hecha") : TEXT("fallida"));
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
			// World Partition transmite en función de la posición del pawn, no de la cámara de
			// la vista; sin esto, un encuadre lejos del punto de aparición (orilla, submarino)
			// se queda con los trozos de terreno cercanos sin cargar.
			Pawn->SetActorLocation(Shot.Location, false, nullptr, ETeleportType::TeleportPhysics);
		}

		// Conjunto «menu»: pide al frontend que muestre Ajustes o Pausa para
		// esta vista concreta (menu_main ya está mostrando el menú principal
		// por defecto). Vuelve a fijar la cámara orbital propia del menú.
		if (AExploredPlayerController* ExploredPC = Cast<AExploredPlayerController>(PC))
		{
			if (Shot.Name == TEXT("menu_settings"))
			{
				ExploredPC->OpenSettings();
			}
			else if (Shot.Name == TEXT("menu_pause"))
			{
				ExploredPC->OpenPauseMenu();
			}
			else if (Shot.Name == TEXT("menu_newgame"))
			{
				// Nueva partida de verdad: el jugador vuelve a su PlayerStart, visible, y el fundido
				// de StartNewGame pone la vista en sus ojos (HUD, ayuda inicial y objetivo).
				if (APawn* Pawn = PC->GetPawn())
				{
					for (TActorIterator<APlayerStart> It(World); It; ++It)
					{
						Pawn->SetActorLocation(It->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
						PC->SetControlRotation(It->GetActorRotation());
						break;
					}
					Pawn->SetActorHiddenInGame(false);
				}
				ExploredPC->ResumeGame();
				ExploredPC->StartNewGame();
			}
			else if (Shot.Name == TEXT("menu_credits"))
			{
				ExploredPC->OpenCredits();
			}
		}
	}
	if (UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Time->SetTime(Time->GetDay(), Shot.Hours);
		Time->SetTimeScale(0.0f);
	}
	// Conjunto «voyage»: se juega (sin el menú principal encima) para ver el HUD de juego.
	AExploredPlayerController* VoyagePC = Shot.Name.StartsWith(TEXT("voyage"))
		? Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
	if (VoyagePC && VoyagePC->GetUIMode() != EExploredUIMode::Playing)
	{
		VoyagePC->ResumeGame();
		VoyagePC->SetViewTarget(Camera);
	}
	if (Shot.Action == TEXT("interact"))
	{
		PlacePawnFacing(Shot.Target);
	}
	if (UExploredVoyageSubsystem* Voyage = Shot.Action.IsNone() ? nullptr : World->GetSubsystem<UExploredVoyageSubsystem>())
	{
		if (Shot.Action == TEXT("build"))
		{
			for (uint8 Part = 0; Part < static_cast<uint8>(EShipPart::Count); ++Part)
			{
				Voyage->RecoverPart(static_cast<EShipPart>(Part));
			}
			Voyage->TryBuildShip();
			// Camino del jugador de día: «Esperar a la noche junto al barco».
			Voyage->WaitForNight();
			UE_LOG(LogExplored, Display, TEXT("[Shots] Tras esperar a la noche: %s"), Voyage->IsStarNightNow() ? TEXT("de noche") : TEXT("de día"));
		}
		else if (Shot.Action == TEXT("depart"))
		{
			Voyage->TryDepart();
		}
	}
	// «wait»: deja correr la secuencia de salida (9 s) y el fundido antes de capturar.
	Timer = Shot.Action == TEXT("wait") ? -10.0f : 0.0f;
	bRequested = false;
	CurrentShotFrameRates.Reset();
}

void UExploredShotSubsystem::Tick(float DeltaTime)
{
	if ((GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) || FAssetCompilingManager::Get().GetNumRemainingAssets() > 0)
	{
		return;
	}
	// World Partition transmite según la posición del pawn (ya teleportado en BeginShot); sin
	// esperar a que termine, una vista lejos del punto de aparición se captura con los trozos
	// de terreno cercanos todavía sin cargar (mar y cielo vacíos alrededor de la cámara).
	if (const UWorldPartitionSubsystem* Partition = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>())
	{
		if (!Partition->IsStreamingCompleted())
		{
			return;
		}
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

	// Muestra de fps de esta vista (para UExploredPlaytestAuditor, si está activo): cada fotograma
	// de espera cuenta, no solo el de la captura, para que el mínimo recoja un posible tirón del
	// streaming de World Partition o de los shaders on-demand.
	if (DeltaTime > UE_KINDA_SMALL_NUMBER)
	{
		CurrentShotFrameRates.Add(1.0f / DeltaTime);
	}

	// Sin esperar al streaming, las texturas del terreno se capturan con los mips más bajos.
	const bool bTexturesReady = IStreamingManager::Get().GetNumWantingResources() == 0;
	if (!bRequested && Timer >= SettleSeconds && (bTexturesReady || Timer >= SettleSeconds + MaxStreamingWaitSeconds))
	{
		if (bBenchMode)
		{
			LogBenchSample(Current);
		}
		if (Shots[Current].Action == TEXT("interact"))
		{
			InteractWithFocus(Shots[Current].Name);
		}
		if (UExploredPlaytestAuditor* Auditor = GetWorld()->GetSubsystem<UExploredPlaytestAuditor>())
		{
			float SumFPS = 0.0f;
			float MinFPS = 0.0f;
			for (int32 Index = 0; Index < CurrentShotFrameRates.Num(); ++Index)
			{
				SumFPS += CurrentShotFrameRates[Index];
				MinFPS = (Index == 0) ? CurrentShotFrameRates[Index] : FMath::Min(MinFPS, CurrentShotFrameRates[Index]);
			}
			const float AvgFPS = CurrentShotFrameRates.Num() > 0 ? SumFPS / CurrentShotFrameRates.Num() : 0.0f;
			Auditor->RecordFrameSample(Shots[Current].Name, AvgFPS, MinFPS);
		}
		const FString File = OutputDir / (Shots[Current].Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, Shots[Current].bShowUI, false);
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
			// El auditor puede seguir acumulando defectos por su cuenta (su propio Tick,
			// independiente del de este subsistema); se le pide el volcado final aquí para
			// que el informe incluya también el rendimiento de todas las vistas ya capturadas.
			if (UExploredPlaytestAuditor* Auditor = GetWorld()->GetSubsystem<UExploredPlaytestAuditor>())
			{
				Auditor->WriteReport();
			}
			UE_LOG(LogExplored, Display, TEXT("[Shots] Terminado"));
			FPlatformMisc::RequestExit(false, TEXT("ExploredShots"));
			return;
		}
		BeginShot(Current);
	}
}

void UExploredShotSubsystem::LogBenchSample(int32 Index)
{
	UWorld* World = GetWorld();
	const FExploredShot& Shot = Shots[Index];
	// GStartTime: FPlatformTime::Seconds() en el arranque del proceso (CoreGlobals.h); la resta
	// da el tiempo de carga real hasta que esta vista está lista para medir.
	const double SecondsSinceStart = FPlatformTime::Seconds() - GStartTime;

	// r.Nanite.ShowStats escribe su resumen (clusters/triángulos visibles) al log la primera vez
	// que se ejecuta tras cambiar de vista; stat gpu/unit no hace falta activarlos para leer los
	// contadores de RenderCore de abajo, pero se dejan encendidos para quien mire las capturas.
	// «stat X» es un interruptor: se activa una sola vez, o la segunda vista lo apagaría.
	if (GEngine && !bBenchStatsEnabled)
	{
		GEngine->Exec(World, TEXT("stat unit"));
		GEngine->Exec(World, TEXT("stat gpu"));
		GEngine->Exec(World, TEXT("stat rhi"));
		GEngine->Exec(World, TEXT("stat streaming"));
		bBenchStatsEnabled = true;
	}
	if (GEngine)
	{
		GEngine->Exec(World, TEXT("r.Nanite.ShowStats 1"));
	}

	// VRAM de texturas (RHI): la parte dominante del presupuesto de vídeo en este proyecto
	// (terreno Nanite + vegetación HISM son mallas, pero sus materiales tiran de un atlas de
	// texturas grande). No sustituye a memreport -full de abajo (que desglosa por streaming pool),
	// pero da una cifra directa en la misma línea que el resto del bench, sin parsear otro fichero.
	FTextureMemoryStats TextureMemoryStats;
	RHIGetTextureMemoryStats(TextureMemoryStats);
	const double TextureVRAMUsedMB = (TextureMemoryStats.StreamingMemorySize + TextureMemoryStats.NonStreamingMemorySize) / (1024.0 * 1024.0);
	const double TexturePoolMB = TextureMemoryStats.TexturePoolSize / (1024.0 * 1024.0);

	// Los contadores de RenderTimer.h están en ciclos de FPlatformTime; RHIGetGPUFrameCycles() es
	// la sustituta no deprecada de GGPUFrameTime para el tiempo de GPU del último fotograma.
	UE_LOG(LogExplored, Display,
		TEXT("[Bench] %s: %.1f s desde el arranque · GameThread=%.2f ms RenderThread=%.2f ms RHIThread=%.2f ms GPU=%.2f ms · ")
		TEXT("VRAM texturas=%.1f MB (pool=%.1f MB)"),
		*Shot.Name, SecondsSinceStart,
		FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime),
		FPlatformTime::ToMilliseconds(GRHIThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),
		TextureVRAMUsedMB, TexturePoolMB);

	// memreport -full escribe un .memreport con marca de tiempo en Saved/Profiling/MemReports/
	// (desglose de streaming de texturas: pool pedido/usado) que pide la medición de VRAM. El
	// nombre de archivo lo decide el motor (no admite -name=): la línea de arriba, con el nombre
	// de la vista, es la referencia para casarlo por hora de log.
	if (GEngine)
	{
		GEngine->Exec(World, TEXT("memreport -full"));
	}
}

TStatId UExploredShotSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredShotSubsystem, STATGROUP_Tickables);
}
