// Conjunto de capturas «f5» de UExploredShotSubsystem: cada pantalla y elemento de la interfaz de
// Baliza Siete (GDD v3 §9) tal como la ve el jugador. Uso: Tools/shots.ps1 -Set f5 -Width 1920 -Height 1080.

#include "Debug/ExploredShotSubsystem.h"

#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "Explored.h"
#include "Items/ExploredPickup.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/BeaconProgressModel.h"
#include "Narrative/BeaconSubsystem.h"
#include "Narrative/BeaconTexts.h"
#include "Player/ExploredCharacter.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/ExploredHUD.h"
#include "UI/ExploredPlayerController.h"
#include "UI/IslandHudModel.h"
#include "WorldGen/IslandLayout.h"
#include "WorldGen/IslandLayoutLoader.h"

namespace ExploredUiShotsDetail
{
	struct FUiShot
	{
		const TCHAR* Name;
		const TCHAR* Action;
		float Hours;
		int32 Day;
	};

	/** Orden pensado para que cada vista parta del estado que deja la anterior. */
	const FUiShot Shots[] = {
		{ TEXT("f5-menu"), TEXT("ui_menu"), AExploredPlayerController::MenuBackdropHour, 1 },
		{ TEXT("f5-menu-aviso"), TEXT("ui_menu_notice"), AExploredPlayerController::MenuBackdropHour, 1 },
		{ TEXT("f5-ajustes"), TEXT("ui_settings"), AExploredPlayerController::MenuBackdropHour, 1 },
		{ TEXT("f5-cargar"), TEXT("ui_load"), AExploredPlayerController::MenuBackdropHour, 1 },
		{ TEXT("f5-creditos"), TEXT("ui_credits"), AExploredPlayerController::MenuBackdropHour, 1 },
		{ TEXT("f5-dia"), TEXT("ui_daycard"), 7.2f, 2 },
		{ TEXT("f5-inventario"), TEXT("ui_inventory"), 15.6f, 2 },
		{ TEXT("f5-pagina"), TEXT("ui_page"), 15.6f, 2 },
		{ TEXT("f5-cuaderno"), TEXT("ui_notebook"), 15.6f, 2 },
		{ TEXT("f5-hud"), TEXT("ui_hud"), 15.6f, 2 },
		{ TEXT("f5-pausa"), TEXT("ui_pause"), 15.6f, 2 },
		{ TEXT("f5-desmayo"), TEXT("ui_faint"), 15.6f, 2 },
		{ TEXT("f5-final"), TEXT("ui_ending"), 22.5f, 3 },
	};

	AExploredPlayerController* Controller(UWorld* World)
	{
		return Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	}

	AExploredCharacter* Iria(UWorld* World)
	{
		const APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		return PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
	}

	/** Vuelve al juego limpio: sin menús, cuaderno ni mochila abiertos, vista en los ojos de Iria. */
	void BackToPlay(UWorld* World)
	{
		AExploredPlayerController* PC = Controller(World);
		AExploredCharacter* Character = Iria(World);
		if (!PC || !Character)
		{
			return;
		}
		PC->CloseNotebook();
		Character->SetBackpackOpen(false);
		PC->ResumeGame();
		Character->SetActorHiddenInGame(false);
		PC->SetViewTarget(Character);
	}

	/** Iria a 1,5 m de la leña de la Cala Varada, mirándola: el aviso «E · Coger leña» queda a la vista. */
	void FaceCalaWood(UWorld* World)
	{
		AExploredPlayerController* PC = Controller(World);
		AExploredCharacter* Character = Iria(World);
		const FVector Wanted(-98.0 * 100.0, 18.0 * 100.0, 0.0);
		AExploredPickup* Best = nullptr;
		for (TActorIterator<AExploredPickup> It(World); It; ++It)
		{
			if (It->IsAvailable() && (!Best || FVector::Dist2D(It->GetActorLocation(), Wanted) < FVector::Dist2D(Best->GetActorLocation(), Wanted)))
			{
				Best = *It;
			}
		}
		if (!PC || !Character || !Best)
		{
			return;
		}
		const FVector Target = Best->GetActorLocation();
		const FVector Away = FVector(-1.0, 0.35, 0.0).GetSafeNormal();
		const FIslandLayout& Layout = *ExploredIsland::Get();
		const FVector2D Stand(Target.X / 100.0 + Away.X * 1.5, Target.Y / 100.0 + Away.Y * 1.5);
		const float GroundCm = FMath::Max(Layout.SampleColumn(Stand.X, Stand.Y).Height, FIslandLayout::SeaLevel) * 100.0f;
		const FVector Feet(Stand.X * 100.0, Stand.Y * 100.0, GroundCm + 100.0);
		Character->SetActorLocation(Feet, false, nullptr, ETeleportType::TeleportPhysics);
		const FVector Eye = Feet + FVector(0.0, 0.0, 60.0);
		PC->SetControlRotation((Target - Eye).Rotation());
	}

	void StockBackpack(UExploredSurvivorComponent& Survivor)
	{
		const TPair<const TCHAR*, int32> Items[] = {
			{ TEXT("cuchillo"), 1 }, { TEXT("cantimplora"), 1 }, { TEXT("barometro"), 1 }, { TEXT("coco"), 2 },
			{ TEXT("lena"), 3 }, { TEXT("fibra"), 4 }, { TEXT("pez_crudo"), 2 },
		};
		for (const TPair<const TCHAR*, int32>& Item : Items)
		{
			const int32 Missing = Item.Value - Survivor.GetItemCount(FName(Item.Key));
			if (Missing > 0)
			{
				Survivor.TryAddItem(FName(Item.Key), Missing);
			}
		}
	}
}

void UExploredShotSubsystem::BuildUiShots()
{
	for (const ExploredUiShotsDetail::FUiShot& Ui : ExploredUiShotsDetail::Shots)
	{
		FExploredShot Shot;
		Shot.Name = Ui.Name;
		Shot.Action = FName(Ui.Action);
		Shot.Hours = Ui.Hours;
		Shot.Day = Ui.Day;
		Shot.bShowUI = true;
		// Cámara de reserva (las vistas de juego miran por los ojos de Iria): sobre la Cala Varada.
		Shot.Location = FVector(-150.0 * 100.0, -20.0 * 100.0, 30.0 * 100.0);
		Shot.Rotation = FRotator(-12.0f, 20.0f, 0.0f);
		Shots.Add(Shot);
	}
}

bool UExploredShotSubsystem::BeginUiShot(const FExploredShot& Shot)
{
	using namespace ExploredUiShotsDetail;
	const FString Action = Shot.Action.ToString();
	if (!Action.StartsWith(TEXT("ui_")))
	{
		return false;
	}
	UWorld* World = GetWorld();
	AExploredPlayerController* PC = Controller(World);
	AExploredCharacter* Character = Iria(World);
	AExploredHUD* HUD = PC ? Cast<AExploredHUD>(PC->GetHUD()) : nullptr;
	UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(World);
	UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>();
	UExploredSurvivorComponent* Survivor = Character ? Character->GetSurvivorComponent() : nullptr;
	UIslandBodyComponent* Body = Character ? Character->GetIslandBodyComponent() : nullptr;
	if (!PC || !Character || !HUD || !Beacon || !Time || !Survivor || !Body)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Shots] %s: falta el controlador, Iria, el HUD o la historia"), *Shot.Name);
		return true;
	}
	const bool bMenu = Action.StartsWith(TEXT("ui_menu")) || Action == TEXT("ui_settings") || Action == TEXT("ui_load")
		|| Action == TEXT("ui_credits");
	if (bMenu)
	{
		Character->SetActorHiddenInGame(true);
		PC->ShowMainMenuWithNotice(Action == TEXT("ui_menu_notice") ? AExploredPlayerController::ObsoleteSaveNotice() : FText::GetEmpty());
		if (Action == TEXT("ui_settings"))
		{
			PC->OpenSettings();
		}
		else if (Action == TEXT("ui_load"))
		{
			PC->OpenLoadGame();
		}
		else if (Action == TEXT("ui_credits"))
		{
			PC->OpenCredits();
		}
		UE_LOG(LogExplored, Display, TEXT("[Shots] %s: frontend"), *Shot.Name);
		return true;
	}

	// Vistas de juego: partida en marcha en la etapa de la balsa (objetivo de verdad), reloj parado.
	if (Action == TEXT("ui_daycard"))
	{
		PC->StartNewGame();
		Beacon->DebugSetStage(EBeaconStage::Balsa);
	}
	BackToPlay(World);
	Time->SetTime(Shot.Day, Shot.Hours);
	Time->SetTimeScale(0.0f);
	StockBackpack(*Survivor);
	Survivor->SelectQuickSlot(0);
	FaceCalaWood(World);
	HUD->ClearTransient();
	FBodyBars Bars;
	Bars.Thirst = 18.0f;
	Bars.Hunger = 62.0f;
	Bars.Energy = 74.0f;
	Bars.Warmth = 90.0f;
	Body->RestoreBars(Bars, Body->GetLastShelterId());

	const FBeaconTexts& Texts = ExploredBeaconTexts::Get();
	if (Action == TEXT("ui_daycard"))
	{
		HUD->ShowCard(FText::FromString(FIslandHudModel::DayCardText(2)), 30.0f);
	}
	else if (Action == TEXT("ui_hud"))
	{
		HUD->PushNotification(INVTEXT("Recogido: Fibra ×2"), FLinearColor::White, 30.0f);
		HUD->ShowSubtitle(INVTEXT("Si el barómetro sigue bajando, esta noche no se duerme en la playa."), 30.0f);
		Survivor->StartTimedAction(30.0f, []() {});
	}
	else if (Action == TEXT("ui_inventory"))
	{
		Character->SetBackpackOpen(true);
	}
	else if (Action == TEXT("ui_page"))
	{
		Beacon->ReadPage(1);
		Beacon->ReadPage(2);
		HUD->ClearTransient();
		Beacon->ReadPage(3);
	}
	else if (Action == TEXT("ui_notebook"))
	{
		HUD->ClearTransient();
		PC->OpenNotebook();
	}
	else if (Action == TEXT("ui_pause"))
	{
		PC->OpenPauseMenu();
	}
	else if (Action == TEXT("ui_faint"))
	{
		Body->PlayBlackout([]() {}, INVTEXT("Despiertas en el refugio, con la boca seca y la cabeza pesada."));
	}
	else if (Action == TEXT("ui_ending"))
	{
		PC->ShowEnding(Texts.Get(FBeaconProgressModel::EpilogueTextId(EBeaconEpilogue::Complete)),
			Texts.Get(FBeaconProgressModel::CommonEpilogueTextId()));
	}
	UE_LOG(LogExplored, Display, TEXT("[Shots] %s: juego (%s)"), *Shot.Name, *Action);
	return true;
}
