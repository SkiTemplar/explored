#include "UI/ExploredUIStyle.h"

#include "Styling/CoreStyle.h"

FExploredUIStyle::FExploredUIStyle()
	: PanelBrush(ColorPaper(), 10.0f)
	, PanelLightBrush(ColorPaperLight(), 6.0f)
	, ButtonNormalBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 4.0f)
	, ButtonHoveredBrush(ColorAccentDim(), 4.0f)
	, ButtonPressedBrush(ColorAccent(), 4.0f)
	, TransparentBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
	, SheetBrush(ColorSheet(), 6.0f)
	, ThumbBrush(ColorSkin(), 22.0f)
	, WhiteBrush(FLinearColor::White)
{
	MenuButtonStyle.SetNormal(ButtonNormalBrush);
	MenuButtonStyle.SetHovered(ButtonHoveredBrush);
	MenuButtonStyle.SetPressed(ButtonPressedBrush);
	MenuButtonStyle.SetDisabled(ButtonNormalBrush);
	MenuButtonStyle.SetNormalPadding(FMargin(18.0f, 10.0f));
	MenuButtonStyle.SetPressedPadding(FMargin(18.0f, 10.0f));

	TabButtonActiveStyle.SetNormal(ButtonPressedBrush);
	TabButtonActiveStyle.SetHovered(ButtonPressedBrush);
	TabButtonActiveStyle.SetPressed(ButtonPressedBrush);
	TabButtonActiveStyle.SetDisabled(ButtonPressedBrush);
	TabButtonActiveStyle.SetNormalPadding(FMargin(14.0f, 8.0f));
	TabButtonActiveStyle.SetPressedPadding(FMargin(14.0f, 8.0f));

	TabButtonInactiveStyle.SetNormal(ButtonNormalBrush);
	TabButtonInactiveStyle.SetHovered(ButtonHoveredBrush);
	TabButtonInactiveStyle.SetPressed(ButtonPressedBrush);
	TabButtonInactiveStyle.SetDisabled(ButtonNormalBrush);
	TabButtonInactiveStyle.SetNormalPadding(FMargin(14.0f, 8.0f));
	TabButtonInactiveStyle.SetPressedPadding(FMargin(14.0f, 8.0f));

	static const FSlateRoundedBoxBrush UncheckedBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.25f), 3.0f, FVector2D(22.0f, 22.0f));
	static const FSlateRoundedBoxBrush CheckedBrush(ColorAccent(), 3.0f, FVector2D(22.0f, 22.0f));
	static const FSlateRoundedBoxBrush CheckHoveredBrush(ColorAccentDim(), 3.0f, FVector2D(22.0f, 22.0f));
	CheckBoxStyleValue.SetUncheckedImage(UncheckedBrush);
	CheckBoxStyleValue.SetUncheckedHoveredImage(CheckHoveredBrush);
	CheckBoxStyleValue.SetUncheckedPressedImage(CheckHoveredBrush);
	CheckBoxStyleValue.SetCheckedImage(CheckedBrush);
	CheckBoxStyleValue.SetCheckedHoveredImage(CheckedBrush);
	CheckBoxStyleValue.SetCheckedPressedImage(CheckedBrush);

	static const FSlateRoundedBoxBrush BarBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 3.0f, FVector2D(1.0f, 6.0f));
	static const FSlateRoundedBoxBrush SliderThumbBrush(ColorAccent(), 8.0f, FVector2D(16.0f, 16.0f));
	SliderStyleValue.SetNormalBarImage(BarBrush);
	SliderStyleValue.SetHoveredBarImage(BarBrush);
	SliderStyleValue.SetNormalThumbImage(SliderThumbBrush);
	SliderStyleValue.SetHoveredThumbImage(SliderThumbBrush);
	SliderStyleValue.SetBarThickness(6.0f);

	static const FSlateRoundedBoxBrush ScrollThumbBrush(ColorAccentDim(), 3.0f);
	ScrollBarStyleValue.SetNormalThumbImage(ScrollThumbBrush);
	ScrollBarStyleValue.SetHoveredThumbImage(ScrollThumbBrush);
	ScrollBarStyleValue.SetDraggedThumbImage(ScrollThumbBrush);

	static const FSlateRoundedBoxBrush ProgressBackBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 3.0f);
	static const FSlateRoundedBoxBrush ProgressFillBrush(ColorAccent(), 3.0f);
	ProgressBarStyleValue.SetBackgroundImage(ProgressBackBrush);
	ProgressBarStyleValue.SetFillImage(ProgressFillBrush);
	ProgressBarStyleValue.SetMarqueeImage(ProgressFillBrush);

	// Cuadro de texto sobre el papel: fondo crema algo más claro, tinta sepia.
	static const FSlateRoundedBoxBrush TextNormalBrush(FLinearColor(1.0f, 0.97f, 0.88f, 0.9f), 3.0f);
	static const FSlateRoundedBoxBrush TextFocusedBrush(FLinearColor(1.0f, 0.93f, 0.78f, 1.0f), 3.0f);
	EditableTextBoxStyleValue.SetBackgroundImageNormal(TextNormalBrush);
	EditableTextBoxStyleValue.SetBackgroundImageHovered(TextFocusedBrush);
	EditableTextBoxStyleValue.SetBackgroundImageFocused(TextFocusedBrush);
	EditableTextBoxStyleValue.SetBackgroundImageReadOnly(TextNormalBrush);
	EditableTextBoxStyleValue.SetTextStyle(FTextBlockStyle()
		.SetFont(FontBody())
		.SetColorAndOpacity(FSlateColor(ColorSheetInk()))
		.SetSelectedBackgroundColor(FSlateColor(ColorAccentDim()))
		.SetHighlightColor(ColorAccent()));
	EditableTextBoxStyleValue.SetForegroundColor(FSlateColor(ColorSheetInk()));
	EditableTextBoxStyleValue.SetFocusedForegroundColor(FSlateColor(ColorSheetInk()));
	EditableTextBoxStyleValue.SetPadding(FMargin(8.0f, 4.0f));
}

FExploredUIStyle& FExploredUIStyle::Get()
{
	static FExploredUIStyle Instance;
	return Instance;
}

FSlateFontInfo FExploredUIStyle::FontTitle() const
{
	return FCoreStyle::GetDefaultFontStyle("Bold", 72);
}

FSlateFontInfo FExploredUIStyle::FontSubtitle() const
{
	return FCoreStyle::GetDefaultFontStyle("Regular", 20);
}

FSlateFontInfo FExploredUIStyle::FontHeading() const
{
	return FCoreStyle::GetDefaultFontStyle("Bold", 24);
}

FSlateFontInfo FExploredUIStyle::FontBody() const
{
	return FCoreStyle::GetDefaultFontStyle("Regular", 16);
}

FSlateFontInfo FExploredUIStyle::FontSmall() const
{
	return FCoreStyle::GetDefaultFontStyle("Regular", 12);
}
