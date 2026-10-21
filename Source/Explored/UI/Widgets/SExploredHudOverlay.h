#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

class AExploredHUD;

/**
 * Capa Slate del HUD de juego: ocupa la pantalla, no recibe clics y en cada fotograma pide a
 * AExploredHUD que se pinte (texto con Lora, cajas redondeadas e iconos de la hoja de UI).
 */
class EXPLORED_API SExploredHudOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredHudOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AExploredHUD>, HUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.0, 1080.0); }

private:
	TWeakObjectPtr<AExploredHUD> HUD;
};
