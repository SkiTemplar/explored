#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Pantalla del final (GDD §0, §6.3): cielo nocturno y unas pocas líneas.
 * «Volver al menú» (o Escape/B) cierra la partida.
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
	/** Pinta el cielo estrellado detrás del texto (posiciones fijas por semilla, titilando). */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	FSimpleDelegate OnContinue;
	TSharedPtr<SWidget> InitialFocus;
	FReply HandleContinue();
};
