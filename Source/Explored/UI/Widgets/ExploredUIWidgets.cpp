#include "UI/Widgets/ExploredUIWidgets.h"

#include "UI/ExploredUIStyle.h"

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
