#pragma once

#include "CoreMinimal.h"
#include "UI/ExploredGameplayMode.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnExploredModeChosen, EExploredGameplayMode);

/** Pantalla de elección de modo de «Nueva partida» (GDD §7 y §10). */
class EXPLORED_API SExploredModeSelect : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredModeSelect) {}
		SLATE_EVENT(FOnExploredModeChosen, OnChosen)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeModeRow(EExploredGameplayMode Mode, const FText& Title, const FText& Description);
	FReply HandleChoose(EExploredGameplayMode Mode);
	FReply HandleBack();

	FOnExploredModeChosen OnChosen;
	FSimpleDelegate OnBack;
};
