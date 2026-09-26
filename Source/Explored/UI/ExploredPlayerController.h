#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/ExploredGameplayMode.h"
#include "UI/SettingsLogic.h"

#include "ExploredPlayerController.generated.h"

class AExploredMenuCamera;
class SExploredFade;
class SExploredSavingIndicator;
class SExploredSettingsPanel;
class UExploredGameUserSettings;
class UExploredSaveSubsystem;

/** Modos de interfaz del PlayerController (GDD §10). */
UENUM(BlueprintType)
enum class EExploredUIMode : uint8
{
	Menu,
	Playing,
	Paused
};

/** Funciones puras del estado de UI, separadas para poder testearlas sin un UWorld en marcha. */
namespace ExploredUI
{
	EXPLORED_API bool ShouldShowCursor(EExploredUIMode Mode);
	EXPLORED_API bool ShouldPauseGame(EExploredUIMode Mode);
	/** A qué modo pasa Escape estando en cada modo (Unchanged = Current si Escape no hace nada ahí). */
	EXPLORED_API EExploredUIMode NextModeOnEscape(EExploredUIMode Current);
}

/**
 * Controlador del jugador: gestiona los modos Menú/Juego/Pausa, el cursor y
 * el modo de entrada de todo el frontend (GDD §10). Es la única clase nueva
 * que toca AExploredGameMode (una línea, PlayerControllerClass) y no toca
 * AExploredCharacter: el personaje ya se genera y posee en el PlayerStart
 * por el flujo normal de AGameModeBase; este controlador solo decide qué
 * cámara y qué modo de entrada usar mientras el menú está encima.
 *
 * Navegación (H6): cada pantalla atiende Escape/B en su propio OnKeyDown
 * (con FInputModeUIOnly las teclas no llegan al InputComponent) y llama a su
 * delegado de «Volver»; el controlador decide el destino con
 * ExploredSettingsLogic::ScreenAfterBack. El binding de Escape/Start del
 * InputComponent solo actúa jugando (abre la pausa) y lleva
 * bExecuteWhenPaused por si la entrada llega con el juego pausado.
 *
 * Integración para el equipo de guardado: RequestSaveGame() y ContinueGame()
 * llaman a UExploredSaveSubsystem (ver UI/ExploredSaveSubsystem.h). Cuando
 * el guardado real cargue el mundo, sustituir el cuerpo de ContinueGame()
 * (hoy solo quita el menú y entra a jugar) por la restauración real.
 */
UCLASS()
class EXPLORED_API AExploredPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AExploredPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	EExploredUIMode GetUIMode() const { return UIMode; }

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void SetUIMode(EExploredUIMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void ContinueGame();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void OpenModeSelect();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void StartNewGame(EExploredGameplayMode Mode);

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void OpenSettings();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void CloseSettings();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void OpenCredits();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void CloseCredits();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void OpenPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void ResumeGame();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void RequestSaveGame();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void ExitToMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void QuitToDesktop();

	/** Ruta del mapa a la que vuelve «Salir al menú» (GameDefaultMap, ver Config/DefaultEngine.ini). */
	UPROPERTY(EditDefaultsOnly, Category = "Explored|UI")
	FName MainMenuMapName = "/Game/Maps/Archipelago";

private:
	void HandleEscape();
	/** «Volver» de la pantalla actual (botón, Escape o B). */
	void NavigateBack();
	void NavigateTo(ExploredSettingsLogic::EMenuScreen Screen);
	void ShowMainMenu();
	/** Añade el overlay al viewport y da el foco a FocusTarget (o al propio overlay). */
	void ShowOverlay(TSharedRef<SWidget> Widget, TSharedPtr<SWidget> FocusTarget, ExploredSettingsLogic::EMenuScreen Screen);
	void HideOverlay();
	/** Funde a negro, entra a jugar y vuelve a fundir (Continuar y Nueva partida). */
	void FadeIntoGameplay();
	AExploredMenuCamera* FindOrSpawnMenuCamera();

	EExploredUIMode UIMode = EExploredUIMode::Menu;
	ExploredSettingsLogic::EMenuScreen CurrentScreen = ExploredSettingsLogic::EMenuScreen::None;
	bool bSettingsOpenedFromPause = false;

	TSharedPtr<SExploredFade> FadeWidget;
	TSharedPtr<SExploredSavingIndicator> SavingIndicator;
	TSharedPtr<SWidget> CurrentOverlay;
	/** Widget con el foco inicial del overlay actual (primer botón, pestaña activa...). */
	TSharedPtr<SWidget> CurrentFocusTarget;
	/** Panel de Ajustes abierto, para que Escape pase por su descarte (M11). */
	TWeakPtr<SExploredSettingsPanel> SettingsPanel;

	TWeakObjectPtr<AExploredMenuCamera> MenuCamera;

	/** Suscripción a UExploredSaveSubsystem::OnSaveCompleted; se retira en EndPlay (H1/M8). */
	TWeakObjectPtr<UExploredSaveSubsystem> SaveSubsystemBound;
	FDelegateHandle SaveCompletedHandle;
};
