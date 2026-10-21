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
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "AssetCompilingManager.h"
#include "ContentStreaming.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#include "Debug/ExploredPlaytestAuditor.h"
#include "Explored.h"
#include "Narrative/BeaconSubsystem.h"
#include "Narrative/LighthouseLamp.h"
#include "Interaction/InteractionComponent.h"
#include "Items/ExploredSurvivorComponent.h"
#include "UI/IslandHudModel.h"
#include "Player/ExploredCharacter.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Engine/PostProcessVolume.h"
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
	/** Ventana de medida de cada vista del bench, después del asentamiento. */
	constexpr float BenchMeasureSeconds = 12.0f;
	/** Media amplitud del barrido de guiñada de la cámara durante la medida (grados). */
	constexpr float BenchHalfSweepDegrees = 30.0f;
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

	if ((bAll || Set == TEXT("spawn")) && Main)
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

	if ((bAll || Set == TEXT("water")) && Main)
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

	if (bAll || Set == TEXT("aerial"))
	{
		// Toda la isla y el islote desde el suroeste.
		AddShot(TEXT("aerial"), FVector(-320.0f, -260.0f, 180.0f), FVector(40.0f, 20.0f, 0.0f), 15.5f);
	}

	if (bBench)
	{
		// Recorrido de rendimiento de F7 por Map_IslaSal (docs/v3/estado-f7.md). Mismos encuadres
		// que la pasada visual f4b donde coinciden, para comparar cifras con las capturas.
		// Cala Varada desde el mar: agua, playa, palmeras y la balsa en un mismo plano.
		AddShot(TEXT("bench-cala"), FVector(-150.0f, -12.0f, 7.0f), FVector(-98.0f, 2.0f, 2.0f), 10.5f);
		// El Palmar a la altura de los ojos de Iria: la vegetación más densa de la isla.
		const FVector2D PalmarEye(-62.0f, -38.0f);
		const FVector2D PalmarLook(-30.0f, -8.0f);
		AddShot(TEXT("bench-palmar"), FVector(PalmarEye.X, PalmarEye.Y, GroundAt(PalmarEye) + 1.7f),
			FVector(PalmarLook.X, PalmarLook.Y, GroundAt(PalmarLook) + 1.7f), 11.0f);
		// Base del faro desde la ladera de la Colina.
		const FVector2D Tower(45.0f, 15.0f);
		const FVector2D FaroEye(22.0f, -6.0f);
		AddShot(TEXT("bench-faro"), FVector(FaroEye.X, FaroEye.Y, GroundAt(FaroEye) + 1.7f),
			FVector(Tower.X, Tower.Y, GroundAt(Tower) + 6.0f), 15.0f);
		// Galería del faro mirando a la Cala por encima del Palmar: el encuadre con más isla y mar a
		// la vista (el peor caso de triángulos y de distancia de dibujado).
		const float TowerTop = GroundAt(Tower) + 12.0f;
		AddShot(TEXT("bench-faro-galeria"), FVector(41.0f, 13.0f, TowerTop + 2.5f), FVector(-100.0f, 10.0f, 0.0f), 17.0f);
		// Ciclón (muro 1, D2 20:30) desde la orilla de la Cala hacia el mar: lluvia, viento y oleaje.
		const FVector2D CycloneEye(-118.0f, 4.0f);
		AddShot(TEXT("bench-ciclon"), FVector(CycloneEye.X, CycloneEye.Y, GroundAt(CycloneEye) + 2.5f), FVector(-200.0f, -20.0f, 2.0f), 20.5f);
		Shots.Last().Day = 2;
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

	if (Set == TEXT("f4") || Set == TEXT("f4b"))
	{
		// Integración de arte de F4 (docs/v3/capturas/f4-*.png) y pasada visual (f4b-*.png, mismas
		// vistas más atardecer y ciclón): cenital, Cala Varada, Palmar, el faro de cerca, el islote
		// con el carguero y la primera persona con un objeto en la mano.
		const FString P = Set + TEXT("-");
		AddShot(P + TEXT("cenital"), FVector(52.0f, 0.0f, 430.0f), FVector(52.5f, 0.0f, 0.0f), 13.0f);
		AddShot(P + TEXT("cala"), FVector(-150.0f, -12.0f, 7.0f), FVector(-98.0f, 2.0f, 2.0f), 10.5f);
		const FVector2D PalmarEye(-62.0f, -38.0f);
		AddShot(P + TEXT("palmar"), FVector(PalmarEye.X, PalmarEye.Y, GroundAt(PalmarEye) + 3.0f),
			FVector(-30.0f, -8.0f, GroundAt(FVector2D(-30.0f, -8.0f)) + 3.0f), 11.0f);
		const FVector2D FaroEye(22.0f, -6.0f);
		const float FaroBase = GroundAt(FVector2D(45.0f, 15.0f));
		AddShot(P + TEXT("faro"), FVector(FaroEye.X, FaroEye.Y, GroundAt(FaroEye) + 4.0f), FVector(42.0f, 15.0f, FaroBase + 6.0f), 15.0f);
		const FVector2D IsloteEye(200.0f, 166.0f);
		AddShot(P + TEXT("islote"), FVector(IsloteEye.X, IsloteEye.Y, GroundAt(IsloteEye) + 9.0f), FVector(229.0f, 148.0f, 3.0f), 16.5f);
		if (Set == TEXT("f4b"))
		{
			// Atardecer desde la ladera de la colina, mirando hacia el sol poniente.
			const float DuskHours = 17.9f;
			const FVector SunDir = ExploredSky::SunDirection(DuskHours, 1.0f + DuskHours / 24.0f).GetSafeNormal2D();
			const FVector2D DuskEye(20.0f, 30.0f);
			const FVector DuskEye3(DuskEye.X, DuskEye.Y, GroundAt(DuskEye) + 6.0f);
			AddShot(P + TEXT("atardecer"), DuskEye3, DuskEye3 + SunDir * 200.0 + FVector(0.0, 0.0, 4.0), DuskHours);
			// El Palmar sin el sombreado por bandas (M_PP_Toon) para compararlo con f4b-palmar.
			AddShot(P + TEXT("palmar-sin-bandas"), FVector(PalmarEye.X, PalmarEye.Y, GroundAt(PalmarEye) + 3.0f),
				FVector(-30.0f, -8.0f, GroundAt(FVector2D(-30.0f, -8.0f)) + 3.0f), 11.0f);
			Shots.Last().Action = TEXT("notoon");
			// Muro 1 del ciclón (D2 20:30) desde la orilla de la Cala Varada hacia el mar.
			AddShot(P + TEXT("ciclon"), FVector(-118.0f, 4.0f, GroundAt(FVector2D(-118.0f, 4.0f)) + 2.5f), FVector(-200.0f, -20.0f, 2.0f), 20.5f);
			Shots.Last().Day = 2;
		}
		// La primera persona va la última: deja el juego en modo de juego con el HUD a la vista.
		// Iria en la Cala Varada mirando al mar con el cuchillo en la mano (acción «firstperson»).
		const FVector2D HandEye(-112.0f, 2.0f);
		AddShot(P + TEXT("mano"), FVector(HandEye.X, HandEye.Y, GroundAt(HandEye) + 1.7f), FVector(-150.0f, -6.0f, 0.5f), 11.0f);
		Shots.Last().Action = TEXT("firstperson");
		Shots.Last().Day = 1;
	}

	if (Set == TEXT("beacon"))
	{
		// Final de F3c: la linterna montada vista desde la galería y la baliza encendida de noche
		// desde la Colina, con las luces del Mistral en el horizonte (acción «light»).
		const FVector2D Tower(45.0f, 15.0f);
		const float TowerTop = GroundAt(Tower) + 12.0f;
		AddShot(TEXT("beacon_lantern"), FVector(41.0f, 13.0f, TowerTop + 2.5f), FVector(Tower.X, Tower.Y, TowerTop + 1.2f), 21.5f);
		Shots.Last().Action = TEXT("lantern");
		AddShot(TEXT("beacon_light"), FVector(10.0f, -20.0f, TowerTop + 6.0f), FVector(Tower.X + 40.0f, Tower.Y - 10.0f, TowerTop), 22.0f);
		Shots.Last().Action = TEXT("light");
	}

	if (Set == TEXT("f6"))
	{
		// Final escenificado de F6, en orden y con el reloj real del final: la llama prende
		// (2 s de los 3), el plano del final con el haz y el Mistral lejos, el Mistral cerca con la
		// bocina, el epílogo y los créditos. Las tres últimas usan la cámara del propio final.
		const FVector2D Tower(45.0f, 15.0f);
		const float TowerTop = GroundAt(Tower) + 12.0f;
		AddShot(TEXT("f6-final-encender"), FVector(36.0f, 9.0f, TowerTop + 3.0f), FVector(Tower.X, Tower.Y, TowerTop + 1.5f), 22.0f);
		Shots.Last().Action = TEXT("ignite");
		Shots.Last().Day = 3;
		for (const TCHAR* Name : { TEXT("f6-final-haz"), TEXT("f6-final-mistral"), TEXT("f6-final-epilogo"), TEXT("f6-final-creditos") })
		{
			FExploredShot Shot;
			Shot.Name = Name;
			Shot.Hours = 22.0f;
			Shot.Day = 3;
			Shot.Action = TEXT("finale");
			Shot.bShowUI = Shot.Name.EndsWith(TEXT("epilogo")) || Shot.Name.EndsWith(TEXT("creditos"));
			Shots.Add(Shot);
		}
	}

	if (Set == TEXT("f5"))
	{
		BuildUiShots();
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

void UExploredShotSubsystem::ShowFirstPerson(const FExploredShot& Shot)
{
	// Vista desde los ojos de Iria con el cuchillo en la mano: el pawn visible, apoyado en el suelo
	// bajo el ojo del encuadre, y la vista en su cámara (antebrazos y objeto en mano).
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	AExploredCharacter* Character = PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		return;
	}
	if (AExploredPlayerController* ExploredPC = Cast<AExploredPlayerController>(PC))
	{
		ExploredPC->ResumeGame();
	}
	Character->SetActorHiddenInGame(false);
	Character->SetActorLocation(Shot.Location - FVector(0.0, 0.0, 70.0), false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(Shot.Rotation);
	PC->SetViewTarget(Character);
	if (UExploredSurvivorComponent* Survivor = Character->GetSurvivorComponent())
	{
		const FName Knife(TEXT("cuchillo"));
		if (Survivor->GetItemCount(Knife) == 0)
		{
			Survivor->TryAddItem(Knife);
		}
		const TArray<FBackpackStack>& Stacks = Survivor->GetBackpack().Stacks;
		const int32 KnifeSlot = Stacks.IndexOfByPredicate([&Knife](const FBackpackStack& Stack) { return Stack.Id == Knife; });
		// Elegir otra ranura antes obliga a refrescar la mano aunque el cuchillo ya fuera la elegida.
		Survivor->SelectQuickSlot(KnifeSlot == 0 ? 1 : 0);
		Survivor->SelectQuickSlot(FMath::Max(KnifeSlot, 0));
		UE_LOG(LogExplored, Display, TEXT("[Shots] %s: primera persona con %s en la mano"), *Shot.Name,
			*FIslandHudModel::HeldItem(Survivor->GetBackpack(), Survivor->GetSelectedQuickSlot()).ToString());
	}
}

void UExploredShotSubsystem::BeginShot(int32 Index)
{
	const FExploredShot& Shot = Shots[Index];
	UWorld* World = GetWorld();
	if (Shot.Action == TEXT("finale"))
	{
		// La cámara es la del final (UBeaconSubsystem::StartFinaleCamera): no se toca la vista.
		// Tiempos desde la captura anterior (4 s de asentamiento + este desfase): haz a ~6 s del
		// encendido, Mistral con la bocina a ~11 s, epílogo a ~21 s y créditos al pedirlos.
		Timer = Shot.Name.EndsWith(TEXT("mistral")) ? 0.5f : Shot.Name.EndsWith(TEXT("epilogo")) ? -5.5f : 0.0f;
		if (Shot.Name.EndsWith(TEXT("creditos")))
		{
			if (AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(World, 0)))
			{
				PC->ShowEndCredits();
			}
		}
		bRequested = false;
		CurrentShotFrameRates.Reset();
		return;
	}
	if (BeginUiShot(Shot))
	{
		// El final funde a negro 2 s antes de enseñar el epílogo: se espera más que en el resto.
		// El desmayo dura 2,5 s (negro con el texto): se captura a mitad.
		Timer = Shot.Action == FName(TEXT("ui_ending")) ? -4.0f : Shot.Action == FName(TEXT("ui_faint")) ? 2.2f : 0.0f;
		bRequested = false;
		CurrentShotFrameRates.Reset();
		return;
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
		Time->SetTime(Shot.Day > 0 ? Shot.Day : Time->GetDay(), Shot.Hours);
		Time->SetTimeScale(0.0f);
	}
	// Conjunto «beacon»: se juega (sin el menú principal encima) para ver el HUD de juego.
	AExploredPlayerController* BeaconPC = Shot.Name.StartsWith(TEXT("beacon")) || Shot.Name.StartsWith(TEXT("f6-final"))
		? Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
	if (BeaconPC && BeaconPC->GetUIMode() != EExploredUIMode::Playing)
	{
		BeaconPC->ResumeGame();
		BeaconPC->SetViewTarget(Camera);
	}
	if (Shot.Action == TEXT("interact"))
	{
		PlacePawnFacing(Shot.Target);
	}
	if (Shot.Action == TEXT("firstperson"))
	{
		ShowFirstPerson(Shot);
	}
	SetToonBands(Shot.Action != TEXT("notoon"));
	if (UBeaconSubsystem* Beacon = Shot.Action.IsNone() ? nullptr : UBeaconSubsystem::Get(World))
	{
		if (Shot.Action == TEXT("lantern"))
		{
			Beacon->DebugSetStage(EBeaconStage::LinternaMontada);
		}
		else if (Shot.Action == TEXT("light"))
		{
			Beacon->DebugSetStage(EBeaconStage::LinternaMontada);
			Beacon->TryLight();
		}
		else if (Shot.Action == TEXT("ignite"))
		{
			Beacon->DebugSetStage(EBeaconStage::LinternaMontada);
			for (TActorIterator<ALighthouseLamp> It(World); It; ++It)
			{
				It->DebugIgnite();
				break;
			}
			// Captura a los 2 s de los 3 que tarda en prender (4 s de asentamiento).
			Timer = 2.0f;
			bRequested = false;
			CurrentShotFrameRates.Reset();
			return;
		}
	}
	// «wait»: deja correr la llegada del Mistral y el fundido antes de capturar.
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
	// La medida del bench no se para: un tirón de streaming durante el barrido es rendimiento real.
	if (const UWorldPartitionSubsystem* Partition = bBenchMeasuring ? nullptr : GetWorld()->GetSubsystem<UWorldPartitionSubsystem>())
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
	const bool bSettled = Timer >= SettleSeconds && (bTexturesReady || Timer >= SettleSeconds + MaxStreamingWaitSeconds);
	// Una vez empezada, la medida del bench sigue aunque el barrido pida texturas nuevas: los
	// fotogramas de streaming son justo los que pueden bajar el mínimo.
	if (!bRequested && (bSettled || bBenchMeasuring))
	{
		if (bBenchMode)
		{
			if (!TickBenchMeasure())
			{
				return;
			}
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
		LogShotFrameRate(Shots[Current].Name);
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
			if (bBenchMode)
			{
				LogBenchSummary();
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

bool UExploredShotSubsystem::TickBenchMeasure()
{
	if (!bBenchMeasuring)
	{
		bBenchMeasuring = true;
		BenchMeasureTimer = 0.0f;
		BenchFrameSeconds.Reset();
		BenchBaseRotation = Camera ? Camera->GetActorRotation() : FRotator::ZeroRotator;
		return false;
	}

	// FApp::GetDeltaTime es el tiempo real del fotograma, sin dilatación ni el tope de
	// MaxDeltaSeconds del mundo: es el que ve el jugador.
	const float FrameSeconds = static_cast<float>(FApp::GetDeltaTime());
	BenchFrameSeconds.Add(FrameSeconds);
	BenchMeasureTimer += FrameSeconds;
	const float Alpha = BenchMeasureTimer / BenchMeasureSeconds;
	if (Camera)
	{
		FRotator Swept = BenchBaseRotation;
		Swept.Yaw += FBenchBudgetModel::SweepYawOffset(Alpha, BenchHalfSweepDegrees);
		Camera->SetActorRotation(Swept);
	}
	if (Alpha < 1.0f)
	{
		return false;
	}

	bBenchMeasuring = false;
	if (Camera)
	{
		Camera->SetActorRotation(BenchBaseRotation);
	}
	const FBenchViewStats Stats = FBenchBudgetModel::ReduceView(Shots[Current].Name, BenchFrameSeconds);
	BenchViews.Add(Stats);
	UE_LOG(LogExplored, Display, TEXT("%s"), *FBenchBudgetModel::FormatView(Stats));
	return true;
}

void UExploredShotSubsystem::LogBenchSummary() const
{
	const FBenchSummary Summary = FBenchBudgetModel::Summarize(BenchViews);
	UE_LOG(LogExplored, Display, TEXT("%s"), *FBenchBudgetModel::FormatSummary(Summary));
}

TStatId UExploredShotSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredShotSubsystem, STATGROUP_Tickables);
}

void UExploredShotSubsystem::LogShotFrameRate(const FString& Name) const
{
	float Sum = 0.0f;
	float Min = TNumericLimits<float>::Max();
	for (const float Fps : CurrentShotFrameRates)
	{
		Sum += Fps;
		Min = FMath::Min(Min, Fps);
	}
	const int32 N = CurrentShotFrameRates.Num();
	UE_LOG(LogExplored, Display, TEXT("[Shots] fps %s: media %.1f, mínimo %.1f (%d fotogramas)"), *Name,
		N > 0 ? Sum / N : 0.0f, N > 0 ? Min : 0.0f, N);
}

void UExploredShotSubsystem::SetToonBands(bool bEnabled) const
{
	// Comparación con y sin M_PP_Toon (pasada visual de F4): solo cambia el peso del blendable.
	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
	{
		for (FWeightedBlendable& Blendable : It->Settings.WeightedBlendables.Array)
		{
			Blendable.Weight = bEnabled ? 1.0f : 0.0f;
		}
	}
}
