#include "UI/Widgets/ExploredUIWidgets.h"

#include "Core/ExploredLocalization.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "UI/ExploredUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"

TSharedRef<SWidget> ExploredUIWidgets::MakeMenuButton(const FText& Label, FOnClicked OnClicked, bool bLarge)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	return SNew(SButton)
		.ButtonStyle(&Style.ButtonStyle())
		.OnClicked(OnClicked)
		.ContentPadding(FMargin(0.0f))
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(bLarge ? Style.FontHeading() : Style.FontBody())
			.ColorAndOpacity(FSlateColor(Style.ColorInk()))
		];
}

TSharedRef<SWidget> ExploredUIWidgets::MakeTabButton(const FText& Label, bool bActive, FOnClicked OnClicked)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	return SNew(SButton)
		.ButtonStyle(&Style.TabButtonStyle(bActive))
		.OnClicked(OnClicked)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Style.FontBody())
			.ColorAndOpacity(FSlateColor(bActive ? Style.ColorAccent() : Style.ColorInkDim()))
		];
}

FString ExploredUIWidgets::CurrentCultureName()
{
	return FInternationalization::Get().GetCurrentCulture()->GetName();
}

FText ExploredUIWidgets::PickLocalized(const FString& Es, const FString& En)
{
	return FText::FromString(ExploredLocalization::Pick(Es, En, CurrentCultureName()));
}

TSharedRef<SWidget> ExploredUIWidgets::MakeScreenPanel(const FText& Title, TAttribute<FText> Subtitle, TSharedRef<SWidget> Body,
	TSharedRef<SWidget> Footer, float Width, float Height)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(Style.BrushPanel())
			.Padding(FMargin(40.0f, 28.0f))
			[
				SNew(SBox)
				.WidthOverride(Width)
				.HeightOverride(Height)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Title)
						.Font(Style.FontHeading())
						.ColorAndOpacity(FSlateColor(Style.ColorInk()))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 16.0f))
					[
						SNew(STextBlock)
						.Text(Subtitle)
						.Font(Style.FontBody())
						.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						Body
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f))
					[
						Footer
					]
				]
			]
		];
}

TSharedRef<SWidget> ExploredUIWidgets::MakeProgressBar(TAttribute<TOptional<float>> Percent, float Height)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	return SNew(SBox)
		.HeightOverride(Height)
		[
			SNew(SProgressBar)
			.Style(&Style.ProgressBarStyle())
			.Percent(Percent)
			.FillColorAndOpacity(FSlateColor(FLinearColor::White))
		];
}
