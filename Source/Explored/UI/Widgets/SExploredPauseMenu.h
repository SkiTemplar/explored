#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Menú de pausa (GDD v3 §9.2): Reanudar, Ajustes, Guardar (selector de ranura), Salir al menú y Salir del
 * juego, con el foco resaltado (W/S o flechas y Enter) y una línea fija con las teclas del juego. «Salir al
 * menú» y «Salir del juego» piden confirmación (Sí/No) si hay progreso sin guardar.
 *
 * Escape, Start o B del mando reanudan (H6): con FInputModeUIOnly la tecla no
 * llega al InputComponent del PlayerController, así que la atiende el widget.
 */
class EXPLORED_API SExploredPauseMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredPauseMenu)
		: _bConfirmExit(false)
	{}
		/** Hay progreso sin guardar: «Salir al menú» y «Salir del juego» preguntan antes. */
		SLATE_ARGUMENT(bool, bConfirmExit)
		SLATE_EVENT(FSimpleDelegate, OnResume)
		SLATE_EVENT(FSimpleDelegate, OnSettings)
		SLATE_EVENT(FSimpleDelegate, OnSave)
		SLATE_EVENT(FSimpleDelegate, OnExitToMenu)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Primer botón: recibe el foco al abrir la pantalla para navegar con teclado y mando. */
	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	FSimpleDelegate OnResume, OnSettings, OnSave, OnExitToMenu, OnQuit;
	TSharedPtr<SWidget> InitialFocus;
	TSharedPtr<SWidget> ExitFocus;
	TSharedPtr<SWidget> ConfirmNoFocus;
	TSharedPtr<SWidget> QuitFocus;
	TSharedPtr<SWidget> QuitConfirmNoFocus;
	TSharedPtr<class SWidgetSwitcher> Pages;
	bool bConfirmExit = false;

	FReply HandleConfirmYes();
	FReply HandleConfirmNo();
	TSharedRef<SWidget> BuildButtons();
	/** Pregunta de «Salir al menú» (bQuit = false) o de «Salir del juego» (true). */
	TSharedRef<SWidget> BuildConfirm(bool bQuit);
	/** Pasa a la página de la pregunta con el foco en «No»; false si no hay que preguntar. */
	bool AskConfirm(int32 Page, const TSharedPtr<SWidget>& NoFocus, FReply& OutReply);

	FReply HandleResume();
	FReply HandleSettings();
	FReply HandleSave();
	FReply HandleExitToMenu();
	FReply HandleQuit();
};
