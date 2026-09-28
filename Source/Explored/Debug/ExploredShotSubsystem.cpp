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

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Debug/ExploredPlaytestAuditor.h"
#include "Explored.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "UI/ExploredPlayerController.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/ExploredVegetationCell.h"
#include "WorldGen/PointsOfInterest.h"
#include "WorldGen/TerrainDensity.h"

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
	const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
	const bool bAll = Set == TEXT("all");
	const bool bBench = Set == TEXT("bench");

	if (bAll || bBench || Set == TEXT("spawn"))
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

		// Vista cercana y oblicua del macizo kárstico (La Meseta): a poca distancia del agua
		// y mirando de refilón a la pared, para revisar de cerca la erosión (barrancos,
		// taludes, muesca de marea) en vez del silueteado general del resto de islas.
		if (const FIslandDesc* Karst = Density.GetLayout().FindIsland(EIslandArchetype::Mesa))
		{
			const float Angle = Karst->Rotation + 2.3f;
			const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			const FVector2D Cam2D = Karst->Center + Dir * Karst->Radius * 0.85f;
			const float Height = FMath::Max(Density.SampleColumn(Cam2D.X, Cam2D.Y).Height, 0.0f) + 10.0f;
			FExploredShot Shot;
			Shot.Name = TEXT("island_Mesa_close");
			Shot.Location = FVector(Cam2D.X, Cam2D.Y, Height) * 100.0;
			const FVector Target(Karst->Center.X * 100.0, Karst->Center.Y * 100.0, Karst->MaxHeight * 55.0);
			Shot.Rotation = (Target - Shot.Location).Rotation();
			Shot.Hours = 16.5f;
			Shots.Add(Shot);
		}
	}

	// Tres vistas para juzgar el cambio de vegetación de Landing (encargo
	// 2026-09-27, sustitución de la vegetación por el pack low poly de
	// Quaternius/Kenney): altura de jugador en la playa, dentro de la selva y
	// la isla vista desde el mar. No están en "bench" (ese trío es
	// spawn/orilla/aérea, para fps) ni en "islands" (esa vista de Landing
	// desde el mar es más lejana, pensada para las 7 islas a la vez).
	if (bAll || Set == TEXT("landing_report"))
	{
		const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
		if (Landing)
		{
			// Playa a la altura del jugador: mismo criterio que "spawn" (ojos a 1.7 m), pero desde
			// la orilla mirando hacia dentro para que las palmeras de la franja de costa entren en
			// cuadro junto con el mar detrás de la cámara.
			{
				const FVector2D Dir = FVector2D(0.85f, 0.35f).GetSafeNormal();
				const FVector2D CamXY = Landing->Center + Dir * Landing->Radius * 1.02f;
				const float GroundZ = FMath::Max(Density.SampleColumn(CamXY.X, CamXY.Y).Height, 0.0f);
				FExploredShot Shot;
				Shot.Name = TEXT("landing_beach");
				Shot.Location = FVector(CamXY.X, CamXY.Y, GroundZ + 1.7f) * 100.0;
				const FVector2D TargetXY = Landing->Center + Dir * Landing->Radius * 0.5f;
				const float TargetZ = Density.SampleColumn(TargetXY.X, TargetXY.Y).Height;
				Shot.Rotation = (FVector(TargetXY.X, TargetXY.Y, TargetZ + 3.0f) * 100.0 - Shot.Location).Rotation();
				Shot.Hours = 10.0f;
				Shots.Add(Shot);
			}
			// Dentro de la selva: a media isla, altura de jugador, mirando hacia el centro (más
			// densidad de JungleWide/Shrub según FVegetationScatter::DefaultRules).
			{
				const FVector2D Dir = FVector2D(0.85f, 0.35f).GetSafeNormal();
				const FVector2D CamXY = Landing->Center + Dir * Landing->Radius * 0.55f;
				const float GroundZ = Density.SampleColumn(CamXY.X, CamXY.Y).Height;
				FExploredShot Shot;
				Shot.Name = TEXT("landing_jungle");
				Shot.Location = FVector(CamXY.X, CamXY.Y, GroundZ + 1.7f) * 100.0;
				const FVector2D TargetXY = Landing->Center + Dir * Landing->Radius * 0.1f;
				const float TargetZ = Density.SampleColumn(TargetXY.X, TargetXY.Y).Height;
				Shot.Rotation = (FVector(TargetXY.X, TargetXY.Y, TargetZ + 1.5f) * 100.0 - Shot.Location).Rotation();
				Shot.Hours = 12.0f;
				Shots.Add(Shot);
			}
			// La isla desde el mar: mismo criterio que el bucle de "islands" pero solo para
			// Landing, para no pagar las otras 6 vistas cuando solo hace falta juzgar esta.
			{
				const float Angle = Landing->Rotation + 0.9f;
				const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
				const FVector2D Cam2D = Landing->Center + Dir * Landing->Radius * 1.45f;
				const float Height = FMath::Max(Density.SampleColumn(Cam2D.X, Cam2D.Y).Height, 0.0f) + 25.0f + Landing->MaxHeight * 0.25f;
				FExploredShot Shot;
				Shot.Name = TEXT("landing_from_sea");
				Shot.Location = FVector(Cam2D.X, Cam2D.Y, Height) * 100.0;
				const FVector Target(Landing->Center.X * 100.0, Landing->Center.Y * 100.0, Landing->MaxHeight * 35.0);
				Shot.Rotation = (Target - Shot.Location).Rotation();
				Shot.Hours = 16.0f;
				Shots.Add(Shot);
			}
		}
	}

	// Playtest automático (auditor de defectos, ver ExploredPlaytestAuditor): recorrido «a pie»
	// por cada isla y sus puntos de interés garantizados. Altura de jugador, en la orilla, dentro
	// de la vegetación, en la cima y en la boca de una cueva si la isla tiene una, cada una a
	// cuatro horas del día (mismos valores que «day», para reaprovechar el mismo punto de
	// asentamiento de iluminación ya validado). Los puntos de interés garantizados (FPoiLayout) se
	// capturan aparte, uno por punto a mediodía: multiplicarlos también por las cuatro horas
	// dispararía el número de capturas sin aportar tanto (la luz importa más para el paisaje
	// general que para verificar un objeto concreto).
	if (bAll || Set == TEXT("playtest"))
	{
		struct FHourNamed { const TCHAR* Name; float Hours; };
		const FHourNamed Hours[] = {
			{ TEXT("amanecer"), 6.2f },
			{ TEXT("mediodia"), 13.0f },
			{ TEXT("atardecer"), 17.6f },
			{ TEXT("noche"), 22.0f },
		};

		// Busca, en pasos de 30° desde Island.Rotation, un ángulo donde FindBeach/FindInland
		// encuentren un punto válido. Doce intentos cubren toda la vuelta.
		auto FindBeachAround = [&Density](const FIslandDesc& Island, FVector& Out) -> bool
		{
			for (int32 Step = 0; Step < 12; ++Step)
			{
				if (FPoiLayout::FindBeach(Density, Island, Island.Rotation + Step * (UE_PI / 6.0f), Out))
				{
					return true;
				}
			}
			return false;
		};
		auto FindInlandAround = [&Density](const FIslandDesc& Island, FVector& Out) -> bool
		{
			for (int32 Step = 0; Step < 12; ++Step)
			{
				if (FPoiLayout::FindInland(Density, Island, Island.Rotation + Step * (UE_PI / 6.0f), 0.5f, Out))
				{
					return true;
				}
			}
			return false;
		};
		auto AddWalkShot = [this](const FString& Name, const FVector& StandLocationMeters, const FVector& LookAtMeters, float ShotHours)
		{
			FExploredShot Shot;
			Shot.Name = Name;
			Shot.Location = StandLocationMeters * 100.0;
			Shot.Rotation = ((LookAtMeters - StandLocationMeters) * 100.0).Rotation();
			Shot.Hours = ShotHours;
			Shots.Add(Shot);
		};

		for (const FIslandDesc& Island : Density.GetLayout().Islands)
		{
			const FString IslandName = LexToString(Island.Archetype);

			FVector Beach = FVector::ZeroVector;
			FVector Inland = FVector::ZeroVector;
			const bool bHasBeach = FindBeachAround(Island, Beach);
			const bool bHasInland = FindInlandAround(Island, Inland);
			const FVector Summit = FPoiLayout::FindSummit(Density, Island);

			// Cueva más cercana al centro de esta isla (si el layout generó alguna ahí).
			const FCaveDesc* IslandCave = nullptr;
			for (const FCaveDesc& Cave : Density.GetCaves())
			{
				const FVector2D CaveXY(Cave.Start.X, Cave.Start.Y);
				if ((CaveXY - Island.Center).Size() <= Island.Radius * 1.5f)
				{
					IslandCave = &Cave;
					break;
				}
			}

			for (const FHourNamed& Hour : Hours)
			{
				if (bHasBeach)
				{
					const FVector Stand = Beach + FVector(0, 0, 1.7f);
					AddWalkShot(FString::Printf(TEXT("%s_orilla_%s"), *IslandName, Hour.Name), Stand,
						FVector(Island.Center.X, Island.Center.Y, Stand.Z), Hour.Hours);
				}
				if (bHasInland)
				{
					const FVector Stand = Inland + FVector(0, 0, 1.7f);
					AddWalkShot(FString::Printf(TEXT("%s_vegetacion_%s"), *IslandName, Hour.Name), Stand,
						FVector(Island.Center.X, Island.Center.Y, Stand.Z), Hour.Hours);
				}
				{
					const FVector Stand = Summit + FVector(0, 0, 1.7f);
					AddWalkShot(FString::Printf(TEXT("%s_cima_%s"), *IslandName, Hour.Name), Stand,
						FVector(Island.Center.X, Island.Center.Y, Summit.Z - 5.0f), Hour.Hours);
				}
				if (IslandCave)
				{
					const FVector Stand = IslandCave->Start + FVector(0, 0, 1.0f);
					AddWalkShot(FString::Printf(TEXT("%s_cueva_%s"), *IslandName, Hour.Name), Stand, IslandCave->End, Hour.Hours);
				}
			}
		}

		// Puntos de interés garantizados: uno por punto, a mediodía, altura de jugador.
		for (const FPointOfInterest& Poi : FPoiLayout::Generate(Density))
		{
			const FIslandDesc* Island = Density.GetLayout().Islands.IsValidIndex(Poi.IslandIndex)
				? &Density.GetLayout().Islands[Poi.IslandIndex] : nullptr;
			const FString IslandName = Island ? LexToString(Island->Archetype) : TEXT("Mar");
			const FVector Stand = Poi.Location + FVector(0, 0, Poi.bUnderwater ? 0.3f : 1.7f);
			const FVector LookAt = Island ? FVector(Island->Center.X, Island->Center.Y, Stand.Z) : Stand + FVector(5.0f, 0.0f, 0.0f);
			AddWalkShot(FString::Printf(TEXT("%s_%s_mediodia"), *IslandName, LexToString(Poi.Type)), Stand, LookAt, 13.0f);
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

	if (bAll || bBench || Set == TEXT("water"))
	{
		const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
		// Mismo sector que «day» (donde se sabe que hay playa despejada cerca), con un ángulo
		// algo distinto para variar la composición.
		const FVector2D ShoreDir = FVector2D(1.0f, -0.3f).GetSafeNormal();

		// Las islas no son círculos perfectos (cabos, calas, arroyos que abren huecos por
		// debajo del nivel del mar tierra adentro): en vez de la altura (contaminada por
		// esos arroyos) se usa NormalizedDistance, la métrica radial propia de WorldGen
		// donde 1.0 es, por construcción, la costa nominal en cualquier dirección.
		float ShoreR = Landing->Radius;
		{
			const int32 Steps = 120;
			const float MinR = Landing->Radius * 0.2f;
			const float MaxR = Landing->Radius * 2.0f;
			for (int32 I = 0; I <= Steps; ++I)
			{
				const float R = FMath::Lerp(MinR, MaxR, static_cast<float>(I) / Steps);
				const FVector2D P = Landing->Center + ShoreDir * R;
				if (Density.SampleColumn(P.X, P.Y).NormalizedDistance >= 1.0f)
				{
					ShoreR = R;
					break;
				}
			}
		}
		{
			const FVector2D ShoreP = Landing->Center + ShoreDir * ShoreR;
			UE_LOG(LogExplored, Display, TEXT("[Shots] Landing Radius=%.1f ShoreR=%.1f HeightAtShore=%.2f"),
				Landing->Radius, ShoreR, Density.SampleColumn(ShoreP.X, ShoreP.Y).Height);
		}

		// Orilla de cerca: casi a ras de agua, a caballo entre la arena y la laguna, para ver
		// la espuma neta que avanza y se retira y la banda de color turquesa.
		{
			const FVector2D CamXY = Landing->Center + ShoreDir * (ShoreR + 2.5f);
			const float GroundZ = Density.SampleColumn(CamXY.X, CamXY.Y).Height;
			FExploredShot Shot;
			Shot.Name = TEXT("shore_closeup");
			// Todo el vector en metros y se pasa a centimetros junto (X e Y también, no solo Z).
			Shot.Location = FVector(CamXY.X, CamXY.Y, FMath::Max(GroundZ, -0.3f) + 0.6f) * 100.0;
			const FVector2D TargetXY = Landing->Center + ShoreDir * (ShoreR - 5.0f);
			const float TargetZ = FMath::Max(Density.SampleColumn(TargetXY.X, TargetXY.Y).Height, 0.0f);
			Shot.Rotation = (FVector(TargetXY.X, TargetXY.Y, TargetZ) * 100.0 - Shot.Location).Rotation();
			Shot.Hours = 10.5f;
			Shots.Add(Shot);
		}

		// Bajo el agua: bien pasada la orilla, dentro de la laguna, mirando hacia la
		// superficie iluminada. No forma parte de las tres posiciones de -ExploredBench
		// (orilla ya cubre la escena de costa; el benchmark quiere spawn/aérea/orilla).
		if (bAll || Set == TEXT("water"))
		{
			const FVector2D CamXY = Landing->Center + ShoreDir * (ShoreR + 50.0f);
			const float GroundZ = Density.SampleColumn(CamXY.X, CamXY.Y).Height;
			const float LowerBound = FMath::Min(GroundZ + 0.5f, -0.6f);
			const float SafeZ = FMath::Clamp(-1.6f, LowerBound, -0.6f);
			FExploredShot Shot;
			Shot.Name = TEXT("underwater");
			Shot.Location = FVector(CamXY.X, CamXY.Y, SafeZ) * 100.0;
			Shot.Rotation = FRotator(20.0f, 200.0f, 0.0f);
			Shot.Hours = 12.5f;
			Shots.Add(Shot);
		}
	}

	if (bAll || bBench || Set == TEXT("aerial"))
	{
		FExploredShot Shot;
		Shot.Name = TEXT("aerial");
		Shot.Location = FVector(0.0, -4200.0 * 100.0, 1800.0 * 100.0);
		Shot.Rotation = FRotator(-32.0f, 90.0f, 0.0f);
		Shot.Hours = 15.5f;
		Shots.Add(Shot);
	}

	if (Set == TEXT("menu"))
	{
		// El menú principal ya está en pantalla por defecto al arrancar el
		// mapa (AExploredPlayerController::BeginPlay); aquí solo se pide a
		// Ajustes y a Pausa que se abran para el resto de capturas (ver
		// BeginShot). La cámara y la hora no importan: se ven detrás del
		// panel de UI, que es lo que hay que verificar.
		for (const TCHAR* Name : { TEXT("menu_main"), TEXT("menu_settings"), TEXT("menu_pause") })
		{
			FExploredShot Shot;
			Shot.Name = Name;
			Shot.Location = FVector(0.0, -3000.0 * 100.0, 1200.0 * 100.0);
			Shot.Rotation = FRotator(-20.0f, 90.0f, 0.0f);
			Shot.Hours = 12.0f;
			Shots.Add(Shot);
		}
	}
}

void UExploredShotSubsystem::BeginShot(int32 Index)
{
	const FExploredShot& Shot = Shots[Index];
	UWorld* World = GetWorld();

	if (Index == 0)
	{
		// Diagnóstico puntual: ¿existen de verdad las celdas de vegetación y sus instancias
		// en el mundo de juego en ejecución, o algo las quita/oculta solo en -game?
		int32 Cells = 0;
		int64 TotalInstances = 0;
		for (TActorIterator<AExploredVegetationCell> It(World); It; ++It)
		{
			++Cells;
			TArray<UHierarchicalInstancedStaticMeshComponent*> Comps;
			It->GetComponents(Comps);
			for (const UHierarchicalInstancedStaticMeshComponent* Comp : Comps)
			{
				if (Comp)
				{
					TotalInstances += Comp->GetInstanceCount();
				}
			}
		}
		UE_LOG(LogExplored, Display, TEXT("[Shots] Vegetación en el mundo de juego: %d celdas, %lld instancias"), Cells, TotalInstances);
	}

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
		}
	}
	if (UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Time->SetTime(Time->GetDay(), Shot.Hours);
		Time->SetTimeScale(0.0f);
	}
	Timer = 0.0f;
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
