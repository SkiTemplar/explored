#include "UI/ExploredPlayerController.h"

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
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "Audio/ExploredMusicSubsystem.h"
#include "Core/ExploredWiringSubsystem.h"
#include "Narrative/BeaconSubsystem.h"
#include "Narrative/FinaleSequenceModel.h"
#include "Player/ExploredCharacter.h"
#include "Survival/BodySignalsComponent.h"
#include "UI/ExploredGameUserSettings.h"
#include "UI/ExploredHUD.h"
#include "UI/ExploredInputSettingsSubsystem.h"
#include "UI/ExploredMenuCamera.h"
#include "UI/ExploredSaveSubsystem.h"
#include "UI/ScreensLogic.h"
#include "UI/Widgets/SExploredCredits.h"
#include "UI/Widgets/SExploredEnding.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "UI/Widgets/SExploredFade.h"
#include "UI/Widgets/SExploredInventoryPanel.h"
#include "UI/Widgets/SExploredMainMenu.h"
#include "UI/Widgets/SExploredNotebook.h"
#include "Narrative/BeaconTexts.h"
#include "Narrative/NotebookModel.h"
#include "UI/Widgets/SExploredPauseMenu.h"
#include "UI/Widgets/SExploredSaveSlots.h"
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
	InventoryPanel = SNew(SExploredInventoryPanel).Owner(TWeakObjectPtr<APlayerController>(this));
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		// Bajo los menús (500): la mochila abierta no tapa la pausa.
		Viewport->AddViewportWidgetContent(InventoryPanel.ToSharedRef(), 400);
		Viewport->AddViewportWidgetContent(FadeWidget.ToSharedRef(), 1000);
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
		if (InventoryPanel.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(InventoryPanel.ToSharedRef());
		}
	}
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr; Viewport && NotebookWidget.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(NotebookWidget.ToSharedRef());
	}
	NotebookWidget.Reset();
	FadeWidget.Reset();
	InventoryPanel.Reset();
	CurrentFocusTarget.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(EndCreditsTicker);
	EndCreditsTicker.Reset();

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
	InputComponent->BindKey(EKeys::J, IE_Pressed, this, &AExploredPlayerController::ToggleNotebook).bExecuteWhenPaused = true;
}

void AExploredPlayerController::HandleEscape()
{
	// Ni la cámara del final ni el epílogo (que quita bCutsceneActive) ni los créditos abren la pausa:
	// HideOverlay borraría su widget y al reanudar la cámara quedaría fija sin control.
	if (!FFinaleSequenceModel::CanOpenPause(bCutsceneActive, bShowingEnding))
	{
		return;
	}
	if (IsNotebookOpen())
	{
		CloseNotebook();
		return;
	}
	using ExploredSettingsLogic::EMenuScreen;
	// Ajustes descarta lo no aplicado al volver (M11): se pasa por el panel.
	if (Navigation.Current() == EMenuScreen::Settings)
	{
		if (TSharedPtr<SExploredSettingsPanel> Panel = SettingsPanel.Pin())
		{
			Panel->RequestBack();
			return;
		}
	}

	const EMenuScreen Next = Navigation.EscapeTarget();
	if (Next != Navigation.Current())
	{
		NavigateTo(Next);
	}
}

void AExploredPlayerController::NavigateBack()
{
	const ExploredSettingsLogic::EMenuScreen Next = Navigation.BackTarget();
	if (Next != Navigation.Current())
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
	case EMenuScreen::Settings: OpenSettings(); break;
	case EMenuScreen::Credits: OpenCredits(); break;
	case EMenuScreen::Pause: OpenPauseMenu(); break;
	// Cargar desde el menú principal, Guardar desde la partida: lo dice la raíz de la pila.
	case EMenuScreen::SaveSlots: ShowSaveSlots(Navigation.Root() == EMenuScreen::MainMenu); break;
	default: break;
	}
}

void AExploredPlayerController::SetUIMode(EExploredUIMode NewMode)
{
	UIMode = NewMode;
	bShowMouseCursor = ExploredUI::ShouldShowCursor(NewMode);
	SetPause(ExploredUI::ShouldPauseGame(NewMode));
	// En el menú principal el mundo sigue vivo detrás (mar, nubes, cámara), pero la partida no corre.
	FreezeWorldClock(NewMode == EExploredUIMode::Menu);

	if (NewMode == EExploredUIMode::Playing)
	{
		Navigation.Reset(ExploredSettingsLogic::EMenuScreen::None);
		SetInputMode(FInputModeGameOnly());
		if (APawn* ControlledPawn = GetPawn())
		{
			SetViewTarget(ControlledPawn);
		}
		// Al volver de la pausa con el inventario abierto, el cursor sigue haciendo falta.
		if (const AExploredCharacter* PlayerCharacter = Cast<AExploredCharacter>(GetPawn()))
		{
			ApplyBackpackInputMode(PlayerCharacter->IsBackpackOpen());
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

void AExploredPlayerController::ApplyBackpackInputMode(bool bOpen)
{
	if (UIMode != EExploredUIMode::Playing)
	{
		return;
	}
	bShowMouseCursor = bOpen;
	if (!bOpen)
	{
		SetInputMode(FInputModeGameOnly());
		return;
	}
	// Sin widget con foco: el teclado sigue en el juego (Tab cierra, WASD mueve) y los botones
	// del inventario no son enfocables, así que un clic no se lleva el teclado.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void AExploredPlayerController::FreezeWorldClock(bool bFreeze)
{
	UTimeOfDaySubsystem* Time = GetWorld() ? GetWorld()->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	if (!Time || bFreeze == (FrozenTimeScale >= 0.0f))
	{
		return;
	}
	if (bFreeze)
	{
		FrozenTimeScale = Time->GetTimeScale();
		Time->SetTimeScale(0.0f);
		return;
	}
	Time->SetTimeScale(FrozenTimeScale);
	FrozenTimeScale = -1.0f;
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
	if (!Navigation.Open(Screen))
	{
		// No debería ocurrir (los botones solo ofrecen transiciones de la tabla): se registra y
		// se rehace el camino desde la raíz para que «Volver» siga llevando a algún sitio.
		UE_LOG(LogTemp, Warning, TEXT("[Explored] Transición de pantalla no prevista: %d -> %d"),
			static_cast<int32>(Navigation.Current()), static_cast<int32>(Screen));
		Navigation.Reset(Navigation.Root());
		Navigation.Open(Screen);
	}
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
	// Una partida de la historia anterior (formato 1) no se carga: se explica en vez de fallar en silencio.
	if (MenuNotice.IsEmpty() && SaveSubsystem && SaveSubsystem->HasObsoleteSave())
	{
		MenuNotice = ObsoleteSaveNotice();
	}
	const FText Notice = MenuNotice;
	// Fondo: la isla al atardecer con la baliza apagada (el reloj queda congelado ahí hasta jugar).
	if (UTimeOfDaySubsystem* Time = GetWorld() ? GetWorld()->GetSubsystem<UTimeOfDaySubsystem>() : nullptr)
	{
		Time->SetTime(1, MenuBackdropHour);
	}

	Navigation.Reset(ExploredSettingsLogic::EMenuScreen::MainMenu);
	TSharedRef<SExploredMainMenu> Menu = SNew(SExploredMainMenu)
		.bCanContinue(bCanContinue)
		.Notice(Notice)
		.OnContinue(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::ContinueGame))
		.OnNewGame(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::StartNewGame))
		.OnLoad(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::OpenLoadGame))
		.OnSettings(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::OpenSettings))
		.OnCredits(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::OpenCredits))
		.OnQuit(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::QuitToDesktop));
	ShowOverlay(Menu, Menu->GetInitialFocus(), ExploredSettingsLogic::EMenuScreen::MainMenu);
}

FText AExploredPlayerController::ObsoleteSaveNotice()
{
	return NSLOCTEXT("ExploredUI", "MenuObsoleteSave",
		"Hay una partida de una versión anterior del juego. No se puede continuar: empieza una nueva.");
}

void AExploredPlayerController::ShowMainMenuWithNotice(const FText& Notice)
{
	MenuNotice = Notice;
	SetUIMode(EExploredUIMode::Menu);
	ShowMainMenu();
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
	// Carga la ranura más reciente: cada sistema recibe su sección y el personaje vuelve a su
	// posición (ver UExploredSaveSubsystem). Si falla, se queda en el menú y lo explica.
	UExploredSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
	if (!SaveSubsystem || !SaveSubsystem->LoadContinueGame())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] Continuar: no se pudo cargar la partida"));
		ShowMainMenuWithNotice(NSLOCTEXT("ExploredUI", "ContinueFailed",
			"No se ha podido cargar la partida guardada. Prueba con «Cargar» o empieza una nueva."));
		return;
	}
	MenuNotice = FText::GetEmpty();
	if (AExploredHUD* ExploredHUD = Cast<AExploredHUD>(GetHUD()))
	{
		ExploredHUD->ClearTransient();
	}
	FadeIntoGameplay();
}

void AExploredPlayerController::StartNewGame()
{
	// El personaje ya está generado y posee su PlayerStart desde el flujo
	// normal de AGameModeBase; aquí se reinicia el estado de la partida y se
	// retira el menú para entrar a jugar.
	UE_LOG(LogTemp, Display, TEXT("[Explored] Nueva partida"));
	MenuNotice = FText::GetEmpty();
	if (AExploredHUD* ExploredHUD = Cast<AExploredHUD>(GetHUD()))
	{
		ExploredHUD->ClearTransient();
	}
	// La partida empieza con el reloj en marcha (BeginNewRun lo pone en el día 1, 07:00).
	FreezeWorldClock(false);
	if (const APawn* PlayerPawn = GetPawn())
	{
		if (UBodySignalsComponent* Body = PlayerPawn->FindComponentByClass<UBodySignalsComponent>())
		{
			Body->SetMode(ESurvivalMode::Survivor);
		}
	}
	// La historia primero: pone el reloj en el día 1 a las 07:00 y los enlaces cuentan desde ahí.
	if (UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this))
	{
		Beacon->BeginNewRun();
	}
	if (UExploredWiringSubsystem* Wiring = UExploredWiringSubsystem::Get(this))
	{
		Wiring->BeginNewRun();
	}
	FadeIntoGameplay();
	// Ayuda de inicio: los controles del GDD v3 §9.2 (el objetivo ya lo enseña el HUD).
	if (AExploredHUD* ExploredHUD = Cast<AExploredHUD>(GetHUD()))
	{
		// Los avisos se apilan hacia arriba desde las barras: el último empujado queda abajo.
		ExploredHUD->PushNotification(NSLOCTEXT("ExploredUI", "HelpMove", "WASD moverse · Espacio saltar · Mayús correr"), FLinearColor::White, 10.0f);
		ExploredHUD->PushNotification(NSLOCTEXT("ExploredUI", "HelpUse", "E interactuar · Tab mochila · J cuaderno · Esc pausa"), FLinearColor::White, 10.0f);
	}
}

void AExploredPlayerController::OpenSettings()
{
	// «Volver» regresa a donde se abrió: lo recuerda la pila de Navigation.
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
	SetCreditsMusic(true);
}

void AExploredPlayerController::CloseCredits()
{
	SetCreditsMusic(false);
	NavigateBack();
}

void AExploredPlayerController::SetCreditsMusic(bool bCredits) const
{
	if (UExploredMusicSubsystem* Music = GetWorld() ? GetWorld()->GetSubsystem<UExploredMusicSubsystem>() : nullptr)
	{
		Music->SetCredits(bCredits);
	}
}

void AExploredPlayerController::OpenPauseMenu()
{
	SetUIMode(EExploredUIMode::Paused);
	const UExploredSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
	TSharedRef<SExploredPauseMenu> Widget = SNew(SExploredPauseMenu)
		.bConfirmExit(!Saves || Saves->HasUnsavedProgress())
		.OnResume(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::ResumeGame))
		.OnSettings(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::OpenSettings))
		.OnSave(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::OpenSaveGame))
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

void AExploredPlayerController::OpenSaveGame()
{
	ShowSaveSlots(false);
}

void AExploredPlayerController::OpenLoadGame()
{
	ShowSaveSlots(true);
}

void AExploredPlayerController::ShowSaveSlots(bool bLoading)
{
	UExploredSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
	TSharedRef<SExploredSaveSlots> Widget = SNew(SExploredSaveSlots)
		.SaveSubsystem(TWeakObjectPtr<UExploredSaveSubsystem>(SaveSubsystem))
		.Mode(bLoading ? ExploredScreens::ESaveSlotsMode::Load : ExploredScreens::ESaveSlotsMode::Save)
		.OnLoadSlot(FOnExploredSlotChosen::CreateUObject(this, &AExploredPlayerController::LoadSlotAndPlay))
		.OnBack(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::NavigateBack));
	ShowOverlay(Widget, Widget->GetInitialFocus(), ExploredSettingsLogic::EMenuScreen::SaveSlots);
}

void AExploredPlayerController::LoadSlotAndPlay(const FString& SlotId)
{
	UExploredSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
	if (!SaveSubsystem)
	{
		return;
	}
	FString Error;
	if (SaveSubsystem->LoadFromSlot(SlotId, Error) != ESaveLoadResult::Ok)
	{
		// El selector solo ofrece ranuras legibles; si falla aquí (el fichero cambió entre medias)
		// se queda en el selector y el registro explica el motivo.
		UE_LOG(LogTemp, Warning, TEXT("[Explored] No se pudo cargar %s: %s"), *SlotId, *Error);
		return;
	}
	MenuNotice = FText::GetEmpty();
	if (AExploredHUD* ExploredHUD = Cast<AExploredHUD>(GetHUD()))
	{
		ExploredHUD->ClearTransient();
	}
	FadeIntoGameplay();
}

void AExploredPlayerController::ExitToMainMenu()
{
	HideOverlay();
	UGameplayStatics::OpenLevel(this, MainMenuMapName);
}

void AExploredPlayerController::ShowEnding(const FText& EpilogueLine, const FText& CommonLine)
{
	const TWeakObjectPtr<AExploredPlayerController> WeakThis(this);
	auto Show = [WeakThis, EpilogueLine, CommonLine]()
	{
		AExploredPlayerController* PC = WeakThis.Get();
		if (!PC)
		{
			return;
		}
		// El final no pasa por la pila de navegación: solo sale de él «Continuar» (a los créditos).
		PC->SetCutsceneActive(false);
		PC->HideOverlay();
		PC->SetUIMode(EExploredUIMode::Paused);
		TSharedRef<SExploredEnding> Widget = SNew(SExploredEnding)
			.EpilogueLine(EpilogueLine)
			.CommonLine(CommonLine)
			.OnContinue(FSimpleDelegate::CreateUObject(PC, &AExploredPlayerController::ShowEndCredits));
		PC->CurrentOverlay = Widget;
		PC->CurrentFocusTarget = Widget->GetInitialFocus();
		if (UGameViewportClient* Viewport = PC->GetWorld()->GetGameViewport())
		{
			Viewport->AddViewportWidgetContent(Widget, 500);
		}
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(PC->CurrentFocusTarget);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		if (PC->FadeWidget.IsValid())
		{
			PC->FadeWidget->FadeFromBlack(PC->GetFinaleTiming().FadeFromBlackSeconds);
		}
		PC->bShowingEnding = true;
		UE_LOG(LogTemp, Display, TEXT("[Explored] Pantalla de epílogo"));
	};
	if (FadeWidget.IsValid())
	{
		FadeWidget->FadeToBlack(GetFinaleTiming().FadeToBlackSeconds, Show);
	}
	else
	{
		Show();
	}
}

void AExploredPlayerController::ShowEndCredits()
{
	HideOverlay();
	SetCreditsMusic(true);
	const FFinaleTiming Timing = GetFinaleTiming();
	TSharedRef<SExploredCredits> Widget = SNew(SExploredCredits)
		.RollSeconds(Timing.EndCreditsSeconds)
		.OnBack(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::ExitToMainMenu));
	CurrentOverlay = Widget;
	CurrentFocusTarget = Widget->GetInitialFocus();
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->AddViewportWidgetContent(Widget, 500);
	}
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(CurrentFocusTarget);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	// El final deja el mundo en pausa: el temporizador va en el ticker del motor, no en el del mundo.
	// Cada fotograma (el delta del ticker es el del fotograma, no el del intervalo): el paso lo decide
	// el modelo con los tiempos del final de UBeaconSubsystem (EndCreditsSeconds).
	FTSTicker::GetCoreTicker().RemoveTicker(EndCreditsTicker);
	EndCreditsElapsed = 0.0f;
	EndCreditsTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, Timing](float DeltaSeconds)
	{
		EndCreditsElapsed += DeltaSeconds;
		if (FFinaleSequenceModel::CreditsStep(Timing, EndCreditsElapsed, false) == EFinaleCreditsStep::Stay)
		{
			return true;
		}
		EndCreditsTicker.Reset();
		ExitToMainMenu();
		return false;
	}));
	UE_LOG(LogTemp, Display, TEXT("[Explored] Créditos del final (%.0f s)"), Timing.EndCreditsSeconds);
}

FFinaleTiming AExploredPlayerController::GetFinaleTiming() const
{
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	return Beacon ? Beacon->FinaleTiming() : FFinaleSequenceModel::Sanitize(FFinaleTiming());
}

void AExploredPlayerController::ToggleNotebook()
{
	if (IsNotebookOpen())
	{
		CloseNotebook();
	}
	else
	{
		OpenNotebook();
	}
}

void AExploredPlayerController::OpenNotebook()
{
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (IsNotebookOpen() || bCutsceneActive || UIMode != EExploredUIMode::Playing || !Beacon || !Viewport)
	{
		return;
	}
	if (AExploredCharacter* PlayerCharacter = Cast<AExploredCharacter>(GetPawn()); PlayerCharacter && PlayerCharacter->IsBackpackOpen())
	{
		PlayerCharacter->SetBackpackOpen(false);
	}
	const FBeaconTexts& Texts = ExploredBeaconTexts::Get();
	TArray<FText> Pages;
	TArray<FText> Notes;
	for (int32 Page = 1; Page <= FNotebookModel::PageCount; ++Page)
	{
		Pages.Add(Texts.Get(FNotebookModel::PageTextId(Page)));
		Notes.Add(Texts.Get(FNotebookModel::NoteTextId(Page)));
	}
	TSharedRef<SExploredNotebook> Widget = SNew(SExploredNotebook)
		.PagesMask(Beacon->GetRunState().Notebook.PagesMask)
		.PageTexts(Pages)
		.NoteTexts(Notes)
		.Objective(Beacon->GetObjectiveText())
		.OnClose(FSimpleDelegate::CreateUObject(this, &AExploredPlayerController::CloseNotebook));
	NotebookWidget = Widget;
	Viewport->AddViewportWidgetContent(Widget, 450);
	SetPause(true);
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(Widget);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	UE_LOG(LogTemp, Display, TEXT("[Explored] Cuaderno abierto (%d/8)"), FNotebookModel::ReadCount(Beacon->GetRunState().Notebook));
}

void AExploredPlayerController::CloseNotebook()
{
	if (!IsNotebookOpen())
	{
		return;
	}
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(NotebookWidget.ToSharedRef());
	}
	NotebookWidget.Reset();
	SetUIMode(EExploredUIMode::Playing);
}

void AExploredPlayerController::QuitToDesktop()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
