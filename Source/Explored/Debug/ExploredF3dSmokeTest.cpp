#include "Debug/ExploredF3dSmokeTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#include "Boats/ExploredRaft.h"
#include "Cooking/ExploredFire.h"
#include "Explored.h"
#include "Interaction/ExploredInteractable.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Ocean/ExploredOcean.h"
#include "Player/ExploredCharacter.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"
#include "Weather/ExploredRainBarrel.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/ExploredIslandPopulator.h"
#include "WorldGen/ExploredShelterVolume.h"
#include "WorldGen/IslandPropsLoader.h"
#include "WorldGen/IslandPropsModel.h"

namespace F3dSmokeDetail
{
	constexpr float WarmUpSeconds = 3.0f;

	template <typename T>
	T* First(UWorld* World)
	{
		TActorIterator<T> It(World);
		return It ? *It : nullptr;
	}

	bool AnyOutdoorFireBurning(UWorld* World)
	{
		for (TActorIterator<AExploredFire> It(World); It; ++It)
		{
			if (It->IsBurning() && !It->IsSheltered())
			{
				return true;
			}
		}
		return false;
	}

	AExploredShelterVolume* CalaShelter(UWorld* World)
	{
		for (TActorIterator<AExploredShelterVolume> It(World); It; ++It)
		{
			if (It->ShelterId.ToString().Contains(TEXT("cala")))
			{
				return *It;
			}
		}
		return nullptr;
	}
}

bool UExploredF3dSmokeTest::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && FParse::Param(FCommandLine::Get(), TEXT("ExploredF3dSmoke"));
#endif
}

TStatId UExploredF3dSmokeTest::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredF3dSmokeTest, STATGROUP_Tickables);
}

APawn* UExploredF3dSmokeTest::GetPawn() const
{
	return UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

bool UExploredF3dSmokeTest::Console(const FString& Command) const
{
	UE_LOG(LogExplored, Display, TEXT("[F3dSmoke] > %s"), *Command);
	return GEngine && GEngine->Exec(GetWorld(), *Command);
}

double UExploredF3dSmokeTest::AbsHours() const
{
	const UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>();
	return Time ? FStormClockModel::ToAbsoluteHours(Time->GetDay(), Time->GetHours()) : -1.0;
}

void UExploredF3dSmokeTest::Face(const FVector& Target) const
{
	APawn* Pawn = GetPawn();
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC)
	{
		const FVector Eye = Pawn->GetActorLocation() + FVector(0.0, 0.0, 60.0);
		PC->SetControlRotation((Target - Eye).Rotation());
	}
}

bool UExploredF3dSmokeTest::Shot(const FString& Name)
{
	const FString File = FPaths::ProjectSavedDir() / TEXT("Shots") / (Name + TEXT(".png"));
	IFileManager::Get().Delete(*File);
	FScreenshotRequest::RequestScreenshot(File, true, false);
	UE_LOG(LogExplored, Display, TEXT("[F3dSmoke] Captura %s"), *File);
	return true;
}

void UExploredF3dSmokeTest::BuildSteps()
{
	auto Add = [this](const TCHAR* Name, TFunction<bool()> Start, TFunction<bool()> IsDone, float Timeout = 10.0f)
	{
		Steps.Add({ Name, MoveTemp(Start), MoveTemp(IsDone), Timeout });
	};
	Add(TEXT("reloj D1 07:00 con días de 24 min"), [] { return true; }, [this]()
	{
		const UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>();
		return Time && Time->GetDay() == 1 && Time->GetHours() >= 7.0f && Time->GetHours() < 7.5f && Time->GetDayLengthMinutes() == 26.0f;
	});
	Add(TEXT("una barra bajo 25 frena un 20 %"), [this]() { return Console(TEXT("BalizaBody 20 70 70 100")); }, [this]()
	{
		const AExploredCharacter* Character = Cast<AExploredCharacter>(GetPawn());
		return Character && FMath::IsNearlyEqual(Character->GetCharacterMovement()->MaxWalkSpeed, 360.0f, 1.0f);
	});
	Add(TEXT("delante de una palma con cocos en la mochila"), [this]()
	{
		APawn* Pawn = GetPawn();
		if (!Pawn)
		{
			return false;
		}
		for (TActorIterator<AExploredIslandPopulator> It(GetWorld()); It; ++It)
		{
			if (AActor* Found = It->FindSpawned(TEXT("palma_cala_1")))
			{
				Pawn->SetActorLocation(Found->GetActorLocation() + FVector(-170.0, 0.0, 150.0), false, nullptr, ETeleportType::TeleportPhysics);
				Face(Found->GetActorLocation() + FVector(0.0, 0.0, 120.0));
			}
		}
		return Console(TEXT("BalizaGive coco 2")) && Console(TEXT("BalizaGive fibra 3"));
	}, [this]() { return Elapsed > 7.0f; }, 12.0f);
	Add(TEXT("captura f3d-hud"), [this]() { return Shot(TEXT("f3d-hud")); },
		[] { return !FScreenshotRequest::IsScreenshotRequested(); });
	Add(TEXT("chubasco guiado D1 15:00"), [this]() { return Console(TEXT("BalizaBody 70 70 70 100")) && Console(TEXT("BalizaTime 1 15.1")); },
		[this]()
		{
			const UExploredWeatherSubsystem* Weather = GetWorld()->GetSubsystem<UExploredWeatherSubsystem>();
			return Weather && Weather->GetStormState() == EStormState::Squall && Weather->GetCurrent().Rain > 0.3f;
		});
	AddRaftSteps();
	AddClimateSteps();
	AddBodySteps();
}

void UExploredF3dSmokeTest::AddRaftSteps()
{
	auto Add = [this](const TCHAR* Name, TFunction<bool()> Start, TFunction<bool()> IsDone, float Timeout = 10.0f)
	{
		Steps.Add({ Name, MoveTemp(Start), MoveTemp(IsDone), Timeout });
	};
	auto Raft = [this]() -> AExploredRaft*
	{
		AExploredRaftSite* Site = F3dSmokeDetail::First<AExploredRaftSite>(GetWorld());
		return Site ? Site->GetRaft() : nullptr;
	};
	Add(TEXT("balsa atada y a bordo (D2 08:00)"), [this]() { return Console(TEXT("BalizaTime 2 8")) && Console(TEXT("BalizaRaft board")); },
		[Raft]() { return Raft() && Raft()->IsOccupied(); });
	Add(TEXT("remar 6 s: avanza y no vuelca"), [this, Raft]()
	{
		MarkLocation = Raft()->GetActorLocation();
		Raft()->SetControls(1.0f, 0.0f);
		return true;
	}, [this, Raft]()
	{
		const AExploredRaft* R = Raft();
		const FRotator Rot = R->GetActorRotation();
		if (FMath::Abs(Rot.Pitch) > 4.01 || FMath::Abs(Rot.Roll) > 4.01)
		{
			UE_LOG(LogExplored, Error, TEXT("[F3dSmoke] Balsa inclinada: %s"), *Rot.ToCompactString());
			return false;
		}
		const double Moved = FVector::Dist2D(R->GetActorLocation(), MarkLocation) / 100.0;
		if (Elapsed > 6.0f)
		{
			UE_LOG(LogExplored, Display, TEXT("[F3dSmoke] Balsa: %.1f m en 6 s, encallada %d"), Moved, R->IsGrounded() ? 1 : 0);
		}
		return Elapsed > 6.0f && (Moved >= 6.0 || R->IsGrounded());
	});
	Add(TEXT("girar y bajar de la balsa"), [Raft]()
	{
		Raft()->SetControls(0.0f, 1.0f);
		return true;
	}, [this, Raft]()
	{
		if (Elapsed < 1.0f)
		{
			return false;
		}
		Raft()->SetControls(0.0f, 0.0f);
		Raft()->Leave();
		return !Raft()->IsOccupied() && GetPawn()->GetAttachParentActor() == nullptr;
	});
	Add(TEXT("a las 18:00 del día 2 no se embarca"), [this, Raft]()
	{
		Console(TEXT("BalizaTime 2 18.2"));
		IExploredInteractable::Execute_Interact(Raft(), GetPawn());
		return true;
	}, [Raft]() { return !Raft()->IsOccupied(); });
	Add(TEXT("18:45 en el islote: vuelta forzosa a Playa Norte con sed y hambre -20"), [this]()
	{
		const TSharedRef<const FIslandProps> Props = ExploredIslandProps::Get();
		const FIslandProp* Hull = Props->FindProp(TEXT("carguero_casco"));
		if (!Hull)
		{
			return false;
		}
		Console(TEXT("BalizaTime 2 18.6"));
		Console(TEXT("BalizaBody 60 60 60 100"));
		const FVector Islet = IslandPropsPlacement::WorldLocation(*Hull, 0.0f);
		GetPawn()->SetActorLocation(Islet + FVector(1500.0, -1500.0, 1500.0), false, nullptr, ETeleportType::TeleportPhysics);
		return Console(TEXT("BalizaTime 2 18.8"));
	}, [this, Body = [this]() { return UIslandBodyComponent::FindOn(GetPawn()); }]()
	{
		const AExploredRaftSite* Site = F3dSmokeDetail::First<AExploredRaftSite>(GetWorld());
		const UIslandBodyComponent* B = Body();
		return Site && B && !B->IsFainting() && FVector::Dist2D(GetPawn()->GetActorLocation(), Site->GetActorLocation()) < 3000.0
			&& FMath::IsNearlyEqual(B->GetBars().Thirst, 40.0f, 2.0f) && FMath::IsNearlyEqual(B->GetBars().Hunger, 40.0f, 2.0f);
	}, 12.0f);
}

void UExploredF3dSmokeTest::AddClimateSteps()
{
	auto Add = [this](const TCHAR* Name, TFunction<bool()> Start, TFunction<bool()> IsDone, float Timeout = 10.0f)
	{
		Steps.Add({ Name, MoveTemp(Start), MoveTemp(IsDone), Timeout });
	};
	auto Weather = [this]() { return GetWorld()->GetSubsystem<UExploredWeatherSubsystem>(); };
	auto Barrel = [this]() { return F3dSmokeDetail::First<AExploredRainBarrel>(GetWorld()); };
	Add(TEXT("hoguera al aire libre antes del ciclón"), [this]()
	{
		Console(TEXT("BalizaTime 2 18.9"));
		Console(TEXT("BalizaGive lena 3"));
		UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
		return Survivor && Survivor->Craft(TEXT("hoguera"));
	}, [this]() { return F3dSmokeDetail::AnyOutdoorFireBurning(GetWorld()); });
	Add(TEXT("muro 1: lluvia, olas, niebla y fuego apagado"), [this]() { return Console(TEXT("BalizaTime 2 19.3")); }, [this, Weather]()
	{
		const UExploredWeatherSubsystem* W = Weather();
		const AExploredOcean* Ocean = F3dSmokeDetail::First<AExploredOcean>(GetWorld());
		return W && Ocean && W->GetStormState() == EStormState::CycloneWall1 && W->IsCycloneWall() && W->GetCurrent().Rain > 0.5f
			&& W->GetCurrent().Fog > 0.3f && Ocean->GetSeaState() > 0.7f && !F3dSmokeDetail::AnyOutdoorFireBurning(GetWorld());
	}, 15.0f);
	Add(TEXT("el barril se llena con la lluvia"), [this, Barrel]()
	{
		MarkCount = Barrel() ? Barrel()->GetCharges() : -1;
		return MarkCount >= 0 && Console(TEXT("BalizaTimeScale 10"));
	}, [this, Barrel]() { return Barrel()->GetCharges() > MarkCount; }, 15.0f);
	Add(TEXT("captura f3d-ciclon"), [this]()
	{
		Console(TEXT("BalizaTimeScale 1"));
		Console(TEXT("BalizaTime 2 20.5"));
		const AExploredRaftSite* Site = F3dSmokeDetail::First<AExploredRaftSite>(GetWorld());
		if (Site)
		{
			GetPawn()->SetActorLocation(Site->GetShoreLocation(), false, nullptr, ETeleportType::TeleportPhysics);
			const FVector2D Dir = FRaftModel::Forward(Site->GetRouteYawDeg());
			Face(GetPawn()->GetActorLocation() + FVector(Dir.X, Dir.Y, 0.0) * 3000.0);
		}
		return true;
	}, [this]() { return Elapsed > 4.0f && Shot(TEXT("f3d-ciclon")); });
	Add(TEXT("espera de la captura"), [] { return true; }, [] { return !FScreenshotRequest::IsScreenshotRequested(); });
	Add(TEXT("ojo: calma sin lluvia"), [this]() { return Console(TEXT("BalizaTime 2 22.3")); }, [Weather]()
	{
		const UExploredWeatherSubsystem* W = Weather();
		return W && W->GetStormState() == EStormState::CycloneEye && !W->IsCycloneWall() && W->GetCurrent().Rain < 0.1f
			&& W->GetCurrent().Wind < 0.4f;
	}, 15.0f);
	Add(TEXT("reloj x40: muro 2 y fin del ciclón a las 02:00"), [this]()
	{
		MarkCount = 0;
		return Console(TEXT("BalizaTime 2 23.2")) && Console(TEXT("BalizaTimeScale 40"));
	}, [this, Weather]()
	{
		const EStormState State = Weather()->GetStormState();
		MarkCount = MarkCount | (State == EStormState::CycloneWall2 ? 1 : 0);
		return MarkCount == 1 && AbsHours() > 50.1 && State == EStormState::Clear;
	}, 20.0f);
}

void UExploredF3dSmokeTest::AddBodySteps()
{
	auto Add = [this](const TCHAR* Name, TFunction<bool()> Start, TFunction<bool()> IsDone, float Timeout = 10.0f)
	{
		Steps.Add({ Name, MoveTemp(Start), MoveTemp(IsDone), Timeout });
	};
	auto Body = [this]() { return UIslandBodyComponent::FindOn(GetPawn()); };
	Add(TEXT("desmayo: 20 s a 0, +3 h y despierta en Cala Varada con 40/40/40/60"), [this]()
	{
		Console(TEXT("BalizaTimeScale 1"));
		Console(TEXT("BalizaTime 3 10"));
		MarkHours = AbsHours();
		return Console(TEXT("BalizaBody 0 50 50 50"));
	}, [this, Body]()
	{
		const UIslandBodyComponent* B = Body();
		const AExploredShelterVolume* Cala = F3dSmokeDetail::CalaShelter(GetWorld());
		if (!B || B->GetFaintCount() != 1 || B->IsFainting() || !Cala)
		{
			return false;
		}
		const FBodyBars& Bars = B->GetBars();
		const double Skipped = AbsHours() - MarkHours;
		UE_LOG(LogExplored, Display, TEXT("[F3dSmoke] Desmayo: +%.2f h, barras %.0f/%.0f/%.0f/%.0f, a %.0f m del refugio"), Skipped,
			Bars.Thirst, Bars.Hunger, Bars.Energy, Bars.Warmth, FVector::Dist2D(GetPawn()->GetActorLocation(), Cala->GetRespawnLocation()) / 100.0);
		return Skipped >= 3.0 && Skipped < 3.5 && FMath::IsNearlyEqual(Bars.Thirst, 40.0f, 2.0f) && FMath::IsNearlyEqual(Bars.Warmth, 60.0f, 2.0f)
			&& FVector::Dist2D(GetPawn()->GetActorLocation(), Cala->GetRespawnLocation()) < 500.0;
	}, 35.0f);
	Add(TEXT("en el refugio de Cala Varada"), [this]()
	{
		Console(TEXT("BalizaTime 3 12"));
		const AExploredShelterVolume* Cala = F3dSmokeDetail::CalaShelter(GetWorld());
		return Cala && GetPawn()->SetActorLocation(Cala->GetRespawnLocation() + FVector(0.0, 0.0, 30.0), false, nullptr, ETeleportType::TeleportPhysics);
	}, [Body]() { return Body() && Body()->IsSheltered(); });
	Add(TEXT("a mediodía no se duerme"), [Body]() { return !Body()->TrySleep(); }, [Body]() { return Body()->GetSleepCount() == 0; });
	// El día 2 a las 21:00 hay ciclón y no se duerme (F3e): se prueba el día 3.
	Add(TEXT("dormir a las 21:00 hasta las 06:00 con energía 100"), [this]()
	{
		return Console(TEXT("BalizaTime 3 21")) && Console(TEXT("BalizaBody 50 50 30 80")) && Console(TEXT("BalizaSleep"));
	}, [this, Body]()
	{
		const UIslandBodyComponent* B = Body();
		const UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>();
		return B && Time && B->GetSleepCount() == 1 && !B->IsFainting() && Time->GetDay() == 4 && Time->GetHours() >= 6.0f
			&& Time->GetHours() < 6.2f && B->GetBars().Energy > 99.0f && FMath::IsNearlyEqual(B->GetBars().Thirst, 25.0f, 1.0f);
	});
}

void UExploredF3dSmokeTest::StartCurrent()
{
	Elapsed = 0.0f;
	if (!Steps[Current].Start())
	{
		UE_LOG(LogExplored, Error, TEXT("[F3dSmoke] FALLO al empezar: %s"), *Steps[Current].Name);
		Finish(false);
	}
}

void UExploredF3dSmokeTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bFinished)
	{
		return;
	}
	if (!bStarted)
	{
		WarmUp += DeltaTime;
		if (WarmUp < F3dSmokeDetail::WarmUpSeconds || !GetPawn() || !F3dSmokeDetail::First<AExploredRaftSite>(GetWorld()))
		{
			return;
		}
		BuildSteps();
		bStarted = true;
		Current = 0;
		StartCurrent();
		return;
	}
	Elapsed += DeltaTime;
	if (Steps[Current].IsDone())
	{
		UE_LOG(LogExplored, Display, TEXT("[F3dSmoke] %s ok (%.1f s)"), *Steps[Current].Name, Elapsed);
		++Current;
		if (Current >= Steps.Num())
		{
			Finish(true);
			return;
		}
		StartCurrent();
		return;
	}
	if (Elapsed > Steps[Current].TimeoutSeconds)
	{
		UE_LOG(LogExplored, Error, TEXT("[F3dSmoke] FALLO por tiempo: %s"), *Steps[Current].Name);
		Finish(false);
	}
}

void UExploredF3dSmokeTest::Finish(bool bOk)
{
	bFinished = true;
	UE_LOG(LogExplored, Display, TEXT("[F3dSmoke] RESULTADO: %s (%d/%d pasos)"), bOk ? TEXT("OK") : TEXT("FALLO"),
		bOk ? Steps.Num() : FMath::Max(0, Current), Steps.Num());
	FGenericPlatformMisc::RequestExit(false);
}
