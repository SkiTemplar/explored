#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Pantalla de créditos (GDD §10, §15): desplazable, sobre el mundo, con la dedicatoria. Escape o B vuelven al menú. */
class EXPLORED_API SExploredCredits : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredCredits) {}
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Botón «Volver»: recibe el foco al abrir la pantalla para navegar con teclado y mando. */
	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	FSimpleDelegate OnBack;
	TSharedPtr<SWidget> InitialFocus;
	FReply HandleBack();
};
