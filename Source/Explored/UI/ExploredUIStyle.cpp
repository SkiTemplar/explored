#include "UI/ExploredUIStyle.h"

#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

#include "Explored.h"
#include "UI/ExploredUIPalette.h"

namespace ExploredUIStyleDetail
{
	/** Slate mide las fuentes en puntos a 96 ppp: 1 pt = 4/3 px. */
	constexpr float PointsPerPixel = 0.75f;

	FString FontPath(const TCHAR* File)
	{
		return FPaths::ProjectContentDir() / TEXT("Slate/Fonts") / File;
	}

	TSharedPtr<const FCompositeFont> LoadFont(const TCHAR* File)
	{
		const FString Path = FontPath(File);
		if (!FPaths::FileExists(Path))
		{
			UE_LOG(LogExplored, Warning, TEXT("[UI] Falta la fuente %s: se usa la del motor"), *Path);
			return nullptr;
		}
		return MakeShared<FStandaloneCompositeFont>(NAME_None, Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	}

	FSlateFontInfo Make(const TSharedPtr<const FCompositeFont>& Font, const TCHAR* Fallback, float Pixels)
	{
		const float Points = FMath::Max(Pixels, 1.0f) * PointsPerPixel;
		return Font.IsValid() ? FSlateFontInfo(Font, Points) : FCoreStyle::GetDefaultFontStyle(Fallback, Points);
	}
}

FExploredUIStyle::FExploredUIStyle()
	: PanelBrush(ColorPaper(), 10.0f, ExploredPalette::Linear(ExploredPalette::MaderaOscura, 0.55f), 2.0f)
	, PanelLightBrush(ColorPaperLight(), 6.0f)
	, ButtonNormalBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 6.0f)
	, ButtonHoveredBrush(ColorAccentDim(), 6.0f)
	, ButtonPressedBrush(ExploredPalette::Linear(ExploredPalette::CieloOcaso), 6.0f)
	, TransparentBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
	, SheetBrush(ColorSheet(), 6.0f, ExploredPalette::Linear(ExploredPalette::MaderaOscura, 0.55f), 2.0f)
	, ThumbBrush(ColorSkin(), 22.0f)
	, WhiteBrush(FLinearColor::White)
	, RoundSmallBrush(FLinearColor::White, 4.0f)
	, RoundMediumBrush(FLinearColor::White, 10.0f)
	, RoundPillBrush(FLinearColor::White, 8.0f)
	, OutlineBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f), 10.0f, FLinearColor::White, 3.0f)
	, SceneHoverBrush(ExploredPalette::Linear(ExploredPalette::Tinta, 0.4f), 8.0f)
{
	using namespace ExploredUIStyleDetail;
	RegularFont = LoadFont(TEXT("Lora-Regular.ttf"));
	BoldFont = LoadFont(TEXT("Lora-Bold.ttf"));
	ItalicFont = LoadFont(TEXT("Lora-Italic.ttf"));

	MenuButtonStyle.SetNormal(ButtonNormalBrush);
	MenuButtonStyle.SetHovered(ButtonHoveredBrush);
	MenuButtonStyle.SetPressed(ButtonPressedBrush);
	MenuButtonStyle.SetDisabled(ButtonNormalBrush);
	MenuButtonStyle.SetNormalPadding(FMargin(16.0f, 6.0f));
	MenuButtonStyle.SetPressedPadding(FMargin(16.0f, 7.0f, 16.0f, 5.0f));

	// Píldora y círculo: radio de media altura (un radio fijo mayor que la caja no se dibuja).
	RoundPillBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;

	SceneButtonStyleValue.SetNormal(TransparentBrush);
	SceneButtonStyleValue.SetHovered(SceneHoverBrush);
	SceneButtonStyleValue.SetPressed(SceneHoverBrush);
	SceneButtonStyleValue.SetDisabled(TransparentBrush);
	SceneButtonStyleValue.SetNormalPadding(FMargin(20.0f, 8.0f));
	SceneButtonStyleValue.SetPressedPadding(FMargin(20.0f, 9.0f, 20.0f, 7.0f));

	TabButtonActiveStyle.SetNormal(ButtonPressedBrush);
	TabButtonActiveStyle.SetHovered(ButtonPressedBrush);
	TabButtonActiveStyle.SetPressed(ButtonPressedBrush);
	TabButtonActiveStyle.SetDisabled(ButtonPressedBrush);
	TabButtonActiveStyle.SetNormalPadding(FMargin(14.0f, 6.0f));
	TabButtonActiveStyle.SetPressedPadding(FMargin(14.0f, 6.0f));

	TabButtonInactiveStyle.SetNormal(ButtonNormalBrush);
	TabButtonInactiveStyle.SetHovered(ButtonHoveredBrush);
	TabButtonInactiveStyle.SetPressed(ButtonPressedBrush);
	TabButtonInactiveStyle.SetDisabled(ButtonNormalBrush);
	TabButtonInactiveStyle.SetNormalPadding(FMargin(14.0f, 6.0f));
	TabButtonInactiveStyle.SetPressedPadding(FMargin(14.0f, 6.0f));

	const FLinearColor Ink = ColorInk();
	static const FSlateRoundedBoxBrush UncheckedBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 4.0f, Ink, 2.0f, FVector2D(24.0f, 24.0f));
	static const FSlateRoundedBoxBrush CheckedBrush(ColorAccent(), 4.0f, Ink, 2.0f, FVector2D(24.0f, 24.0f));
	static const FSlateRoundedBoxBrush CheckHoveredBrush(ColorAccentDim(), 4.0f, Ink, 2.0f, FVector2D(24.0f, 24.0f));
	CheckBoxStyleValue.SetUncheckedImage(UncheckedBrush);
	CheckBoxStyleValue.SetUncheckedHoveredImage(CheckHoveredBrush);
	CheckBoxStyleValue.SetUncheckedPressedImage(CheckHoveredBrush);
	CheckBoxStyleValue.SetCheckedImage(CheckedBrush);
	CheckBoxStyleValue.SetCheckedHoveredImage(CheckedBrush);
	CheckBoxStyleValue.SetCheckedPressedImage(CheckedBrush);

	static const FSlateRoundedBoxBrush BarBrush(ExploredPalette::Linear(ExploredPalette::ArenaOscura), 3.0f, FVector2D(1.0f, 6.0f));
	static const FSlateRoundedBoxBrush SliderThumbBrush(ColorAccent(), 10.0f, Ink, 2.0f, FVector2D(20.0f, 20.0f));
	SliderStyleValue.SetNormalBarImage(BarBrush);
	SliderStyleValue.SetHoveredBarImage(BarBrush);
	SliderStyleValue.SetNormalThumbImage(SliderThumbBrush);
	SliderStyleValue.SetHoveredThumbImage(SliderThumbBrush);
	SliderStyleValue.SetBarThickness(6.0f);

	static const FSlateRoundedBoxBrush ScrollThumbBrush(ExploredPalette::Linear(ExploredPalette::MaderaClara), 3.0f);
	ScrollBarStyleValue.SetNormalThumbImage(ScrollThumbBrush);
	ScrollBarStyleValue.SetHoveredThumbImage(ScrollThumbBrush);
	ScrollBarStyleValue.SetDraggedThumbImage(ScrollThumbBrush);

	static const FSlateRoundedBoxBrush ProgressBackBrush(ExploredPalette::Linear(ExploredPalette::ArenaOscura), 4.0f);
	static const FSlateRoundedBoxBrush ProgressFillBrush(ExploredPalette::Linear(ExploredPalette::Laton), 4.0f);
	ProgressBarStyleValue.SetBackgroundImage(ProgressBackBrush);
	ProgressBarStyleValue.SetFillImage(ProgressFillBrush);
	ProgressBarStyleValue.SetMarqueeImage(ProgressFillBrush);

	static const FSlateRoundedBoxBrush TextNormalBrush(ExploredPalette::Linear(ExploredPalette::ArenaClara, 0.7f), 4.0f);
	static const FSlateRoundedBoxBrush TextFocusedBrush(ExploredPalette::Linear(ExploredPalette::ArenaClara), 4.0f);
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

FLinearColor FExploredUIStyle::ColorPaper() const { return ExploredPalette::Linear(ExploredPalette::Papel, 0.97f); }
FLinearColor FExploredUIStyle::ColorPaperLight() const { return ExploredPalette::Linear(ExploredPalette::ArenaClara, 0.85f); }
FLinearColor FExploredUIStyle::ColorInk() const { return ExploredPalette::Linear(ExploredPalette::Tinta); }
FLinearColor FExploredUIStyle::ColorInkDim() const { return ExploredPalette::Linear(ExploredPalette::MaderaOscura); }
FLinearColor FExploredUIStyle::ColorAccent() const { return ExploredPalette::Linear(ExploredPalette::RojoFaro); }
FLinearColor FExploredUIStyle::ColorAccentDim() const { return ExploredPalette::Linear(ExploredPalette::ArenaClara); }
FLinearColor FExploredUIStyle::ColorDisabled() const { return ExploredPalette::Linear(ExploredPalette::RocaMedia); }
FLinearColor FExploredUIStyle::ColorSheet() const { return ExploredPalette::Linear(ExploredPalette::Papel); }
FLinearColor FExploredUIStyle::ColorSheetInk() const { return ExploredPalette::Linear(ExploredPalette::Tinta); }
FLinearColor FExploredUIStyle::ColorSkin() const { return ExploredPalette::Linear(ExploredPalette::Piel); }
FLinearColor FExploredUIStyle::ColorWarning() const { return ExploredPalette::Linear(ExploredPalette::RojoFaro); }
FLinearColor FExploredUIStyle::ColorNoteInk() const { return ExploredPalette::Linear(ExploredPalette::AguaProfunda); }
FLinearColor FExploredUIStyle::ColorOnScene() const { return ExploredPalette::Linear(ExploredPalette::Papel); }

FSlateFontInfo FExploredUIStyle::Serif(float Pixels) const
{
	return ExploredUIStyleDetail::Make(RegularFont, TEXT("Regular"), Pixels);
}

FSlateFontInfo FExploredUIStyle::SerifBold(float Pixels) const
{
	return ExploredUIStyleDetail::Make(BoldFont, TEXT("Bold"), Pixels);
}

FSlateFontInfo FExploredUIStyle::SerifItalic(float Pixels) const
{
	return ExploredUIStyleDetail::Make(ItalicFont, TEXT("Italic"), Pixels);
}

FSlateFontInfo FExploredUIStyle::FontTitle() const { return SerifBold(84.0f); }
FSlateFontInfo FExploredUIStyle::FontSubtitle() const { return SerifItalic(30.0f); }
FSlateFontInfo FExploredUIStyle::FontHeading() const { return SerifBold(34.0f); }
FSlateFontInfo FExploredUIStyle::FontBody() const { return Serif(26.0f); }
FSlateFontInfo FExploredUIStyle::FontSmall() const { return Serif(MinTextPixels + 1.0f); }

const FSlateBrush* FExploredUIStyle::BrushRound(int32 RadiusClass) const
{
	return RadiusClass <= 0 ? &RoundSmallBrush : RadiusClass == 1 ? &RoundMediumBrush : &RoundPillBrush;
}

UTexture2D* FExploredUIStyle::IconTexture() const
{
	if (bTriedIconSheet)
	{
		return IconSheet;
	}
	bTriedIconSheet = true;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Slate/T_IconosUI.png");
	IconSheet = FPaths::FileExists(Path) ? FImageUtils::ImportFileAsTexture2D(Path) : nullptr;
	if (!IconSheet)
	{
		UE_LOG(LogExplored, Warning, TEXT("[UI] No se pudo cargar la hoja de iconos %s"), *Path);
		return nullptr;
	}
	// Vive lo que el proceso: la comparten todos los widgets y el HUD.
	IconSheet->AddToRoot();
	IconSheet->Filter = TF_Bilinear;
	IconSheet->LODGroup = TEXTUREGROUP_UI;
	return IconSheet;
}

const FSlateBrush* FExploredUIStyle::IconBrush(EExploredIcon Icon) const
{
	const int32 Count = static_cast<int32>(EExploredIcon::Count);
	if (IconBrushes.Num() != Count)
	{
		IconBrushes.SetNum(Count);
		UTexture2D* Texture = IconTexture();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FSlateBrush& Brush = IconBrushes[Index];
			Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
			Brush.ImageSize = FVector2D(ExploredIconSheet::CellPixels, ExploredIconSheet::CellPixels);
			if (Texture)
			{
				Brush.SetResourceObject(Texture);
				Brush.SetUVRegion(ExploredIconSheet::UVRegion(static_cast<EExploredIcon>(Index)));
			}
		}
	}
	const int32 Index = static_cast<int32>(Icon);
	return IconBrushes.IsValidIndex(Index) ? &IconBrushes[Index] : &TransparentBrush;
}
