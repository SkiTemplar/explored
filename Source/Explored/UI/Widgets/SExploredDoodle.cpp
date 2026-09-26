#include "UI/Widgets/SExploredDoodle.h"

#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"

#include "UI/ExploredUIStyle.h"

void SExploredDoodle::Construct(const FArguments& InArgs)
{
	Side = FMath::Max(InArgs._Size, 8.0f);
	Strokes = ExploredMapView::RecipeDoodle(InArgs._RecipeId);
	SetVisibility(EVisibility::HitTestInvisible);
}

FVector2D SExploredDoodle::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(Side, Side);
}

int32 SExploredDoodle::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	FLinearColor Pencil = FExploredUIStyle::Get().ColorSheetInk();
	Pencil.A = 0.7f;
	Pencil = Pencil * InWidgetStyle.GetColorAndOpacityTint();
	for (const ExploredMapView::FPolyline& Stroke : Strokes)
	{
		TArray<FVector2f> Points;
		Points.Reserve(Stroke.Num());
		for (const FVector2D& P : Stroke)
		{
			Points.Add(FVector2f(static_cast<float>(P.X) * Size.X, static_cast<float>(P.Y) * Size.Y));
		}
		if (Points.Num() >= 2)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points),
				ESlateDrawEffect::None, Pencil, true, 1.2f);
		}
	}
	return LayerId;
}
