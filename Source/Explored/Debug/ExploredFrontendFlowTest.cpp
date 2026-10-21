#include "Debug/ExploredFrontendFlowTest.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"

#include "Explored.h"
#include "Items/BackpackModel.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/BeaconSubsystem.h"
#include "Player/ExploredCharacter.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/ExploredPlayerController.h"
#include "UI/ExploredSaveSubsystem.h"

namespace FrontendFlowDetail
{
	const TCHAR* const Slot = TEXT("manual2");
	/** Segundos con el menú abierto en los que el reloj y las barras no deben moverse. */
	constexpr float MenuIdleSeconds = 3.0f;
	constexpr float MaxLocationErrorCm = 300.0f;
	constexpr float MaxHoursError = 0.25f;

	float BarsSum(const UIslandBodyComponent* Body)
	{
		const FBodyBars& Bars = Body->GetBars();
		return Bars.Thirst + Bars.Hunger + Bars.Energy + Bars.Warmth;
	}
}

bool UExploredFrontendFlowTest::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("ExploredFrontendFlow"));
#endif
}

TStatId UExploredFrontendFlowTest::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredFrontendFlowTest, STATGROUP_Tickables);
}

AExploredPlayerController* UExploredFrontendFlowTest::Controller() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
}

AExploredCharacter* UExploredFrontendFlowTest::Iria() const
{
	const AExploredPlayerController* PC = Controller();
	return PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
}

bool UExploredFrontendFlowTest::InMode(uint8 Mode) const
{
	const AExploredPlayerController* PC = Controller();
	return PC && Iria() && static_cast<uint8>(PC->GetUIMode()) == Mode;
}

UExploredFrontendFlowTest::FSnapshot UExploredFrontendFlowTest::Take() const
{
	FSnapshot Out;
	const AExploredCharacter* Character = Iria();
	const UWorld* World = Character ? Character->GetWorld() : nullptr;
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(Character);
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	const UExploredSurvivorComponent* Survivor = Character ? Character->GetSurvivorComponent() : nullptr;
	if (!Character || !Beacon || !Time || !Survivor)
	{
		return Out;
	}
	Out.Stage = Beacon->GetStageIndex();
	Out.Pages = Beacon->GetRunState().Notebook.PagesMask;
	Out.Items = FBackpackModel::ToCounts(Survivor->GetBackpack());
	Out.Day = Time->GetDay();
	Out.Hours = Time->GetHours();
	Out.Location = Character->GetActorLocation();
	return Out;
}

bool UExploredFrontendFlowTest::Matches(const FSnapshot& Expected, const FSnapshot& Got) const
{
	using namespace FrontendFlowDetail;
	const bool bItems = Expected.Items.OrderIndependentCompareEqual(Got.Items);
	const bool bClock = Expected.Day == Got.Day && FMath::Abs(Expected.Hours - Got.Hours) <= MaxHoursError;
	const bool bPlace = FVector::Dist(Expected.Location, Got.Location) <= MaxLocationErrorCm;
	UE_LOG(LogExplored, Display, TEXT("[FrontendFlow] Etapa %d/%d · páginas %d/%d · mochila %s · reloj D%d %.2f/D%d %.2f · posición a %.0f cm"),
		Expected.Stage, Got.Stage, Expected.Pages, Got.Pages, bItems ? TEXT("igual") : TEXT("DISTINTA"), Expected.Day, Expected.Hours,
		Got.Day, Got.Hours, FVector::Dist(Expected.Location, Got.Location));
	return Expected.Stage == Got.Stage && Expected.Pages == Got.Pages && bItems && bClock && bPlace;
}

void UExploredFrontendFlowTest::BuildSteps()
{
	using namespace FrontendFlowDetail;
	const uint8 Menu = static_cast<uint8>(EExploredUIMode::Menu);
	const uint8 Playing = static_cast<uint8>(EExploredUIMode::Playing);

	Steps.Add({ TEXT("menú principal al arrancar"), [] { return true; }, [this, Menu] { return InMode(Menu); }, 60.0f });
	Steps.Add({ TEXT("con el menú abierto no corren ni el reloj ni las barras"), [this]
	{
		const UIslandBodyComponent* Body = Iria() ? Iria()->GetIslandBodyComponent() : nullptr;
		const UTimeOfDaySubsystem* Time = Iria() ? Iria()->GetWorld()->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
		MenuBarsSum = Body ? BarsSum(Body) : -1.0f;
		MenuHours = Time ? Time->GetHours() : -1.0f;
		return Body && Time;
	}, [this]
	{
		if (Elapsed < MenuIdleSeconds)
		{
			return false;
		}
		const UIslandBodyComponent* Body = Iria() ? Iria()->GetIslandBodyComponent() : nullptr;
		const UTimeOfDaySubsystem* Time = Iria() ? Iria()->GetWorld()->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
		const bool bStill = Body && Time && FMath::IsNearlyEqual(BarsSum(Body), MenuBarsSum, 0.01f)
			&& FMath::IsNearlyEqual(Time->GetHours(), MenuHours, 0.001f);
		if (!bStill)
		{
			UE_LOG(LogExplored, Error, TEXT("[FrontendFlow] El menú deja correr la partida"));
			Finish(false);
		}
		return bStill;
	}, 10.0f });
	Steps.Add({ TEXT("nueva partida"), [this] { Controller()->StartNewGame(); return true; }, [this, Playing] { return InMode(Playing); } });
	Steps.Add({ TEXT("jugar: etapa, página, mochila, reloj y sitio"), [this]
	{
		AExploredCharacter* Character = Iria();
		UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(Character);
		UTimeOfDaySubsystem* Time = Character->GetWorld()->GetSubsystem<UTimeOfDaySubsystem>();
		UExploredSurvivorComponent* Survivor = Character->GetSurvivorComponent();
		Beacon->DebugSetStage(EBeaconStage::Balsa);
		Beacon->ReadPage(2);
		Survivor->TryAddItem(FName(TEXT("lena")), 2);
		Survivor->TryAddItem(FName(TEXT("fibra")), 3);
		Time->SetTime(2, 10.5f);
		Character->SetActorLocation(Character->GetActorLocation() + FVector(600.0, 300.0, 50.0), false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}, [this] { return Elapsed > 1.5f; } });
	Steps.Add({ TEXT("guardar en manual2"), [this]
	{
		UExploredSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>();
		FString Error;
		Saved = Take();
		const bool bOk = Save && Save->SaveToSlot(Slot, Error);
		if (!bOk)
		{
			UE_LOG(LogExplored, Error, TEXT("[FrontendFlow] No se pudo guardar: %s"), *Error);
		}
		return bOk;
	}, [] { return true; } });
	Steps.Add({ TEXT("volver al menú (recarga el mapa)"), [this] { Controller()->ExitToMainMenu(); return true; },
		[this, Menu] { return Elapsed > 2.0f && InMode(Menu); }, 60.0f });
	Steps.Add({ TEXT("continuar"), [this] { Controller()->ContinueGame(); return true; }, [this, Playing] { return Elapsed > 1.0f && InMode(Playing); } });
	Steps.Add({ TEXT("el estado es el guardado"), [] { return true; }, [this]
	{
		if (Elapsed < 1.0f)
		{
			return false;
		}
		const bool bOk = Matches(Saved, Take());
		if (!bOk)
		{
			Finish(false);
		}
		return bOk;
	} });
}

void UExploredFrontendFlowTest::Tick(float DeltaTime)
{
	if (bFinished)
	{
		return;
	}
	Elapsed += DeltaTime;
	if (Current == INDEX_NONE)
	{
		BuildSteps();
		Current = 0;
		Elapsed = 0.0f;
		Steps[0].Start();
		return;
	}
	FStep& Step = Steps[Current];
	if (Step.IsDone())
	{
		if (bFinished)
		{
			return;
		}
		UE_LOG(LogExplored, Display, TEXT("[FrontendFlow] %s ok (%.1f s)"), *Step.Name, Elapsed);
		if (++Current >= Steps.Num())
		{
			Finish(true);
			return;
		}
		Elapsed = 0.0f;
		if (!Steps[Current].Start())
		{
			UE_LOG(LogExplored, Error, TEXT("[FrontendFlow] FALLO al empezar: %s"), *Steps[Current].Name);
			Finish(false);
		}
		return;
	}
	if (Elapsed > Step.TimeoutSeconds)
	{
		UE_LOG(LogExplored, Error, TEXT("[FrontendFlow] FALLO por tiempo: %s"), *Step.Name);
		Finish(false);
	}
}

void UExploredFrontendFlowTest::Finish(bool bOk)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	UE_LOG(LogExplored, Display, TEXT("[FrontendFlow] RESULTADO: %s (%d/%d pasos)"), bOk ? TEXT("OK") : TEXT("FALLO"),
		bOk ? Steps.Num() : FMath::Max(0, Current), Steps.Num());
	FPlatformMisc::RequestExit(false, TEXT("ExploredFrontendFlow"));
}
