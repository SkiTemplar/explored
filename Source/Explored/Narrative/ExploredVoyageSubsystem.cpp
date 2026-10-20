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
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/PointsOfInterest.h"
#include "WorldGen/TerrainDensity.h"

namespace ExploredVoyageDetail
{
	/** Altura de la arena (m) en la que se busca sitio para la grada, junto a la orilla. */
	constexpr float ShipyardMinHeight = 0.5f;
	constexpr float ShipyardMaxHeight = 1.4f;
	/** Desplazamiento lateral de la grada respecto a la línea del punto de aparición (m). */
	constexpr float ShipyardSideOffset = 7.0f;
	/** Secuencia de salida: dónde arranca el barco (por delante de la grada) y cuánto dura. */
	constexpr double DepartureStartAheadCm = 1800.0;
	constexpr float DepartureSequenceSeconds = 9.0f;
	/** Hora a la que deja el reloj «Esperar a la noche junto al Limón». */
	constexpr float WaitUntilHours = 20.5f;
	/** Altura (m) del claro tierra adentro donde cae el motor en la isla de inicio. */
	constexpr float InlandMinHeight = 3.0f;
	constexpr float InlandMaxHeight = 25.0f;

	bool PartForPoi(EPoiType Type, EShipPart& OutPart)
	{
		switch (Type)
		{
		case EPoiType::WreckFuselage: OutPart = EShipPart::Fuselage; return true;
		case EPoiType::WreckWing: OutPart = EShipPart::Wing; return true;
		case EPoiType::WreckTail: OutPart = EShipPart::Tail; return true;
		case EPoiType::WreckEngine: OutPart = EShipPart::Engine; return true;
		default: return false;
		}
	}

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
	SpawnSalvageSites(InWorld);
	SpawnShipyard(InWorld);
}

void UExploredVoyageSubsystem::SpawnSalvageSites(UWorld& World)
{
	const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
	int32 Spawned = 0;
	for (const FPointOfInterest& Poi : FPoiLayout::Generate(Density))
	{
		EShipPart Part;
		if (!ExploredVoyageDetail::PartForPoi(Poi.Type, Part))
		{
			continue;
		}
		FVector Location = Poi.Location;
		// v1: sin barcos que lleguen a otras islas, todas las piezas están en la isla de inicio.
		// El motor (en Esmeralda según FPoiLayout) se lleva tierra adentro de la isla de llegada.
		if (Part == EShipPart::Engine && !FindLandingInland(Density, Location))
		{
			UE_LOG(LogExplored, Warning, TEXT("[Viaje] No hay sitio para el motor en la isla de inicio; se queda en Esmeralda"));
		}
		const FTransform Transform(FRotator(0.0f, Poi.Yaw, 0.0f), Location * 100.0);
		AExploredSalvageSite* Site = World.SpawnActorDeferred<AExploredSalvageSite>(AExploredSalvageSite::StaticClass(), Transform);
		if (!Site)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Viaje] No se pudo crear el pecio %s"), LexToString(Poi.Type));
			continue;
		}
		Site->Configure(Part);
		Site->FinishSpawning(Transform);
		SalvageSites.Add(Site);
		++Spawned;
		UE_LOG(LogExplored, Display, TEXT("[Viaje] Pecio %s en (%.0f, %.0f, %.1f) m%s"), LexToString(Poi.Type),
			Location.X, Location.Y, Location.Z, Poi.bUnderwater ? TEXT(" bajo el agua") : TEXT(""));
	}
	if (Spawned != static_cast<int32>(EShipPart::Count))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Viaje] Se esperaban %d pecios y se colocaron %d"), static_cast<int32>(EShipPart::Count), Spawned);
	}
}

bool UExploredVoyageSubsystem::FindLandingInland(const FTerrainDensity& Density, FVector& OutMeters)
{
	const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
	if (!Landing)
	{
		return false;
	}
	// Al otro lado de la isla respecto a la playa de llegada, a media distancia del centro:
	// obliga a cruzar la selva sin salir de la isla.
	const float Angle = Landing->Rotation + PI * 0.6f;
	const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
	for (float R = Landing->Radius * 0.35f; R < Landing->Radius * 0.9f; R += 2.0f)
	{
		const FVector2D P = Landing->Center + Dir * R;
		const float H = Density.SampleColumn(P.X, P.Y).Height;
		if (H > ExploredVoyageDetail::InlandMinHeight && H < ExploredVoyageDetail::InlandMaxHeight)
		{
			OutMeters = FVector(P.X, P.Y, H);
			return true;
		}
	}
	return false;
}

bool UExploredVoyageSubsystem::FindShipyardLocation(const FTerrainDensity& Density, FVector& OutMeters, FRotator& OutRotation)
{
	const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
	if (!Landing)
	{
		return false;
	}
	// Misma recta que FindSpawnPoint del horneado: del centro de la isla hacia la laguna.
	// La grada va más allá del punto de aparición, en la arena junto a la orilla.
	const FVector2D Dir(FMath::Cos(Landing->Rotation), FMath::Sin(Landing->Rotation));
	const FVector2D Side(-Dir.Y, Dir.X);
	for (float R = 0.0f; R < Landing->Radius * 1.4f; R += 1.0f)
	{
		const FVector2D P = Landing->Center - Dir * R + Side * ExploredVoyageDetail::ShipyardSideOffset;
		const float H = Density.SampleColumn(P.X, P.Y).Height;
		if (H > ExploredVoyageDetail::ShipyardMinHeight && H < ExploredVoyageDetail::ShipyardMaxHeight)
		{
			OutMeters = FVector(P.X, P.Y, H);
			// Proa (+X de la malla) hacia el mar.
			OutRotation = FVector(-Dir.X, -Dir.Y, 0.0f).Rotation();
			return true;
		}
	}
	return false;
}

void UExploredVoyageSubsystem::SpawnShipyard(UWorld& World)
{
	const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
	FVector Location;
	FRotator Rotation;
	if (!FindShipyardLocation(Density, Location, Rotation))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Viaje] No se encontró arena para la grada del Limón"));
		return;
	}
	const FTransform Transform(Rotation, Location * 100.0);
	Shipyard = World.SpawnActor<AExploredShipyard>(AExploredShipyard::StaticClass(), Transform);
	UE_LOG(LogExplored, Display, TEXT("[Viaje] Grada del Limón en (%.0f, %.0f, %.1f) m"), Location.X, Location.Y, Location.Z);
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
		Notify(NSLOCTEXT("ExploredVoyage", "AllParts", "Ya tienes todo: construye el Limón en la grada de la playa"),
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
	Notify(NSLOCTEXT("ExploredVoyage", "ShipBuilt", "El Limón está listo. Zarpa de noche, guiándote por las estrellas"),
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
			Notify(NSLOCTEXT("ExploredVoyage", "WaitNight", "De día no se ven las estrellas: espera a la noche junto al Limón"),
				FLinearColor(0.75f, 0.85f, 1.0f, 1.0f));
		}
		return false;
	}
	UE_LOG(LogExplored, Display, TEXT("[Viaje] El jugador zarpa en el Limón"));
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
	UE_LOG(LogExplored, Display, TEXT("[Viaje] Espera junto al Limón hasta las %.1f h"), ExploredVoyageDetail::WaitUntilHours);
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
		return NSLOCTEXT("ExploredVoyage", "ObjBuild", "Construye el Limón en la grada de la playa");
	case EVoyageStage::SetSail:
		return IsStarNightNow()
			? NSLOCTEXT("ExploredVoyage", "ObjSailNow", "Es de noche: zarpa en el Limón")
			: NSLOCTEXT("ExploredVoyage", "ObjSailWait", "Espera a la noche para zarpar en el Limón");
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
