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

	/** Nombre de la cultura activa («es», «en»...), para elegir los textos bilingües de los datos. */
	EXPLORED_API FString CurrentCultureName();

	/** Texto de datos en el idioma activo (ExploredLocalization::Pick: cae al español si falta el inglés). */
	EXPLORED_API FText PickLocalized(const FString& Es, const FString& En);

	/**
	 * Pantalla estándar del frontend (museo, logros, ranuras): panel centrado
	 * de Width × Height con título, subtítulo opcional, cuerpo que llena y pie.
	 */
	EXPLORED_API TSharedRef<SWidget> MakeScreenPanel(const FText& Title, TAttribute<FText> Subtitle, TSharedRef<SWidget> Body,
		TSharedRef<SWidget> Footer, float Width, float Height);

	/** Barra de progreso del estilo del frontend (0–1). */
	EXPLORED_API TSharedRef<SWidget> MakeProgressBar(TAttribute<TOptional<float>> Percent, float Height = 8.0f);
}
