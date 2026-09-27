#include "ExploredGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "Boats/ExploredBoat.h"
#include "Building/BuildingSubsystem.h"
#include "Building/ExploredBuildingPiece.h"
#include "Cooking/ExploredFire.h"
#include "Core/SystemLinks.h"
#include "Player/ExploredCharacter.h"
#include "Survival/BodySignalsComponent.h"
#include "UI/ExploredHUD.h"
#include "UI/ExploredPlayerController.h"

namespace ExploredGameModeDetail
{
	/** Se reaparece al lado del fuego, no encima (cm). */
	const FVector RespawnOffset(150.0, 0.0, 100.0);
}

AExploredGameMode::AExploredGameMode()
{
	DefaultPawnClass = AExploredCharacter::StaticClass();
	HUDClass = AExploredHUD::StaticClass();
	PlayerControllerClass = AExploredPlayerController::StaticClass();
}

void AExploredGameMode::GatherRespawnPoints(TArray<FVector>& OutPoints) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AExploredFire> It(World); It; ++It)
	{
		if (It->IsRespawnPoint())
		{
			OutPoints.Add(It->GetActorLocation());
		}
	}
	if (const UBuildingSubsystem* Building = World->GetSubsystem<UBuildingSubsystem>())
	{
		if (const FBuildingModel* Model = Building->GetModel())
		{
			for (const int32 PieceId : Model->GetRespawnPoints())
			{
				if (const AExploredBuildingPiece* Actor = Building->FindActor(PieceId))
				{
					OutPoints.Add(Actor->GetActorLocation());
				}
			}
		}
	}
}

void AExploredGameMode::HandlePlayerDeath(APawn* Pawn)
{
	if (!Pawn || bDeathInProgress)
	{
		return;
	}
	const UBodySignalsComponent* Body = Pawn->FindComponentByClass<UBodySignalsComponent>();
	const FSurvivalModeSettings Mode = Body ? Body->GetModeSettings() : FSurvivalModeSettings();

	TArray<FVector> Points;
	GatherRespawnPoints(Points);
	const ExploredLinks::ERespawnDecision Decision = ExploredLinks::DecideRespawn(Mode, Points.Num() > 0);
	UE_LOG(LogTemp, Display, TEXT("[Explored] Muerte del jugador: %s"),
		Decision == ExploredLinks::ERespawnDecision::GameOver ? TEXT("sin reaparición (Náufrago)") : TEXT("reaparece"));

	bDeathInProgress = true;
	// Muerto a bordo: se baja del barco, o seguiría enganchado al asiento tras
	// reaparecer y el contexto de entrada del barco seguiría activo.
	for (TActorIterator<AExploredBoat> It(GetWorld()); It; ++It)
	{
		if (It->GetOccupant() == Pawn)
		{
			It->Leave();
			break;
		}
	}
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		Pawn->DisableInput(PC);
	}

	FTimerHandle Handle;
	if (Decision == ExploredLinks::ERespawnDecision::GameOver)
	{
		GetWorldTimerManager().SetTimer(Handle, this, &AExploredGameMode::ReturnToMenu, FMath::Max(RespawnDelaySeconds, 0.1f), false);
		return;
	}
	const TWeakObjectPtr<APawn> WeakPawn(Pawn);
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &AExploredGameMode::RespawnPawn, WeakPawn),
		FMath::Max(RespawnDelaySeconds, 0.1f), false);
}

void AExploredGameMode::RespawnPawn(TWeakObjectPtr<APawn> WeakPawn)
{
	bDeathInProgress = false;
	APawn* Pawn = WeakPawn.Get();
	if (!Pawn)
	{
		return;
	}
	// Los fuegos pueden haberse apagado durante la espera: se vuelve a mirar.
	TArray<FVector> Points;
	GatherRespawnPoints(Points);
	FVector Location = Pawn->GetActorLocation();
	FRotator Rotation = Pawn->GetActorRotation();
	const int32 Nearest = ExploredLinks::NearestPoint(Points, Pawn->GetActorLocation());
	if (Nearest != INDEX_NONE)
	{
		Location = Points[Nearest] + ExploredGameModeDetail::RespawnOffset;
	}
	else if (AActor* Start = FindPlayerStart(Pawn->GetController()))
	{
		Location = Start->GetActorLocation();
		Rotation = Start->GetActorRotation();
	}
	Pawn->TeleportTo(Location, Rotation, /*bIsATest*/ false, /*bNoCheck*/ true);

	if (UBodySignalsComponent* Body = Pawn->FindComponentByClass<UBodySignalsComponent>())
	{
		Body->ApplyRespawn();
	}
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		PC->SetControlRotation(Rotation);
		Pawn->EnableInput(PC);
	}
}

void AExploredGameMode::ReturnToMenu()
{
	bDeathInProgress = false;
	// Recargar el mapa vuelve al menú principal (AExploredPlayerController lo muestra al empezar).
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString*/ true)));
}
