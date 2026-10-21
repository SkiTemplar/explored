#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Geometry.h"

class FSlateWindowElementList;
struct FSlateBrush;
enum class EExploredIcon : uint8;

/**
 * Pincel del HUD de Baliza Siete: dibuja cajas redondeadas, texto con Lora e iconos de la hoja sobre
 * la geometría de SExploredHudOverlay, en píxeles de diseño (1080p; Slate escala con los ppp).
 * Lo usa AExploredHUD::PaintOverlay; no guarda estado entre fotogramas.
 */
class EXPLORED_API FExploredHudPainter
{
public:
	FExploredHudPainter(const FGeometry& InGeometry, FSlateWindowElementList& InElements, int32 InLayer);

	FVector2f Size() const;
	/** Última capa usada (OnPaint la devuelve). */
	int32 GetLayer() const { return Layer; }

	/** Caja redondeada teñida: 0 radio pequeño, 1 medio, 2 píldora. */
	void Round(const FVector2f& Pos, const FVector2f& BoxSize, int32 RadiusClass, const FLinearColor& Color);
	/** Contorno redondeado (3 px). */
	void Outline(const FVector2f& Pos, const FVector2f& BoxSize, const FLinearColor& Color);
	void Brush(const FVector2f& Pos, const FVector2f& BoxSize, const FSlateBrush* Brush, const FLinearColor& Color);
	void Icon(const FVector2f& Pos, float Side, EExploredIcon Icon, const FLinearColor& Color);
	/** Texto con la esquina superior izquierda en Pos; con sombra, una copia oscura desplazada 2 px debajo. */
	void Text(const FVector2f& Pos, const FString& Line, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow = false);

	static FVector2f Measure(const FString& Line, const FSlateFontInfo& Font);
	/** Parte un texto en líneas que caben en MaxWidth. */
	static TArray<FString> Wrap(const FString& Text, const FSlateFontInfo& Font, float MaxWidth);

private:
	const FGeometry& Geometry;
	FSlateWindowElementList& Elements;
	/** Capa del siguiente dibujo: cada llamada usa una nueva, así el orden de pintado es el de las llamadas (Slate agrupa por textura dentro de una capa). */
	int32 Layer = 0;
	int32 NextLayer();
};
