#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Pantalla del final «Zarpar» (GDD §0, §6.3): cielo nocturno, unas pocas líneas y la
 * dedicatoria. «Volver al menú» (o Escape/B) cierra la partida.
 */
class EXPLORED_API SExploredEnding : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredEnding) {}
		/** Días que ha durado la partida, para la línea de cierre. */
		SLATE_ARGUMENT(int32, DaysSurvived)
		SLATE_EVENT(FSimpleDelegate, OnContinue)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	FSimpleDelegate OnContinue;
	TSharedPtr<SWidget> InitialFocus;
	FReply HandleContinue();
};
