#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Menú de pausa (GDD §10, §15): Reanudar, Mapa, Museo, Logros, Ajustes,
 * Guardar (selector de ranura), Salir al menú y Salir del juego.
 *
 * Escape, Start o B del mando reanudan (H6): con FInputModeUIOnly la tecla no
 * llega al InputComponent del PlayerController, así que la atiende el widget.
 */
class EXPLORED_API SExploredPauseMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredPauseMenu) {}
		SLATE_EVENT(FSimpleDelegate, OnResume)
		SLATE_EVENT(FSimpleDelegate, OnMap)
		SLATE_EVENT(FSimpleDelegate, OnMuseum)
		SLATE_EVENT(FSimpleDelegate, OnAchievements)
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
	FSimpleDelegate OnResume, OnMap, OnMuseum, OnAchievements, OnSettings, OnSave, OnExitToMenu, OnQuit;
	TSharedPtr<SWidget> InitialFocus;

	FReply HandleResume();
	FReply HandleMap();
	FReply HandleMuseum();
	FReply HandleAchievements();
	FReply HandleSettings();
	FReply HandleSave();
	FReply HandleExitToMenu();
	FReply HandleQuit();
};
