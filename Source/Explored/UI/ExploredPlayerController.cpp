#include "UI/ExploredPlayerController.h"

#include "Achievements/AchievementsSubsystem.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "UI/ExploredGameUserSettings.h"
#include "UI/ExploredInputSettingsSubsystem.h"
#include "UI/ExploredMenuCamera.h"
#include "UI/ExploredSaveSubsystem.h"
#include "UI/Widgets/SExploredAchievementToast.h"
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

	// H7: al arrancar el motor los ajustes se aplican antes de que exista un
	// mundo; aquí se completa lo que depende de él (audio, duración del día,
	// ambiente) y se re-aplica lo visual por si el mapa se abrió desde el editor.
	if (UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get())
	{
		Settings->ApplyPreviewSettings(!GIsEditor);
		Settings->ApplyToWorld(this);
	}

	FadeWidget = SNew(SExploredFade);
	SavingIndicator = SNew(SExploredSavingIndicator);
	AchievementToast = SNew(SExploredAchievementToast);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->AddViewportWidgetContent(SavingIndicator.ToSharedRef(), 900);
		Viewport->AddViewportWidgetContent(AchievementToast.ToSharedRef(), 950);
		Viewport->AddViewportWidgetContent(FadeWidget.ToSharedRef(), 1000);
	}

	if (UExploredSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>() : nullptr)
	{
		SaveSubsystemBound = SaveSubsystem;
		SaveCompletedHandle = SaveSubsystem->OnSaveCompleted.AddWeakLambda(this, [this]()
		{
			if (SavingIndicator.IsValid())
			{
				SavingIndicator->Show();
			}
		});
	}

	if (UAchievementsSubsystem* Achievements = UAchievementsSubsystem::Get(this))
	{
		TWeakObjectPtr<UAchievementsSubsystem> WeakAchievements(Achievements);
		AchievementsBound = Achievements;
		AchievementUnlockedHandle = Achievements->OnAchievementUnlocked.AddWeakLambda(this, [this, WeakAchievements](FName AchievementId)
		{
			const UAchievementsSubsystem* Source = WeakAchievements.Get();
			if (Source && AchievementToast.IsValid())
			{
				AchievementToast->Enqueue(Source->GetDisplayName(AchievementId), Source->GetDescription(AchievementId));
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

void AExploredPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// M8: el subsistema de guardado sobrevive a OpenLevel; sin quitar el handle
	// se acumulaba una suscripción por cada carga de mapa.
	if (UExploredSaveSubsystem* SaveSubsystem = SaveSubsystemBound.Get())
	{
		SaveSubsystem->OnSaveCompleted.Remove(SaveCompletedHandle);
	}
	SaveSubsystemBound.Reset();
	SaveCompletedHandle.Reset();
	if (UAchievementsSubsystem* Achievements = AchievementsBound.Get())
	{
		Achievements->OnAchievementUnlocked.Remove(AchievementUnlockedHandle);
	}
	AchievementsBound.Reset();
	AchievementUnlockedHandle.Reset();

	// M8: los widgets añadidos al viewport no son del mundo; si no se retiran,
	// se quedan pintados (y referenciados) tras parar PIE o cambiar de mapa.
	HideOverlay();
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		if (FadeWidget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(FadeWidget.ToSharedRef());
		}
		if (SavingIndicator.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(SavingIndicator.ToSharedRef());
		}
		if (AchievementToast.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(AchievementToast.ToSharedRef());
		}
	}
	FadeWidget.Reset();
	SavingIndicator.Reset();
	AchievementToast.Reset();
	CurrentFocusTarget.Reset();

	Super::EndPlay(EndPlayReason);
}

void AExploredPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Meta-entrada del controlador (no pasa por Enhanced Input, que es del personaje).
	// Jugando, Escape o Start abren la pausa. En la pausa y en los menús la
	// entrada es FInputModeUIOnly y la tecla la atiende el widget con foco;
	// bExecuteWhenPaused cubre el caso de que llegue aquí con el juego pausado.
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AExploredPlayerController::HandleEscape).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &AExploredPlayerController::HandleEscape).bExecuteWhenPaused = true;
}

void AExploredPlayerController::HandleEscape()
{
	// Ajustes descarta lo no aplicado al volver (M11): se pasa por el panel.
	if (CurrentScreen == ExploredSettingsLogic::EMenuScreen::Settings)
	{
		if (TSharedPtr<SExploredSettingsPanel> Panel = SettingsPanel.Pin())
		{
			Panel->RequestBack();
			return;
		}
	}

	const ExploredSettingsLogic::EMenuScreen Next = ExploredSettingsLogic::ScreenAfterEscape(CurrentScreen, bSettingsOpenedFromPause);
	if (Next != CurrentScreen)
	{
		NavigateTo(Next);
	}
}

void AExploredPlayerController::NavigateBack()
{
	const ExploredSettingsLogic::EMenuScreen Next = ExploredSettingsLogic::ScreenAfterBack(CurrentScreen, bSettingsOpenedFromPause);
	if (Next != CurrentScreen)
	{
		NavigateTo(Next);
	}
}

void AExploredPlayerController::NavigateTo(ExploredSettingsLogic::EMenuScreen Screen)
{
	using ExploredSettingsLogic::EMenuScreen;
	switch (Screen)
	{
	case EMenuScreen::None: ResumeGame(); break;
	case EMenuScreen::MainMenu: ShowMainMenu(); break;
	case EMenuScreen::ModeSelect: OpenModeSelect(); break;
	case EMenuScreen::Settings: OpenSettings(); break;
	case EMenuScreen::Credits: OpenCredits(); break;
	case EMenuScreen::Pause: OpenPauseMenu(); break;
	default: break;
	}
}

void AExploredPlayerController::SetUIMode(EExploredUIMode NewMode)
{
	UIMode = NewMode;
	bShowMouseCursor = ExploredUI::ShouldShowCursor(NewMode);
	SetPause(ExploredUI::ShouldPauseGame(NewMode));

	if (NewMode == EExploredUIMode::Playing)
	{
		CurrentScreen = ExploredSettingsLogic::EMenuScreen::None;
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
		if (CurrentFocusTarget.IsValid())
		{
			InputMode.SetWidgetToFocus(CurrentFocusTarget);
		}
		else if (CurrentOverlay.IsValid())
		{
			InputMode.SetWidgetToFocus(CurrentOverlay);
		}
		SetInputMode(InputMode);
		// En pausa se congela la vista del juego: la camara de menu solo tiene
		// sentido en el menu principal (con el mundo pausado ni siquiera se colocaria).
		if (NewMode == EExploredUIMode::Menu)
		{
			SetViewTarget(FindOrSpawnMenuCamera());
		}
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

void AExploredPlayerController::ShowOverlay(TSharedRef<SWidget> Widget, TSharedPtr<SWidget> FocusTarget, ExploredSettingsLogic::EMenuScreen Screen)
{
	HideOverlay();
	CurrentOverlay = Widget;
	CurrentScreen = Screen;
	// H6: el foco va al primer control para que las flechas, el d-pad y
	// Aceptar funcionen desde el principio; si no hay, al propio overlay, que
	// acepta foco y atiende Escape/B.
	CurrentFocusTarget = FocusTarget.IsValid() ? FocusTarget : TSharedPtr<SWidget>(Widget);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->AddViewportWidgetContent(Widget, 500);
	}
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(CurrentFocusTarget);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void AExploredPlayerController::HideOverlay()
{
	if (CurrentOverlay.IsValid())
	{
		UWorld* World = GetWorld();
		if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(CurrentOverlay.ToSharedRef());
		}
		CurrentOverlay.Reset();
	}
	CurrentFocusTarget.Reset();
	SettingsPanel.Reset();
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
	ShowOverlay(Menu, Menu->GetInitialFocus(), ExploredSettingsLogic::EMenuScreen::MainMenu);
}

void AExploredPlayerController::FadeIntoGameplay()
{
	if (!FadeWidget.IsValid())
	{
		HideOverlay();
		SetUIMode(EExploredUIMode::Playing);
		return;
	}
	// M8: el PC puede destruirse durante el fundido (parar PIE, cambiar de mapa);
	// la lambda comprueba un puntero débil en vez de capturar this crudo.
	const TWeakObjectPtr<AExploredPlayerController> WeakThis(this);
	FadeWidget->FadeToBlack(0.6f, [WeakThis]()
	{
		AExploredPlayerController* PC = WeakThis.Get();
		if (!PC)
		{
			return;
		}
		PC->HideOverlay();
		PC->SetUIMode(EExploredUIMode::Playing);
		if (PC->FadeWidget.IsValid())
		{
			PC->FadeWidget->FadeFromBlack(0.6f);
		}
	});
}

void AExploredPlayerController::ContinueGame()
{
	// Carga la ranura más reciente: cada sistema recibe su sección y el
	// personaje vuelve a su posición (ver UExploredSaveSubsystem). Si no hay
	// ninguna legible, se entra a jugar tal cual.
	if (UExploredSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>() : nullptr)
	{
		SaveSubsystem->LoadContinueGame();
	}
	FadeIntoGameplay();
}

void AExploredPlayerController::OpenModeSelect()
{
	TSharedRef<SExploredModeSelect> Widget = SNew(SExploredModeSelect)
		.OnChosen(FOnExploredModeChosen::CreateUObject(this, &AExploredPlayerController::StartNewGame))
		.OnBack(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::NavigateBack));
	ShowOverlay(Widget, Widget->GetInitialFocus(), ExploredSettingsLogic::EMenuScreen::ModeSelect);
}

void AExploredPlayerController::StartNewGame(EExploredGameplayMode Mode)
{
	// El personaje ya está generado y posee su PlayerStart desde el flujo
	// normal de AGameModeBase; aquí solo se retira el menú y se entra a
	// jugar. El modo (Explorador/Superviviente/Náufrago) queda anotado para
	// que el equipo de Survival lo lea (GDD §7); de momento solo se registra.
	UE_LOG(LogTemp, Display, TEXT("[Explored] Nueva partida, modo %d"), static_cast<int32>(Mode));
	if (UAchievementsSubsystem* Achievements = UAchievementsSubsystem::Get(this))
	{
		// Vacía las estadísticas de partida y fija el modo para logros como «Náufrago de verdad».
		Achievements->BeginRun(Mode);
	}
	FadeIntoGameplay();
}

void AExploredPlayerController::OpenSettings()
{
	// Se recuerda desde dónde se abrió para que «Volver» regrese allí.
	if (CurrentScreen != ExploredSettingsLogic::EMenuScreen::Settings)
	{
		bSettingsOpenedFromPause = (UIMode == EExploredUIMode::Paused);
	}

	UExploredGameUserSettings* Settings = Cast<UExploredGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	UExploredInputSettingsSubsystem* InputSettings = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UExploredInputSettingsSubsystem>() : nullptr;

	TSharedRef<SExploredSettingsPanel> Widget = SNew(SExploredSettingsPanel)
		.Settings(Settings)
		.InputSettings(InputSettings)
		.WorldContextObject(this)
		.OnBack(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::CloseSettings));
	ShowOverlay(Widget, Widget->GetInitialFocus(), ExploredSettingsLogic::EMenuScreen::Settings);
	SettingsPanel = Widget;
}

void AExploredPlayerController::CloseSettings()
{
	NavigateBack();
}

void AExploredPlayerController::OpenCredits()
{
	TSharedRef<SExploredCredits> Widget = SNew(SExploredCredits)
		.OnBack(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::CloseCredits));
	ShowOverlay(Widget, Widget->GetInitialFocus(), ExploredSettingsLogic::EMenuScreen::Credits);
}

void AExploredPlayerController::CloseCredits()
{
	NavigateBack();
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
	ShowOverlay(Widget, Widget->GetInitialFocus(), ExploredSettingsLogic::EMenuScreen::Pause);
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
