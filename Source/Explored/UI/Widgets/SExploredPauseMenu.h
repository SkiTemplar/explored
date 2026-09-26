#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Menú de pausa (GDD §10): Reanudar, Diario, Ajustes, Guardar, Salir al menú, Salir del juego. */
class EXPLORED_API SExploredPauseMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredPauseMenu) {}
		SLATE_EVENT(FSimpleDelegate, OnResume)
		SLATE_EVENT(FSimpleDelegate, OnSettings)
		SLATE_EVENT(FSimpleDelegate, OnSave)
		SLATE_EVENT(FSimpleDelegate, OnExitToMenu)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FSimpleDelegate OnResume, OnSettings, OnSave, OnExitToMenu, OnQuit;

	FReply HandleResume();
	FReply HandleJournal() { return FReply::Handled(); }
	FReply HandleSettings();
	FReply HandleSave();
	FReply HandleExitToMenu();
	FReply HandleQuit();
};
