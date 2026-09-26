#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class URuinsSubsystem;

/**
 * Museo y catálogo de tesoros (GDD §7, §16): una ficha por tesoro con su
 * silueta (desconocido), fotografiado o hallado, su procedencia (siempre, como
 * pide el GDD: «silueta y procedencia de cada tesoro, completo o no») y el
 * lugar donde se recogió; arriba, lo expuesto frente al objetivo del logro
 * «Coleccionista» y lo registrado del catálogo.
 *
 * Se abre desde la pausa («Museo») y desde el apartado «Colección» del mapa
 * en las manos; «Volver», Escape o B regresan a donde se abrió
 * (ExploredSettingsLogic::FMenuNavigation). Las fichas son botones sin acción
 * para que la cruceta y las flechas recorran la lista y la desplacen.
 * Qué se muestra lo decide ExploredScreens::BuildMuseumEntries (ScreensLogicSpec).
 */
class EXPLORED_API SExploredMuseum : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredMuseum) {}
		SLATE_ARGUMENT(TWeakObjectPtr<const URuinsSubsystem>, Ruins)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Primera ficha (o «Volver» si no hay catálogo). */
	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	FReply HandleBack();

	FSimpleDelegate OnBack;
	TSharedPtr<SWidget> InitialFocus;
};
