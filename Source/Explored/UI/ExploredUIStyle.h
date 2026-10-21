#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"

class UTexture2D;
enum class EExploredIcon : uint8;

/**
 * Estilo visual compartido de la interfaz de Baliza Siete (GDD v3 §9): cuaderno de papel y tinta.
 * Todos los colores salen de la paleta única (ExploredUIPalette.h, generado desde
 * Tools/Style/paleta.json); la tipografía es Lora (serif, OFL, Content/Slate/Fonts) y los iconos,
 * siluetas de una hoja generada por script (Content/Slate/T_IconosUI.png, la única textura de UI).
 *
 * Es un singleton de vida estática (Get() nunca se destruye durante la
 * ejecución): los widgets pueden guardar punteros a los FButtonStyle/etc.
 * que devuelve (SButton::ButtonStyle guarda un `const FButtonStyle*`, no una
 * copia) sin arriesgarse a que queden colgando.
 */
class EXPLORED_API FExploredUIStyle
{
public:
	static FExploredUIStyle& Get();

	// Papel y tinta del cuaderno (paleta: papel, arena_clara, tinta, madera_oscura, rojo_faro, cielo_ocaso).
	FLinearColor ColorPaper() const;
	FLinearColor ColorPaperLight() const;
	FLinearColor ColorInk() const;
	FLinearColor ColorInkDim() const;
	FLinearColor ColorAccent() const;
	FLinearColor ColorAccentDim() const;
	FLinearColor ColorDisabled() const;
	/** Papel de las hojas que se sostienen: igual que el panel, opaco. */
	FLinearColor ColorSheet() const;
	FLinearColor ColorSheetInk() const;
	FLinearColor ColorSkin() const;
	/** Aviso (sobrescribir, ranura dañada, barra baja). */
	FLinearColor ColorWarning() const;
	/** Nota de Iria en el cuaderno: tinta azul (agua_profunda). */
	FLinearColor ColorNoteInk() const;
	/** Texto claro sobre la escena (menú principal, carteles): papel. */
	FLinearColor ColorOnScene() const;

	/** Fuentes por tamaño en píxeles de diseño (1080p). Nada por debajo de 22 px (GDD v3 §9). */
	FSlateFontInfo Serif(float Pixels) const;
	FSlateFontInfo SerifBold(float Pixels) const;
	FSlateFontInfo SerifItalic(float Pixels) const;
	static constexpr float MinTextPixels = 22.0f;

	FSlateFontInfo FontTitle() const;
	FSlateFontInfo FontSubtitle() const;
	FSlateFontInfo FontHeading() const;
	FSlateFontInfo FontBody() const;
	FSlateFontInfo FontSmall() const;

	const FSlateBrush* BrushPanel() const { return &PanelBrush; }
	const FSlateBrush* BrushPanelLight() const { return &PanelLightBrush; }
	/** Hoja de papel crema con esquinas algo redondeadas (mapa en las manos). */
	const FSlateBrush* BrushSheet() const { return &SheetBrush; }
	/** Pulgar: óvalo de color piel. */
	const FSlateBrush* BrushThumb() const { return &ThumbBrush; }
	/** Relleno liso blanco para teñir (siluetas, manchas). */
	const FSlateBrush* BrushWhite() const { return &WhiteBrush; }
	/** Caja blanca redondeada para teñir: radio pequeño (4), medio (10) o píldora. */
	const FSlateBrush* BrushRound(int32 RadiusClass) const;
	/** Solo el contorno redondeado (blanco, 3 px), para teñir. */
	const FSlateBrush* BrushOutline() const { return &OutlineBrush; }

	/** Icono plano de la hoja, en blanco: se tiñe con ColorAndOpacity o el tinte del dibujo. Nunca nulo. */
	const FSlateBrush* IconBrush(EExploredIcon Icon) const;
	/** Textura de la hoja de iconos (cargada del PNG la primera vez; nula sin el fichero). */
	UTexture2D* IconTexture() const;

	/** Botón de menú: fondo transparente en reposo, resalte ámbar al pasar el ratón, hueco al pulsar. */
	const FButtonStyle& ButtonStyle() const { return MenuButtonStyle; }
	/** Botón sobre fondo oscuro: transparente, velo de tinta al pasar el ratón. */
	const FButtonStyle& SceneButtonStyle() const { return SceneButtonStyleValue; }
	/** Botón secundario más pequeño (pestañas de ajustes). */
	const FButtonStyle& TabButtonStyle(bool bActive) const { return bActive ? TabButtonActiveStyle : TabButtonInactiveStyle; }
	const FCheckBoxStyle& CheckBoxStyle() const { return CheckBoxStyleValue; }
	const FSliderStyle& SliderStyle() const { return SliderStyleValue; }
	const FScrollBarStyle& ScrollBarStyle() const { return ScrollBarStyleValue; }
	/** Barra de progreso de logros y del museo: fondo oscuro y relleno ámbar. */
	const FProgressBarStyle& ProgressBarStyle() const { return ProgressBarStyleValue; }
	/** Cuadro de texto corto (texto de las marcas del mapa). */
	const FEditableTextBoxStyle& EditableTextBoxStyle() const { return EditableTextBoxStyleValue; }

private:
	FExploredUIStyle();

	FSlateRoundedBoxBrush PanelBrush;
	FSlateRoundedBoxBrush PanelLightBrush;
	FSlateRoundedBoxBrush ButtonNormalBrush;
	FSlateRoundedBoxBrush ButtonHoveredBrush;
	FSlateRoundedBoxBrush ButtonPressedBrush;
	FSlateColorBrush TransparentBrush;
	FSlateRoundedBoxBrush SheetBrush;
	FSlateRoundedBoxBrush ThumbBrush;
	FSlateColorBrush WhiteBrush;
	FSlateRoundedBoxBrush RoundSmallBrush;
	FSlateRoundedBoxBrush RoundMediumBrush;
	FSlateRoundedBoxBrush RoundPillBrush;
	FSlateRoundedBoxBrush OutlineBrush;
	mutable TArray<FSlateBrush> IconBrushes;
	mutable UTexture2D* IconSheet = nullptr;
	mutable bool bTriedIconSheet = false;
	TSharedPtr<const struct FCompositeFont> RegularFont;
	TSharedPtr<const struct FCompositeFont> BoldFont;
	TSharedPtr<const struct FCompositeFont> ItalicFont;

	FButtonStyle MenuButtonStyle;
	FButtonStyle SceneButtonStyleValue;
	FSlateRoundedBoxBrush SceneHoverBrush;
	FButtonStyle TabButtonActiveStyle;
	FButtonStyle TabButtonInactiveStyle;
	FCheckBoxStyle CheckBoxStyleValue;
	FSliderStyle SliderStyleValue;
	FScrollBarStyle ScrollBarStyleValue;
	FProgressBarStyle ProgressBarStyleValue;
	FEditableTextBoxStyle EditableTextBoxStyleValue;
};
