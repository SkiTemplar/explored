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

	/**
	 * Botón de menú navegable con teclado y mando: con el foco lleva una pastilla naranja (cielo_ocaso,
	 * el de las pestañas activas) y la marca «›» (Lora no trae el triángulo «▸»); deshabilitado, el texto sale atenuado y no se
	 * puede pulsar ni enfocar. OutFocusable recibe el SButton (el que debe recibir el foco inicial).
	 */
	EXPLORED_API TSharedRef<SWidget> MakeFocusMenuButton(const FText& Label, FOnClicked OnClicked, bool bEnabled = true,
		TSharedPtr<SWidget>* OutFocusable = nullptr);

	/** Botón sobre la escena o un fondo oscuro (final, créditos): texto de papel con sombra. */
	EXPLORED_API TSharedRef<SWidget> MakeSceneButton(const FText& Label, FOnClicked OnClicked);

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
