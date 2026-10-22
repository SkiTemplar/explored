#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/PlayerController.h"
#include "UI/SettingsLogic.h"

#include "ExploredPlayerController.generated.h"

class AExploredMenuCamera;
class SExploredFade;
class SExploredInventoryPanel;
class SExploredSettingsPanel;
class UExploredGameUserSettings;
class UExploredSaveSubsystem;
struct FFinaleTiming;

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
 * ExploredSettingsLogic::FMenuNavigation::BackTarget. El binding de Escape/Start del
 * InputComponent solo actúa jugando (abre la pausa) y lleva
 * bExecuteWhenPaused por si la entrada llega con el juego pausado.
 *
 * Las pantallas se encadenan con ExploredSettingsLogic::FMenuNavigation (pila
 * pura con spec): «Volver» regresa a la pantalla desde la que se abrió cada
 * una, también en cadenas como juego → pausa → ajustes → pausa → juego.
 *
 * La pausa abre Ajustes y el selector de ranura para Guardar; el menú
 * principal abre Cargar; la vista del inventario (SExploredInventoryPanel)
 * sale sola mientras la mochila está abierta.
 *
 * Guardado: «Guardar» de la pausa abre el selector de ranuras manuales,
 * RequestSaveGame() guarda en la automática y ContinueGame() carga la ranura
 * más reciente a través de UExploredSaveSubsystem (ver
 * UI/ExploredSaveSubsystem.h y docs/tecnico/guardado.md).
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

	/**
	 * Inventario abierto en partida (Tab): cursor visible y juego + interfaz, para pulsar sus
	 * botones sin soltar el teclado; cerrado, vuelve a solo juego. Fuera de partida no hace nada.
	 */
	void ApplyBackpackInputMode(bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void ContinueGame();

	/** «Nueva partida»: reinicia los contadores de la partida y entra a jugar. */
	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void StartNewGame();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void OpenSettings();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void CloseSettings();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void OpenCredits();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void CloseCredits();
	/** Tema de créditos del director musical (UExploredMusicSubsystem::SetCredits). */
	void SetCreditsMusic(bool bCredits) const;

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void OpenPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void ResumeGame();

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void RequestSaveGame();

	/** Selector de ranura para guardar (pausa → «Guardar»). */
	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void OpenSaveGame();

	/** Selector de ranura para cargar (menú principal → «Cargar»). */
	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void OpenLoadGame();

	/** Carga una ranura y funde a jugar (lo llama el selector en modo Cargar). */
	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void LoadSlotAndPlay(const FString& SlotId);

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void ExitToMainMenu();

	/**
	 * Final de Baliza Siete (UBeaconSubsystem): funde a negro y muestra el epílogo
	 * (SExploredEnding); «Continuar» pasa a los créditos, que vuelven solos al menú.
	 */
	void ShowEnding(const FText& EpilogueLine, const FText& CommonLine);

	/** Créditos del final: «Volver» o EndCreditsSeconds después, al menú principal. */
	void ShowEndCredits();

	/** La pantalla de epílogo ya está en el viewport (prueba de humo de la historia). */
	bool IsShowingEnding() const { return bShowingEnding; }

	/** Los créditos del final están en pantalla y vuelven solos al menú (bot de la partida completa). */
	bool IsRollingEndCredits() const { return bRollingEndCredits; }

	/** El menú principal es la raíz del frontend y se está en el menú (no jugando). */
	bool IsInMainMenu() const;

	/** Opción de URL con la que «Salir al menú» y los créditos del final recargan el mapa. */
	static const TCHAR* ReturnToMenuOption() { return TEXT("ExploredMenu"); }

	/** El mundo arranca en el menú principal: sin «-SkipMenu», o recargado por «Salir al menú». */
	static bool StartsInMainMenu(const UWorld* World);

	/** El mundo se ha cargado desde «Salir al menú» o desde los créditos del final. */
	static bool IsReturningToMenu(const UWorld* World);

	/** Secuencia de salida del barco: sin pausa ni HUD de juego mientras dura. */
	void SetCutsceneActive(bool bActive) { bCutsceneActive = bActive; }
	bool IsCutsceneActive() const { return bCutsceneActive; }

	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void QuitToDesktop();

	/** Cuaderno de guardia (J): se abre jugando, pausa el mundo y se cierra con J, Tab o Esc. */
	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void ToggleNotebook();
	void OpenNotebook();
	void CloseNotebook();
	UFUNCTION(BlueprintPure, Category = "Explored|UI")
	bool IsNotebookOpen() const { return NotebookWidget.IsValid(); }

	/** Aviso del menú principal (partida de otra versión, carga fallida). */
	const FText& GetMenuNotice() const { return MenuNotice; }

	/** Hora a la que se ve la isla detrás del menú principal (atardecer, baliza apagada). */
	static constexpr float MenuBackdropHour = 17.9f;

	/** Menú principal (Continuar, Nueva partida, Cargar, Ajustes, Créditos, Salir) sobre la isla al atardecer. */
	void ShowMainMenu();
	/** Menú principal con un aviso (partida de otra versión, carga fallida). */
	void ShowMainMenuWithNotice(const FText& Notice);
	/** Aviso al encontrar una partida de la versión anterior (formato 1). */
	static FText ObsoleteSaveNotice();

	/** Ruta del mapa a la que vuelve «Salir al menú» (GameDefaultMap, ver Config/DefaultEngine.ini). */
	UPROPERTY(EditDefaultsOnly, Category = "Explored|UI")
	FName MainMenuMapName = "/Game/Maps/Map_IslaSal";

private:
	void HandleEscape();
	/** «Volver» de la pantalla actual (botón, Escape o B). */
	void NavigateBack();
	void NavigateTo(ExploredSettingsLogic::EMenuScreen Screen);
	/** Congela el reloj (y con él el cuerpo) mientras se está en el menú principal. */
	void FreezeWorldClock(bool bFreeze);
	/** Selector de ranura en modo Cargar (bLoading) o Guardar. */
	void ShowSaveSlots(bool bLoading);
	/** Añade el overlay al viewport y da el foco a FocusTarget (o al propio overlay). */
	void ShowOverlay(TSharedRef<SWidget> Widget, TSharedPtr<SWidget> FocusTarget, ExploredSettingsLogic::EMenuScreen Screen);
	void HideOverlay();
	/** Funde a negro, entra a jugar y vuelve a fundir (Continuar y Nueva partida). */
	void FadeIntoGameplay();
	AExploredMenuCamera* FindOrSpawnMenuCamera();
	/** Tiempos del final de UBeaconSubsystem (los mismos que programan la bocina y el fundido). */
	FFinaleTiming GetFinaleTiming() const;

	EExploredUIMode UIMode = EExploredUIMode::Menu;
	bool bCutsceneActive = false;
	bool bShowingEnding = false;
	bool bRollingEndCredits = false;
	/** Pantalla actual y camino hasta ella (la base es None jugando o MainMenu). */
	ExploredSettingsLogic::FMenuNavigation Navigation;

	TSharedPtr<SExploredFade> FadeWidget;
	/** Vista del inventario: siempre en el viewport, visible solo con la mochila abierta. */
	TSharedPtr<SExploredInventoryPanel> InventoryPanel;
	TSharedPtr<SWidget> CurrentOverlay;
	/** Widget con el foco inicial del overlay actual (primer botón, pestaña activa...). */
	TSharedPtr<SWidget> CurrentFocusTarget;
	/** Panel de Ajustes abierto, para que Escape pase por su descarte (M11). */
	TWeakPtr<SExploredSettingsPanel> SettingsPanel;

	TWeakObjectPtr<AExploredMenuCamera> MenuCamera;

	TSharedPtr<SWidget> NotebookWidget;
	FText MenuNotice;
	/** Escala del reloj guardada al congelarlo en el menú; < 0 = no está congelado. */
	float FrozenTimeScale = -1.0f;

	/** Vuelta al menú tras los créditos del final; con el mundo en pausa no corren los temporizadores del mundo. */
	FTSTicker::FDelegateHandle EndCreditsTicker;
	/** Segundos reales desde que empezaron los créditos del final (FFinaleSequenceModel::CreditsStep). */
	float EndCreditsElapsed = 0.0f;

};
