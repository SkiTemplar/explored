#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

#include "UI/MapViewLogic.h"

/**
 * Boceto a lápiz de una receta anotada al margen del mapa (GDD §8.5): unos
 * trazos deterministas a partir del id (ExploredMapView::RecipeDoodle), en
 * un recuadro pequeño. Solo dibuja; no atiende entrada.
 */
class EXPLORED_API SExploredDoodle : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredDoodle)
		: _Size(48.0f)
	{}
		SLATE_ARGUMENT(FName, RecipeId)
		/** Lado del recuadro en unidades de Slate. */
		SLATE_ARGUMENT(float, Size)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TArray<ExploredMapView::FPolyline> Strokes;
	float Side = 48.0f;
};
