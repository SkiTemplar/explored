#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

#include "Ruins/RuinsModel.h"
#include "UI/MapViewLogic.h"

class UCartographyComponent;
class UTexture2D;

/** La hoja pide otra zona visible (arrastre, rueda, flechas, cruceta o gatillos). */
DECLARE_DELEGATE_OneParam(FOnMapSheetViewRequested, const ExploredMapView::FMapView& /*NewView*/);
/** Pulsación sin arrastre sobre la hoja: punto de mapa y marca pulsada (INDEX_NONE si ninguna). */
DECLARE_DELEGATE_TwoParams(FOnMapSheetClicked, const FVector2D& /*MapPosition*/, int32 /*MarkIndex*/);

/**
 * Hoja del mapa dibujado a mano (GDD §5, §8.2): papel, manchas de agua,
 * bocetos tenues de los miradores, trazos de costa con la tinta que les
 * quede, anotaciones de wayfinding (GDD §6.2) y marcas del jugador. Lee el
 * estado del modelo en cada pintado; no guarda copia.
 *
 * Interactiva (bInteractive): arrastrar desplaza, la rueda acerca bajo el
 * cursor, un clic sin arrastre avisa con el punto de mapa y la marca pulsada.
 * Con el foco, flechas/cruceta desplazan, RePág/AvPág, +/− y los botones
 * superiores (LB/RB) acercan y alejan, y Aceptar (Intro, A) pulsa en el
 * centro, donde se dibuja una cruz. Toda la cuenta está en ExploredMapView
 * (UI/MapViewLogic.h, con MapViewLogicSpec): la hoja no guarda la zona
 * visible, la pide con OnViewRequested y la lee de ViewCenter/ViewSize.
 *
 * La presentación como objeto en las manos es de SExploredMapInHands.
 */
class EXPLORED_API SExploredMapSheet : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredMapSheet)
		: _ViewCenter(FVector2D(0.5, 0.5))
		, _ViewSize(1.0f)
		, _ShowWayfinding(true)
		, _SelectedMark(INDEX_NONE)
		, _bInteractive(false)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<UCartographyComponent>, Cartography)
		/** Centro de la zona visible, en coordenadas de mapa [0, 1]. */
		SLATE_ATTRIBUTE(FVector2D, ViewCenter)
		/** Lado de la zona visible en coordenadas de mapa (1 = hoja entera). */
		SLATE_ATTRIBUTE(float, ViewSize)
		/** Capa de anotaciones de wayfinding visible. */
		SLATE_ATTRIBUTE(bool, ShowWayfinding)
		/** Marca resaltada (INDEX_NONE si ninguna). */
		SLATE_ATTRIBUTE(int32, SelectedMark)
		/** Atiende ratón, teclado y mando (el mapa en las manos); si no, solo dibuja. */
		SLATE_ARGUMENT(bool, bInteractive)
		SLATE_EVENT(FOnMapSheetViewRequested, OnViewRequested)
		SLATE_EVENT(FOnMapSheetClicked, OnSheetClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Anotaciones de wayfinding que dibujar (URuinsSubsystem::GetMapAnnotations al abrir el mapa). */
	void SetAnnotations(const TArray<FWayfindingAnnotation>& Annotations);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual bool SupportsKeyboardFocus() const override { return bInteractive; }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent) override;

private:
	ExploredMapView::FMapView CurrentView() const;
	ExploredMapView::FSheetTransform MakeTransform(const FGeometry& Geometry) const;
	void RequestView(const ExploredMapView::FMapView& NewView) const;
	void ClickAt(const FGeometry& Geometry, const FVector2D& Local) const;

	TWeakObjectPtr<UCartographyComponent> Cartography;
	TAttribute<FVector2D> ViewCenter;
	TAttribute<float> ViewSize;
	TAttribute<bool> ShowWayfinding;
	TAttribute<int32> SelectedMark;
	bool bInteractive = false;
	FOnMapSheetViewRequested OnViewRequested;
	FOnMapSheetClicked OnSheetClicked;

	/** Trazos de las anotaciones ya generados (coordenadas de mapa) y su técnica. */
	TArray<TPair<EWayfindingTechnique, ExploredMapView::FPolyline>> AnnotationLines;

	/** Arrastre con el botón izquierdo: distancia recorrida desde que se pulsó (píxeles locales). */
	bool bPressed = false;
	bool bDragging = false;
	double DragDistance = 0.0;

	/** Textura de papel (Tools/Textures, `MapPaper`); si no está importada, papel liso. */
	TStrongObjectPtr<UTexture2D> PaperTexture;
	FSlateBrush PaperBrush;
	FSlateBrush FlatBrush;
	/** Óvalo para las manchas de agua. */
	FSlateBrush StainBrush;
};
