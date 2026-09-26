#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"

/**
 * Estilo visual compartido del frontend: «diario de explorador» cálido y
 * limpio. Todo en código, sin assets de UI (pinceles de color/redondeados y
 * tipografía por defecto del motor). Un único punto de verdad para que todos
 * los widgets Slate del módulo compartan look & feel.
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

	// Paleta cálida: papel envejecido, tinta sepia, acento ámbar.
	FLinearColor ColorPaper() const { return FLinearColor(0.114f, 0.090f, 0.071f, 0.82f); }
	FLinearColor ColorPaperLight() const { return FLinearColor(0.160f, 0.130f, 0.100f, 0.90f); }
	FLinearColor ColorInk() const { return FLinearColor(0.96f, 0.90f, 0.78f, 1.0f); }
	FLinearColor ColorInkDim() const { return FLinearColor(0.80f, 0.72f, 0.60f, 0.85f); }
	FLinearColor ColorAccent() const { return FLinearColor(0.86f, 0.55f, 0.18f, 1.0f); }
	FLinearColor ColorAccentDim() const { return FLinearColor(0.60f, 0.38f, 0.13f, 1.0f); }
	FLinearColor ColorDisabled() const { return FLinearColor(0.35f, 0.32f, 0.28f, 0.6f); }

	FSlateFontInfo FontTitle() const;
	FSlateFontInfo FontSubtitle() const;
	FSlateFontInfo FontHeading() const;
	FSlateFontInfo FontBody() const;
	FSlateFontInfo FontSmall() const;

	const FSlateBrush* BrushPanel() const { return &PanelBrush; }
	const FSlateBrush* BrushPanelLight() const { return &PanelLightBrush; }

	/** Botón de menú: fondo transparente en reposo, resalte ámbar al pasar el ratón, hueco al pulsar. */
	const FButtonStyle& ButtonStyle() const { return MenuButtonStyle; }
	/** Botón secundario más pequeño (pestañas de ajustes). */
	const FButtonStyle& TabButtonStyle(bool bActive) const { return bActive ? TabButtonActiveStyle : TabButtonInactiveStyle; }
	const FCheckBoxStyle& CheckBoxStyle() const { return CheckBoxStyleValue; }
	const FSliderStyle& SliderStyle() const { return SliderStyleValue; }
	const FScrollBarStyle& ScrollBarStyle() const { return ScrollBarStyleValue; }

private:
	FExploredUIStyle();

	FSlateRoundedBoxBrush PanelBrush;
	FSlateRoundedBoxBrush PanelLightBrush;
	FSlateRoundedBoxBrush ButtonNormalBrush;
	FSlateRoundedBoxBrush ButtonHoveredBrush;
	FSlateRoundedBoxBrush ButtonPressedBrush;
	FSlateColorBrush TransparentBrush;

	FButtonStyle MenuButtonStyle;
	FButtonStyle TabButtonActiveStyle;
	FButtonStyle TabButtonInactiveStyle;
	FCheckBoxStyle CheckBoxStyleValue;
	FSliderStyle SliderStyleValue;
	FScrollBarStyle ScrollBarStyleValue;
};
