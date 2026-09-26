#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UAchievementsSubsystem;

/**
 * Lista de logros (GDD §15, §16): nombre, descripción y barra de progreso de
 * cada uno en el orden de achievements.json; los ocultos salen como «???»
 * sin descripción ni barra hasta conseguirlos, y los que no se pueden
 * conseguir en el modo de la partida actual lo dicen. Arriba, el recuento.
 *
 * Se abre desde el menú principal («Logros») y desde la pausa; «Volver»,
 * Escape o B regresan a donde se abrió. Las filas son botones sin acción para
 * recorrer la lista con la cruceta. Las reglas están en
 * ExploredScreens::BuildAchievementRows (ScreensLogicSpec).
 */
class EXPLORED_API SExploredAchievements : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredAchievements) {}
		SLATE_ARGUMENT(TWeakObjectPtr<const UAchievementsSubsystem>, Achievements)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Primera fila (o «Volver» si no hay logros cargados). */
	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	FReply HandleBack();

	FSimpleDelegate OnBack;
	TSharedPtr<SWidget> InitialFocus;
};
