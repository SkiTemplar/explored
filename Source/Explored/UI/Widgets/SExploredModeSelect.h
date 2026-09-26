#pragma once

#include "CoreMinimal.h"
#include "UI/ExploredGameplayMode.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnExploredModeChosen, EExploredGameplayMode);

/** Pantalla de elección de modo de «Nueva partida» (GDD §7 y §10). Escape o B vuelven al menú. */
class EXPLORED_API SExploredModeSelect : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredModeSelect) {}
		SLATE_EVENT(FOnExploredModeChosen, OnChosen)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Primer botón: recibe el foco al abrir la pantalla para navegar con teclado y mando. */
	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	TSharedRef<SWidget> MakeModeRow(EExploredGameplayMode Mode, const FText& Title, const FText& Description);
	FReply HandleChoose(EExploredGameplayMode Mode);
	FReply HandleBack();

	FOnExploredModeChosen OnChosen;
	FSimpleDelegate OnBack;
	TSharedPtr<SWidget> InitialFocus;
};
