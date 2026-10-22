#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Créditos (GDD v3 §9.2): rollo de 60 s sobre la noche (música de docs/creditos-musica.md y la fuente). Escape o B vuelven. */
class EXPLORED_API SExploredCredits : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredCredits) {}
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Segundos de rollo ya pasados (las capturas lo adelantan). */
	float GetRollSeconds() const { return Elapsed; }
	void SetRollSeconds(float Seconds) { Elapsed = FMath::Max(0.0f, Seconds); }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Botón «Volver»: recibe el foco al abrir la pantalla para navegar con teclado y mando. */
	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	FSimpleDelegate OnBack;
	TSharedPtr<SWidget> InitialFocus;
	TSharedPtr<class SScrollBox> Scroll;
	float Elapsed = 0.0f;
	/** Alto visible del rollo (px): los huecos de entrada y salida miden eso, para que empiece y acabe vacío. */
	float RollViewHeight = 840.0f;
	FReply HandleBack();
};
