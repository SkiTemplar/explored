#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Menú principal (GDD §10): título, subtítulo y los cinco botones sobre el mundo real.
 * Escape no hace nada aquí (ver ExploredSettingsLogic::ScreenAfterBack).
 */
class EXPLORED_API SExploredMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredMainMenu)
		: _bCanContinue(false)
	{}
		SLATE_ARGUMENT(bool, bCanContinue)
		SLATE_EVENT(FSimpleDelegate, OnContinue)
		SLATE_EVENT(FSimpleDelegate, OnNewGame)
		SLATE_EVENT(FSimpleDelegate, OnSettings)
		SLATE_EVENT(FSimpleDelegate, OnCredits)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** «Continuar» si hay partida guardada; si no, «Nueva partida». */
	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	FSimpleDelegate OnContinue;
	FSimpleDelegate OnNewGame;
	FSimpleDelegate OnSettings;
	FSimpleDelegate OnCredits;
	FSimpleDelegate OnQuit;
	TSharedPtr<SWidget> InitialFocus;

	FReply HandleContinue();
	FReply HandleNewGame();
	FReply HandleSettings();
	FReply HandleCredits();
	FReply HandleQuit();
};
