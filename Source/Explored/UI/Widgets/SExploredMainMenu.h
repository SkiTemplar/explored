#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Menú principal (GDD §10): título, subtítulo y los cinco botones sobre el mundo real. */
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

private:
	FSimpleDelegate OnContinue;
	FSimpleDelegate OnNewGame;
	FSimpleDelegate OnSettings;
	FSimpleDelegate OnCredits;
	FSimpleDelegate OnQuit;

	FReply HandleContinue();
	FReply HandleNewGame();
	FReply HandleSettings();
	FReply HandleCredits();
	FReply HandleQuit();
};
