#include "UI/ExploredPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "UI/ExploredGameUserSettings.h"
#include "UI/ExploredInputSettingsSubsystem.h"
#include "UI/ExploredMenuCamera.h"
#include "UI/ExploredSaveSubsystem.h"
#include "UI/Widgets/SExploredCredits.h"
#include "UI/Widgets/SExploredFade.h"
#include "UI/Widgets/SExploredMainMenu.h"
#include "UI/Widgets/SExploredModeSelect.h"
#include "UI/Widgets/SExploredPauseMenu.h"
#include "UI/Widgets/SExploredSavingIndicator.h"
#include "UI/Widgets/SExploredSettingsPanel.h"

namespace ExploredUI
{
	bool ShouldShowCursor(EExploredUIMode Mode)
	{
		return Mode != EExploredUIMode::Playing;
	}

	bool ShouldPauseGame(EExploredUIMode Mode)
	{
		return Mode == EExploredUIMode::Paused;
	}

	EExploredUIMode NextModeOnEscape(EExploredUIMode Current)
	{
		switch (Current)
		{
		case EExploredUIMode::Playing: return EExploredUIMode::Paused;
		case EExploredUIMode::Paused: return EExploredUIMode::Playing;
		default: return Current;
		}
	}
}

AExploredPlayerController::AExploredPlayerController()
{
	bShowMouseCursor = true;
}

void AExploredPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// El frontend es puramente local: en un futuro con red, un PlayerController
	// no local (por ejemplo, en el servidor de un listen server) no tiene
	// viewport propio y no debe tocar Slate ni el cursor de otra máquina.
	if (!IsLocalController())
	{
		return;
	}

	FadeWidget = SNew(SExploredFade);
	SavingIndicator = SNew(SExploredSavingIndicator);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->AddViewportWidgetContent(SavingIndicator.ToSharedRef(), 900);
		Viewport->AddViewportWidgetContent(FadeWidget.ToSharedRef(), 1000);
	}

	if (UExploredSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>() : nullptr)
	{
		SaveSubsystem->OnSaveCompleted.AddLambda([this]()
		{
			if (SavingIndicator.IsValid())
			{
				SavingIndicator->Show();
			}
		});
	}

	const bool bSkipMenu = FParse::Param(FCommandLine::Get(), TEXT("SkipMenu"));
	if (bSkipMenu)
	{
		SetUIMode(EExploredUIMode::Playing);
	}
	else
	{
		SetUIMode(EExploredUIMode::Menu);
		ShowMainMenu();
	}
}

void AExploredPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Meta-entrada del controlador (no pasa por Enhanced Input, que es del personaje).
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AExploredPlayerController::HandleEscape);
}

void AExploredPlayerController::HandleEscape()
{
	const EExploredUIMode Next = ExploredUI::NextModeOnEscape(UIMode);
	if (Next == UIMode)
	{
		return;
	}
	if (Next == EExploredUIMode::Paused)
	{
		OpenPauseMenu();
	}
	else if (Next == EExploredUIMode::Playing)
	{
		ResumeGame();
	}
}

void AExploredPlayerController::SetUIMode(EExploredUIMode NewMode)
{
	UIMode = NewMode;
	bShowMouseCursor = ExploredUI::ShouldShowCursor(NewMode);
	SetPause(ExploredUI::ShouldPauseGame(NewMode));

	if (NewMode == EExploredUIMode::Playing)
	{
		SetInputMode(FInputModeGameOnly());
		if (APawn* ControlledPawn = GetPawn())
		{
			SetViewTarget(ControlledPawn);
		}
	}
	else
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		if (CurrentOverlay.IsValid())
		{
			InputMode.SetWidgetToFocus(CurrentOverlay);
		}
		SetInputMode(InputMode);
		SetViewTarget(FindOrSpawnMenuCamera());
	}
}

AExploredMenuCamera* AExploredPlayerController::FindOrSpawnMenuCamera()
{
	if (MenuCamera.IsValid())
	{
		return MenuCamera.Get();
	}
	for (TActorIterator<AExploredMenuCamera> It(GetWorld()); It; ++It)
	{
		MenuCamera = *It;
		return MenuCamera.Get();
	}
	AExploredMenuCamera* Spawned = GetWorld()->SpawnActor<AExploredMenuCamera>();
	MenuCamera = Spawned;
	return Spawned;
}

void AExploredPlayerController::ShowOverlay(TSharedRef<SWidget> Widget)
{
	HideOverlay();
	CurrentOverlay = Widget;
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->AddViewportWidgetContent(Widget, 500);
	}
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void AExploredPlayerController::HideOverlay()
{
	if (CurrentOverlay.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(CurrentOverlay.ToSharedRef());
		}
		CurrentOverlay.Reset();
	}
}

void AExploredPlayerController::ShowMainMenu()
{
	UExploredSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
	const bool bCanContinue = SaveSubsystem && SaveSubsystem->HasSaveGame();

	TSharedRef<SExploredMainMenu> Menu = SNew(SExploredMainMenu)
		.bCanContinue(bCanContinue)
		.OnContinue(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::ContinueGame))
		.OnNewGame(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::OpenModeSelect))
		.OnSettings(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::OpenSettings))
		.OnCredits(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::OpenCredits))
		.OnQuit(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::QuitToDesktop));
	ShowOverlay(Menu);
}

void AExploredPlayerController::ContinueGame()
{
	// TODO(equipo de guardado): sustituir por la carga real del USaveGame
	// (semilla, deltas del mundo, jugador y progreso) antes de entrar a jugar.
	if (!FadeWidget.IsValid())
	{
		SetUIMode(EExploredUIMode::Playing);
		HideOverlay();
		return;
	}
	FadeWidget->FadeToBlack(0.6f, [this]()
	{
		HideOverlay();
		SetUIMode(EExploredUIMode::Playing);
		FadeWidget->FadeFromBlack(0.6f);
	});
}

void AExploredPlayerController::OpenModeSelect()
{
	TSharedRef<SExploredModeSelect> Widget = SNew(SExploredModeSelect)
		.OnChosen(FOnExploredModeChosen::CreateUObject(this, &AExploredPlayerController::StartNewGame))
		.OnBack(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::ShowMainMenu));
	ShowOverlay(Widget);
}

void AExploredPlayerController::StartNewGame(EExploredGameplayMode Mode)
{
	// El personaje ya está generado y posee su PlayerStart desde el flujo
	// normal de AGameModeBase; aquí solo se retira el menú y se entra a
	// jugar. El modo (Explorador/Superviviente/Náufrago) queda anotado para
	// que el equipo de Survival lo lea (GDD §7); de momento solo se registra.
	UE_LOG(LogTemp, Display, TEXT("[Explored] Nueva partida, modo %d"), static_cast<int32>(Mode));

	if (!FadeWidget.IsValid())
	{
		HideOverlay();
		SetUIMode(EExploredUIMode::Playing);
		return;
	}
	FadeWidget->FadeToBlack(0.6f, [this]()
	{
		HideOverlay();
		SetUIMode(EExploredUIMode::Playing);
		FadeWidget->FadeFromBlack(0.6f);
	});
}

void AExploredPlayerController::OpenSettings()
{
	bSettingsOpenedFromPause = (UIMode == EExploredUIMode::Paused);

	UExploredGameUserSettings* Settings = Cast<UExploredGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	UExploredInputSettingsSubsystem* InputSettings = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UExploredInputSettingsSubsystem>() : nullptr;

	TSharedRef<SExploredSettingsPanel> Widget = SNew(SExploredSettingsPanel)
		.Settings(Settings)
		.InputSettings(InputSettings)
		.WorldContextObject(this)
		.OnBack(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::CloseSettings));
	ShowOverlay(Widget);
}

void AExploredPlayerController::CloseSettings()
{
	if (bSettingsOpenedFromPause)
	{
		OpenPauseMenu();
	}
	else
	{
		ShowMainMenu();
	}
}

void AExploredPlayerController::OpenCredits()
{
	TSharedRef<SExploredCredits> Widget = SNew(SExploredCredits)
		.OnBack(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::CloseCredits));
	ShowOverlay(Widget);
}

void AExploredPlayerController::CloseCredits()
{
	ShowMainMenu();
}

void AExploredPlayerController::OpenPauseMenu()
{
	SetUIMode(EExploredUIMode::Paused);
	TSharedRef<SExploredPauseMenu> Widget = SNew(SExploredPauseMenu)
		.OnResume(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::ResumeGame))
		.OnSettings(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::OpenSettings))
		.OnSave(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::RequestSaveGame))
		.OnExitToMenu(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::ExitToMainMenu))
		.OnQuit(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::QuitToDesktop));
	ShowOverlay(Widget);
}

void AExploredPlayerController::ResumeGame()
{
	HideOverlay();
	SetUIMode(EExploredUIMode::Playing);
}

void AExploredPlayerController::RequestSaveGame()
{
	if (UExploredSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>() : nullptr)
	{
		SaveSubsystem->RequestSave();
	}
}

void AExploredPlayerController::ExitToMainMenu()
{
	HideOverlay();
	UGameplayStatics::OpenLevel(this, MainMenuMapName);
}

void AExploredPlayerController::QuitToDesktop()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
