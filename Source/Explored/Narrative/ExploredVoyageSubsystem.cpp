#include "Narrative/ExploredVoyageSubsystem.h"

#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"

#include "Audio/ExploredMusicSubsystem.h"
#include "Core/ExploredWiringSubsystem.h"
#include "Explored.h"
#include "Narrative/ExploredVoyageActors.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "UI/ExploredHUD.h"
#include "UI/ExploredPlayerController.h"
#include "WorldGen/IslandLayout.h"
#include "WorldGen/IslandLayoutLoader.h"

namespace ExploredVoyageDetail
{
	/** Secuencia de salida: dónde arranca el barco (por delante de la grada) y cuánto dura. */
	constexpr double DepartureStartAheadCm = 1800.0;
	constexpr float DepartureSequenceSeconds = 9.0f;
	/** Hora a la que deja el reloj «Esperar a la noche junto al barco». */
	constexpr float WaitUntilHours = 20.5f;
	/** Punto de autor de island.json donde va la grada (sitio de la balsa, GDD §5.7). */
	const FName RaftSitePoint(TEXT("raft_site"));

	const FExploredProgress* GetProgress(const UWorld* World)
	{
		const UExploredWiringSubsystem* Wiring = World ? World->GetSubsystem<UExploredWiringSubsystem>() : nullptr;
		return Wiring ? &Wiring->GetProgress() : nullptr;
	}
}

bool UExploredVoyageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExploredVoyageSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Los pecios del bucle anterior se retiraron con el procedural; la balsa de F3 sustituye a la grada.
	SpawnShipyard(InWorld);
}

bool UExploredVoyageSubsystem::FindShipyardLocation(const FIslandLayout& Layout, FVector& OutMeters, FRotator& OutRotation)
{
	const FIslandPoint* Site = Layout.FindPoint(ExploredVoyageDetail::RaftSitePoint);
	if (!Site)
	{
		return false;
	}
	const float Height = FMath::Max(Layout.SampleColumn(Site->Location.X, Site->Location.Y).Height, FIslandLayout::SeaLevel);
	OutMeters = FVector(Site->Location.X, Site->Location.Y, Height);
	// Proa (+X de la malla) hacia el mar, según el yaw de autor del punto.
	OutRotation = FRotator(0.0f, Site->Yaw, 0.0f);
	return true;
}

void UExploredVoyageSubsystem::SpawnShipyard(UWorld& World)
{
	FVector Location;
	FRotator Rotation;
	if (!FindShipyardLocation(*ExploredIsland::Get(), Location, Rotation))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Viaje] island.json no tiene el punto «raft_site»; no hay grada"));
		return;
	}
	const FTransform Transform(Rotation, Location * 100.0);
	Shipyard = World.SpawnActor<AExploredShipyard>(AExploredShipyard::StaticClass(), Transform);
	UE_LOG(LogExplored, Display, TEXT("[Viaje] Grada del barco en (%.0f, %.0f, %.1f) m"), Location.X, Location.Y, Location.Z);
}

EVoyageStage UExploredVoyageSubsystem::GetStage() const
{
	const FExploredProgress* Progress = ExploredVoyageDetail::GetProgress(GetWorld());
	return Progress ? Progress->GetVoyageStage() : EVoyageStage::Salvage;
}

int32 UExploredVoyageSubsystem::CountParts() const
{
	const FExploredProgress* Progress = ExploredVoyageDetail::GetProgress(GetWorld());
	return Progress ? Progress->CountShipParts() : 0;
}

bool UExploredVoyageSubsystem::HasPart(EShipPart Part) const
{
	const FExploredProgress* Progress = ExploredVoyageDetail::GetProgress(GetWorld());
	return Progress && Progress->HasShipPart(Part);
}

bool UExploredVoyageSubsystem::RecoverPart(EShipPart Part)
{
	UExploredWiringSubsystem* Wiring = GetWorld() ? GetWorld()->GetSubsystem<UExploredWiringSubsystem>() : nullptr;
	if (!Wiring || Wiring->GetProgress().HasShipPart(Part))
	{
		return false;
	}
	FExploredProgress& Progress = Wiring->GetProgress();
	Progress.AddShipPart(Part);
	const int32 Count = Progress.CountShipParts();
	Notify(FText::Format(NSLOCTEXT("ExploredVoyage", "PartRecovered", "{0} del Albatros rescatado ({1}/4)"), PartName(Part), Count),
		FLinearColor(1.0f, 0.86f, 0.35f, 1.0f));
	if (Progress.HasAllShipParts())
	{
		Notify(NSLOCTEXT("ExploredVoyage", "AllParts", "Ya tienes todo: construye el barco en la grada de la playa"),
			FLinearColor(1.0f, 0.95f, 0.55f, 1.0f));
	}
	if (AExploredShipyard* Yard = Shipyard.Get())
	{
		Yard->RefreshMesh();
	}
	return true;
}

bool UExploredVoyageSubsystem::TryBuildShip()
{
	UExploredWiringSubsystem* Wiring = GetWorld() ? GetWorld()->GetSubsystem<UExploredWiringSubsystem>() : nullptr;
	if (!Wiring || !Wiring->GetProgress().BuildShip())
	{
		return false;
	}
	Notify(NSLOCTEXT("ExploredVoyage", "ShipBuilt", "El barco está listo. Zarpa de noche, guiándote por las estrellas"),
		FLinearColor(1.0f, 0.92f, 0.3f, 1.0f));
	if (AExploredShipyard* Yard = Shipyard.Get())
	{
		Yard->RefreshMesh();
	}
	return true;
}

bool UExploredVoyageSubsystem::TryDepart()
{
	UExploredWiringSubsystem* Wiring = GetWorld() ? GetWorld()->GetSubsystem<UExploredWiringSubsystem>() : nullptr;
	if (!Wiring)
	{
		return false;
	}
	if (bDepartureRunning)
	{
		return false;
	}
	DepartureHours = GetHours();
	if (!Wiring->GetProgress().CanDepartAt(DepartureHours))
	{
		if (Wiring->GetProgress().CanDepart() && !IsStarNightNow())
		{
			Notify(NSLOCTEXT("ExploredVoyage", "WaitNight", "De día no se ven las estrellas: espera a la noche junto al barco"),
				FLinearColor(0.75f, 0.85f, 1.0f, 1.0f));
		}
		return false;
	}
	UE_LOG(LogExplored, Display, TEXT("[Viaje] El jugador zarpa en el barco"));
	bDepartureRunning = true;
	PlayDepartureSequence();
	return true;
}

void UExploredVoyageSubsystem::PlayDepartureSequence()
{
	UWorld* World = GetWorld();
	AExploredShipyard* Yard = Shipyard.Get();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!World || !Yard || !PC)
	{
		FinishDepartureSequence();
		return;
	}
	if (AExploredPlayerController* ExploredPC = Cast<AExploredPlayerController>(PC))
	{
		ExploredPC->SetCutsceneActive(true);
	}
	// La grada queda vacía: el barco ya está en el agua, unos metros por delante de la proa.
	Yard->SetActorHiddenInGame(true);
	const FVector Forward = Yard->GetActorForwardVector().GetSafeNormal2D();
	FVector Start = Yard->GetActorLocation() + Forward * ExploredVoyageDetail::DepartureStartAheadCm;
	Start.Z = 0.0;
	const FRotator Heading(0.0f, Forward.Rotation().Yaw, 0.0f);
	World->SpawnActor<AExploredDepartingShip>(AExploredDepartingShip::StaticClass(), Start, Heading);

	// Cámara fija en la orilla, detrás y a un lado, mirando cómo se aleja.
	const FVector Side(-Forward.Y, Forward.X, 0.0);
	const FVector CameraLocation = Yard->GetActorLocation() - Forward * 600.0 + Side * 900.0 + FVector(0.0, 0.0, 350.0);
	const FVector LookAt = Start + Forward * 2500.0 + FVector(0.0, 0.0, 200.0);
	if (ACameraActor* Camera = World->SpawnActor<ACameraActor>(CameraLocation, (LookAt - CameraLocation).Rotation()))
	{
		PC->SetViewTargetWithBlend(Camera, 1.5f, VTBlend_EaseInOut, 2.0f);
	}
	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->DisableInput(PC);
		Pawn->SetActorHiddenInGame(true);
	}
	World->GetTimerManager().SetTimer(DepartureTimer, FTimerDelegate::CreateUObject(this, &UExploredVoyageSubsystem::FinishDepartureSequence),
		ExploredVoyageDetail::DepartureSequenceSeconds, false);
}

void UExploredVoyageSubsystem::FinishDepartureSequence()
{
	// El final se anota al terminar la secuencia: si se interrumpe, la partida sigue pudiendo zarpar.
	if (UExploredWiringSubsystem* Wiring = GetWorld() ? GetWorld()->GetSubsystem<UExploredWiringSubsystem>() : nullptr)
	{
		Wiring->GetProgress().Depart(DepartureHours);
	}
	bDepartureRunning = false;
	if (AExploredPlayerController* PC = GetWorld() ? Cast<AExploredPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr)
	{
		PC->ShowEnding();
	}
}

void UExploredVoyageSubsystem::WaitForNight()
{
	UTimeOfDaySubsystem* Time = GetWorld() ? GetWorld()->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	if (!Time || IsStarNightNow())
	{
		return;
	}
	// Si ya pasó la medianoche (antes de las 05:00 cuenta como noche), aquí siempre es de día:
	// la noche siguiente es la de hoy a las 20:30.
	Time->SetTime(Time->GetDay(), ExploredVoyageDetail::WaitUntilHours);
	Notify(NSLOCTEXT("ExploredVoyage", "NightFell", "Cae la noche. Las estrellas ya marcan el rumbo."),
		FLinearColor(0.75f, 0.85f, 1.0f, 1.0f));
	UE_LOG(LogExplored, Display, TEXT("[Viaje] Espera junto al barco hasta las %.1f h"), ExploredVoyageDetail::WaitUntilHours);
}

bool UExploredVoyageSubsystem::IsStarNightNow() const
{
	return FExploredProgress::IsStarNight(GetHours());
}

float UExploredVoyageSubsystem::GetHours() const
{
	const UTimeOfDaySubsystem* Time = GetWorld() ? GetWorld()->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	return Time ? Time->GetHours() : 12.0f;
}

FText UExploredVoyageSubsystem::GetObjectiveText() const
{
	switch (GetStage())
	{
	case EVoyageStage::Salvage:
	{
		const FText Base = FText::Format(NSLOCTEXT("ExploredVoyage", "ObjSalvage", "Rescata las piezas del Albatros ({0}/4)"), CountParts());
		const FText Hint = DescribeNearestSalvage();
		return Hint.IsEmpty() ? Base : FText::Format(NSLOCTEXT("ExploredVoyage", "ObjSalvageHint", "{0} · la más cercana {1}"), Base, Hint);
	}
	case EVoyageStage::Build:
		return NSLOCTEXT("ExploredVoyage", "ObjBuild", "Construye el barco en la grada de la playa");
	case EVoyageStage::SetSail:
		return IsStarNightNow()
			? NSLOCTEXT("ExploredVoyage", "ObjSailNow", "Es de noche: zarpa en el barco")
			: NSLOCTEXT("ExploredVoyage", "ObjSailWait", "Espera a la noche para zarpar en el barco");
	default:
		return FText::GetEmpty();
	}
}

FText UExploredVoyageSubsystem::DescribeNearestSalvage() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return FText::GetEmpty();
	}
	const FVector From = Pawn->GetActorLocation();
	const AExploredSalvageSite* Nearest = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (const TWeakObjectPtr<AExploredSalvageSite>& Weak : SalvageSites)
	{
		const AExploredSalvageSite* Site = Weak.Get();
		if (!Site || HasPart(Site->GetPart()))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared2D(From, Site->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Nearest = Site;
		}
	}
	if (!Nearest)
	{
		return FText::GetEmpty();
	}
	// Dirección relativa a la vista (sin brújula en la v1): delante, detrás, izquierda o derecha.
	const FVector ToSite = (Nearest->GetActorLocation() - From).GetSafeNormal2D();
	const FVector Forward = PC->GetControlRotation().Vector().GetSafeNormal2D();
	const float Dot = FVector::DotProduct(Forward, ToSite);
	const float Cross = FVector::CrossProduct(Forward, ToSite).Z;
	FText Side;
	if (Dot > 0.7071f)
	{
		Side = NSLOCTEXT("ExploredVoyage", "DirAhead", "delante");
	}
	else if (Dot < -0.7071f)
	{
		Side = NSLOCTEXT("ExploredVoyage", "DirBehind", "detrás");
	}
	else
	{
		Side = Cross > 0.0f ? NSLOCTEXT("ExploredVoyage", "DirRight", "a la derecha") : NSLOCTEXT("ExploredVoyage", "DirLeft", "a la izquierda");
	}
	const int32 Meters = FMath::RoundToInt32(FMath::Sqrt(BestDistSq) / 100.0);
	const FText Depth = Nearest->GetActorLocation().Z < 0.0
		? NSLOCTEXT("ExploredVoyage", "DirUnderwater", " (bajo el agua)") : FText::GetEmpty();
	return FText::Format(NSLOCTEXT("ExploredVoyage", "DirFormat", "a {0} m, {1}{2}"), Meters, Side, Depth);
}

FText UExploredVoyageSubsystem::PartName(EShipPart Part)
{
	switch (Part)
	{
	case EShipPart::Fuselage: return NSLOCTEXT("ExploredVoyage", "PartFuselage", "Fuselaje");
	case EShipPart::Wing: return NSLOCTEXT("ExploredVoyage", "PartWing", "Ala");
	case EShipPart::Tail: return NSLOCTEXT("ExploredVoyage", "PartTail", "Cola");
	case EShipPart::Engine: return NSLOCTEXT("ExploredVoyage", "PartEngine", "Motor");
	default: return FText::GetEmpty();
	}
}

void UExploredVoyageSubsystem::Notify(const FText& Text, const FLinearColor& Color) const
{
	// Cada hito del viaje lleva el acento musical de descubrimiento.
	if (UExploredMusicSubsystem* Music = GetWorld() ? GetWorld()->GetSubsystem<UExploredMusicSubsystem>() : nullptr)
	{
		Music->NotifyDiscovery();
	}
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (AExploredHUD* HUD = PC ? Cast<AExploredHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->PushNotification(Text, Color, 4.0f);
	}
}
