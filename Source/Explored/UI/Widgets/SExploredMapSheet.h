#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SLeafWidget.h"

class UCartographyComponent;
class UTexture2D;

/**
 * Hoja del mapa dibujado a mano (GDD §5, §8.2): papel, bocetos tenues de los
 * miradores, trazos de costa con la tinta que les quede y marcas del jugador.
 * Lee el estado del modelo en cada pintado; no guarda copia.
 *
 * TODO(P-UI): presentarla como objeto en las manos (no a pantalla completa).
 */
class EXPLORED_API SExploredMapSheet : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredMapSheet)
		: _ViewCenter(FVector2D(0.5, 0.5))
		, _ViewSize(1.0f)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<UCartographyComponent>, Cartography)
		/** Centro de la zona visible, en coordenadas de mapa [0, 1]. */
		SLATE_ATTRIBUTE(FVector2D, ViewCenter)
		/** Lado de la zona visible en coordenadas de mapa (1 = hoja entera). */
		SLATE_ATTRIBUTE(float, ViewSize)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TWeakObjectPtr<UCartographyComponent> Cartography;
	TAttribute<FVector2D> ViewCenter;
	TAttribute<float> ViewSize;

	/** Textura de papel (Tools/Textures, `MapPaper`); si no está importada, papel liso. */
	TStrongObjectPtr<UTexture2D> PaperTexture;
	FSlateBrush PaperBrush;
	FSlateBrush FlatBrush;
};
