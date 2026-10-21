#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Pantalla del epílogo de Baliza Siete (GDD v3 §2, «Final»): cielo nocturno, la línea que
 * depende de las páginas leídas (8 de 8 o menos) y la línea común. «Continuar» (o Escape/B)
 * pasa a los créditos.
 */
class EXPLORED_API SExploredEnding : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredEnding) {}
		/** Línea del epílogo según las páginas leídas (notebook.json, epilogo_completo/parcial). */
		SLATE_ARGUMENT(FText, EpilogueLine)
		/** Línea común final (epilogo_comun). */
		SLATE_ARGUMENT(FText, CommonLine)
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
