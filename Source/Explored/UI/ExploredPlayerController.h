#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/ExploredGameplayMode.h"

#include "ExploredPlayerController.generated.h"

class AExploredMenuCamera;
class SExploredFade;
class SExploredSavingIndicator;
class UExploredGameUserSettings;

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
 * Guardado: RequestSaveGame() guarda en la ranura automática y ContinueGame()
 * carga la ranura más reciente a través de UExploredSaveSubsystem (ver
 * UI/ExploredSaveSubsystem.h y docs/tecnico/guardado.md).
 */
UCLASS()
class EXPLORED_API AExploredPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AExploredPlayerController();

	virtual void BeginPlay() override;
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
	void ShowMainMenu();
	void ShowOverlay(TSharedRef<SWidget> Widget);
	void HideOverlay();
	AExploredMenuCamera* FindOrSpawnMenuCamera();

	EExploredUIMode UIMode = EExploredUIMode::Menu;
	bool bSettingsOpenedFromPause = false;

	TSharedPtr<SExploredFade> FadeWidget;
	TSharedPtr<SExploredSavingIndicator> SavingIndicator;
	TSharedPtr<SWidget> CurrentOverlay;

	TWeakObjectPtr<AExploredMenuCamera> MenuCamera;
};
