#pragma once

#include "CoreMinimal.h"
#include "Rendering/SlateRenderTransform.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "Ruins/RuinsModel.h"
#include "UI/MapViewLogic.h"

class SButton;
class SEditableTextBox;
class SExploredMapSheet;
class SWidgetSwitcher;
class UCartographyComponent;

/**
 * El mapa en las manos (GDD §5.6, §8.2, §13): la hoja dibujada a mano sube
 * desde abajo sujeta por los pulgares, algo ladeada, con un cuaderno al
 * margen. No es un menú a pantalla completa: el mundo sigue visible alrededor.
 *
 * Decisión de diseño (P-UI2): mientras se mira el mapa el mundo NO se pausa
 * ni se ralentiza. El tiempo, la marea y la lluvia siguen (el papel se puede
 * mojar mientras se lee: la tinta corre a la vista), pero la entrada es del
 * mapa, así que el personaje se queda quieto con él en las manos. Abierto
 * desde la pausa, el juego sigue pausado como estaba.
 *
 * - Hoja: SExploredMapSheet interactiva (arrastrar, rueda, flechas, cruceta,
 *   LB/RB) con la capa de anotaciones de wayfinding y las manchas de agua.
 * - Marcas: se elige un sello de story_es.json (`map_marks`) o «Nota», se
 *   escribe un texto corto (tope del modelo) y se pulsa en la hoja para
 *   ponerla ahí, o «Marcar donde estoy» (con la deriva del dibujo). Pulsar una
 *   marca la selecciona y enseña su sello, texto y origen.
 * - Rumbos: la capa de wayfinding se puede ocultar; lista de lo aprendido.
 * - Recetas: las anotadas al margen, con su boceto.
 * - «Colección» abre el catálogo del museo (GDD §7) desde el propio mapa.
 *
 * Teclas: M, View, Escape, B o Start guardan el mapa (M no mientras se
 * escribe; ExploredMapView::ShouldCloseMapOnKey). Y del mando pasa el foco
 * entre la hoja y el cuaderno.
 */
class EXPLORED_API SExploredMapInHands : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredMapInHands) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UCartographyComponent>, Cartography)
		/** Anotaciones de wayfinding (URuinsSubsystem::GetMapAnnotations). */
		SLATE_ARGUMENT(TArray<FWayfindingAnnotation>, Annotations)
		/** Para resolver los nombres de las recetas (UItemRegistrySubsystem::Resolve). */
		SLATE_ARGUMENT(TWeakObjectPtr<const UObject>, WorldContextObject)
		SLATE_EVENT(FSimpleDelegate, OnClose)
		SLATE_EVENT(FSimpleDelegate, OnOpenCollection)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** La hoja: recibe el foco al abrir para desplazar con el mando desde el principio. */
	TSharedPtr<SWidget> GetInitialFocus() const;

private:
	TSharedRef<SWidget> BuildNotebook();
	TSharedRef<SWidget> BuildMarksPage();
	TSharedRef<SWidget> BuildWayfindingPage();
	TSharedRef<SWidget> BuildRecipesPage();
	TSharedRef<SWidget> MakeStampButton(FName StampId, const FText& Label);
	TSharedRef<SWidget> MakeTabButton(int32 Tab, const FText& Label);

	void LoadStampLabels();
	FText StampLabel(FName StampId) const;
	FText RecipeLabel(FName RecipeId) const;

	void HandleViewRequested(const ExploredMapView::FMapView& NewView);
	void HandleSheetClicked(const FVector2D& MapPosition, int32 MarkIndex);
	FReply HandleSelectStamp(FName StampId);
	FReply HandleMarkHere();
	FReply HandleSelectTab(int32 Tab);
	FReply HandleOpenCollection();
	FReply HandleClose();
	void HandleTextChanged(const FText& NewText);
	void RefreshStampButtons();

	bool IsTypingText() const;
	bool CanCommit() const;
	FText GetStatusText() const { return StatusText; }
	FText GetSelectedMarkText() const;
	FText GetWetnessText() const;
	FText GetRemainingText() const;
	TOptional<FSlateRenderTransform> GetPaperTransform() const;

	TWeakObjectPtr<UCartographyComponent> Cartography;
	TWeakObjectPtr<const UObject> WorldContextObject;
	TArray<FWayfindingAnnotation> Annotations;
	FSimpleDelegate OnClose;
	FSimpleDelegate OnOpenCollection;

	TArray<ExploredMapView::FMapStampLabel> Stamps;
	/** Sello elegido (NAME_None = nota solo de texto). */
	FName SelectedStamp;
	FString PendingText;
	int32 SelectedMark = INDEX_NONE;
	ExploredMapView::FMapView View;
	bool bShowWayfinding = true;
	int32 ActiveTab = 0;
	FText StatusText;

	/** 0 → 1 mientras la hoja sube a las manos (instantáneo con «Reducir movimiento»). */
	float RaiseAlpha = 0.0f;
	bool bReduceMotion = false;

	TSharedPtr<SExploredMapSheet> Sheet;
	TSharedPtr<SEditableTextBox> MarkTextBox;
	TSharedPtr<SWidgetSwitcher> Pages;
	TSharedPtr<SWidget> NotebookFirstFocus;
	/** Botones de sello en el orden de StampButtonIds (se re-estilan al elegir). */
	TArray<TSharedPtr<SButton>> StampButtons;
	TArray<FName> StampButtonIds;
	TArray<TSharedPtr<SButton>> TabButtons;
};
