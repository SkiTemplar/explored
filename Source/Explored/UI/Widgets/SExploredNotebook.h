#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;

/**
 * Cuaderno de guardia (J, GDD v3 §9.2): un cuaderno abierto de papel. A la izquierda, el índice de
 * las 8 páginas (las no encontradas, con candado), «Páginas X/8» y el objetivo actual; a la derecha,
 * la página elegida con el texto de Roig en tinta y la nota de Iria en tinta azul. Flechas o A/D
 * pasan de página; J, Tab o Esc lo cierran. La lógica de qué se enseña está en
 * ExploredScreens::BuildNotebookView.
 */
class EXPLORED_API SExploredNotebook : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredNotebook) {}
		/** Bit i = página i + 1 leída. */
		SLATE_ARGUMENT(uint8, PagesMask)
		/** Texto de Roig y nota de Iria de las 8 páginas (índice 0 = página 1). */
		SLATE_ARGUMENT(TArray<FText>, PageTexts)
		SLATE_ARGUMENT(TArray<FText>, NoteTexts)
		SLATE_ARGUMENT(FText, Objective)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Página que se ve (0 = ninguna). */
	int32 GetShownPage() const { return Shown; }

private:
	void ShowPage(int32 Page);
	TSharedRef<SWidget> BuildIndex() const;
	TSharedRef<SWidget> BuildPage() const;

	uint8 PagesMask = 0;
	TArray<FText> PageTexts;
	TArray<FText> NoteTexts;
	FText Objective;
	FSimpleDelegate OnClose;
	int32 Shown = 0;
	TSharedPtr<SBox> IndexBox;
	TSharedPtr<SBox> PageBox;
};
