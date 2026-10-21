#include "UI/Widgets/SExploredHudOverlay.h"

#include "UI/ExploredHUD.h"
#include "UI/ExploredHudPainter.h"

void SExploredHudOverlay::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;
	SetVisibility(EVisibility::HitTestInvisible);
}

int32 SExploredHudOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (AExploredHUD* Owner = HUD.Get())
	{
		FExploredHudPainter Painter(AllottedGeometry, OutDrawElements, LayerId);
		Owner->PaintOverlay(Painter);
		return Painter.GetLayer();
	}
	return LayerId;
}
