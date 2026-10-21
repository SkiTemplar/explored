#include "UI/ExploredHudPainter.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

#include "UI/ExploredUIPalette.h"
#include "UI/ExploredUIStyle.h"

FExploredHudPainter::FExploredHudPainter(const FGeometry& InGeometry, FSlateWindowElementList& InElements, int32 InLayer)
	: Geometry(InGeometry)
	, Elements(InElements)
	, Layer(InLayer)
{
}

int32 FExploredHudPainter::NextLayer()
{
	return ++Layer;
}

FVector2f FExploredHudPainter::Size() const
{
	return FVector2f(Geometry.GetLocalSize());
}

void FExploredHudPainter::Brush(const FVector2f& Pos, const FVector2f& BoxSize, const FSlateBrush* InBrush, const FLinearColor& Color)
{
	if (!InBrush || Color.A <= 0.0f)
	{
		return;
	}
	FSlateDrawElement::MakeBox(Elements, NextLayer(), Geometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(Pos)), InBrush,
		ESlateDrawEffect::None, Color);
}

void FExploredHudPainter::Round(const FVector2f& Pos, const FVector2f& BoxSize, int32 RadiusClass, const FLinearColor& Color)
{
	Brush(Pos, BoxSize, FExploredUIStyle::Get().BrushRound(RadiusClass), Color);
}

void FExploredHudPainter::Outline(const FVector2f& Pos, const FVector2f& BoxSize, const FLinearColor& Color)
{
	// Cuatro tiras: un contorno de caja redondeada se rellenaría con el tinte.
	constexpr float Thick = 3.0f;
	const FSlateBrush* White = FExploredUIStyle::Get().BrushWhite();
	Brush(Pos, FVector2f(BoxSize.X, Thick), White, Color);
	Brush(FVector2f(Pos.X, Pos.Y + BoxSize.Y - Thick), FVector2f(BoxSize.X, Thick), White, Color);
	Brush(FVector2f(Pos.X, Pos.Y + Thick), FVector2f(Thick, BoxSize.Y - 2.0f * Thick), White, Color);
	Brush(FVector2f(Pos.X + BoxSize.X - Thick, Pos.Y + Thick), FVector2f(Thick, BoxSize.Y - 2.0f * Thick), White, Color);
}

void FExploredHudPainter::Icon(const FVector2f& Pos, float Side, EExploredIcon InIcon, const FLinearColor& Color)
{
	Brush(Pos, FVector2f(Side, Side), FExploredUIStyle::Get().IconBrush(InIcon), Color);
}

void FExploredHudPainter::Text(const FVector2f& Pos, const FString& Line, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow)
{
	if (Line.IsEmpty() || Color.A <= 0.0f)
	{
		return;
	}
	const FVector2f TextSize = Measure(Line, Font);
	if (bShadow)
	{
		const FLinearColor Shadow = ExploredPalette::Linear(ExploredPalette::Tinta, 0.65f * Color.A);
		FSlateDrawElement::MakeText(Elements, NextLayer(), Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Pos + FVector2f(2.0f, 2.0f))),
			Line, Font, ESlateDrawEffect::None, Shadow);
	}
	FSlateDrawElement::MakeText(Elements, NextLayer(), Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Pos)), Line, Font,
		ESlateDrawEffect::None, Color);
}

FVector2f FExploredHudPainter::Measure(const FString& Line, const FSlateFontInfo& Font)
{
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		return FVector2f(Line.Len() * Font.Size * 0.7f, Font.Size * 1.4f);
	}
	const TSharedRef<FSlateFontMeasure> Service = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return FVector2f(Service->Measure(Line, Font));
}

TArray<FString> FExploredHudPainter::Wrap(const FString& Text, const FSlateFontInfo& Font, float MaxWidth)
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	TArray<FString> Lines;
	FString Current;
	for (const FString& Word : Words)
	{
		const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
		if (!Current.IsEmpty() && Measure(Candidate, Font).X > MaxWidth)
		{
			Lines.Add(Current);
			Current = Word;
		}
		else
		{
			Current = Candidate;
		}
	}
	if (!Current.IsEmpty())
	{
		Lines.Add(Current);
	}
	return Lines;
}
