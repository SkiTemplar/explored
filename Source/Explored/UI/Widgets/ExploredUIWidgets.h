#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

/**
 * Fábricas pequeñas para no repetir el estilo de botón en cada widget del
 * frontend (GDD §10: paneles cálidos, botones con hover/pulsado claros).
 */
namespace ExploredUIWidgets
{
	/** Botón de menú a ancho de texto, con la tipografía y colores de FExploredUIStyle. */
	EXPLORED_API TSharedRef<SWidget> MakeMenuButton(const FText& Label, FOnClicked OnClicked, bool bLarge = true);

	/** Botón de pestaña (Ajustes): resaltado si bActive. */
	EXPLORED_API TSharedRef<SWidget> MakeTabButton(const FText& Label, bool bActive, FOnClicked OnClicked);
}
